/* SPDX-License-Identifier: MIT */
/* sim.* functions for rigid bodies and projectiles, and keeping bodies with a
 * saved game. */
#include "core/log.h"
#include "script/script_internal.h"
#include "sim/tier.h"

#include <stdio.h>
#include <string.h>

static zb_bodies *BD(lua_State *L) {
  zb_tier *t = zb_script_from(L)->cfg.tier;
  zb_bodies *b = t ? zb_tier_bodies(t) : NULL;
  if (!b) {
    luaL_error(L, "no rigid bodies (the material registry is missing)");
  }
  return b;
}

static void set_number(lua_State *L, const char *name, double v) {
  lua_pushnumber(L, v);
  lua_setfield(L, -2, name);
}

static void set_bool(lua_State *L, const char *name, bool v) {
  lua_pushboolean(L, v);
  lua_setfield(L, -2, name);
}

/* A vector { east, north, up } (missing parts are 0). */
static void check_vector(lua_State *L, int idx, const char *what, double out[3]) {
  static const char *const parts[3] = {"east", "north", "up"};
  if (!lua_istable(L, idx)) {
    luaL_error(L, "%s must be a table { east, north, up }", what);
  }
  for (int k = 0; k < 3; ++k) {
    lua_getfield(L, idx, parts[k]);
    out[k] = lua_isnil(L, -1) ? 0.0 : luaL_checknumber(L, -1);
    lua_pop(L, 1);
    if (!(out[k] > -1e6 && out[k] < 1e6)) {
      luaL_error(L, "%s.%s is out of range", what, parts[k]);
    }
  }
}

static void push_vector(lua_State *L, const double v[3]) {
  lua_createtable(L, 0, 3);
  set_number(L, "east", v[0]);
  set_number(L, "north", v[1]);
  set_number(L, "up", v[2]);
}

static void opt_vector(lua_State *L, int idx, const char *field, double out[3]) {
  lua_getfield(L, idx, field);
  if (!lua_isnil(L, -1)) {
    check_vector(L, lua_gettop(L), field, out);
  }
  lua_pop(L, 1);
}

static double opt_number(lua_State *L, int idx, const char *field, double def) {
  lua_getfield(L, idx, field);
  double v = lua_isnil(L, -1) ? def : luaL_checknumber(L, -1);
  lua_pop(L, 1);
  return v;
}

/* @luaclass Vector
 * A direction or amount in the east-north-up frame of a place.
 * @field east number?
 * @field north number?
 * @field up number?
 */

/* Reads a body description at idx; returns false with a message. */
static bool read_desc(lua_State *L, int idx, zb_body_desc *d, char *why, size_t why_len) {
  memset(d, 0, sizeof *d);
  const zb_materials *m = zb_script_from(L)->cfg.materials;
  lua_getfield(L, idx, "shape");
  const char *shape = lua_tostring(L, -1);
  d->shape = ZB_SHAPES;
  for (int k = 0; shape && k < ZB_SHAPES; ++k) {
    d->shape = strcmp(shape, zb_shape_name((zb_shape)k)) ? d->shape : (zb_shape)k;
  }
  lua_pop(L, 1);
  if (d->shape == ZB_SHAPES) {
    snprintf(why, why_len, "shape must be sphere, capsule or box");
    return false;
  }
  lua_getfield(L, idx, "material");
  const char *ref = lua_tostring(L, -1);
  d->material = ref && m ? zb_materials_find(m, ref) : -1;
  lua_pop(L, 1);
  if (d->material < 0) {
    snprintf(why, why_len, "no material %s", ref ? ref : "(none)");
    return false;
  }
  lua_getfield(L, idx, "pos");
  d->pos = zb_script_check_pos(L, lua_gettop(L));
  lua_pop(L, 1);
  d->radius = opt_number(L, idx, "radius", 0.5);
  d->half_length = opt_number(L, idx, "length", 2.0) / 2.0;
  d->half[0] = d->half[1] = d->half[2] = 0.5;
  lua_getfield(L, idx, "size");
  if (lua_istable(L, -1)) {
    for (int k = 0; k < 3; ++k) {
      lua_rawgeti(L, -1, k + 1);
      d->half[k] = luaL_checknumber(L, -1) / 2.0;
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);
  opt_vector(L, idx, "velocity", d->velocity);
  opt_vector(L, idx, "spin", d->spin);
  opt_vector(L, idx, "axis_x", d->axis_x);
  opt_vector(L, idx, "axis_z", d->axis_z);
  lua_getfield(L, idx, "asleep");
  d->asleep = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return true;
}

/* @lua sim.spawn(body: BodyDesc): integer
 * Puts a rigid body into the world: a boulder, a log, a crate. It falls,
 * rolls, collides, floats or sinks in the active tier, and waits where it is
 * outside it. Returns its id; errors for a description that does not work
 * (sizes 1 cm .. 50 m, a solid or powder material).
 * @field shape string "sphere", "capsule" or "box"
 * @field material string A material key or id, e.g. "base:rock" or "wood"
 * @field pos PlanetPos The centre
 * @field radius number? Sphere and capsule, metres (default 0.5)
 * @field length number? Capsule: the length of its straight part (default 2)
 * @field size number[]? Box: { x, y, z } in metres (default 1, 1, 1)
 * @field velocity Vector? m/s { east, north, up }
 * @field spin Vector? rad/s about { east, north, up }
 * @field axis_x Vector? With axis_z: the body's own axes (default upright: x east, z up)
 * @field axis_z Vector?
 * @field asleep boolean? Starts at rest
 */
static int s_spawn(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  zb_bodies *b = BD(L);
  zb_body_desc d;
  char why[96];
  if (!read_desc(L, 1, &d, why, sizeof why)) {
    return luaL_error(L, "%s", why);
  }
  uint32_t id = zb_bodies_add(b, &d);
  if (!id) {
    return luaL_error(L, "this body cannot be made (sizes 0.01..50 m, a solid or powder material)");
  }
  lua_pushinteger(L, id);
  return 1;
}

static void push_state(lua_State *L, uint32_t id, const zb_body_state *st) {
  const zb_materials *m = zb_script_from(L)->cfg.materials;
  lua_createtable(L, 0, 20);
  lua_pushinteger(L, id);
  lua_setfield(L, -2, "id");
  lua_pushstring(L, zb_shape_name(st->shape));
  lua_setfield(L, -2, "shape");
  lua_pushstring(L, m->info[st->material].key);
  lua_setfield(L, -2, "material");
  zb_script_push_pos(L, &st->pos);
  lua_setfield(L, -2, "pos");
  push_vector(L, st->velocity);
  lua_setfield(L, -2, "velocity");
  push_vector(L, st->spin);
  lua_setfield(L, -2, "spin");
  push_vector(L, st->axis_x);
  lua_setfield(L, -2, "axis_x");
  push_vector(L, st->axis_y);
  lua_setfield(L, -2, "axis_y");
  push_vector(L, st->axis_z);
  lua_setfield(L, -2, "axis_z");
  set_number(L, "speed", st->speed);
  set_number(L, "mass", st->mass);
  set_number(L, "submerged", st->submerged);
  set_bool(L, "asleep", st->asleep);
  set_bool(L, "frozen", st->frozen);
  set_bool(L, "grounded", st->grounded);
  if (st->shape == ZB_SHAPE_BOX) {
    lua_createtable(L, 3, 0);
    for (int k = 0; k < 3; ++k) {
      lua_pushnumber(L, 2.0 * st->half[k]);
      lua_rawseti(L, -2, k + 1);
    }
    lua_setfield(L, -2, "size");
  } else {
    set_number(L, "radius", st->radius);
    if (st->shape == ZB_SHAPE_CAPSULE) {
      set_number(L, "length", 2.0 * st->half_length);
    }
  }
}

/* @lua sim.body(id: integer): Body?
 * A body as it is now, or nil when there is no such body.
 * @field id integer
 * @field shape string
 * @field material string Key
 * @field pos PlanetPos The centre
 * @field velocity Vector m/s
 * @field spin Vector rad/s
 * @field speed number m/s
 * @field mass number kg
 * @field submerged number Share of it under water, 0 .. 1
 * @field asleep boolean At rest
 * @field frozen boolean Outside the active tier
 * @field grounded boolean Touching the ground
 * @field axis_x Vector Its own axes in east, north, up terms
 * @field axis_y Vector
 * @field axis_z Vector
 * @field radius number? Sphere and capsule
 * @field length number? Capsule
 * @field size number[]? Box
 */
static int s_body(lua_State *L) {
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  zb_body_state st;
  if (!zb_bodies_get(BD(L), id, &st)) {
    lua_pushnil(L);
    return 1;
  }
  push_state(L, id, &st);
  return 1;
}

/* @lua sim.push(id: integer, impulse: Vector): boolean
 * Gives a body a push at its centre (N s { east, north, up }) and wakes it;
 * false when there is no such body.
 */
static int s_push(lua_State *L) {
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  double imp[3];
  check_vector(L, 2, "impulse", imp);
  lua_pushboolean(L, zb_bodies_push(BD(L), id, imp));
  return 1;
}

/* @lua sim.remove_body(id: integer): boolean
 * Takes a body out of the world; false when there was no such body.
 */
static int s_remove_body(lua_State *L) {
  lua_pushboolean(L, zb_bodies_remove(BD(L), (uint32_t)luaL_checkinteger(L, 1)));
  return 1;
}

/* @lua sim.bodies_near(pos: PlanetPos, radius: number): integer[]
 * Ids of the bodies whose centre is within radius metres (up to 256).
 */
static int s_bodies_near(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  double r = luaL_checknumber(L, 2);
  uint32_t ids[256];
  int n = zb_bodies_near(BD(L), &pos, r, ids, 256);
  lua_createtable(L, n, 0);
  for (int k = 0; k < n; ++k) {
    lua_pushinteger(L, ids[k]);
    lua_rawseti(L, -2, k + 1);
  }
  return 1;
}

/* @lua sim.body_stats(): BodyStats
 * @field count integer Bodies in the world
 * @field awake integer Moving bodies
 * @field frozen integer Bodies outside the active tier
 * @field contacts integer Contacts in the last step
 * @field projectiles integer Projectiles in flight
 * @field kinetic number J of motion in the awake bodies
 */
static int s_body_stats(lua_State *L) {
  zb_bodies_stats st;
  zb_bodies_get_stats(BD(L), &st);
  lua_createtable(L, 0, 6);
  const struct {
    const char *name;
    int v;
  } ints[] = {{"count", st.count},
              {"awake", st.awake},
              {"frozen", st.frozen},
              {"contacts", st.contacts},
              {"projectiles", st.projectiles}};
  for (size_t k = 0; k < sizeof ints / sizeof ints[0]; ++k) {
    lua_pushinteger(L, ints[k].v);
    lua_setfield(L, -2, ints[k].name);
  }
  set_number(L, "kinetic", st.kinetic);
  return 1;
}

static void push_hit(lua_State *L, const zb_hit *h) {
  lua_createtable(L, 0, 4);
  lua_pushinteger(L, h->body);
  lua_setfield(L, -2, "body");
  zb_script_push_pos(L, &h->pos);
  lua_setfield(L, -2, "pos");
  push_vector(L, h->normal);
  lua_setfield(L, -2, "normal");
  set_number(L, "distance", h->distance);
}

/* @lua sim.launch(pos: PlanetPos, velocity: Vector, radius?: number, mass?: number): integer
 * Throws a projectile (a stone, an arrow, a bolt of magic) from pos: a small
 * sphere of radius metres (default 0.05, up to 0.5) and mass kg (default 0.5,
 * up to 100) under gravity. It never passes through anything; a hit stops it
 * and pushes the body it struck. Returns its id; errors when out of range.
 */
static int s_launch(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  double v[3];
  check_vector(L, 2, "velocity", v);
  uint32_t id = zb_bodies_launch(BD(L), &pos, v, luaL_optnumber(L, 3, 0.05), luaL_optnumber(L, 4, 0.5));
  if (!id) {
    return luaL_error(L, "radius must be 0.001..0.5 m and mass 0.001..100 kg (at most 256 in flight)");
  }
  lua_pushinteger(L, id);
  return 1;
}

/* @lua sim.projectile(id: integer): Projectile?
 * A projectile in flight or stopped in the last 10 seconds, else nil.
 * @field flying boolean
 * @field pos PlanetPos
 * @field velocity Vector m/s
 * @field hit Hit? What stopped it: { body (0: the ground), pos, normal, distance }
 */
static int s_projectile(lua_State *L) {
  zb_projectile_state ps;
  if (!zb_bodies_projectile(BD(L), (uint32_t)luaL_checkinteger(L, 1), &ps)) {
    lua_pushnil(L);
    return 1;
  }
  lua_createtable(L, 0, 4);
  set_bool(L, "flying", ps.flying);
  zb_script_push_pos(L, &ps.pos);
  lua_setfield(L, -2, "pos");
  push_vector(L, ps.velocity);
  lua_setfield(L, -2, "velocity");
  if (ps.hit.hit) {
    push_hit(L, &ps.hit);
    lua_setfield(L, -2, "hit");
  }
  return 1;
}

/* @lua sim.sweep(from: PlanetPos, to: PlanetPos, radius?: number): Hit?
 * Sweeps a sphere of radius metres (default 0: a ray) from one place to
 * another: the first body or ground it touches, or nil.
 * @field body integer The body's id; 0 for the ground
 * @field pos PlanetPos The sphere's centre at the moment of contact
 * @field normal Vector The surface it touched, outwards
 * @field distance number Metres along the sweep
 */
static int s_sweep(lua_State *L) {
  zb_planet_pos from = zb_script_check_pos(L, 1), to = zb_script_check_pos(L, 2);
  double r = luaL_optnumber(L, 3, 0.0);
  luaL_argcheck(L, r >= 0.0 && r <= 10.0, 3, "radius must be 0..10 metres");
  zb_hit h = zb_bodies_sweep(BD(L), &from, &to, r);
  if (!h.hit) {
    lua_pushnil(L);
    return 1;
  }
  push_hit(L, &h);
  return 1;
}

static void each_id(void *ctx, const zb_body_pose *p) {
  lua_State *L = ctx;
  lua_pushinteger(L, p->id);
  lua_rawseti(L, -2, (lua_Integer)lua_rawlen(L, -2) + 1);
}

void zb_script_push_bodies(lua_State *L) {
  zb_tier *t = zb_script_from(L)->cfg.tier;
  zb_bodies *b = t ? zb_tier_bodies(t) : NULL;
  lua_newtable(L);
  if (!b) {
    return;
  }
  lua_newtable(L);
  zb_bodies_each(b, each_id, L);
  int ids = lua_gettop(L);
  lua_Integer n = (lua_Integer)lua_rawlen(L, ids);
  for (lua_Integer k = 1; k <= n; ++k) {
    lua_rawgeti(L, ids, k);
    uint32_t id = (uint32_t)lua_tointeger(L, -1);
    lua_pop(L, 1);
    zb_body_state st;
    zb_bodies_get(b, id, &st);
    push_state(L, id, &st);
    /* Only what spawning it again needs. */
    const char *drop[] = {"id", "spin", "axis_y", "speed", "mass", "submerged", "frozen", "grounded"};
    for (size_t i = 0; i < sizeof drop / sizeof drop[0]; ++i) {
      lua_pushnil(L);
      lua_setfield(L, -2, drop[i]);
    }
    lua_rawseti(L, ids - 1, k);
  }
  lua_pop(L, 1);
}

/* One saved body (a table) back into the world; raises when it does not fit. */
static int restore_one(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  zb_body_desc d;
  char why[96];
  if (!read_desc(L, 1, &d, why, sizeof why) || !zb_bodies_add(BD(L), &d)) {
    return luaL_error(L, "not a body");
  }
  return 0;
}

int zb_script_restore_bodies(lua_State *L, int idx) {
  zb_tier *t = zb_script_from(L)->cfg.tier;
  zb_bodies *b = t ? zb_tier_bodies(t) : NULL;
  if (!b) {
    return 0;
  }
  idx = lua_absindex(L, idx);
  zb_bodies_clear(b);
  int restored = 0;
  lua_Integer n = lua_istable(L, idx) ? (lua_Integer)lua_rawlen(L, idx) : 0;
  for (lua_Integer k = 1; k <= n; ++k) {
    lua_pushcfunction(L, restore_one);
    lua_rawgeti(L, idx, k);
    if (lua_pcall(L, 1, 0, 0) == LUA_OK) {
      restored++;
    } else {
      zb_warn("saved body %d left out: %s", (int)k, lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  }
  return restored;
}

void zb_script_open_bodies(zb_script *s) {
  static const luaL_Reg fns[] = {{"spawn", s_spawn},
                                 {"body", s_body},
                                 {"push", s_push},
                                 {"remove_body", s_remove_body},
                                 {"bodies_near", s_bodies_near},
                                 {"body_stats", s_body_stats},
                                 {"launch", s_launch},
                                 {"projectile", s_projectile},
                                 {"sweep", s_sweep},
                                 {NULL, NULL}};
  lua_getglobal(s->L, "sim");
  luaL_setfuncs(s->L, fns, 0);
  lua_pop(s->L, 1);
}

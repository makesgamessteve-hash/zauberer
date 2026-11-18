/* SPDX-License-Identifier: MIT */
/* sim.* functions for creatures: species from data, spawning, reading,
 * hurting and healing, stimuli, the player's whereabouts and events. */
#include "script/script_internal.h"
#include "sim/tier.h"

#include <stdio.h>
#include <string.h>

static zb_creatures *CR(lua_State *L) {
  zb_tier *t = zb_script_from(L)->cfg.tier;
  if (!t) {
    luaL_error(L, "no active tier");
  }
  return zb_tier_creatures(t);
}

static double num(lua_State *L, int idx, const char *field, double def) {
  lua_getfield(L, idx, field);
  double v = lua_isnil(L, -1) ? def : luaL_checknumber(L, -1);
  lua_pop(L, 1);
  return v;
}

static bool flag(lua_State *L, int idx, const char *field) {
  lua_getfield(L, idx, field);
  bool v = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return v;
}

static const char *const DIETS[ZB_DIETS] = {"herbivore", "carnivore", "omnivore"};

/* @lua sim.define_species(species: Species): integer
 * Defines a creature species (a later definition of the same id replaces it);
 * returns its index. Errors for values out of range or too many species.
 * @field id string
 * @field length number Metres nose to tail
 * @field mass number kg
 * @field walk number m/s
 * @field run number m/s (at least walk, up to 60)
 * @field flies boolean?
 * @field cruise number? Metres above the ground for fliers
 * @field diet string "herbivore", "carnivore" or "omnivore"
 * @field pack boolean? Keeps together with its kind
 * @field sight number Metres in daylight
 * @field hearing number Metres
 * @field smell number Metres
 * @field health number
 * @field aggression number? 0 .. 1
 * @field fear_of_fire number? 0 .. 1
 * @field fear_of_player number? 0 .. 1
 * @field resist table? Share of each harm ignored { heat = .., impact = .., cold = .., poison = .., smoke = .., magic = .. }
 * @field magical boolean?
 * @field mana string? The colour of a magical one's mana
 * @field motes number? Mana it releases when it dies
 * @field color number[]? { r, g, b } for drawing
 */
static int s_define_species(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  zb_animal a;
  memset(&a, 0, sizeof a);
  lua_getfield(L, 1, "id");
  snprintf(a.id, sizeof a.id, "%s", luaL_checkstring(L, -1));
  lua_pop(L, 1);
  a.length = num(L, 1, "length", 1.0);
  a.mass = num(L, 1, "mass", 30.0);
  a.walk = num(L, 1, "walk", 1.0);
  a.run = num(L, 1, "run", a.walk);
  a.flies = flag(L, 1, "flies");
  a.cruise = num(L, 1, "cruise", 15.0);
  lua_getfield(L, 1, "diet");
  const char *diet = luaL_optstring(L, -1, "herbivore");
  a.diet = ZB_DIETS;
  for (int k = 0; k < ZB_DIETS; ++k) {
    a.diet = strcmp(diet, DIETS[k]) ? a.diet : (zb_diet)k;
  }
  lua_pop(L, 1);
  a.pack = flag(L, 1, "pack");
  a.sight = num(L, 1, "sight", 60.0);
  a.hearing = num(L, 1, "hearing", 80.0);
  a.smell = num(L, 1, "smell", 0.0);
  a.health = num(L, 1, "health", 30.0);
  a.aggression = num(L, 1, "aggression", 0.0);
  a.fear_of_fire = num(L, 1, "fear_of_fire", 0.8);
  a.fear_of_player = num(L, 1, "fear_of_player", 0.5);
  lua_getfield(L, 1, "resist");
  for (int h = 0; h < ZB_HARMS && lua_istable(L, -1); ++h) {
    a.resist[h] = num(L, lua_gettop(L), zb_harm_name((zb_harm)h), 0.0);
  }
  lua_pop(L, 1);
  a.magical = flag(L, 1, "magical");
  a.mana_color = -1;
  lua_getfield(L, 1, "mana");
  const char *color = lua_tostring(L, -1);
  for (int k = 0; color && k < ZB_MANA_COLORS; ++k) {
    a.mana_color = strcmp(color, zb_mana_color_name((zb_mana_color)k)) ? a.mana_color : k;
  }
  lua_pop(L, 1);
  a.motes = num(L, 1, "motes", 0.0);
  a.color[0] = a.color[1] = a.color[2] = 0.5f;
  lua_getfield(L, 1, "color");
  for (int k = 0; k < 3 && lua_istable(L, -1); ++k) {
    lua_rawgeti(L, -1, k + 1);
    a.color[k] = (float)luaL_optnumber(L, -1, 0.5);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  int index = zb_creatures_define(CR(L), &a);
  if (index < 0) {
    return luaL_error(L, "species %s cannot be defined (sizes, speeds, health or diet out of range, or too many)",
                      a.id);
  }
  lua_pushinteger(L, index);
  return 1;
}

/* @lua sim.spawn_creature(species: string, pos: PlanetPos): integer
 * A creature of a species at a place (on the ground, or at its cruising
 * height); returns its id. Errors for an unknown species.
 */
static int s_spawn_creature(lua_State *L) {
  const char *id = luaL_checkstring(L, 1);
  zb_planet_pos pos = zb_script_check_pos(L, 2);
  int k = zb_creatures_find_species(CR(L), id);
  if (k < 0) {
    return luaL_error(L, "no species %s", id);
  }
  uint32_t got = zb_creatures_spawn(CR(L), k, &pos);
  if (!got) {
    return luaL_error(L, "too many creatures");
  }
  lua_pushinteger(L, got);
  return 1;
}

static void set_number(lua_State *L, const char *name, double v) {
  lua_pushnumber(L, v);
  lua_setfield(L, -2, name);
}

/* @lua sim.creature(id: integer): Creature?
 * A creature as it is now, or nil.
 * @field id integer
 * @field species string
 * @field pos PlanetPos
 * @field heading number Radians clockwise from north
 * @field speed number m/s
 * @field health number
 * @field hunger number 0 .. 1
 * @field fear number 0 .. 1
 * @field energy number 0 .. 1
 * @field action string "wander", "graze", "rest", "flee", "hunt" or "investigate"
 * @field alive boolean
 * @field frozen boolean Outside the active tier
 * @field sees_player number? Metres to the player when it perceives the player
 */
static int s_creature(lua_State *L) {
  zb_creature_state st;
  if (!zb_creatures_get(CR(L), (uint32_t)luaL_checkinteger(L, 1), &st)) {
    lua_pushnil(L);
    return 1;
  }
  lua_createtable(L, 0, 14);
  lua_pushinteger(L, st.id);
  lua_setfield(L, -2, "id");
  lua_pushstring(L, zb_creatures_species(CR(L), st.species)->id);
  lua_setfield(L, -2, "species");
  zb_script_push_pos(L, &st.pos);
  lua_setfield(L, -2, "pos");
  set_number(L, "heading", st.heading);
  set_number(L, "speed", st.speed);
  set_number(L, "health", st.health);
  set_number(L, "hunger", st.hunger);
  set_number(L, "fear", st.fear);
  set_number(L, "energy", st.energy);
  lua_pushstring(L, zb_action_name(st.action));
  lua_setfield(L, -2, "action");
  lua_pushboolean(L, st.alive);
  lua_setfield(L, -2, "alive");
  lua_pushboolean(L, st.frozen);
  lua_setfield(L, -2, "frozen");
  if (st.sees_player >= 0.0) {
    set_number(L, "sees_player", st.sees_player);
  }
  return 1;
}

/* @lua sim.creatures_near(pos: PlanetPos, radius: number): integer[]
 * Ids of the creatures within radius metres (up to 256), the dead included.
 */
static int s_creatures_near(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  uint32_t ids[256];
  int n = zb_creatures_near(CR(L), &pos, luaL_checknumber(L, 2), ids, 256);
  lua_createtable(L, n, 0);
  for (int k = 0; k < n; ++k) {
    lua_pushinteger(L, ids[k]);
    lua_rawseti(L, -2, k + 1);
  }
  return 1;
}

static zb_harm check_harm(lua_State *L, int idx) {
  const char *name = luaL_checkstring(L, idx);
  for (int h = 0; h < ZB_HARMS; ++h) {
    if (!strcmp(name, zb_harm_name((zb_harm)h))) {
      return (zb_harm)h;
    }
  }
  luaL_error(L, "harm must be heat, impact, cold, poison, smoke or magic");
  return ZB_HARM_MAGIC;
}

/* @lua sim.hurt(id: integer, harm: string, amount: number): boolean
 * Hurts a creature (amount > 0, less its resistance to the harm) or heals it
 * (amount < 0); returns whether it lives.
 */
static int s_hurt(lua_State *L) {
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  zb_harm h = check_harm(L, 2);
  double amount = luaL_checknumber(L, 3);
  luaL_argcheck(L, amount >= -1e6 && amount <= 1e6, 3, "amount must be -1e6..1e6");
  lua_pushboolean(L, zb_creatures_hurt(CR(L), id, h, amount));
  return 1;
}

/* @lua sim.stimulus(kind: string, pos: PlanetPos, strength: number, seconds: number)
 * Something creatures notice for a while: a "noise" (heard within strength x
 * their hearing), a "light" (seen; in the dark wild animals take it for
 * fire) or a "threat" (feared).
 */
static int s_stimulus(lua_State *L) {
  const char *kind = luaL_checkstring(L, 1);
  zb_planet_pos pos = zb_script_check_pos(L, 2);
  double strength = luaL_checknumber(L, 3), seconds = luaL_checknumber(L, 4);
  zb_stimulus_kind k = !strcmp(kind, "noise")    ? ZB_STIMULUS_NOISE
                       : !strcmp(kind, "light")  ? ZB_STIMULUS_LIGHT
                       : !strcmp(kind, "threat") ? ZB_STIMULUS_THREAT
                                                 : (zb_stimulus_kind)-1;
  luaL_argcheck(L, (int)k >= 0, 1, "kind must be noise, light or threat");
  luaL_argcheck(L, strength >= 0.0 && strength <= 10.0, 3, "strength must be 0..10");
  luaL_argcheck(L, seconds >= 0.0 && seconds <= 3600.0, 4, "seconds must be 0..3600");
  zb_creatures_stimulus(CR(L), k, &pos, strength, seconds);
  return 0;
}

/* @lua sim.set_player(pos: PlanetPos|nil, on_foot?: boolean)
 * Where the player is for the creatures (nil: nowhere), and whether on foot.
 */
static int s_set_player(lua_State *L) {
  if (lua_isnoneornil(L, 1)) {
    zb_creatures_set_player(CR(L), NULL, false);
    return 0;
  }
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_creatures_set_player(CR(L), &pos, lua_toboolean(L, 2));
  return 0;
}

/* @lua sim.creature_events(): CreatureEvent[]
 * What happened to creatures since last asked.
 * @field kind string "died", "fled" or "attacked" (the player)
 * @field id integer
 * @field species string
 * @field pos PlanetPos
 * @field cause string? For deaths: the harm that killed it
 * @field magical boolean? For deaths of magical creatures
 * @field mana string? Their colour
 * @field motes number? The mana they release
 */
static int s_creature_events(lua_State *L) {
  zb_creatures *c = CR(L);
  zb_creature_event ev[64];
  int n = zb_creatures_events(c, ev, 64);
  lua_createtable(L, n, 0);
  static const char *const KINDS[] = {"died", "fled", "attacked"};
  for (int k = 0; k < n; ++k) {
    const zb_animal *a = zb_creatures_species(c, ev[k].species);
    lua_createtable(L, 0, 8);
    lua_pushstring(L, KINDS[ev[k].kind]);
    lua_setfield(L, -2, "kind");
    lua_pushinteger(L, ev[k].id);
    lua_setfield(L, -2, "id");
    lua_pushstring(L, a->id);
    lua_setfield(L, -2, "species");
    zb_script_push_pos(L, &ev[k].pos);
    lua_setfield(L, -2, "pos");
    if (ev[k].kind == ZB_CREATURE_DIED) {
      lua_pushstring(L, zb_harm_name(ev[k].cause));
      lua_setfield(L, -2, "cause");
      if (a->magical) {
        lua_pushboolean(L, true);
        lua_setfield(L, -2, "magical");
        if (a->mana_color >= 0) {
          lua_pushstring(L, zb_mana_color_name((zb_mana_color)a->mana_color));
          lua_setfield(L, -2, "mana");
        }
        set_number(L, "motes", a->motes);
      }
    }
    lua_rawseti(L, -2, k + 1);
  }
  return 1;
}

/* @lua sim.creature_stats(): CreatureStats
 * @field alive integer In the active tier
 * @field dead integer
 * @field frozen integer Outside the active tier
 * @field actions table How many do what { wander = .., flee = .., ... }
 */
static int s_creature_stats(lua_State *L) {
  zb_creatures_stats st;
  zb_creatures_get_stats(CR(L), &st);
  lua_createtable(L, 0, 4);
  lua_pushinteger(L, st.alive);
  lua_setfield(L, -2, "alive");
  lua_pushinteger(L, st.dead);
  lua_setfield(L, -2, "dead");
  lua_pushinteger(L, st.frozen);
  lua_setfield(L, -2, "frozen");
  lua_createtable(L, 0, ZB_ACTIONS);
  for (int a = 0; a < ZB_ACTIONS; ++a) {
    lua_pushinteger(L, st.by_action[a]);
    lua_setfield(L, -2, zb_action_name((zb_action)a));
  }
  lua_setfield(L, -2, "actions");
  return 1;
}

/* @lua sim.remove_creature(id: integer): boolean
 * Takes a creature (alive or dead) out of the world.
 */
static int s_remove_creature(lua_State *L) {
  lua_pushboolean(L, zb_creatures_remove(CR(L), (uint32_t)luaL_checkinteger(L, 1)));
  return 1;
}

void zb_script_open_creatures(zb_script *s) {
  static const luaL_Reg fns[] = {{"define_species", s_define_species},
                                 {"spawn_creature", s_spawn_creature},
                                 {"creature", s_creature},
                                 {"creatures_near", s_creatures_near},
                                 {"hurt", s_hurt},
                                 {"stimulus", s_stimulus},
                                 {"set_player", s_set_player},
                                 {"creature_events", s_creature_events},
                                 {"creature_stats", s_creature_stats},
                                 {"remove_creature", s_remove_creature},
                                 {NULL, NULL}};
  lua_getglobal(s->L, "sim");
  luaL_setfuncs(s->L, fns, 0);
  lua_pop(s->L, 1);
}

/* SPDX-License-Identifier: MIT */
/* Lua: the registries of data entries (registry) and what the simulations
 * know about them (sim). */
#include "script/script_internal.h"

#include <string.h>

#include "sim/fire.h"
#include "sim/material.h"

static const zb_materials *M(lua_State *L) { return zb_script_from(L)->cfg.materials; }

/* The registry of a kind; raises an error for unknown kinds. */
static const zb_registry *check_kind(lua_State *L, int idx) {
  const char *kind = luaL_checkstring(L, idx);
  if (strcmp(kind, "materials") != 0) {
    luaL_argerror(L, idx, "unknown registry (materials)");
  }
  return M(L) ? &M(L)->registry : NULL;
}

/* @lua registry.keys(kind: string): string[]
 * The keys ("package:id") of a registry's entries in load order. Kinds:
 * "materials".
 */
static int r_keys(lua_State *L) {
  const zb_registry *r = check_kind(L, 1);
  size_t n = r ? r->count : 0;
  lua_createtable(L, (int)n, 0);
  for (size_t i = 0; i < n; ++i) {
    lua_pushstring(L, r->entry[i].key);
    lua_rawseti(L, -2, (lua_Integer)i + 1);
  }
  return 1;
}

/* @lua registry.get(kind: string, ref: string): RegistryEntry?
 * An entry as its packages left it: its fields, plus key, package (that added
 * it) and changed (the last package that overrode or patched it). ref is a key
 * or an id that only one package defines; nil when there is no such entry.
 * @field key string "package:id"
 * @field package string The package that added it
 * @field changed string? The package that last overrode or patched it
 */
static int r_get(lua_State *L) {
  const zb_registry *r = check_kind(L, 1);
  const zb_registry_entry *e = r ? zb_registry_find(r, luaL_checkstring(L, 2)) : NULL;
  if (!e) {
    lua_pushnil(L);
    return 1;
  }
  zb_script_push_luon(L, &e->value);
  lua_pushstring(L, e->key);
  lua_setfield(L, -2, "key");
  lua_pushstring(L, e->package);
  lua_setfield(L, -2, "package");
  if (e->changed) {
    lua_pushstring(L, e->changed);
    lua_setfield(L, -2, "changed");
  }
  return 1;
}

static int check_material(lua_State *L, int idx) {
  const char *ref = luaL_checkstring(L, idx);
  int i = M(L) ? zb_materials_find(M(L), ref) : -1;
  if (i < 0) {
    luaL_error(L, "no material %s", ref);
  }
  return i;
}

static void set_number(lua_State *L, const char *name, double v) {
  lua_pushnumber(L, v);
  lua_setfield(L, -2, name);
}

/* @lua sim.material(ref: string): Material?
 * What the simulations use of a material (SI units, deg C); nil when there is
 * no such material.
 * @field key string "package:id"
 * @field name string Display name
 * @field phase string "solid", "powder", "liquid" or "gas"
 * @field density number kg/m^3
 * @field specific_heat number J/(kg K)
 * @field conductivity number W/(m K)
 * @field melts table? { at, to, latent } (also boils, freezes and condenses)
 * @field burns table? { flammability, ignition, heat, rate, products = { key = kg per kg } }
 * @field hardness number 0 .. 10
 * @field strength number MPa
 * @field friction number 0 .. 1.5
 * @field mana table { conductivity, and 0 .. 1 per colour }
 * @field color number[] { r, g, b }, 0 .. 1
 */
static int s_material(lua_State *L) {
  const char *ref = luaL_checkstring(L, 1);
  const zb_materials *m = M(L);
  int i = m ? zb_materials_find(m, ref) : -1;
  if (i < 0) {
    lua_pushnil(L);
    return 1;
  }
  const zb_material_info *x = &m->info[i];
  lua_createtable(L, 0, 16);
  lua_pushstring(L, x->key);
  lua_setfield(L, -2, "key");
  lua_pushstring(L, x->name);
  lua_setfield(L, -2, "name");
  lua_pushstring(L, zb_phase_name(x->phase));
  lua_setfield(L, -2, "phase");
  set_number(L, "density", x->density);
  set_number(L, "specific_heat", x->specific_heat);
  set_number(L, "conductivity", x->conductivity);
  set_number(L, "hardness", x->hardness);
  set_number(L, "strength", x->strength);
  set_number(L, "friction", x->friction);
  static const char *const transitions[ZB_TRANSITIONS] = {"melts", "boils", "freezes", "condenses"};
  for (int k = 0; k < ZB_TRANSITIONS; ++k) {
    const zb_transition *t = &x->transition[k];
    if (t->to >= 0) {
      lua_createtable(L, 0, 3);
      set_number(L, "at", t->at);
      set_number(L, "latent", t->latent);
      lua_pushstring(L, m->info[t->to].key);
      lua_setfield(L, -2, "to");
      lua_setfield(L, -2, transitions[k]);
    }
  }
  if (x->flammability > 0.0f) {
    lua_createtable(L, 0, 5);
    set_number(L, "flammability", x->flammability);
    set_number(L, "ignition", x->ignition);
    set_number(L, "heat", x->heat);
    set_number(L, "rate", x->burn_rate);
    lua_newtable(L);
    for (int p = 0; p < ZB_MATERIAL_PRODUCTS; ++p) {
      if (x->product[p] >= 0) {
        set_number(L, m->info[x->product[p]].key, x->product_share[p]);
      }
    }
    lua_setfield(L, -2, "products");
    lua_setfield(L, -2, "burns");
  }
  lua_createtable(L, 0, ZB_MANA_COLORS + 1);
  set_number(L, "conductivity", x->mana_conductivity);
  for (int c = 0; c < ZB_MANA_COLORS; ++c) {
    set_number(L, zb_mana_color_name((zb_mana_color)c), x->affinity[c]);
  }
  lua_setfield(L, -2, "mana");
  lua_createtable(L, 3, 0);
  for (int c = 0; c < 3; ++c) {
    lua_pushnumber(L, x->color[c]);
    lua_rawseti(L, -2, c + 1);
  }
  lua_setfield(L, -2, "color");
  return 1;
}

/* @lua sim.material_at(ref: string, temperature: number): string
 * The material ref becomes at a temperature (deg C) through its phase
 * changes: water at -5 is ice, ice at 150 is steam. Errors for an unknown
 * material.
 */
static int s_material_at(lua_State *L) {
  int i = check_material(L, 1);
  float t = (float)luaL_checknumber(L, 2);
  lua_pushstring(L, M(L)->info[zb_material_at_temperature(M(L), i, t)].key);
  return 1;
}

/* ------------------------------------------------------------------ fire */

static zb_fire *F(lua_State *L) {
  zb_fire *f = zb_script_from(L)->cfg.fire;
  if (!f) {
    luaL_error(L, "no fire simulation (the material registry is missing)");
  }
  return f;
}

/* @lua sim.ignite(pos: PlanetPos, radius?: number): integer
 * Sets fire to the fuel within radius metres (default 2, up to 64) of a place
 * in the active tier: lightning, a spell, a torch. Returns the cells (about
 * 2 m) that caught; wet or bare ground and water do not.
 */
static int s_ignite(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  double radius = luaL_optnumber(L, 2, 2.0);
  luaL_argcheck(L, radius >= 0.0 && radius <= 64.0, 2, "radius must be 0..64 metres");
  lua_pushinteger(L, zb_fire_ignite(F(L), &pos, radius));
  return 1;
}

/* @lua sim.add_heat(pos: PlanetPos, radius: number, joules: number)
 * Adds heat spread over radius metres (up to 64): enough of it dries and then
 * ignites the fuel there.
 */
static int s_add_heat(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  double radius = luaL_checknumber(L, 2), joules = luaL_checknumber(L, 3);
  luaL_argcheck(L, radius >= 0.0 && radius <= 64.0, 2, "radius must be 0..64 metres");
  luaL_argcheck(L, joules >= 0.0 && joules <= 1e12, 3, "joules must be 0..1e12");
  zb_fire_add_heat(F(L), &pos, radius, joules);
  return 0;
}

/* @lua sim.fire_at(pos: PlanetPos): FireCell
 * The 2 m cell of the heat and fire simulation at a place.
 * @field loaded boolean In the active tier
 * @field burning boolean On fire
 * @field burnt boolean Burnt out
 * @field water boolean Under water
 * @field temperature number deg C
 * @field fuel number kg/m^2
 * @field moisture number kg of water per kg of fuel
 * @field material string? The fuel's material key
 */
static int s_fire_at(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_fire_cell c;
  zb_fire_cell_at(F(L), &pos, &c);
  lua_createtable(L, 0, 8);
  lua_pushboolean(L, c.loaded);
  lua_setfield(L, -2, "loaded");
  lua_pushboolean(L, c.burning);
  lua_setfield(L, -2, "burning");
  lua_pushboolean(L, c.burnt);
  lua_setfield(L, -2, "burnt");
  lua_pushboolean(L, c.water);
  lua_setfield(L, -2, "water");
  set_number(L, "temperature", c.temperature);
  set_number(L, "fuel", c.fuel);
  set_number(L, "moisture", c.moisture);
  if (c.material >= 0 && M(L)) {
    lua_pushstring(L, M(L)->info[c.material].key);
    lua_setfield(L, -2, "material");
  }
  return 1;
}

/* @lua sim.fire_stats(): FireStats
 * The heat and fire simulation as a whole.
 * @field chunks integer Chunks in the active tier
 * @field awake integer Of those, chunks with heat or fire
 * @field burning integer Cells on fire
 * @field burnt integer Cells burnt out so far
 * @field embers integer Embers thrown so far
 * @field spots integer Spot fires they started
 * @field max_temperature number deg C of the hottest cell
 */
static int s_fire_stats(lua_State *L) {
  zb_fire_stats st;
  zb_fire_get_stats(F(L), &st);
  lua_createtable(L, 0, 7);
  const struct {
    const char *name;
    int value;
  } ints[] = {{"chunks", st.chunks}, {"awake", st.awake},   {"burning", st.burning},
              {"burnt", st.burnt},   {"embers", st.embers}, {"spots", st.spots}};
  for (size_t i = 0; i < sizeof ints / sizeof ints[0]; ++i) {
    lua_pushinteger(L, ints[i].value);
    lua_setfield(L, -2, ints[i].name);
  }
  set_number(L, "max_temperature", st.max_temperature);
  return 1;
}

/* @lua sim.set_weather(weather: Weather)
 * The wind and rain the fire sees (until the weather simulation drives them).
 * @field wind_east number? m/s towards the east
 * @field wind_north number? m/s towards the north
 * @field rain number? mm/h
 */
static int s_set_weather(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  zb_fire_weather w = *zb_fire_weather_get(F(L));
  const struct {
    const char *name;
    double *value, lo, hi;
  } fields[] = {{"wind_east", &w.wind_east, -60.0, 60.0},
                {"wind_north", &w.wind_north, -60.0, 60.0},
                {"rain", &w.rain, 0.0, 500.0}};
  for (size_t i = 0; i < sizeof fields / sizeof fields[0]; ++i) {
    lua_getfield(L, 1, fields[i].name);
    if (!lua_isnil(L, -1)) {
      double v = luaL_checknumber(L, -1);
      if (v < fields[i].lo || v > fields[i].hi) {
        return luaL_error(L, "%s must be %d..%d", fields[i].name, (int)fields[i].lo, (int)fields[i].hi);
      }
      *fields[i].value = v;
    }
    lua_pop(L, 1);
  }
  zb_fire_set_weather(F(L), &w);
  return 0;
}

/* @lua sim.weather(): Weather
 * The wind and rain the fire sees.
 */
static int s_weather(lua_State *L) {
  const zb_fire_weather *w = zb_fire_weather_get(F(L));
  lua_createtable(L, 0, 3);
  set_number(L, "wind_east", w->wind_east);
  set_number(L, "wind_north", w->wind_north);
  set_number(L, "rain", w->rain);
  return 1;
}

void zb_script_open_sim(zb_script *s) {
  static const luaL_Reg registry_fns[] = {{"keys", r_keys}, {"get", r_get}, {NULL, NULL}};
  static const luaL_Reg sim_fns[] = {
      {"material", s_material},       {"material_at", s_material_at}, {"ignite", s_ignite},
      {"add_heat", s_add_heat},       {"fire_at", s_fire_at},         {"fire_stats", s_fire_stats},
      {"set_weather", s_set_weather}, {"weather", s_weather},         {NULL, NULL}};
  lua_newtable(s->L);
  luaL_setfuncs(s->L, registry_fns, 0);
  lua_setglobal(s->L, "registry");
  lua_newtable(s->L);
  luaL_setfuncs(s->L, sim_fns, 0);
  lua_setglobal(s->L, "sim");
}

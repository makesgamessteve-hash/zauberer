/* SPDX-License-Identifier: MIT */
/* Lua: the registries of data entries (registry) and what the simulations
 * know about them (sim). */
#include "script/script_internal.h"

#include <math.h>
#include <string.h>

#include "sim/air.h"
#include "sim/fire.h"
#include "sim/material.h"
#include "sim/tier.h"
#include "sim/water.h"

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

static zb_tier tier_of(lua_State *L) {
  const zb_script_config *cfg = &zb_script_from(L)->cfg;
  zb_tier t = {cfg->fire, cfg->water, cfg->air};
  return t;
}

/* The weather in use (the fire's, without a weather control). */
static zb_weather current_weather(lua_State *L) {
  zb_weather_control *wc = zb_script_from(L)->cfg.weather;
  if (wc) {
    return wc->now;
  }
  const zb_fire_weather *fw = zb_fire_weather_get(F(L));
  zb_weather w = {0.0, fw->rain, 0.0, 0.0, fw->wind_east, fw->wind_north, 0.0};
  return w;
}

/* @lua sim.set_weather(weather: Weather)
 * Sets the weather the simulations see, instead of the world's own, until
 * sim.natural_weather(): for tests, spells and story moments. Fields left
 * out keep their current values.
 * @field wind_east number? m/s towards the east (-60 .. 60)
 * @field wind_north number? m/s towards the north (-60 .. 60)
 * @field rain number? mm/h (0 .. 500)
 * @field snow number? mm/h of water (0 .. 500)
 * @field cloud number? 0 clear .. 1 overcast
 * @field storm number? 0 .. 1: thunderstorms (lightning)
 */
static int s_set_weather(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  zb_weather w = current_weather(L);
  const struct {
    const char *name;
    double *value, lo, hi;
  } fields[] = {{"wind_east", &w.wind_east, -60.0, 60.0},
                {"wind_north", &w.wind_north, -60.0, 60.0},
                {"rain", &w.rain, 0.0, 500.0},
                {"snow", &w.snow, 0.0, 500.0},
                {"cloud", &w.cloud, 0.0, 1.0},
                {"storm", &w.storm, 0.0, 1.0}};
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
  zb_weather_control *wc = zb_script_from(L)->cfg.weather;
  if (wc) {
    wc->overridden = true;
    wc->now = w;
  }
  zb_tier t = tier_of(L);
  zb_tier_apply_weather(&t, &w);
  return 0;
}

/* @lua sim.natural_weather()
 * Returns to the world's own weather (from the next second).
 */
static int s_natural_weather(lua_State *L) {
  zb_weather_control *wc = zb_script_from(L)->cfg.weather;
  if (wc) {
    wc->overridden = false;
  }
  return 0;
}

/* @lua sim.weather(): Weather
 * The weather the simulations see now, with natural (the world's own, not
 * one the scripts set) and strikes (lightning so far).
 */
static int s_weather(lua_State *L) {
  zb_weather w = current_weather(L);
  zb_weather_control *wc = zb_script_from(L)->cfg.weather;
  lua_createtable(L, 0, 9);
  set_number(L, "wind_east", w.wind_east);
  set_number(L, "wind_north", w.wind_north);
  set_number(L, "rain", w.rain);
  set_number(L, "snow", w.snow);
  set_number(L, "cloud", w.cloud);
  set_number(L, "storm", w.storm);
  set_number(L, "temperature", w.temperature);
  lua_pushboolean(L, wc && !wc->overridden);
  lua_setfield(L, -2, "natural");
  lua_pushinteger(L, wc ? wc->strikes : 0);
  lua_setfield(L, -2, "strikes");
  return 1;
}

/* @lua sim.lightning(pos: PlanetPos): integer
 * Lightning strikes a place in the active tier: a flash, and fire in the dry
 * fuel there. Returns the cells that caught.
 */
static int s_lightning(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  int lit = zb_fire_ignite(F(L), &pos, 3.0);
  zb_weather_control *wc = zb_script_from(L)->cfg.weather;
  if (wc) {
    wc->strikes++;
  }
  lua_pushinteger(L, lit);
  return 1;
}

/* --------------------------------------------------------------- water */

static zb_surface_water *WT(lua_State *L) {
  zb_surface_water *w = zb_script_from(L)->cfg.water;
  if (!w) {
    luaL_error(L, "no surface water simulation");
  }
  return w;
}

/* @lua sim.add_water(pos: PlanetPos, radius: number, cubic_metres: number): integer
 * Pours water over the ground within radius metres (up to 64) of a place in
 * the active tier: a spell, a spring, a broken dam. Returns the cells (about
 * 2 m) that got some; the sea, lakes and rivers take none.
 */
static int s_add_water(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  double radius = luaL_checknumber(L, 2), volume = luaL_checknumber(L, 3);
  luaL_argcheck(L, radius >= 0.0 && radius <= 64.0, 2, "radius must be 0..64 metres");
  luaL_argcheck(L, volume >= 0.0 && volume <= 1e7, 3, "cubic_metres must be 0..1e7");
  lua_pushinteger(L, zb_surface_water_add(WT(L), &pos, radius, volume));
  return 1;
}

/* @lua sim.water_at(pos: PlanetPos): WaterCell
 * The 2 m cell of the surface-water simulation at a place.
 * @field loaded boolean In the active tier
 * @field fixed boolean Sea, lake or river (a water body of the terrain)
 * @field ground number Metres above sea level
 * @field depth number Metres of water on it
 * @field level number The water's surface (the ground when dry)
 * @field soaked number Metres of water held in the ground
 * @field capacity number Metres the ground can hold
 * @field flow number m^3/s leaving the cell
 */
static int s_water_at(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_water_cell c;
  zb_surface_water_cell_at(WT(L), &pos, &c);
  lua_createtable(L, 0, 8);
  lua_pushboolean(L, c.loaded);
  lua_setfield(L, -2, "loaded");
  lua_pushboolean(L, c.fixed);
  lua_setfield(L, -2, "fixed");
  set_number(L, "ground", c.ground);
  set_number(L, "depth", c.depth);
  set_number(L, "level", c.level);
  set_number(L, "soaked", c.soaked);
  set_number(L, "capacity", c.capacity);
  set_number(L, "flow", c.flow);
  return 1;
}

/* @lua sim.water_stats(): WaterStats
 * Where the water of the active tier is, in m^3: on_ground + soaked + drained
 * equals rained + added.
 * @field chunks integer Chunks in the active tier
 * @field awake integer Of those, chunks with moving water
 * @field wet integer Cells with more than a centimetre of water
 * @field on_ground number m^3 standing or running
 * @field soaked number m^3 soaked into the ground
 * @field drained number m^3 drained into seas, lakes and rivers
 * @field rained number m^3 of rain
 * @field added number m^3 poured in
 * @field max_depth number Metres
 * @field max_flow number m^3/s out of the busiest cell
 */
static int s_water_stats(lua_State *L) {
  zb_water_stats st;
  zb_surface_water_get_stats(WT(L), &st);
  lua_createtable(L, 0, 10);
  lua_pushinteger(L, st.chunks);
  lua_setfield(L, -2, "chunks");
  lua_pushinteger(L, st.awake);
  lua_setfield(L, -2, "awake");
  lua_pushinteger(L, st.wet);
  lua_setfield(L, -2, "wet");
  set_number(L, "on_ground", st.on_ground);
  set_number(L, "soaked", st.soaked);
  set_number(L, "drained", st.drained);
  set_number(L, "rained", st.rained);
  set_number(L, "added", st.added);
  set_number(L, "max_depth", st.max_depth);
  set_number(L, "max_flow", st.max_flow);
  return 1;
}

/* ------------------------------------------------------------------ air */

static zb_air *AIR(lua_State *L) {
  zb_air *air = zb_script_from(L)->cfg.air;
  if (!air) {
    luaL_error(L, "no air simulation");
  }
  return air;
}

static zb_gas check_gas(lua_State *L, int idx) {
  const char *name = luaL_checkstring(L, idx);
  for (int g = 0; g < ZB_GASES; ++g) {
    if (strcmp(name, zb_gas_name((zb_gas)g)) == 0) {
      return (zb_gas)g;
    }
  }
  luaL_argerror(L, idx, "gas must be smoke, steam, scent or fumes");
  return ZB_GAS_SMOKE;
}

/* @lua sim.wind_at(pos: PlanetPos): Wind
 * The local wind at a place: the weather's wind shaped by the ground (the
 * weather's wind outside the active tier).
 * @field loaded boolean In the active tier
 * @field east number m/s towards the east
 * @field north number m/s towards the north
 * @field speed number m/s
 */
static int s_wind_at(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_air_sample a;
  zb_air_sample_at(AIR(L), &pos, &a);
  lua_createtable(L, 0, 4);
  lua_pushboolean(L, a.loaded);
  lua_setfield(L, -2, "loaded");
  set_number(L, "east", a.east);
  set_number(L, "north", a.north);
  set_number(L, "speed", sqrt(a.east * a.east + a.north * a.north));
  return 1;
}

/* @lua sim.gas_at(pos: PlanetPos): Gases
 * The gases in the air at a place, kg/m^3 of the lowest air layer.
 * @field smoke number
 * @field steam number
 * @field scent number
 * @field fumes number
 */
static int s_gas_at(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_air_sample a;
  zb_air_sample_at(AIR(L), &pos, &a);
  lua_createtable(L, 0, ZB_GASES);
  for (int g = 0; g < ZB_GASES; ++g) {
    set_number(L, zb_gas_name((zb_gas)g), a.gas[g]);
  }
  return 1;
}

/* @lua sim.emit(pos: PlanetPos, gas: string, kg: number)
 * Releases kg of a gas ("smoke", "steam", "scent" or "fumes") into the air
 * above a place in the active tier: a creature's scent, a potion's fumes.
 */
static int s_emit(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_gas gas = check_gas(L, 2);
  double kg = luaL_checknumber(L, 3);
  luaL_argcheck(L, kg >= 0.0 && kg <= 1e6, 3, "kg must be 0..1e6");
  zb_air_emit(AIR(L), &pos, gas, kg);
  return 0;
}

/* @lua sim.air_stats(): AirStats
 * The air of the active tier.
 * @field chunks integer Chunks in the active tier
 * @field awake integer Of those, chunks with gas
 * @field mass table kg of each gas in the air, by name
 * @field emitted table kg of each gas released so far, by name
 */
static int s_air_stats(lua_State *L) {
  zb_air_stats st;
  zb_air_get_stats(AIR(L), &st);
  lua_createtable(L, 0, 4);
  lua_pushinteger(L, st.chunks);
  lua_setfield(L, -2, "chunks");
  lua_pushinteger(L, st.awake);
  lua_setfield(L, -2, "awake");
  const double *tables[2] = {st.mass, st.emitted};
  const char *names[2] = {"mass", "emitted"};
  for (int t = 0; t < 2; ++t) {
    lua_createtable(L, 0, ZB_GASES);
    for (int g = 0; g < ZB_GASES; ++g) {
      set_number(L, zb_gas_name((zb_gas)g), tables[t][g]);
    }
    lua_setfield(L, -2, names[t]);
  }
  return 1;
}

void zb_script_open_sim(zb_script *s) {
  static const luaL_Reg registry_fns[] = {{"keys", r_keys}, {"get", r_get}, {NULL, NULL}};
  static const luaL_Reg sim_fns[] = {{"material", s_material},
                                     {"material_at", s_material_at},
                                     {"ignite", s_ignite},
                                     {"add_heat", s_add_heat},
                                     {"fire_at", s_fire_at},
                                     {"fire_stats", s_fire_stats},
                                     {"set_weather", s_set_weather},
                                     {"weather", s_weather},
                                     {"natural_weather", s_natural_weather},
                                     {"lightning", s_lightning},
                                     {"add_water", s_add_water},
                                     {"water_at", s_water_at},
                                     {"water_stats", s_water_stats},
                                     {"wind_at", s_wind_at},
                                     {"gas_at", s_gas_at},
                                     {"emit", s_emit},
                                     {"air_stats", s_air_stats},
                                     {NULL, NULL}};
  lua_newtable(s->L);
  luaL_setfuncs(s->L, registry_fns, 0);
  lua_setglobal(s->L, "registry");
  lua_newtable(s->L);
  luaL_setfuncs(s->L, sim_fns, 0);
  lua_setglobal(s->L, "sim");
}

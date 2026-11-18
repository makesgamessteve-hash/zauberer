/* SPDX-License-Identifier: MIT */
/* mana.*: the mana field of the active tier and the world's ley network. */
#include "script/script_internal.h"
#include "sim/tier.h"
#include "world/dvec.h"

#include <string.h>

static zb_mana *MN(lua_State *L) {
  zb_tier *t = zb_script_from(L)->cfg.tier;
  if (!t) {
    luaL_error(L, "no active tier");
  }
  return zb_tier_mana(t);
}

static zb_mana_color check_color(lua_State *L, int idx) {
  const char *name = luaL_checkstring(L, idx);
  for (int k = 0; k < ZB_MANA_COLORS; ++k) {
    if (!strcmp(name, zb_mana_color_name((zb_mana_color)k))) {
      return (zb_mana_color)k;
    }
  }
  luaL_error(L, "color must be fire, storm, light, life, water, earth, void or decay");
  return ZB_MANA_FIRE;
}

static void set_number(lua_State *L, const char *name, double v) {
  lua_pushnumber(L, v);
  lua_setfield(L, -2, name);
}

/* A table { fire = .., storm = .., ... }. */
static void push_colors(lua_State *L, const double v[ZB_MANA_COLORS]) {
  lua_createtable(L, 0, ZB_MANA_COLORS);
  for (int k = 0; k < ZB_MANA_COLORS; ++k) {
    set_number(L, zb_mana_color_name((zb_mana_color)k), v[k]);
  }
}

/* @lua mana.colors(): string[]
 * The eight colours in the order of the mana wheel: fire, storm, light, life,
 * water, earth, void, decay (opposites are four apart).
 */
static int m_colors(lua_State *L) {
  lua_createtable(L, ZB_MANA_COLORS, 0);
  for (int k = 0; k < ZB_MANA_COLORS; ++k) {
    lua_pushstring(L, zb_mana_color_name((zb_mana_color)k));
    lua_rawseti(L, -2, k + 1);
  }
  return 1;
}

/* @lua mana.at(pos: PlanetPos): ManaCell
 * The mana of the 32 m cell at a place in the active tier.
 * @field loaded boolean In the active tier
 * @field stored table Mana per colour { fire = .., storm = .., ... }
 * @field cap table What each colour holds when full
 * @field ley_distance number Metres to the nearest ley line
 * @field nexus_distance number Metres to the nearest nexus
 * @field blighted boolean Overdrawn land
 */
static int m_at(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_mana_sample s;
  zb_mana_sample_at(MN(L), &pos, &s);
  lua_createtable(L, 0, 6);
  lua_pushboolean(L, s.loaded);
  lua_setfield(L, -2, "loaded");
  push_colors(L, s.stored);
  lua_setfield(L, -2, "stored");
  push_colors(L, s.cap);
  lua_setfield(L, -2, "cap");
  set_number(L, "ley_distance", s.ley_distance);
  set_number(L, "nexus_distance", s.nexus_distance);
  lua_pushboolean(L, s.blighted);
  lua_setfield(L, -2, "blighted");
  return 1;
}

/* @lua mana.draw(pos: PlanetPos, color: string, amount: number, radius?: number): number
 * Draws up to amount mana (0 .. 1e6) of a colour from the cells within radius
 * metres (default 0: the cell at pos; up to 256); returns how much it got.
 * Emptying a cell with heavy drawing blights it.
 */
static int m_draw(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_mana_color c = check_color(L, 2);
  double amount = luaL_checknumber(L, 3), radius = luaL_optnumber(L, 4, 0.0);
  luaL_argcheck(L, amount >= 0.0 && amount <= 1e6, 3, "amount must be 0..1e6");
  luaL_argcheck(L, radius >= 0.0 && radius <= 256.0, 4, "radius must be 0..256 metres");
  lua_pushnumber(L, zb_mana_draw(MN(L), &pos, c, amount, radius));
  return 1;
}

/* @lua mana.release(pos: PlanetPos, color: string, amount: number)
 * Gives mana back to the field at a place (0 .. 1e6): motes dissolving,
 * spells fading.
 */
static int m_release(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_mana_color c = check_color(L, 2);
  double amount = luaL_checknumber(L, 3);
  luaL_argcheck(L, amount >= 0.0 && amount <= 1e6, 3, "amount must be 0..1e6");
  zb_mana_release(MN(L), &pos, c, amount);
  return 0;
}

/* @lua mana.stats(): ManaStats
 * @field chunks integer Chunks in the active tier
 * @field blighted integer Blighted cells among them
 * @field stored table Mana per colour over the active tier
 * @field cap table Capacity per colour over the active tier
 * @field drawn number Mana drawn so far
 */
static int m_stats(lua_State *L) {
  zb_mana_stats st;
  zb_mana_get_stats(MN(L), &st);
  lua_createtable(L, 0, 5);
  lua_pushinteger(L, st.chunks);
  lua_setfield(L, -2, "chunks");
  lua_pushinteger(L, st.blighted);
  lua_setfield(L, -2, "blighted");
  push_colors(L, st.stored);
  lua_setfield(L, -2, "stored");
  push_colors(L, st.cap);
  lua_setfield(L, -2, "cap");
  set_number(L, "drawn", st.drawn);
  return 1;
}

/* @lua mana.ley(pos: PlanetPos): LeySample
 * The ley network near a place (anywhere on the planet).
 * @field line_distance number Metres to the nearest ley line
 * @field line_colors string[] The two colours it carries
 * @field nexus_distance number Metres to the nearest nexus
 * @field nexus PlanetPos The nearest nexus (at sea level)
 * @field nexus_colors string[] Its two colours
 * @field strength number 0 .. 1: the network's strength
 */
static int m_ley(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_world *w = zb_script_from(L)->world;
  const zb_ley *ley = zb_world_ley(w);
  const zb_planet *p = zb_world_planet(w);
  zb_ley_sample s;
  zb_ley_sample_at(ley, zb_dv_norm(zb_planet_to_cart(p, &pos)), &s);
  lua_createtable(L, 0, 6);
  set_number(L, "line_distance", s.line_distance);
  set_number(L, "nexus_distance", s.nexus_distance);
  set_number(L, "strength", ley->strength);
  int a, b;
  zb_ley_line_colors(ley, s.line, &a, &b);
  int pair[2][2] = {{a, b}, {ley->nexus[s.nexus].colors[0], ley->nexus[s.nexus].colors[1]}};
  const char *fields[2] = {"line_colors", "nexus_colors"};
  for (int k = 0; k < 2; ++k) {
    lua_createtable(L, 2, 0);
    for (int j = 0; j < 2; ++j) {
      lua_pushstring(L, zb_mana_color_name((zb_mana_color)pair[k][j]));
      lua_rawseti(L, -2, j + 1);
    }
    lua_setfield(L, -2, fields[k]);
  }
  zb_planet_pos np = zb_planet_from_cart(p, zb_dv_scale(ley->nexus[s.nexus].dir, p->radius));
  np.alt = 0.0f;
  zb_script_push_pos(L, &np);
  lua_setfield(L, -2, "nexus");
  return 1;
}

void zb_script_open_mana(zb_script *s) {
  static const luaL_Reg fns[] = {{"colors", m_colors}, {"at", m_at},   {"draw", m_draw}, {"release", m_release},
                                 {"stats", m_stats},   {"ley", m_ley}, {NULL, NULL}};
  lua_newtable(s->L);
  luaL_setfuncs(s->L, fns, 0);
  lua_setglobal(s->L, "mana");
}

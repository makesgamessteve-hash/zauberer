/* SPDX-License-Identifier: MIT */
/* Lua: the registries of data entries (registry) and what the simulations
 * know about them (sim). */
#include "script/script_internal.h"

#include <string.h>

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

void zb_script_open_sim(zb_script *s) {
  static const luaL_Reg registry_fns[] = {{"keys", r_keys}, {"get", r_get}, {NULL, NULL}};
  static const luaL_Reg sim_fns[] = {{"material", s_material}, {"material_at", s_material_at}, {NULL, NULL}};
  lua_newtable(s->L);
  luaL_setfuncs(s->L, registry_fns, 0);
  lua_setglobal(s->L, "registry");
  lua_newtable(s->L);
  luaL_setfuncs(s->L, sim_fns, 0);
  lua_setglobal(s->L, "sim");
}

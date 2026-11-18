/* SPDX-License-Identifier: MIT */
/* The material registry (concept 5.4): every solid, powder, liquid and gas
 * the simulations know, from the merged data/materials.lua of all packages.
 * Values are simplified from real tables, in SI units with temperatures in
 * degrees Celsius. The simulations index materials by their position here;
 * the terrain's surface materials map onto entries of the same name. */
#ifndef ZB_SIM_MATERIAL_H
#define ZB_SIM_MATERIAL_H

#include "core/registry.h"
#include "world/terrain.h"

#define ZB_MATERIALS_MAX 250
#define ZB_MATERIAL_PRODUCTS 4
#define ZB_MANA_COLORS 8

typedef enum zb_phase { ZB_PHASE_SOLID, ZB_PHASE_POWDER, ZB_PHASE_LIQUID, ZB_PHASE_GAS, ZB_PHASE_COUNT } zb_phase;

/* The eight colours of mana (concept 3.2). */
typedef enum zb_mana_color {
  ZB_MANA_FIRE,
  ZB_MANA_STORM,
  ZB_MANA_LIGHT,
  ZB_MANA_LIFE,
  ZB_MANA_WATER,
  ZB_MANA_EARTH,
  ZB_MANA_VOID,
  ZB_MANA_DECAY
} zb_mana_color;

/* A change of phase at a temperature: melting and boiling when heated,
 * freezing and condensing when cooled. to < 0: none. */
typedef struct zb_transition {
  int to;
  float at;     /* deg C */
  float latent; /* J/kg taken (heating) or given back (cooling) */
} zb_transition;

typedef enum zb_transition_kind { ZB_MELTS, ZB_BOILS, ZB_FREEZES, ZB_CONDENSES, ZB_TRANSITIONS } zb_transition_kind;

typedef struct zb_material_info {
  const char *key; /* "base:granite" (owned by the registry) */
  const char *name;
  zb_phase phase;
  float density;       /* kg/m^3 */
  float specific_heat; /* J/(kg K) */
  float conductivity;  /* W/(m K) */
  zb_transition transition[ZB_TRANSITIONS];
  /* Combustion; flammability 0 means it does not burn. */
  float flammability; /* 0..1: how readily a flame takes */
  float ignition;     /* deg C */
  float heat;         /* J/kg released */
  float burn_rate;    /* kg/(m^2 s) of exposed surface */
  int product[ZB_MATERIAL_PRODUCTS];
  float product_share[ZB_MATERIAL_PRODUCTS]; /* kg per kg burnt */
  /* Mechanics. */
  float hardness; /* 0..10 (Mohs) */
  float strength; /* MPa, what a piece of it carries */
  float friction; /* 0..1.5 */
  /* Magic. */
  float mana_conductivity;        /* 0..1 */
  float affinity[ZB_MANA_COLORS]; /* 0..1 per colour */
  float color[3];                 /* palette colour, 0..1 */
} zb_material_info;

typedef struct zb_materials {
  zb_registry registry; /* the merged entries, kept for scripts */
  int count;
  zb_material_info info[ZB_MATERIALS_MAX];
  int surface[ZB_MAT_COUNT]; /* the entry of each terrain surface material */
  int air, water;            /* entries the simulations rely on */
} zb_materials;

/* Builds the set from a merged registry (taken over, also on failure).
 * Entries with a missing or wrong field, an unknown material in a transition
 * or product, or a transition to the wrong phase are dropped with a warning
 * naming the entry and field. Fails (err) when an entry the engine relies on
 * (air, water and the terrain's surface materials) is missing. */
zb_result zb_materials_build(zb_materials *m, zb_registry *merged, char *err, size_t err_len);
void zb_materials_free(zb_materials *m);
/* An entry by key or unambiguous id, or -1. */
int zb_materials_find(const zb_materials *m, const char *ref);
const char *zb_phase_name(zb_phase p);
const char *zb_mana_color_name(zb_mana_color c);
/* The phase a material turns into at a temperature, following its
 * transitions (at most a few steps): the entry it becomes. */
int zb_material_at_temperature(const zb_materials *m, int material, float temperature);

#endif

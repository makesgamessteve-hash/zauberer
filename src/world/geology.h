/* SPDX-License-Identifier: MIT */
/* Deep-time geology: about 500 million years of plate tectonics on a
 * Fibonacci lattice (drift about Euler poles, subduction, collision and
 * welding, rifting and the supercontinent cycle, hotspots), with erosion and
 * sedimentation, sea-level change, ice ages and climate eras. It leaves a rock
 * record per point (strata with rock, environment, thickness and age),
 * mineral deposits by geological event, a ley network along old structures,
 * and checks the result against Earth-likeness targets (re-running the final
 * phase with derived seeds when it misses). The final state is amplified onto
 * a macro grid of about 6 km cells and eroded (stream power and hillslope
 * diffusion). Everything is a pure function of the seed and options. */
#ifndef ZB_WORLD_GEOLOGY_H
#define ZB_WORLD_GEOLOGY_H

#include "core/alloc.h"
#include "world/lattice.h"

#define ZB_STRATA 8          /* strata kept per point, youngest first */
#define ZB_GEO_MAX_PLATES 32 /* plates alive at once */

typedef enum zb_rock {
  ZB_ROCK_NONE,
  ZB_ROCK_GRANITE,
  ZB_ROCK_GNEISS,
  ZB_ROCK_SCHIST,
  ZB_ROCK_MARBLE,
  ZB_ROCK_QUARTZITE,
  ZB_ROCK_BASALT,
  ZB_ROCK_ANDESITE,
  ZB_ROCK_GABBRO,
  ZB_ROCK_SANDSTONE,
  ZB_ROCK_RED_SANDSTONE,
  ZB_ROCK_CONGLOMERATE,
  ZB_ROCK_SHALE,
  ZB_ROCK_LIMESTONE,
  ZB_ROCK_CHALK,
  ZB_ROCK_COAL,
  ZB_ROCK_SALT,
  ZB_ROCK_GYPSUM,
  ZB_ROCK_TILL,
  ZB_ROCK_PELAGIC_CLAY,
  ZB_ROCK_COUNT
} zb_rock;

/* Where a stratum formed. */
typedef enum zb_rock_env {
  ZB_ENV_NONE,
  ZB_ENV_DEEP_SEA,
  ZB_ENV_SHALLOW_SEA,
  ZB_ENV_RIVER,
  ZB_ENV_SWAMP,
  ZB_ENV_DESERT,
  ZB_ENV_SALT_BASIN,
  ZB_ENV_VOLCANIC,
  ZB_ENV_GLACIAL,
  ZB_ENV_MOUNTAIN_ROOT, /* metamorphosed in a collision */
  ZB_ENV_COUNT
} zb_rock_env;

typedef struct zb_stratum {
  uint8_t rock;       /* zb_rock */
  uint8_t env;        /* zb_rock_env */
  uint16_t thickness; /* metres */
  uint16_t age;       /* million years before present when it formed */
} zb_stratum;

typedef enum zb_fossil {
  ZB_FOSSIL_NONE,
  ZB_FOSSIL_SEA_CREATURES, /* marine limestone, chalk and shale */
  ZB_FOSSIL_GIANT_BEASTS,  /* river and desert deposits of the age of giants */
  ZB_FOSSIL_AMBER,         /* young forest (coal) strata */
  ZB_FOSSIL_PLANTS,        /* older coal swamps */
  ZB_FOSSIL_MANA,          /* any sediment of the Age of Mana, with a residual glow */
  ZB_FOSSIL_COUNT
} zb_fossil;

/* Point flags: what happened at a piece of crust (they travel with it). */
enum {
  ZB_GEO_CRATON = 1 << 0,       /* old continental core */
  ZB_GEO_ARC = 1 << 1,          /* volcanic arc above a subduction zone */
  ZB_GEO_COLLISION = 1 << 2,    /* continental collision belt */
  ZB_GEO_SUTURE = 1 << 3,       /* where two plates welded */
  ZB_GEO_RIFT = 1 << 4,         /* continental rift */
  ZB_GEO_RIDGE = 1 << 5,        /* formed at a spreading ridge */
  ZB_GEO_TRENCH = 1 << 6,       /* ocean floor beside a subduction zone (current) */
  ZB_GEO_HOTSPOT = 1 << 7,      /* volcanic island or seamount from a hotspot */
  ZB_GEO_FLOOD_BASALT = 1 << 8, /* lava plateau */
  ZB_GEO_GLACIATED = 1 << 9,    /* shaped by ice sheets */
  ZB_GEO_ACTIVE_VOLCANO = 1 << 10,
  ZB_GEO_BOUNDARY = 1 << 11, /* at a plate boundary today */
};

/* Mineral deposits (bits). */
enum {
  ZB_MIN_COPPER = 1 << 0,
  ZB_MIN_GOLD = 1 << 1,
  ZB_MIN_SILVER = 1 << 2,
  ZB_MIN_TIN = 1 << 3,
  ZB_MIN_IRON = 1 << 4,
  ZB_MIN_SALT = 1 << 5,
  ZB_MIN_GYPSUM = 1 << 6,
  ZB_MIN_SULFUR = 1 << 7,
  ZB_MIN_COAL = 1 << 8,
  ZB_MIN_NAPHTHA = 1 << 9,
  ZB_MIN_BITUMEN = 1 << 10,
  ZB_MIN_LAPIS = 1 << 11,
  ZB_MIN_TURQUOISE = 1 << 12,
  ZB_MIN_CARNELIAN = 1 << 13,
  ZB_MIN_MANA_CRYSTAL = 1 << 14,
  ZB_MIN_CLAY = 1 << 15,
  ZB_MIN_LIMESTONE = 1 << 16, /* exposed marine limestone: karst country */
  ZB_MIN_COUNT = 17
};

typedef enum zb_geology_target {
  ZB_GEO_TARGET_EARTH,
  ZB_GEO_TARGET_PANGAEA,
  ZB_GEO_TARGET_ARCHIPELAGO,
  ZB_GEO_TARGET_ICE_WORLD,
  ZB_GEO_TARGET_HOTHOUSE,
  ZB_GEO_TARGET_COUNT
} zb_geology_target;

typedef struct zb_geology_options {
  double radius;        /* planet radius in metres (macro grid size and distances) */
  int points;           /* lattice points (40,000) */
  int steps;            /* time steps (125) */
  double span_myr;      /* simulated time (500) */
  int plates;           /* plates at the start (0: from the target) */
  double land_fraction; /* target share of land (0.35; an archipelago wants about 0.2) */
  zb_geology_target target;
  int max_retries;        /* re-runs of the final phase when targets are missed (3) */
  int macro_res;          /* macro cells per cube-face edge (0: about 6 km cells) */
  int erosion_iterations; /* macro erosion passes (12) */
} zb_geology_options;

/* Per lattice point, valid between steps. */
typedef struct zb_geology_points {
  int n;
  const uint8_t *plate;
  const uint8_t *continental; /* 1 continental crust, 0 oceanic */
  const float *thickness;     /* crust, km */
  const float *born;          /* crust formation, Myr before present */
  const float *elevation;     /* metres above the current sea level */
  const float *orogeny;       /* last mountain building, Myr before present (< 0: never) */
  const uint16_t *flags;
  const uint32_t *minerals; /* after the final phase */
  const zb_stratum *strata; /* ZB_STRATA per point, youngest first */
  const uint8_t *basement;  /* zb_rock below the strata */
} zb_geology_points;

/* The global state at the end of a step (the time-lapse and the chronicle). */
typedef struct zb_geology_era {
  float myr_ago;
  float sea_level;   /* metres relative to the present datum */
  float temperature; /* global anomaly, about -1.5 (ice age) .. 1.5 (hothouse) */
  float humidity;    /* about -1 (arid) .. 1 (humid) */
  bool ice_age;
  uint8_t plates;
  float land_fraction;
} zb_geology_era;

typedef struct zb_geology_metrics {
  double land_fraction;
  int continents;                /* landmasses larger than 1 % of the surface */
  int islands;                   /* smaller landmasses */
  double largest_landmass;       /* share of the surface */
  double continental_land;       /* share of the land on continental crust */
  int large_continents;          /* landmasses larger than 2 % of the surface */
  int continents_with_mountains; /* large continents with a mountain belt */
  bool young_ranges, old_ranges;
  double ice_fraction;    /* share of the surface under land ice */
  unsigned climate_bands; /* land in bit 0 tropics, 1 temperate, 2 polar */
  int attempts;
  bool passed;
  double score; /* 0 when every target is met */
} zb_geology_metrics;

typedef struct zb_geology_macro {
  int res;           /* cells per face edge; cell (face, x, y) is (face * res + y) * res + x */
  float *height;     /* metres above sea level */
  float *lake;       /* water depth of filled depressions, metres */
  float *drainage;   /* drainage area, km^2 */
  int32_t *receiver; /* the cell water flows to, -1 at the sea or the edge */
  int32_t *point;    /* nearest lattice point */
  float *area;       /* km^2 per cell, res * res (the same on every face) */
} zb_geology_macro;

typedef enum zb_ley_kind { ZB_LEY_TRIPLE_JUNCTION, ZB_LEY_VOLCANO, ZB_LEY_SUTURE, ZB_LEY_HOTSPOT } zb_ley_kind;

typedef struct zb_ley_node {
  int32_t point;
  uint8_t kind;   /* zb_ley_kind */
  float strength; /* 0..1, older and larger structures are stronger */
} zb_ley_node;

typedef struct zb_ley_edge {
  int32_t a, b;         /* node indices */
  int32_t path, length; /* lattice points path[path .. path + length) from a to b */
  bool loop;            /* an extra edge beyond the spanning tree */
} zb_ley_edge;

typedef struct zb_ley_network {
  int nodes, edges;
  const zb_ley_node *node;
  const zb_ley_edge *edge;
  const int32_t *path;
} zb_ley_network;

typedef enum zb_geology_phase {
  ZB_GEO_PHASE_TECTONICS,
  ZB_GEO_PHASE_MACRO,
  ZB_GEO_PHASE_EROSION,
  ZB_GEO_PHASE_FINISH,
  ZB_GEO_PHASE_DONE
} zb_geology_phase;

typedef struct zb_geology_progress {
  zb_geology_phase phase;
  int step, steps; /* tectonic steps done (restarts when the final phase re-runs) */
  int attempt;     /* 0 for the first run of the final phase */
  int erosion, erosion_iterations;
  double myr_ago;
  double fraction; /* 0..1 of the whole run */
} zb_geology_progress;

typedef struct zb_geology zb_geology;

void zb_geology_default_options(zb_geology_options *o, double radius);
/* NULL and a message on invalid options or when out of memory. */
zb_geology *zb_geology_create(const zb_allocator *a, uint64_t seed, const zb_geology_options *o, char *err,
                              size_t err_len);
void zb_geology_destroy(zb_geology *g);
/* Does the next bounded piece of work (one tectonic step, one macro row band,
 * one erosion pass) and returns the phase now reached. Stop calling it to
 * cancel. ZB_GEO_PHASE_DONE when finished. */
zb_geology_phase zb_geology_advance(zb_geology *g);
/* Runs to the end. */
void zb_geology_run(zb_geology *g);
void zb_geology_get_progress(const zb_geology *g, zb_geology_progress *out);

const zb_lattice *zb_geology_lattice(const zb_geology *g);
void zb_geology_get_points(const zb_geology *g, zb_geology_points *out);
/* Eras of the steps run so far (the accepted run once done). */
const zb_geology_era *zb_geology_eras(const zb_geology *g, int *count);
const zb_geology_metrics *zb_geology_get_metrics(const zb_geology *g);
/* Valid once done. */
const zb_geology_macro *zb_geology_get_macro(const zb_geology *g);
void zb_geology_get_ley(const zb_geology *g, zb_ley_network *out);
/* Macro height (metres above sea level) at a unit direction, bilinear; once done. */
double zb_geology_height(const zb_geology *g, zb_dvec3 dir);
/* The nearest lattice point to a direction. */
int zb_geology_point_at(const zb_geology *g, zb_dvec3 dir);
/* A hash of the final state (golden tests on every platform). */
uint64_t zb_geology_hash(const zb_geology *g);

zb_fossil zb_stratum_fossil(const zb_stratum *s, uint64_t salt);
const char *zb_rock_name(zb_rock r);
const char *zb_rock_env_name(zb_rock_env e);
const char *zb_fossil_name(zb_fossil f);
/* Name of a single mineral bit, e.g. ZB_MIN_COPPER -> "copper". */
const char *zb_mineral_name(uint32_t bit);
const char *zb_geology_target_name(zb_geology_target t);

#endif

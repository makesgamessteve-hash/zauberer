/* SPDX-License-Identifier: MIT */
/* Geology internals, shared by world/geology*.c and the unit tests. */
#ifndef ZB_WORLD_GEOLOGY_INTERNAL_H
#define ZB_WORLD_GEOLOGY_INTERNAL_H

#include "core/hash.h"
#include "world/cubegrid.h"
#include "world/geology.h"

#define ZB_GEO_MAX_HOTSPOTS 16
#define ZB_GEO_NO_PLATE 255

/* Earth-scale reference for plate speeds: plates move at Earth-like angular
 * rates whatever the planet's radius. */
#define ZB_GEO_REF_RADIUS_KM 6371.0
#define ZB_GEO_PI 3.141592653589793238

typedef struct zb_geo_plate {
  bool alive;
  zb_dvec3 axis; /* Euler pole (unit) */
  double rate;   /* radians per Myr about the pole */
  int last_rift; /* step of the last rift (or birth) */
} zb_geo_plate;

typedef struct zb_geo_hotspot {
  zb_dvec3 dir;
  float start, end; /* active from start to end, Myr before present */
} zb_geo_hotspot;

/* The state that travels with the crust (double-buffered during advection). */
typedef struct zb_geo_crust {
  uint8_t *plate, *continental, *basement;
  uint8_t *lineage; /* the craton this continental crust grew from (0: ocean floor or unknown) */
  float *thickness, *born, *orogeny, *glacial;
  uint16_t *flags;
  zb_stratum *strata;
} zb_geo_crust;

/* Everything a re-run of the final phase starts from. */
typedef struct zb_geo_snapshot {
  bool valid;
  zb_geo_crust crust;
  float *elevation;
  zb_geo_plate plates[ZB_GEO_MAX_PLATES];
  float contact[ZB_GEO_LINEAGES * ZB_GEO_LINEAGES];
  int next_lineage;
  int step;
  float sea_level;
} zb_geo_snapshot;

struct zb_geology {
  const zb_allocator *a;
  uint64_t seed;
  zb_geology_options opt;
  zb_lattice lat;
  int n;
  zb_geo_crust cur, next;
  float *elevation; /* metres relative to the present datum (sea level 0 at the end) */
  uint32_t *minerals;
  zb_geo_plate plates[ZB_GEO_MAX_PLATES];
  zb_geo_hotspot hotspots[ZB_GEO_MAX_HOTSPOTS];
  int hotspot_count;
  int step;        /* steps done */
  double dt;       /* Myr per step */
  float sea_level; /* current eustatic sea level (datum metres) */
  zb_rng rng;
  zb_geology_era *eras;      /* opt.steps entries */
  zb_geology_era *best_eras; /* the eras of the best run of the final phase */
  double cycle[4];           /* phases of the climate and sea-level cycles */
  /* Target presets. */
  int min_plates;
  int continental_target; /* continental points at the start: margins regrow towards it */
  double rift_share, rift_chance, temp_offset;
  /* Scratch. */
  int32_t *scratch_a, *scratch_b;
  float *scratch_f;
  double *hist;                                     /* ZB_GEO_HIST_BINS */
  uint16_t *collisions;                             /* plate pair counts this step, MAX_PLATES^2 */
  float contact[ZB_GEO_LINEAGES * ZB_GEO_LINEAGES]; /* last collision of two lineages, Myr ago (< 0 never) */
  int next_lineage; /* the next lineage for a new terrane (island arcs far from continents) */
  /* Final phase and steering. */
  int final_start;
  int attempt;
  zb_geo_snapshot start, best;
  double best_score;
  zb_geology_metrics metrics;
  zb_geology_phase phase;
  /* Macro grid. */
  zb_geology_macro macro;
  int macro_rows_done; /* face rows amplified so far (6 * res in total) */
  int erosion_done;
  zb_cubegrid grid; /* the macro grid's cells, areas and neighbours */
  /* Ley network. */
  zb_ley_node *ley_nodes;
  zb_ley_edge *ley_edges;
  int32_t *ley_path;
  int ley_node_count, ley_edge_count, ley_path_len;
};

/* geology.c */
zb_result zb_geo_crust_alloc(const zb_allocator *a, zb_geo_crust *c, int n);
void zb_geo_crust_free(const zb_allocator *a, zb_geo_crust *c);
void zb_geo_crust_copy(zb_geo_crust *dst, const zb_geo_crust *src, int n);
/* Starting world: plates, cratons, hotspots, motions. */
void zb_geo_init_world(zb_geology *g);
/* One time step of deep time. */
void zb_geo_step(zb_geology *g);
/* Elevation of every point from its crust (no sea level). */
void zb_geo_update_elevation(zb_geology *g);
/* Elevation at which `fraction` of the values (weighted, or not when weights
 * is NULL) lie above: a histogram at 1 m resolution in hist (ZB_GEO_HIST_BINS). */
#define ZB_GEO_HIST_LO (-12000)
#define ZB_GEO_HIST_BINS 24001
float zb_geo_level_for_fraction(double *hist, const float *values, const float *weights, size_t n, double fraction);
/* Strata helpers. */
void zb_geo_add_stratum(zb_stratum *col, zb_rock rock, zb_rock_env env, double metres, double myr_ago);
double zb_geo_erode_strata(zb_stratum *col, double metres);
void zb_geo_metamorphose(zb_stratum *col);
double zb_geo_sediment_thickness(const zb_stratum *col);
/* Splits a plate along a great circle through its centre; false without a free plate id. */
bool zb_geo_rift_plate(zb_geology *g, int plate, zb_dvec3 cut_normal);

/* geology_final.c */
void zb_geo_compute_metrics(zb_geology *g, zb_geology_metrics *m);
void zb_geo_compute_minerals(zb_geology *g);
void zb_geo_build_ley(zb_geology *g);

/* geology_macro.c */
zb_result zb_geo_macro_alloc(zb_geology *g);
void zb_geo_macro_free(zb_geology *g);
/* Amplifies `rows` face rows of the macro grid from the lattice; true when all are done. */
bool zb_geo_macro_amplify(zb_geology *g, int rows);
/* One erosion pass; the last pass also fits the sea level and routes water. */
void zb_geo_macro_erode(zb_geology *g);
/* Water routing on the current heights (receivers, drainage, lakes). */
void zb_geo_macro_route(zb_geology *g);
/* Neighbour k (0..7: E, W, N, S, NE, NW, SE, SW) of a cell, -1 at cube corners. */
int32_t zb_geo_macro_nbr(const zb_geology *g, int32_t cell, int k);
zb_dvec3 zb_geo_macro_dir(const zb_geology *g, int32_t cell);

#endif

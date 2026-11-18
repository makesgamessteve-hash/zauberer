/* SPDX-License-Identifier: MIT */
/* Terrain generation v1: noise-based continents, mountains and hills, and the
 * first chunk stages (0 coarse, 1 terrain, 2 surface). Pure functions of the
 * world seed and the chunk: every peer and platform generates the same bytes.
 * Deep-time geology replaces the continent model in M2. */
#ifndef ZB_WORLD_TERRAIN_H
#define ZB_WORLD_TERRAIN_H

#include "core/alloc.h"
#include "world/planet.h"

#define ZB_COARSE_RES 17   /* stage 0 samples per chunk edge (16 m apart) */
#define ZB_HEIGHT_RES 257  /* stage 1 samples per chunk edge (1 m apart, edges shared) */
#define ZB_SURFACE_RES 256 /* stage 2 cells per chunk edge */

typedef enum zb_material {
  ZB_MAT_NONE,
  ZB_MAT_SOIL,
  ZB_MAT_SAND,
  ZB_MAT_ROCK,
  ZB_MAT_GRAVEL,
  ZB_MAT_SNOW,
  ZB_MAT_ICE,
  ZB_MAT_MUD,
  ZB_MAT_COUNT
} zb_material;

enum { ZB_STAGE_NONE = -1, ZB_STAGE_COARSE = 0, ZB_STAGE_TERRAIN = 1, ZB_STAGE_SURFACE = 2 };

typedef struct zb_terrain_params {
  double land_fraction;   /* share of the surface above sea level (0.35) */
  double ocean_depth;     /* metres (4000) */
  double lowland_height;  /* height of continental interiors (600) */
  double hill_height;     /* amplitude of hills (120) */
  double mountain_height; /* amplitude of mountain ranges (3500) */
  double detail_height;   /* amplitude of 1 m detail (2.5) */
} zb_terrain_params;

typedef struct zb_terrain {
  zb_planet planet;
  uint64_t seed;
  zb_terrain_params params;
  double continent_threshold; /* sea level in continent-field units */
} zb_terrain;

typedef struct zb_chunk_terrain {
  zb_chunk chunk;
  int stage; /* highest completed stage, ZB_STAGE_NONE before generation */
  float min_height, max_height;
  /* Stage 0: macro heights with a one-sample border (index (y+1)*19 + x+1). */
  float macro[(ZB_COARSE_RES + 2) * (ZB_COARSE_RES + 2)];
  uint8_t coarse_material[ZB_COARSE_RES * ZB_COARSE_RES];
  float *height;     /* stage 1: ZB_HEIGHT_RES^2 heights above sea level, row-major */
  uint8_t *material; /* stage 2: ZB_SURFACE_RES^2 materials, row-major */
} zb_chunk_terrain;

void zb_terrain_default_params(zb_terrain_params *out);
/* params may be NULL for the defaults. */
void zb_terrain_init(zb_terrain *t, const zb_planet *p, uint64_t seed, const zb_terrain_params *params);

/* Height above sea level of the large-scale shape (no 1 m detail). */
double zb_terrain_macro_height(const zb_terrain *t, zb_dvec3 dir);
/* Full analytic height (macro + detail); the generated heightfield follows it
 * within the interpolation error of the macro grid. */
double zb_terrain_height(const zb_terrain *t, zb_dvec3 dir);
/* The surface rule: height (m), slope (tangent), sine of latitude, and a
 * jitter in [-1, 1] that dithers the borders between materials. */
zb_material zb_terrain_material_rule(double height, double slope, double sin_lat, double jitter);
const char *zb_material_name(zb_material m);

void zb_chunk_terrain_init(zb_chunk_terrain *ct, zb_chunk c);
/* Advances the chunk to the stage (running the earlier stages first).
 * Thread-safe for different chunks: the terrain is read-only. */
zb_result zb_terrain_generate(const zb_terrain *t, const zb_allocator *a, zb_chunk_terrain *ct, int stage);
/* Frees the stage 1 and 2 arrays and resets the chunk to no stage. */
void zb_chunk_terrain_release(const zb_allocator *a, zb_chunk_terrain *ct);
/* Heap and inline bytes held by the chunk at its current stage. */
size_t zb_chunk_terrain_bytes(const zb_chunk_terrain *ct);
/* Stage 0 sample (x, y in 0..16). */
float zb_chunk_coarse_height(const zb_chunk_terrain *ct, int x, int y);
/* Height at a local offset (nominal metres): bilinear in the heightfield from
 * stage 1, else in the coarse grid. */
float zb_chunk_height_at(const zb_chunk_terrain *ct, float lx, float ly);
/* Hash of everything generated so far (for golden tests). */
uint64_t zb_chunk_terrain_hash(const zb_chunk_terrain *ct);

#endif

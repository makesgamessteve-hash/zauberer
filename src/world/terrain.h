/* SPDX-License-Identifier: MIT */
/* Terrain generation v1: noise-based continents, mountains and hills, and the
 * first chunk stages (0 coarse, 1 terrain, 2 surface). Pure functions of the
 * world seed and the chunk: every peer and platform generates the same bytes.
 * Deep-time geology replaces the continent model in M2. */
#ifndef ZB_WORLD_TERRAIN_H
#define ZB_WORLD_TERRAIN_H

#include "core/alloc.h"
#include "world/flora.h"
#include "world/planet.h"
#include "world/water.h"

#define ZB_COARSE_RES 17   /* stage 0 samples per chunk edge (16 m apart) */
#define ZB_HEIGHT_RES 257  /* stage 1 samples per chunk edge (1 m apart, edges shared) */
#define ZB_SURFACE_RES 256 /* stage 2 cells per chunk edge */
#define ZB_CANOPY_RES 9    /* stage 0 tree cover samples per chunk edge (32 m apart) */
#define ZB_PLANTS_MAX 8192 /* plants per chunk at stage 3 */

typedef enum zb_material {
  ZB_MAT_NONE,
  ZB_MAT_SOIL,
  ZB_MAT_SAND,
  ZB_MAT_ROCK,
  ZB_MAT_GRAVEL,
  ZB_MAT_SNOW,
  ZB_MAT_ICE,
  ZB_MAT_MUD,
  ZB_MAT_ASH, /* burnt ground, for a while after a fire */
  ZB_MAT_COUNT
} zb_material;

enum { ZB_STAGE_NONE = -1, ZB_STAGE_COARSE = 0, ZB_STAGE_TERRAIN = 1, ZB_STAGE_SURFACE = 2, ZB_STAGE_ECOLOGY = 3 };

/* A plant of stage 3. */
typedef struct zb_plant {
  uint16_t x, y;   /* position in the chunk, 1/256 m (nominal) */
  uint8_t species; /* index in the flora */
  uint8_t size;    /* 0 .. 255: the species' smallest .. largest grown plant */
  uint8_t variant; /* a random byte per plant (mirroring, shade) */
  uint8_t layer;   /* zb_plant_layer, or ZB_PLANT_REMOVED */
} zb_plant;

#define ZB_PLANT_REMOVED 0xFF /* the layer of a plant the players removed (a delta) */

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
  /* A created world (terrain v2): heights from its macro grid, materials by
   * biome. NULL: the noise world of terrain v1. */
  const zb_world_bundle *bundle;
  /* The plant species (stage 3 and the canopy of stage 0); NULL: no plants. */
  const zb_flora *flora;
} zb_terrain;

typedef struct zb_chunk_terrain {
  zb_chunk chunk;
  int stage; /* highest completed stage, ZB_STAGE_NONE before generation */
  float min_height, max_height;
  /* Stage 0: macro heights with a one-sample border (index (y+1)*19 + x+1). */
  float macro[(ZB_COARSE_RES + 2) * (ZB_COARSE_RES + 2)];
  uint8_t coarse_material[ZB_COARSE_RES * ZB_COARSE_RES];
  uint8_t canopy[ZB_CANOPY_RES * ZB_CANOPY_RES]; /* stage 0: tree cover 0 .. 255 every 32 m */
  uint8_t clump;                                 /* stage 0: the dominant tree species + 1 (0: none) */
  float *height;                                 /* stage 1: ZB_HEIGHT_RES^2 heights above sea level, row-major */
  uint8_t *material;                             /* stage 2: ZB_SURFACE_RES^2 materials, row-major */
  zb_plant *plants;                              /* stage 3: trees first, then shrubs, then ground cover */
  uint32_t plant_count;
} zb_chunk_terrain;

void zb_terrain_default_params(zb_terrain_params *out);
/* params may be NULL for the defaults. */
void zb_terrain_init(zb_terrain *t, const zb_planet *p, uint64_t seed, const zb_terrain_params *params);

/* Switches to a created world's macro grid and biomes (the bundle must
 * outlive the terrain); NULL returns to the noise world. */
void zb_terrain_use_bundle(zb_terrain *t, const zb_world_bundle *bundle);
/* Grows plants from a flora (which must outlive the terrain); NULL: none. */
void zb_terrain_use_flora(zb_terrain *t, const zb_flora *flora);
/* The climate of a chunk at sea level: annual mean, coldest and warmest month
 * (deg C) and annual precipitation (mm). From the created world, or for the
 * noise world a simple one by latitude with rainy and dry regions. */
void zb_terrain_chunk_climate(const zb_terrain *t, zb_chunk c, double *mean, double *coldest, double *warmest,
                              double *precipitation);
/* The surface material for a biome (zb_biome), with the same height, slope
 * and jitter inputs as zb_terrain_material_rule. */
zb_material zb_terrain_biome_material(int biome, double height, double slope, double jitter);
/* Height above sea level of the large-scale shape (no 1 m detail). */
double zb_terrain_macro_height(const zb_terrain *t, zb_dvec3 dir);
/* Full analytic height (macro + detail); the generated heightfield follows it
 * within the interpolation error of the macro grid. */
double zb_terrain_height(const zb_terrain *t, zb_dvec3 dir);
/* The surface rule: height (m), slope (tangent), sine of latitude, and a
 * jitter in [-1, 1] that dithers the borders between materials. */
zb_material zb_terrain_material_rule(double height, double slope, double sin_lat, double jitter);
const char *zb_material_name(zb_material m);

/* Water at a place: the sea below sea level, and in a created world lakes at
 * their fill level and rivers in their channels. */
typedef struct zb_water_sample {
  zb_water_kind kind;
  double surface; /* metres above sea level (0 for the sea) */
  double depth;   /* from the surface down to the ground */
  zb_dvec3 flow;  /* current in m/s, tangent to the sphere (rivers only) */
} zb_water_sample;

void zb_terrain_water(const zb_terrain *t, zb_dvec3 dir, zb_water_sample *out);
/* Water over a chunk on an n x n grid every step metres (n = 256 / step + 1,
 * step a power of two up to 64): its kind and surface per vertex. The ground
 * comes from the chunk's heights (coarse ones raise rivers to stay visible
 * over the uncarved ground). Rivers count where they come within half a step
 * of a vertex, so narrow ones still show. */
zb_result zb_terrain_water_grid(const zb_terrain *t, const zb_allocator *a, const zb_chunk_terrain *ct, int step,
                                uint8_t *kind, float *surface);

void zb_chunk_terrain_init(zb_chunk_terrain *ct, zb_chunk c);
/* Advances the chunk to the stage (running the earlier stages first).
 * Thread-safe for different chunks: the terrain is read-only. */
zb_result zb_terrain_generate(const zb_terrain *t, const zb_allocator *a, zb_chunk_terrain *ct, int stage);
/* Drops the data above a stage (stage 0 keeps the coarse grid). */
void zb_chunk_terrain_trim(const zb_allocator *a, zb_chunk_terrain *ct, int stage);
/* Frees the stage 1-3 arrays and resets the chunk to no stage. */
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

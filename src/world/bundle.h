/* SPDX-License-Identifier: MIT */
/* The world bundle: everything world creation computed once (deep-time
 * geology, climate, water, biomes, provinces, the ley network and the eras),
 * stored with the world so that loading never re-runs the long simulations.
 * The file is a versioned header and sections, each zlib-compressed with a
 * checksum; unknown sections are skipped. Lattice positions are not stored:
 * they follow from the point count. */
#ifndef ZB_WORLD_BUNDLE_H
#define ZB_WORLD_BUNDLE_H

#include "core/array.h"
#include "world/climate.h"
#include "world/geology.h"

#define ZB_BUNDLE_VERSION 1
#define ZB_GENERATOR_VERSION 1 /* world generation; saved worlds keep the version they were made with */

typedef enum zb_world_quality {
  ZB_WORLD_QUICK,
  ZB_WORLD_NORMAL,
  ZB_WORLD_HIGH,
  ZB_WORLD_QUALITY_COUNT
} zb_world_quality;

/* The "new world" options (concept 4.9) that world generation uses so far. */
typedef struct zb_world_options {
  uint64_t seed;
  double radius; /* metres */
  zb_geology_target target;
  double land_fraction;
  double temperature_offset; /* degrees Celsius, -10 (ice age) .. 10 (hothouse) */
  double axial_tilt;         /* degrees */
  zb_world_quality quality;
} zb_world_options;

typedef struct zb_world_bundle {
  const zb_allocator *a;
  zb_world_options options;
  uint32_t generator_version;
  zb_geology_metrics metrics;
  /* Macro grid (about 6 km cells). */
  zb_cubegrid grid;
  float *height, *lake, *drainage, *discharge, *groundwater;
  int32_t *receiver, *point;
  uint8_t *biome, *order, *province;
  uint16_t *cell_flags;
  /* Climate grid: ZB_MONTHS planes of temperature (sea level) and precipitation. */
  zb_cubegrid climate_grid;
  float *temperature, *precipitation;
  /* Lattice points. */
  int points;
  uint8_t *plate, *continental, *basement, *lineage;
  float *thickness, *born, *orogeny, *elevation;
  uint16_t *geo_flags;
  uint32_t *minerals;
  zb_stratum *strata;
  /* Ley network, provinces, lineage contacts and eras. */
  int ley_node_count, ley_edge_count, ley_path_len;
  zb_ley_node *ley_nodes;
  zb_ley_edge *ley_edges;
  int32_t *ley_path;
  int province_count;
  zb_province *provinces;
  int lineages;
  float *contact;
  int era_count;
  zb_geology_era *eras;
} zb_world_bundle;

void zb_world_default_options(zb_world_options *o);
const char *zb_world_quality_name(zb_world_quality q);

/* Copies a finished geology and its climate into a bundle. */
zb_result zb_world_bundle_build(const zb_allocator *a, const zb_world_options *o, const zb_geology *g,
                                const zb_climate *c, zb_world_bundle *out);
void zb_world_bundle_free(zb_world_bundle *b);
/* Appends the file image to out (a byte array). */
zb_result zb_world_bundle_write(const zb_world_bundle *b, zb_array *out);
/* Reads a file image; a message in err on a damaged or newer file. */
zb_result zb_world_bundle_read(const zb_allocator *a, const void *data, size_t len, zb_world_bundle *out, char *err,
                               size_t err_len);
uint64_t zb_world_bundle_hash(const zb_world_bundle *b);

/* Queries at a unit direction. */
double zb_world_bundle_height(const zb_world_bundle *b, zb_dvec3 dir); /* smooth across cube edges */
zb_biome zb_world_bundle_biome(const zb_world_bundle *b, zb_dvec3 dir);
int32_t zb_world_bundle_cell(const zb_world_bundle *b, zb_dvec3 dir);
/* The lattice point whose rock record lies under a direction. */
int32_t zb_world_bundle_point(const zb_world_bundle *b, zb_dvec3 dir);
void zb_world_bundle_climate(const zb_world_bundle *b, zb_dvec3 dir, int month, double height, double *temperature,
                             double *precipitation);

#endif

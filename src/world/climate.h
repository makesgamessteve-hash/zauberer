/* SPDX-License-Identifier: MIT */
/* The living planet's present: monthly climate from insolation and the axial
 * tilt, altitude, continentality and ocean currents, three-cell winds that
 * carry moisture from the oceans (orographic rain and rain shadows); rivers
 * with discharge and order, lakes that may close into salt lakes, glaciers and
 * groundwater on the macro grid; Whittaker biomes refined by altitude,
 * drainage, coasts and seasons, magical biomes around strong ley nexuses, and
 * biogeographic provinces. Deterministic from its inputs. */
#ifndef ZB_WORLD_CLIMATE_H
#define ZB_WORLD_CLIMATE_H

#include "core/alloc.h"
#include "world/cubegrid.h"

#define ZB_MONTHS 12

typedef enum zb_biome {
  ZB_BIOME_NONE,
  /* Aquatic */
  ZB_BIOME_DEEP_OCEAN,
  ZB_BIOME_SHELF,
  ZB_BIOME_CORAL_REEF,
  ZB_BIOME_KELP_FOREST,
  ZB_BIOME_SEA_ICE,
  ZB_BIOME_LAKE,
  ZB_BIOME_SALT_LAKE,
  ZB_BIOME_RIVER,
  /* Coastal */
  ZB_BIOME_BEACH,
  ZB_BIOME_DUNES,
  ZB_BIOME_CLIFFS,
  ZB_BIOME_MANGROVE,
  ZB_BIOME_SALT_MARSH,
  ZB_BIOME_ESTUARY,
  /* Tropical */
  ZB_BIOME_RAINFOREST,
  ZB_BIOME_SEASONAL_FOREST,
  ZB_BIOME_CLOUD_FOREST,
  ZB_BIOME_SAVANNA,
  /* Arid */
  ZB_BIOME_SAND_DESERT,
  ZB_BIOME_ROCKY_DESERT,
  ZB_BIOME_SALT_FLAT,
  ZB_BIOME_XERIC_SHRUBLAND,
  /* Temperate */
  ZB_BIOME_MEDITERRANEAN,
  ZB_BIOME_STEPPE,
  ZB_BIOME_DECIDUOUS_FOREST,
  ZB_BIOME_MIXED_FOREST,
  ZB_BIOME_TEMPERATE_RAINFOREST,
  /* Cold */
  ZB_BIOME_TAIGA,
  ZB_BIOME_TUNDRA,
  ZB_BIOME_POLAR_DESERT,
  ZB_BIOME_ICE_SHEET,
  /* Mountain */
  ZB_BIOME_MONTANE_FOREST,
  ZB_BIOME_ALPINE_MEADOW,
  ZB_BIOME_SCREE,
  ZB_BIOME_GLACIER,
  ZB_BIOME_VOLCANIC_SLOPES,
  /* Wetland */
  ZB_BIOME_MARSH,
  ZB_BIOME_SWAMP_FOREST,
  ZB_BIOME_PEAT_BOG,
  ZB_BIOME_FLOODPLAIN,
  /* Magical overlays where mana is extreme */
  ZB_BIOME_ASHLANDS,
  ZB_BIOME_STORMSPIRES,
  ZB_BIOME_GLASSWOOD,
  ZB_BIOME_WILDHEART,
  ZB_BIOME_DROWNED_VALE,
  ZB_BIOME_DEEPSTONE,
  ZB_BIOME_HOLLOWS,
  ZB_BIOME_MIREFEN,
  ZB_BIOME_COUNT
} zb_biome;

typedef enum zb_biome_group {
  ZB_GROUP_AQUATIC,
  ZB_GROUP_COASTAL,
  ZB_GROUP_TROPICAL,
  ZB_GROUP_ARID,
  ZB_GROUP_TEMPERATE,
  ZB_GROUP_COLD,
  ZB_GROUP_MOUNTAIN,
  ZB_GROUP_WETLAND,
  ZB_GROUP_MAGICAL,
  ZB_GROUP_COUNT
} zb_biome_group;

/* Macro cell flags. */
enum {
  ZB_CELL_OCEAN = 1 << 0,
  ZB_CELL_LAKE = 1 << 1,
  ZB_CELL_SALT_LAKE = 1 << 2, /* a lake without outflow */
  ZB_CELL_RIVER = 1 << 3,
  ZB_CELL_RIVER_MOUTH = 1 << 4,
  ZB_CELL_GLACIER = 1 << 5,
  ZB_CELL_SEA_ICE = 1 << 6,
  ZB_CELL_COAST = 1 << 7,  /* land beside the ocean */
  ZB_CELL_OASIS = 1 << 8,  /* desert with shallow groundwater */
  ZB_CELL_VOLCANO = 1 << 9 /* an active volcano in the cell's geology */
};

/* What climate reads; the macro grid and its arrays must outlive it. */
typedef struct zb_climate_input {
  const zb_cubegrid *grid;   /* the macro grid */
  const float *height;       /* metres above sea level */
  const float *lake;         /* lake depth (NULL: no lakes) */
  const int32_t *receiver;   /* downhill cell, -1 at the sea (NULL: no rivers) */
  const uint16_t *geo_flags; /* zb_geology point flags per cell (NULL: none) */
  const uint32_t *minerals;  /* zb_geology minerals per cell (NULL: none) */
  const uint8_t *lineage;    /* crust lineage per cell for biogeography (NULL: by landmass) */
  const float *contact;      /* lineages x lineages: last meeting, Myr ago, < 0 never (NULL: none) */
  int lineages;
  double radius;             /* metres */
  double axial_tilt;         /* degrees (23) */
  double temperature_offset; /* degrees Celsius added everywhere (exotic worlds) */
  uint64_t seed;
  /* Ley nexuses for magical biomes. */
  const zb_dvec3 *nexus;
  const float *nexus_strength;
  const uint8_t *nexus_kind; /* zb_ley_kind */
  int nexus_count;
} zb_climate_input;

typedef struct zb_province {
  zb_dvec3 centre;
  float area;      /* km^2 */
  int landmass;    /* index of its connected landmass */
  int lineage;     /* the crust lineage it grew on (0: unknown) */
  float isolation; /* 0 (well connected) .. 1 (a remote island or a long-isolated continent): endemism */
} zb_province;

typedef struct zb_climate_stats {
  int cells, land_cells;
  double land_area;                  /* km^2 */
  double biome_area[ZB_BIOME_COUNT]; /* km^2 */
  int rivers, great_rivers;          /* river mouths; those carrying over 2,000 m^3/s */
  int max_order;
  double lake_area, salt_lake_area, glacier_area; /* km^2 */
  int provinces;
  int magical_patches;
} zb_climate_stats;

typedef struct zb_climate zb_climate;

typedef enum zb_climate_phase {
  ZB_CLIMATE_PHASE_GRID,
  ZB_CLIMATE_PHASE_MONTHS,
  ZB_CLIMATE_PHASE_HYDROLOGY,
  ZB_CLIMATE_PHASE_BIOMES,
  ZB_CLIMATE_PHASE_DONE
} zb_climate_phase;

/* Climate input from a finished deep-time geology: per-cell geology flags,
 * minerals and lineages (owned by the link), the ley nexuses and the lineage
 * contacts. The geology must outlive the climate. */
struct zb_geology;
typedef struct zb_climate_link {
  const zb_allocator *a;
  uint16_t *geo_flags;
  uint32_t *minerals;
  uint8_t *lineage;
  zb_dvec3 *nexus;
  float *strength;
  uint8_t *kind;
} zb_climate_link;
zb_result zb_climate_input_from_geology(zb_climate_input *in, zb_climate_link *link, const zb_allocator *a,
                                        const struct zb_geology *g, uint64_t seed);
void zb_climate_link_free(zb_climate_link *link);

/* NULL and a message on invalid input or when out of memory. */
zb_climate *zb_climate_create(const zb_allocator *a, const zb_climate_input *in, char *err, size_t err_len);
void zb_climate_destroy(zb_climate *c);
/* One bounded piece of work (one month of climate, hydrology, biomes); returns
 * the phase reached. */
zb_climate_phase zb_climate_advance(zb_climate *c);
void zb_climate_run(zb_climate *c);
double zb_climate_progress(const zb_climate *c);

/* Climate grid (about a quarter of the macro resolution). */
const zb_cubegrid *zb_climate_grid(const zb_climate *c);
/* Monthly mean temperature (deg C) and precipitation (mm) at a direction, at
 * sea level or at a given height (lapse rate 6.5 C/km). */
void zb_climate_month(const zb_climate *c, zb_dvec3 dir, int month, double height, double *temperature,
                      double *precipitation);
/* Annual mean temperature and total precipitation. */
void zb_climate_annual(const zb_climate *c, zb_dvec3 dir, double height, double *temperature, double *precipitation);

/* Per macro cell, once done. */
const uint8_t *zb_climate_biomes(const zb_climate *c);
const uint16_t *zb_climate_flags(const zb_climate *c);
const float *zb_climate_discharge(const zb_climate *c);      /* m^3/s */
const uint8_t *zb_climate_river_order(const zb_climate *c);  /* Strahler order, 0 off rivers */
const float *zb_climate_groundwater(const zb_climate *c);    /* depth to the water table, m */
const uint8_t *zb_climate_province_map(const zb_climate *c); /* 0: none, else province index + 1 */
const zb_province *zb_climate_provinces(const zb_climate *c, int *count);
zb_biome zb_climate_biome_at(const zb_climate *c, zb_dvec3 dir);
/* How much two provinces share their life, 0..1: the same province 1, one
 * landmass and lineages that met recently high, separate landmasses low. */
double zb_climate_province_affinity(const zb_climate *c, int a, int b);
void zb_climate_get_stats(const zb_climate *c, zb_climate_stats *out);
uint64_t zb_climate_hash(const zb_climate *c);

/* The Whittaker-style rule for land away from coasts, water and mountains:
 * annual temperature (C), annual precipitation (mm), warmest month (C), share
 * of the year's rain in the driest six months, and a 0..1 roughness (rocky
 * versus sandy deserts). Exposed for tests and tools. */
zb_biome zb_biome_classify(double temperature, double precipitation, double warmest, double dry_half_share,
                           double rocky);
zb_biome_group zb_biome_group_of(zb_biome b);
const char *zb_biome_name(zb_biome b);
/* Daily mean top-of-atmosphere insolation (W/m^2) at a latitude for a solar
 * declination (both radians). */
double zb_insolation(double latitude, double declination);

#endif

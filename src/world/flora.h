/* SPDX-License-Identifier: MIT */
/* The plant species registry (data/flora.lua, against the sprite atlas
 * sprites/flora.lua) and where plants grow: a species' climate niche, the
 * ground it takes, and how much of each layer (trees, shrubs, ground cover)
 * a biome carries. */
#ifndef ZB_WORLD_FLORA_H
#define ZB_WORLD_FLORA_H

#include "core/alloc.h"

#define ZB_FLORA_MAX 64
#define ZB_SEASONS 4 /* spring, summer, autumn, winter */

typedef enum zb_plant_layer { ZB_PLANT_TREE, ZB_PLANT_SHRUB, ZB_PLANT_GROUND, ZB_PLANT_LAYERS } zb_plant_layer;

typedef struct zb_species {
  char id[32];
  char name[48];
  zb_plant_layer layer;
  int sprite;           /* atlas frame */
  int clump;            /* far-forest impostor frame, -1 for none */
  float height[2];      /* metres: smallest and largest grown plant */
  float width;          /* sprite width / height */
  float temperature[3]; /* annual mean, deg C: lowest, best, highest */
  float rain[3];        /* annual mm: least, best, most */
  float frost;          /* lowest coldest-month mean, deg C */
  uint32_t grows_on;    /* bit (1 << zb_material) per material */
  float slope;          /* steepest ground, tangent */
  bool wet;             /* wetlands and river banks only */
  float density;        /* relative abundance */
  float seasons[ZB_SEASONS][3];
} zb_species;

typedef struct zb_flora {
  int count;
  zb_species species[ZB_FLORA_MAX];
  int frames, columns, frame_width, frame_height; /* the atlas */
} zb_flora;

/* What a place offers a plant. */
typedef struct zb_site {
  double temperature;   /* annual mean, deg C */
  double coldest;       /* the coldest month's mean */
  double precipitation; /* mm per year */
  double slope;         /* tangent */
  int material;         /* zb_material */
  int biome;            /* zb_biome */
  bool wet;             /* a wetland, a river bank or an oasis */
} zb_site;

/* Parses the registry; err names the species and field on failure. */
zb_result zb_flora_parse(zb_flora *f, const zb_allocator *a, const char *species, size_t species_len, const char *atlas,
                         size_t atlas_len, char *err, size_t err_len);
/* Index of a species by id, or -1. */
int zb_flora_find(const zb_flora *f, const char *id);
/* How well a species grows at a site, 0 (it cannot) .. 1 (its optimum): the
 * product of the climate part and the ground part below. */
double zb_species_suitability(const zb_species *s, const zb_site *site);
/* The climate part: 0 outside its niche or below its frost limit. */
double zb_species_climate(const zb_species *s, double temperature, double coldest, double precipitation);
/* The ground part: its material, not too steep, wet where it needs it. */
bool zb_species_ground_ok(const zb_species *s, int material, double slope, bool wet);
/* How much of a layer a biome carries (0 none .. 1 a closed forest). */
double zb_biome_cover(int biome, zb_plant_layer layer);
/* True for biomes whose ground is wet (marshes, swamps, bogs, floodplains, ...). */
bool zb_biome_is_wet(int biome);

#endif

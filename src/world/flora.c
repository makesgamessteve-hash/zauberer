/* SPDX-License-Identifier: MIT */
#include "world/flora.h"

#include <stdio.h>
#include <string.h>

#include "core/luon.h"
#include "world/climate.h"
#include "world/terrain.h"

static bool read_numbers(const zb_value *t, float *out, size_t n) {
  if (!zb_value_is(t, ZB_VAL_TABLE) || zb_table_len(t) != n) {
    return false;
  }
  for (size_t i = 0; i < n; ++i) {
    const zb_value *v = zb_table_index(t, i + 1);
    if (!zb_value_is(v, ZB_VAL_NUMBER)) {
      return false;
    }
    out[i] = (float)v->as.num;
  }
  return true;
}

static int frame_index(const zb_value *frames, const char *name) {
  for (size_t i = 1; i <= zb_table_len(frames); ++i) {
    if (strcmp(zb_value_str(zb_table_index(frames, i), ""), name) == 0) {
      return (int)i - 1;
    }
  }
  return -1;
}

static bool parse_species(zb_species *s, const zb_value *v, const zb_value *frames, char *err, size_t err_len) {
  memset(s, 0, sizeof *s);
  const char *id = zb_value_str(zb_table_get(v, "id"), NULL);
  if (!id || !*id || strlen(id) >= sizeof s->id) {
    snprintf(err, err_len, "a species needs an id (up to %d characters)", (int)sizeof s->id - 1);
    return false;
  }
  snprintf(s->id, sizeof s->id, "%s", id);
  snprintf(s->name, sizeof s->name, "%s", zb_value_str(zb_table_get(v, "name"), id));
  const char *layer = zb_value_str(zb_table_get(v, "layer"), "");
  static const char *const layers[ZB_PLANT_LAYERS] = {"tree", "shrub", "ground"};
  s->layer = ZB_PLANT_LAYERS;
  for (int i = 0; i < ZB_PLANT_LAYERS; ++i) {
    if (strcmp(layer, layers[i]) == 0) {
      s->layer = (zb_plant_layer)i;
    }
  }
  const char *field = NULL;
  s->sprite = frame_index(frames, zb_value_str(zb_table_get(v, "sprite"), ""));
  const char *clump = zb_value_str(zb_table_get(v, "clump"), NULL);
  s->clump = clump ? frame_index(frames, clump) : -1;
  s->width = (float)zb_value_num(zb_table_get(v, "width"), 0.0);
  s->frost = (float)zb_value_num(zb_table_get(v, "frost"), -60.0);
  s->slope = (float)zb_value_num(zb_table_get(v, "slope"), 1.0);
  s->density = (float)zb_value_num(zb_table_get(v, "density"), 1.0);
  s->wet = zb_value_bool(zb_table_get(v, "wet"), false);
  if (s->layer == ZB_PLANT_LAYERS) {
    field = "layer";
  } else if (s->sprite < 0) {
    field = "sprite";
  } else if (clump && s->clump < 0) {
    field = "clump";
  } else if (!read_numbers(zb_table_get(v, "height"), s->height, 2) || s->height[0] <= 0.0f ||
             s->height[1] < s->height[0]) {
    field = "height";
  } else if (s->width <= 0.0f) {
    field = "width";
  } else if (!read_numbers(zb_table_get(v, "temperature"), s->temperature, 3) ||
             !(s->temperature[0] < s->temperature[1] && s->temperature[1] < s->temperature[2])) {
    field = "temperature";
  } else if (!read_numbers(zb_table_get(v, "rain"), s->rain, 3) ||
             !(s->rain[0] < s->rain[1] && s->rain[1] < s->rain[2])) {
    field = "rain";
  } else if (s->density <= 0.0f) {
    field = "density";
  }
  const zb_value *on = zb_table_get(v, "grows_on");
  for (size_t i = 1; !field && i <= zb_table_len(on); ++i) {
    const char *m = zb_value_str(zb_table_index(on, i), "");
    int found = -1;
    for (int k = ZB_MAT_NONE + 1; k < ZB_MAT_COUNT; ++k) {
      found = strcmp(m, zb_material_name((zb_material)k)) == 0 ? k : found;
    }
    if (found < 0) {
      field = "grows_on";
    } else {
      s->grows_on |= 1u << found;
    }
  }
  if (!field && !s->grows_on) {
    field = "grows_on";
  }
  const zb_value *seasons = zb_table_get(v, "seasons");
  for (int k = 0; !field && k < ZB_SEASONS; ++k) {
    const zb_value *season = zb_table_index(seasons, (size_t)k + 1);
    if (zb_table_len(seasons) != ZB_SEASONS || !read_numbers(season, s->seasons[k], 3)) {
      field = "seasons";
    }
  }
  if (field) {
    snprintf(err, err_len, "species '%s': bad or missing %s", s->id, field);
    return false;
  }
  return true;
}

zb_result zb_flora_parse(zb_flora *f, const zb_allocator *a, const char *species, size_t species_len, const char *atlas,
                         size_t atlas_len, char *err, size_t err_len) {
  memset(f, 0, sizeof *f);
  zb_luon_doc sd, ad;
  zb_luon_error le;
  if (zb_luon_parse(&ad, a, atlas, atlas_len, &le) != ZB_OK) {
    snprintf(err, err_len, "flora atlas: %d:%d: %s", le.line, le.column, le.message);
    return ZB_ERR_INVALID;
  }
  if (zb_luon_parse(&sd, a, species, species_len, &le) != ZB_OK) {
    snprintf(err, err_len, "flora: %d:%d: %s", le.line, le.column, le.message);
    zb_luon_free(&ad);
    return ZB_ERR_INVALID;
  }
  zb_result r = ZB_OK;
  const zb_value *frames = zb_table_get(&ad.root, "frames");
  f->frames = (int)zb_table_len(frames);
  f->columns = (int)zb_value_num(zb_table_get(&ad.root, "columns"), 0.0);
  f->frame_width = (int)zb_value_num(zb_table_get(&ad.root, "frame_width"), 0.0);
  f->frame_height = (int)zb_value_num(zb_table_get(&ad.root, "frame_height"), 0.0);
  size_t n = zb_table_len(&sd.root);
  if (f->frames == 0 || f->columns <= 0) {
    snprintf(err, err_len, "flora atlas: no frames");
    r = ZB_ERR_INVALID;
  } else if (n == 0 || n > ZB_FLORA_MAX) {
    snprintf(err, err_len, "flora: between 1 and %d species", ZB_FLORA_MAX);
    r = ZB_ERR_INVALID;
  }
  for (size_t i = 0; r == ZB_OK && i < n; ++i) {
    if (!parse_species(&f->species[i], zb_table_index(&sd.root, i + 1), frames, err, err_len)) {
      r = ZB_ERR_INVALID;
    } else if (zb_flora_find(f, f->species[i].id) >= 0) {
      snprintf(err, err_len, "species '%s' is listed twice", f->species[i].id);
      r = ZB_ERR_INVALID;
    } else {
      f->count++;
    }
  }
  zb_luon_free(&sd);
  zb_luon_free(&ad);
  if (r != ZB_OK) {
    f->count = 0;
  }
  return r;
}

int zb_flora_find(const zb_flora *f, const char *id) {
  for (int i = 0; i < f->count; ++i) {
    if (strcmp(f->species[i].id, id) == 0) {
      return i;
    }
  }
  return -1;
}

/* 1 at the optimum, falling quadratically to 0 at the limits. */
static double niche(double x, const float r[3]) {
  if (x <= r[0] || x >= r[2]) {
    return 0.0;
  }
  double u = x < r[1] ? (r[1] - x) / (r[1] - r[0]) : (x - r[1]) / (r[2] - r[1]);
  return 1.0 - u * u;
}

double zb_species_climate(const zb_species *s, double temperature, double coldest, double precipitation) {
  if (coldest < (double)s->frost) {
    return 0.0;
  }
  return niche(temperature, s->temperature) * niche(precipitation, s->rain);
}

bool zb_species_ground_ok(const zb_species *s, int material, double slope, bool wet) {
  return material > ZB_MAT_NONE && material < ZB_MAT_COUNT && (s->grows_on & (1u << material)) &&
         slope <= (double)s->slope && (wet || !s->wet);
}

double zb_species_suitability(const zb_species *s, const zb_site *site) {
  if (!zb_species_ground_ok(s, site->material, site->slope, site->wet)) {
    return 0.0;
  }
  return zb_species_climate(s, site->temperature, site->coldest, site->precipitation);
}

bool zb_biome_is_wet(int biome) {
  switch ((zb_biome)biome) {
    case ZB_BIOME_MARSH:
    case ZB_BIOME_SWAMP_FOREST:
    case ZB_BIOME_PEAT_BOG:
    case ZB_BIOME_FLOODPLAIN:
    case ZB_BIOME_MANGROVE:
    case ZB_BIOME_SALT_MARSH:
    case ZB_BIOME_ESTUARY:
    case ZB_BIOME_MIREFEN:
    case ZB_BIOME_DROWNED_VALE: return true;
    default: return false;
  }
}

double zb_biome_cover(int biome, zb_plant_layer layer) {
  /* Trees, shrubs, ground cover. */
  static const struct {
    zb_biome biome;
    float cover[ZB_PLANT_LAYERS];
  } TABLE[] = {
      {ZB_BIOME_RAINFOREST, {1.0f, 0.4f, 0.6f}},       {ZB_BIOME_SEASONAL_FOREST, {0.85f, 0.4f, 0.6f}},
      {ZB_BIOME_CLOUD_FOREST, {1.0f, 0.5f, 0.8f}},     {ZB_BIOME_SAVANNA, {0.1f, 0.3f, 1.0f}},
      {ZB_BIOME_SAND_DESERT, {0.004f, 0.04f, 0.02f}},  {ZB_BIOME_ROCKY_DESERT, {0.004f, 0.08f, 0.03f}},
      {ZB_BIOME_XERIC_SHRUBLAND, {0.03f, 0.6f, 0.3f}}, {ZB_BIOME_MEDITERRANEAN, {0.35f, 0.8f, 0.6f}},
      {ZB_BIOME_STEPPE, {0.02f, 0.2f, 1.0f}},          {ZB_BIOME_DECIDUOUS_FOREST, {0.95f, 0.4f, 0.6f}},
      {ZB_BIOME_MIXED_FOREST, {0.95f, 0.35f, 0.6f}},   {ZB_BIOME_TEMPERATE_RAINFOREST, {1.0f, 0.5f, 0.9f}},
      {ZB_BIOME_TAIGA, {0.8f, 0.25f, 0.5f}},           {ZB_BIOME_TUNDRA, {0.01f, 0.2f, 0.8f}},
      {ZB_BIOME_MONTANE_FOREST, {0.85f, 0.35f, 0.6f}}, {ZB_BIOME_ALPINE_MEADOW, {0.02f, 0.1f, 1.0f}},
      {ZB_BIOME_MARSH, {0.02f, 0.1f, 1.0f}},           {ZB_BIOME_SWAMP_FOREST, {0.8f, 0.3f, 0.9f}},
      {ZB_BIOME_PEAT_BOG, {0.05f, 0.3f, 0.8f}},        {ZB_BIOME_FLOODPLAIN, {0.4f, 0.4f, 1.0f}},
      {ZB_BIOME_MANGROVE, {0.7f, 0.2f, 0.4f}},         {ZB_BIOME_BEACH, {0.03f, 0.05f, 0.15f}},
      {ZB_BIOME_DUNES, {0.01f, 0.05f, 0.2f}},          {ZB_BIOME_SALT_MARSH, {0.0f, 0.2f, 0.8f}},
      {ZB_BIOME_ESTUARY, {0.0f, 0.1f, 0.6f}},          {ZB_BIOME_WILDHEART, {1.2f, 0.6f, 0.9f}},
      {ZB_BIOME_GLASSWOOD, {0.8f, 0.2f, 0.3f}},        {ZB_BIOME_MIREFEN, {0.2f, 0.3f, 1.0f}},
      {ZB_BIOME_HOLLOWS, {0.5f, 0.4f, 0.6f}},          {ZB_BIOME_DROWNED_VALE, {0.3f, 0.2f, 0.8f}},
      {ZB_BIOME_STORMSPIRES, {0.05f, 0.1f, 0.3f}},     {ZB_BIOME_DEEPSTONE, {0.05f, 0.1f, 0.2f}},
      {ZB_BIOME_ASHLANDS, {0.02f, 0.05f, 0.1f}},       {ZB_BIOME_VOLCANIC_SLOPES, {0.05f, 0.1f, 0.2f}},
      {ZB_BIOME_CLIFFS, {0.02f, 0.1f, 0.2f}},
  };
  if (layer < 0 || layer >= ZB_PLANT_LAYERS) {
    return 0.0;
  }
  for (size_t i = 0; i < sizeof TABLE / sizeof TABLE[0]; ++i) {
    if ((int)TABLE[i].biome == biome) {
      return (double)TABLE[i].cover[layer];
    }
  }
  return 0.0; /* water, ice, scree and the like */
}

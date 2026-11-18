/* SPDX-License-Identifier: MIT */
#include "world/bundle.h"

#include <stdio.h>
#include <string.h>

#include "core/deflate.h"
#include "core/hash.h"
#include "world/dvec.h"

#define MAGIC "ZBWB"

void zb_world_default_options(zb_world_options *o) {
  memset(o, 0, sizeof *o);
  o->seed = 1;
  o->radius = ZB_PLANET_DEFAULT_RADIUS;
  o->target = ZB_GEO_TARGET_EARTH;
  o->land_fraction = 0.35;
  o->temperature_offset = 0.0;
  o->axial_tilt = 23.0;
  o->quality = ZB_WORLD_NORMAL;
}

const char *zb_world_quality_name(zb_world_quality q) {
  static const char *const names[ZB_WORLD_QUALITY_COUNT] = {"quick", "normal", "high"};
  return (int)q >= 0 && q < ZB_WORLD_QUALITY_COUNT ? names[q] : "?";
}

/* ---- Lifetime ------------------------------------------------------------------------ */

/* clang-format off */
#define FIELDS(X)                                                                                                    \
  X(height) X(lake) X(drainage) X(discharge) X(groundwater) X(receiver) X(point) X(biome) X(order) X(province)     \
  X(cell_flags) X(temperature) X(precipitation) X(plate) X(continental) X(basement) X(lineage) X(thickness)        \
  X(born) X(orogeny) X(elevation) X(geo_flags) X(minerals) X(strata) X(ley_nodes) X(ley_edges) X(ley_path)         \
  X(provinces) X(contact) X(eras)
/* clang-format on */

void zb_world_bundle_free(zb_world_bundle *b) {
  if (!b->a) {
    return;
  }
#define FREE_FIELD(f) zb_free(b->a, b->f);
  FIELDS(FREE_FIELD)
#undef FREE_FIELD
  zb_cubegrid_free(&b->grid);
  zb_cubegrid_free(&b->climate_grid);
  memset(b, 0, sizeof *b);
}

/* Allocates every array for the sizes already set in b. */
static zb_result allocate(zb_world_bundle *b, int res, int climate_res) {
  const zb_allocator *a = b->a;
  double r_km = b->options.radius / 1000.0;
  if (zb_cubegrid_init(&b->grid, a, res, r_km) != ZB_OK ||
      zb_cubegrid_init(&b->climate_grid, a, climate_res, r_km) != ZB_OK) {
    return ZB_ERR_NOMEM;
  }
  size_t mc = b->grid.cells, cc = b->climate_grid.cells, n = (size_t)b->points;
  b->height = zb_alloc(a, sizeof(float) * mc);
  b->lake = zb_alloc(a, sizeof(float) * mc);
  b->drainage = zb_alloc(a, sizeof(float) * mc);
  b->discharge = zb_alloc(a, sizeof(float) * mc);
  b->groundwater = zb_alloc(a, sizeof(float) * mc);
  b->receiver = zb_alloc(a, sizeof(int32_t) * mc);
  b->point = zb_alloc(a, sizeof(int32_t) * mc);
  b->biome = zb_alloc(a, mc);
  b->order = zb_alloc(a, mc);
  b->province = zb_alloc(a, mc);
  b->cell_flags = zb_alloc(a, sizeof(uint16_t) * mc);
  b->temperature = zb_alloc(a, sizeof(float) * cc * ZB_MONTHS);
  b->precipitation = zb_alloc(a, sizeof(float) * cc * ZB_MONTHS);
  b->plate = zb_alloc(a, n);
  b->continental = zb_alloc(a, n);
  b->basement = zb_alloc(a, n);
  b->lineage = zb_alloc(a, n);
  b->thickness = zb_alloc(a, sizeof(float) * n);
  b->born = zb_alloc(a, sizeof(float) * n);
  b->orogeny = zb_alloc(a, sizeof(float) * n);
  b->elevation = zb_alloc(a, sizeof(float) * n);
  b->geo_flags = zb_alloc(a, sizeof(uint16_t) * n);
  b->minerals = zb_alloc(a, sizeof(uint32_t) * n);
  b->strata = zb_alloc(a, sizeof(zb_stratum) * n * ZB_STRATA);
  b->ley_nodes = zb_alloc(a, sizeof(zb_ley_node) * (size_t)(b->ley_node_count + 1));
  b->ley_edges = zb_alloc(a, sizeof(zb_ley_edge) * (size_t)(b->ley_edge_count + 1));
  b->ley_path = zb_alloc(a, sizeof(int32_t) * (size_t)(b->ley_path_len + 1));
  b->provinces = zb_alloc(a, sizeof(zb_province) * (size_t)(b->province_count + 1));
  b->contact = zb_alloc(a, sizeof(float) * (size_t)(b->lineages * b->lineages + 1));
  b->eras = zb_alloc(a, sizeof(zb_geology_era) * (size_t)(b->era_count + 1));
#define CHECK_FIELD(f) ok = ok && b->f;
  bool ok = true;
  FIELDS(CHECK_FIELD)
#undef CHECK_FIELD
  return ok ? ZB_OK : ZB_ERR_NOMEM;
}

zb_result zb_world_bundle_build(const zb_allocator *a, const zb_world_options *o, const zb_geology *g,
                                const zb_climate *c, zb_world_bundle *b) {
  memset(b, 0, sizeof *b);
  const zb_geology_macro *m = zb_geology_get_macro(g);
  const uint8_t *biomes = zb_climate_biomes(c);
  if (!m || !biomes) {
    return ZB_ERR_STATE;
  }
  b->a = a;
  b->options = *o;
  b->generator_version = ZB_GENERATOR_VERSION;
  b->metrics = *zb_geology_get_metrics(g);
  zb_geology_points pts;
  zb_geology_get_points(g, &pts);
  zb_ley_network ley;
  zb_geology_get_ley(g, &ley);
  int provinces;
  const zb_province *pv = zb_climate_provinces(c, &provinces);
  int eras;
  const zb_geology_era *era = zb_geology_eras(g, &eras);
  b->points = pts.n;
  b->ley_node_count = ley.nodes;
  b->ley_edge_count = ley.edges;
  b->ley_path_len = 0;
  for (int e = 0; e < ley.edges; ++e) {
    int end = ley.edge[e].path + ley.edge[e].length;
    b->ley_path_len = end > b->ley_path_len ? end : b->ley_path_len;
  }
  b->province_count = provinces;
  b->lineages = ZB_GEO_LINEAGES;
  b->era_count = eras;
  if (allocate(b, m->res, zb_climate_grid(c)->res) != ZB_OK) {
    zb_world_bundle_free(b);
    return ZB_ERR_NOMEM;
  }
  size_t mc = b->grid.cells, cc = b->climate_grid.cells, n = (size_t)b->points;
  memcpy(b->height, m->height, sizeof(float) * mc);
  memcpy(b->lake, m->lake, sizeof(float) * mc);
  memcpy(b->drainage, m->drainage, sizeof(float) * mc);
  memcpy(b->receiver, m->receiver, sizeof(int32_t) * mc);
  memcpy(b->point, m->point, sizeof(int32_t) * mc);
  memcpy(b->discharge, zb_climate_discharge(c), sizeof(float) * mc);
  memcpy(b->groundwater, zb_climate_groundwater(c), sizeof(float) * mc);
  memcpy(b->biome, biomes, mc);
  memcpy(b->order, zb_climate_river_order(c), mc);
  memcpy(b->province, zb_climate_province_map(c), mc);
  memcpy(b->cell_flags, zb_climate_flags(c), sizeof(uint16_t) * mc);
  memcpy(b->temperature, zb_climate_monthly(c, false), sizeof(float) * cc * ZB_MONTHS);
  memcpy(b->precipitation, zb_climate_monthly(c, true), sizeof(float) * cc * ZB_MONTHS);
  memcpy(b->plate, pts.plate, n);
  memcpy(b->continental, pts.continental, n);
  memcpy(b->basement, pts.basement, n);
  memcpy(b->lineage, pts.lineage, n);
  memcpy(b->thickness, pts.thickness, sizeof(float) * n);
  memcpy(b->born, pts.born, sizeof(float) * n);
  memcpy(b->orogeny, pts.orogeny, sizeof(float) * n);
  memcpy(b->elevation, pts.elevation, sizeof(float) * n);
  memcpy(b->geo_flags, pts.flags, sizeof(uint16_t) * n);
  memcpy(b->minerals, pts.minerals, sizeof(uint32_t) * n);
  memcpy(b->strata, pts.strata, sizeof(zb_stratum) * n * ZB_STRATA);
  memcpy(b->ley_nodes, ley.node, sizeof(zb_ley_node) * (size_t)ley.nodes);
  memcpy(b->ley_edges, ley.edge, sizeof(zb_ley_edge) * (size_t)ley.edges);
  memcpy(b->ley_path, ley.path, sizeof(int32_t) * (size_t)b->ley_path_len);
  memcpy(b->provinces, pv, sizeof(zb_province) * (size_t)provinces);
  memcpy(b->contact, zb_geology_contacts(g), sizeof(float) * ZB_GEO_LINEAGES * ZB_GEO_LINEAGES);
  memcpy(b->eras, era, sizeof(zb_geology_era) * (size_t)eras);
  return ZB_OK;
}

/* ---- Serialization ------------------------------------------------------------------------ */

typedef struct writer {
  zb_array raw;
  bool failed;
} writer;

static void put(writer *w, const void *data, size_t len) {
  if (w->failed || zb_array_reserve(&w->raw, w->raw.len + len) != ZB_OK) {
    w->failed = true;
    return;
  }
  memcpy(w->raw.data + w->raw.len, data, len);
  w->raw.len += len;
}

static void put_u8(writer *w, uint8_t v) { put(w, &v, 1); }
static void put_u16(writer *w, uint16_t v) {
  uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
  put(w, b, 2);
}
static void put_u32(writer *w, uint32_t v) {
  uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
  put(w, b, 4);
}
static void put_u64(writer *w, uint64_t v) {
  put_u32(w, (uint32_t)v);
  put_u32(w, (uint32_t)(v >> 32));
}
static void put_f32(writer *w, float v) {
  uint32_t u;
  memcpy(&u, &v, 4);
  put_u32(w, u);
}
static void put_f64(writer *w, double v) {
  uint64_t u;
  memcpy(&u, &v, 8);
  put_u64(w, u);
}
static void put_f32s(writer *w, const float *v, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    put_f32(w, v[i]);
  }
}
static void put_i32s(writer *w, const int32_t *v, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    put_u32(w, (uint32_t)v[i]);
  }
}
static void put_u16s(writer *w, const uint16_t *v, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    put_u16(w, v[i]);
  }
}

typedef struct reader {
  const uint8_t *p;
  size_t len, pos;
  bool failed;
} reader;

static const uint8_t *take(reader *r, size_t len) {
  if (r->failed || r->len - r->pos < len) {
    r->failed = true;
    return NULL;
  }
  const uint8_t *p = r->p + r->pos;
  r->pos += len;
  return p;
}
static uint8_t get_u8(reader *r) {
  const uint8_t *p = take(r, 1);
  return p ? p[0] : 0;
}
static uint16_t get_u16(reader *r) {
  const uint8_t *p = take(r, 2);
  if (!p) {
    return 0;
  }
  return (uint16_t)(p[0] | p[1] << 8);
}
static uint32_t get_u32(reader *r) {
  const uint8_t *p = take(r, 4);
  return p ? (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24 : 0;
}
static uint64_t get_u64(reader *r) {
  uint64_t lo = get_u32(r);
  return lo | (uint64_t)get_u32(r) << 32;
}
static float get_f32(reader *r) {
  uint32_t u = get_u32(r);
  float v;
  memcpy(&v, &u, 4);
  return v;
}
static double get_f64(reader *r) {
  uint64_t u = get_u64(r);
  double v;
  memcpy(&v, &u, 8);
  return v;
}
static void get_f32s(reader *r, float *v, size_t n) {
  for (size_t i = 0; i < n && !r->failed; ++i) {
    v[i] = get_f32(r);
  }
}
static void get_i32s(reader *r, int32_t *v, size_t n) {
  for (size_t i = 0; i < n && !r->failed; ++i) {
    v[i] = (int32_t)get_u32(r);
  }
}
static void get_u16s(reader *r, uint16_t *v, size_t n) {
  for (size_t i = 0; i < n && !r->failed; ++i) {
    v[i] = get_u16(r);
  }
}
static void get_bytes(reader *r, void *v, size_t n) {
  const uint8_t *p = take(r, n);
  if (p) {
    memcpy(v, p, n);
  }
}

/* Each section: tag, raw size, stored size, checksum of the raw bytes, zlib data. */
static void section(zb_array *out, const char tag[4], writer *w, bool *failed) {
  if (w->failed || *failed) {
    *failed = true;
    return;
  }
  zb_array z;
  zb_array_init(&z, out->a, 1);
  if (zb_deflate(w->raw.data, w->raw.len, &z) != ZB_OK) {
    zb_array_free(&z);
    *failed = true;
    return;
  }
  writer head;
  zb_array_init(&head.raw, out->a, 1);
  head.failed = false;
  put(&head, tag, 4);
  put_u64(&head, (uint64_t)w->raw.len);
  put_u64(&head, (uint64_t)z.len);
  put_u64(&head, zb_hash64(w->raw.data, w->raw.len, 0x62756e646c65ull));
  if (head.failed || zb_array_reserve(out, out->len + head.raw.len + z.len) != ZB_OK) {
    *failed = true;
  } else {
    memcpy(out->data + out->len, head.raw.data, head.raw.len);
    memcpy(out->data + out->len + head.raw.len, z.data, z.len);
    out->len += head.raw.len + z.len;
  }
  zb_array_free(&head.raw);
  zb_array_free(&z);
  w->raw.len = 0;
}

zb_result zb_world_bundle_write(const zb_world_bundle *b, zb_array *out) {
  size_t start = out->len;
  writer w;
  zb_array_init(&w.raw, out->a, 1);
  w.failed = false;
  bool failed = false;
  /* Header. */
  put(&w, MAGIC, 4);
  put_u32(&w, ZB_BUNDLE_VERSION);
  put_u32(&w, b->generator_version);
  put_u32(&w, 8); /* sections */
  if (w.failed || zb_array_reserve(out, out->len + w.raw.len) != ZB_OK) {
    zb_array_free(&w.raw);
    return ZB_ERR_NOMEM;
  }
  memcpy(out->data + out->len, w.raw.data, w.raw.len);
  out->len += w.raw.len;
  w.raw.len = 0;
  /* Options and metrics. */
  const zb_world_options *o = &b->options;
  put_u64(&w, o->seed);
  put_f64(&w, o->radius);
  put_u32(&w, (uint32_t)o->target);
  put_f64(&w, o->land_fraction);
  put_f64(&w, o->temperature_offset);
  put_f64(&w, o->axial_tilt);
  put_u32(&w, (uint32_t)o->quality);
  const zb_geology_metrics *m = &b->metrics;
  put_f64(&w, m->land_fraction);
  put_u32(&w, (uint32_t)m->continents);
  put_u32(&w, (uint32_t)m->islands);
  put_f64(&w, m->largest_landmass);
  put_f64(&w, m->continental_land);
  put_u32(&w, (uint32_t)m->large_continents);
  put_u32(&w, (uint32_t)m->continents_with_mountains);
  put_u8(&w, m->young_ranges);
  put_u8(&w, m->old_ranges);
  put_f64(&w, m->ice_fraction);
  put_u32(&w, m->climate_bands);
  put_u32(&w, (uint32_t)m->attempts);
  put_u8(&w, m->passed);
  put_f64(&w, m->score);
  section(out, "OPTS", &w, &failed);
  /* Macro grid. */
  size_t mc = b->grid.cells;
  put_u32(&w, (uint32_t)b->grid.res);
  put_f32s(&w, b->height, mc);
  put_f32s(&w, b->lake, mc);
  put_f32s(&w, b->drainage, mc);
  put_f32s(&w, b->discharge, mc);
  put_f32s(&w, b->groundwater, mc);
  put_i32s(&w, b->receiver, mc);
  put_i32s(&w, b->point, mc);
  put(&w, b->biome, mc);
  put(&w, b->order, mc);
  put(&w, b->province, mc);
  put_u16s(&w, b->cell_flags, mc);
  section(out, "MACR", &w, &failed);
  /* Climate grid. */
  size_t cc = b->climate_grid.cells * ZB_MONTHS;
  put_u32(&w, (uint32_t)b->climate_grid.res);
  put_f32s(&w, b->temperature, cc);
  put_f32s(&w, b->precipitation, cc);
  section(out, "CLIM", &w, &failed);
  /* Lattice points and their rock record. */
  size_t n = (size_t)b->points;
  put_u32(&w, (uint32_t)b->points);
  put(&w, b->plate, n);
  put(&w, b->continental, n);
  put(&w, b->basement, n);
  put(&w, b->lineage, n);
  put_f32s(&w, b->thickness, n);
  put_f32s(&w, b->born, n);
  put_f32s(&w, b->orogeny, n);
  put_f32s(&w, b->elevation, n);
  put_u16s(&w, b->geo_flags, n);
  for (size_t i = 0; i < n; ++i) {
    put_u32(&w, b->minerals[i]);
  }
  for (size_t i = 0; i < n * ZB_STRATA; ++i) {
    put_u8(&w, b->strata[i].rock);
    put_u8(&w, b->strata[i].env);
    put_u16(&w, b->strata[i].thickness);
    put_u16(&w, b->strata[i].age);
  }
  section(out, "LATT", &w, &failed);
  /* Ley network. */
  put_u32(&w, (uint32_t)b->ley_node_count);
  put_u32(&w, (uint32_t)b->ley_edge_count);
  put_u32(&w, (uint32_t)b->ley_path_len);
  for (int k = 0; k < b->ley_node_count; ++k) {
    put_u32(&w, (uint32_t)b->ley_nodes[k].point);
    put_u8(&w, b->ley_nodes[k].kind);
    put_f32(&w, b->ley_nodes[k].strength);
  }
  for (int k = 0; k < b->ley_edge_count; ++k) {
    const zb_ley_edge *e = &b->ley_edges[k];
    put_u32(&w, (uint32_t)e->a);
    put_u32(&w, (uint32_t)e->b);
    put_u32(&w, (uint32_t)e->path);
    put_u32(&w, (uint32_t)e->length);
    put_u8(&w, e->loop);
  }
  put_i32s(&w, b->ley_path, (size_t)b->ley_path_len);
  section(out, "LEY ", &w, &failed);
  /* Provinces. */
  put_u32(&w, (uint32_t)b->province_count);
  for (int k = 0; k < b->province_count; ++k) {
    const zb_province *p = &b->provinces[k];
    put_f64(&w, p->centre.x);
    put_f64(&w, p->centre.y);
    put_f64(&w, p->centre.z);
    put_f32(&w, p->area);
    put_u32(&w, (uint32_t)p->landmass);
    put_u32(&w, (uint32_t)p->lineage);
    put_f32(&w, p->isolation);
  }
  section(out, "PROV", &w, &failed);
  /* Lineage contacts. */
  put_u32(&w, (uint32_t)b->lineages);
  put_f32s(&w, b->contact, (size_t)(b->lineages * b->lineages));
  section(out, "LINE", &w, &failed);
  /* Eras. */
  put_u32(&w, (uint32_t)b->era_count);
  for (int k = 0; k < b->era_count; ++k) {
    const zb_geology_era *e = &b->eras[k];
    put_f32(&w, e->myr_ago);
    put_f32(&w, e->sea_level);
    put_f32(&w, e->temperature);
    put_f32(&w, e->humidity);
    put_u8(&w, e->ice_age);
    put_u8(&w, e->plates);
    put_f32(&w, e->land_fraction);
  }
  section(out, "ERAS", &w, &failed);
  zb_array_free(&w.raw);
  if (failed) {
    out->len = start;
    return ZB_ERR_NOMEM;
  }
  return ZB_OK;
}

static bool fail(char *err, size_t err_len, const char *why) {
  snprintf(err, err_len, "%s", why);
  return false;
}

/* Section payload readers; each checks sizes against the header counts. */
static bool read_opts(zb_world_bundle *b, reader *r) {
  zb_world_options *o = &b->options;
  o->seed = get_u64(r);
  o->radius = get_f64(r);
  o->target = (zb_geology_target)get_u32(r);
  o->land_fraction = get_f64(r);
  o->temperature_offset = get_f64(r);
  o->axial_tilt = get_f64(r);
  o->quality = (zb_world_quality)get_u32(r);
  zb_geology_metrics *m = &b->metrics;
  m->land_fraction = get_f64(r);
  m->continents = (int)get_u32(r);
  m->islands = (int)get_u32(r);
  m->largest_landmass = get_f64(r);
  m->continental_land = get_f64(r);
  m->large_continents = (int)get_u32(r);
  m->continents_with_mountains = (int)get_u32(r);
  m->young_ranges = get_u8(r) != 0;
  m->old_ranges = get_u8(r) != 0;
  m->ice_fraction = get_f64(r);
  m->climate_bands = get_u32(r);
  m->attempts = (int)get_u32(r);
  m->passed = get_u8(r) != 0;
  m->score = get_f64(r);
  return !r->failed && o->radius >= ZB_PLANET_MIN_RADIUS && o->radius <= ZB_PLANET_MAX_RADIUS && (int)o->target >= 0 &&
         o->target < ZB_GEO_TARGET_COUNT && (int)o->quality >= 0 && o->quality < ZB_WORLD_QUALITY_COUNT;
}

/* The sizes live at the start of each section: read them all before allocating. */
typedef struct sizes {
  int res, climate_res, points, nodes, edges, path, provinces, lineages, eras;
} sizes;

static void read_macro(zb_world_bundle *b, reader *r) {
  size_t mc = b->grid.cells;
  get_f32s(r, b->height, mc);
  get_f32s(r, b->lake, mc);
  get_f32s(r, b->drainage, mc);
  get_f32s(r, b->discharge, mc);
  get_f32s(r, b->groundwater, mc);
  get_i32s(r, b->receiver, mc);
  get_i32s(r, b->point, mc);
  get_bytes(r, b->biome, mc);
  get_bytes(r, b->order, mc);
  get_bytes(r, b->province, mc);
  get_u16s(r, b->cell_flags, mc);
  for (size_t c = 0; c < mc && !r->failed; ++c) {
    if (b->receiver[c] >= (int32_t)mc || b->receiver[c] < -1 || b->point[c] < 0 || b->point[c] >= b->points ||
        b->biome[c] >= ZB_BIOME_COUNT || b->province[c] > b->province_count) {
      r->failed = true;
    }
  }
}

static void read_lattice(zb_world_bundle *b, reader *r) {
  size_t n = (size_t)b->points;
  get_bytes(r, b->plate, n);
  get_bytes(r, b->continental, n);
  get_bytes(r, b->basement, n);
  get_bytes(r, b->lineage, n);
  get_f32s(r, b->thickness, n);
  get_f32s(r, b->born, n);
  get_f32s(r, b->orogeny, n);
  get_f32s(r, b->elevation, n);
  get_u16s(r, b->geo_flags, n);
  for (size_t i = 0; i < n && !r->failed; ++i) {
    b->minerals[i] = get_u32(r);
  }
  for (size_t i = 0; i < n * ZB_STRATA && !r->failed; ++i) {
    zb_stratum *s = &b->strata[i];
    s->rock = get_u8(r);
    s->env = get_u8(r);
    s->thickness = get_u16(r);
    s->age = get_u16(r);
    if (s->rock >= ZB_ROCK_COUNT || s->env >= ZB_ENV_COUNT) {
      r->failed = true;
    }
  }
}

static void read_ley(zb_world_bundle *b, reader *r) {
  for (int k = 0; k < b->ley_node_count && !r->failed; ++k) {
    b->ley_nodes[k].point = (int32_t)get_u32(r);
    b->ley_nodes[k].kind = get_u8(r);
    b->ley_nodes[k].strength = get_f32(r);
    r->failed = r->failed || b->ley_nodes[k].point < 0 || b->ley_nodes[k].point >= b->points;
  }
  for (int k = 0; k < b->ley_edge_count && !r->failed; ++k) {
    zb_ley_edge *e = &b->ley_edges[k];
    e->a = (int32_t)get_u32(r);
    e->b = (int32_t)get_u32(r);
    e->path = (int32_t)get_u32(r);
    e->length = (int32_t)get_u32(r);
    e->loop = get_u8(r) != 0;
    r->failed = r->failed || e->a < 0 || e->a >= b->ley_node_count || e->b < 0 || e->b >= b->ley_node_count ||
                e->path < 0 || e->length < 0 || e->path + e->length > b->ley_path_len;
  }
  get_i32s(r, b->ley_path, (size_t)b->ley_path_len);
  for (int k = 0; k < b->ley_path_len && !r->failed; ++k) {
    r->failed = b->ley_path[k] < 0 || b->ley_path[k] >= b->points;
  }
}

static void read_provinces(zb_world_bundle *b, reader *r) {
  for (int k = 0; k < b->province_count && !r->failed; ++k) {
    zb_province *p = &b->provinces[k];
    p->centre.x = get_f64(r);
    p->centre.y = get_f64(r);
    p->centre.z = get_f64(r);
    p->area = get_f32(r);
    p->landmass = (int)get_u32(r);
    p->lineage = (int)get_u32(r);
    p->isolation = get_f32(r);
  }
}

static void read_eras(zb_world_bundle *b, reader *r) {
  for (int k = 0; k < b->era_count && !r->failed; ++k) {
    zb_geology_era *e = &b->eras[k];
    e->myr_ago = get_f32(r);
    e->sea_level = get_f32(r);
    e->temperature = get_f32(r);
    e->humidity = get_f32(r);
    e->ice_age = get_u8(r) != 0;
    e->plates = get_u8(r);
    e->land_fraction = get_f32(r);
  }
}

typedef struct raw_section {
  char tag[4];
  uint8_t *data;
  size_t len;
} raw_section;

#define SECTIONS 8

static bool unpack(const zb_allocator *a, reader *file, raw_section *sec, char *err, size_t err_len) {
  static const char *const tags[SECTIONS] = {"OPTS", "MACR", "CLIM", "LATT", "LEY ", "PROV", "LINE", "ERAS"};
  uint32_t count = get_u32(file);
  for (uint32_t s = 0; s < count && !file->failed; ++s) {
    const uint8_t *tag = take(file, 4);
    uint64_t raw = get_u64(file), stored = get_u64(file), sum = get_u64(file);
    const uint8_t *z = take(file, (size_t)stored);
    if (file->failed || raw > ((uint64_t)1 << 32)) {
      return fail(err, err_len, "the world bundle is cut short");
    }
    int which = -1;
    for (int k = 0; k < SECTIONS && which < 0; ++k) {
      which = memcmp(tag, tags[k], 4) == 0 ? k : -1;
    }
    if (which < 0 || sec[which].data) {
      continue; /* a section from a newer version, or a repeat: skipped */
    }
    sec[which].data = zb_alloc(a, (size_t)raw + 1);
    sec[which].len = (size_t)raw;
    if (!sec[which].data) {
      return fail(err, err_len, "out of memory");
    }
    if (zb_inflate(z, (size_t)stored, sec[which].data, (size_t)raw) != ZB_OK ||
        zb_hash64(sec[which].data, (size_t)raw, 0x62756e646c65ull) != sum) {
      return fail(err, err_len, "the world bundle is damaged (checksum)");
    }
  }
  for (int k = 0; k < SECTIONS; ++k) {
    if (!sec[k].data) {
      return fail(err, err_len, "the world bundle misses a section");
    }
  }
  return true;
}

zb_result zb_world_bundle_read(const zb_allocator *a, const void *data, size_t len, zb_world_bundle *b, char *err,
                               size_t err_len) {
  memset(b, 0, sizeof *b);
  b->a = a;
  reader file = {data, len, 0, false};
  const uint8_t *magic = take(&file, 4);
  if (!magic || memcmp(magic, MAGIC, 4) != 0) {
    fail(err, err_len, "not a world bundle");
    return ZB_ERR_INVALID;
  }
  uint32_t version = get_u32(&file);
  b->generator_version = get_u32(&file);
  if (version > ZB_BUNDLE_VERSION || b->generator_version > ZB_GENERATOR_VERSION) {
    fail(err, err_len, "the world bundle comes from a newer version of the game");
    return ZB_ERR_INVALID;
  }
  raw_section sec[SECTIONS];
  memset(sec, 0, sizeof sec);
  bool ok = unpack(a, &file, sec, err, err_len);
  reader r[SECTIONS];
  for (int k = 0; k < SECTIONS; ++k) {
    r[k] = (reader){sec[k].data, sec[k].len, 0, false};
  }
  if (ok && !read_opts(b, &r[0])) {
    ok = fail(err, err_len, "the world bundle has invalid options");
  }
  sizes z = {0};
  if (ok) {
    z.res = (int)get_u32(&r[1]);
    z.climate_res = (int)get_u32(&r[2]);
    z.points = (int)get_u32(&r[3]);
    z.nodes = (int)get_u32(&r[4]);
    z.edges = (int)get_u32(&r[4]);
    z.path = (int)get_u32(&r[4]);
    z.provinces = (int)get_u32(&r[5]);
    z.lineages = (int)get_u32(&r[6]);
    z.eras = (int)get_u32(&r[7]);
    /* Every count must fit what its section can hold before anything is allocated. */
    size_t mc = (size_t)6 * (size_t)z.res * (size_t)z.res;
    size_t cc = (size_t)6 * (size_t)z.climate_res * (size_t)z.climate_res;
    bool fits = z.res >= 2 && z.res <= 4096 && z.climate_res >= 2 && z.climate_res <= 4096 && z.points >= 1 &&
                z.points <= ZB_LATTICE_MAX_POINTS && z.nodes >= 0 && z.edges >= 0 && z.path >= 0 && z.provinces >= 0 &&
                z.provinces <= 255 && z.lineages >= 1 && z.lineages <= 256 && z.eras >= 0 &&
                sec[1].len >= 4 + mc * 33 && sec[2].len >= 4 + cc * ZB_MONTHS * 8 &&
                sec[3].len >= 4 + (size_t)z.points * (26 + 6 * ZB_STRATA) &&
                sec[4].len >= 12 + (size_t)z.nodes * 9 + (size_t)z.edges * 17 + (size_t)z.path * 4 &&
                sec[5].len >= 4 + (size_t)z.provinces * 40 &&
                sec[6].len >= 4 + (size_t)z.lineages * (size_t)z.lineages * 4 && sec[7].len >= 4 + (size_t)z.eras * 22;
    if (!fits) {
      ok = fail(err, err_len, "the world bundle has inconsistent sizes");
    }
  }
  if (ok) {
    b->points = z.points;
    b->ley_node_count = z.nodes;
    b->ley_edge_count = z.edges;
    b->ley_path_len = z.path;
    b->province_count = z.provinces;
    b->lineages = z.lineages;
    b->era_count = z.eras;
    if (allocate(b, z.res, z.climate_res) != ZB_OK) {
      ok = fail(err, err_len, "out of memory");
    }
  }
  if (ok) {
    size_t cc = b->climate_grid.cells * ZB_MONTHS;
    read_macro(b, &r[1]);
    get_f32s(&r[2], b->temperature, cc);
    get_f32s(&r[2], b->precipitation, cc);
    read_lattice(b, &r[3]);
    read_ley(b, &r[4]);
    read_provinces(b, &r[5]);
    get_f32s(&r[6], b->contact, (size_t)(b->lineages * b->lineages));
    read_eras(b, &r[7]);
    for (int k = 1; k < SECTIONS && ok; ++k) {
      if (r[k].failed) {
        ok = fail(err, err_len, "the world bundle has invalid data");
      }
    }
  }
  for (int k = 0; k < SECTIONS; ++k) {
    zb_free(a, sec[k].data);
  }
  if (!ok) {
    zb_world_bundle_free(b);
    return ZB_ERR_INVALID;
  }
  return ZB_OK;
}

uint64_t zb_world_bundle_hash(const zb_world_bundle *b) {
  zb_hasher h;
  zb_hasher_init(&h, 0x776f726c64ull);
  zb_hasher_u64(&h, b->options.seed);
  zb_hasher_f64(&h, b->options.radius);
  size_t mc = b->grid.cells, n = (size_t)b->points;
  zb_hasher_bytes(&h, b->height, sizeof(float) * mc);
  zb_hasher_bytes(&h, b->biome, mc);
  zb_hasher_bytes(&h, b->cell_flags, sizeof(uint16_t) * mc);
  zb_hasher_bytes(&h, b->discharge, sizeof(float) * mc);
  zb_hasher_bytes(&h, b->province, mc);
  zb_hasher_bytes(&h, b->temperature, sizeof(float) * b->climate_grid.cells * ZB_MONTHS);
  zb_hasher_bytes(&h, b->elevation, sizeof(float) * n);
  zb_hasher_bytes(&h, b->strata, sizeof(zb_stratum) * n * ZB_STRATA);
  zb_hasher_bytes(&h, b->minerals, sizeof(uint32_t) * n);
  zb_hasher_bytes(&h, b->ley_path, sizeof(int32_t) * (size_t)b->ley_path_len);
  return zb_hasher_final(&h);
}

/* ---- Queries ------------------------------------------------------------------------------- */

double zb_world_bundle_height(const zb_world_bundle *b, zb_dvec3 dir) {
  return zb_cubegrid_sample_smooth(&b->grid, b->height, dir);
}

int32_t zb_world_bundle_cell(const zb_world_bundle *b, zb_dvec3 dir) { return zb_cubegrid_cell_at(&b->grid, dir); }

zb_biome zb_world_bundle_biome(const zb_world_bundle *b, zb_dvec3 dir) {
  return (zb_biome)b->biome[zb_world_bundle_cell(b, dir)];
}

int32_t zb_world_bundle_point(const zb_world_bundle *b, zb_dvec3 dir) { return b->point[zb_world_bundle_cell(b, dir)]; }

void zb_world_bundle_climate(const zb_world_bundle *b, zb_dvec3 dir, int month, double height, double *temperature,
                             double *precipitation) {
  size_t off = (size_t)(month % ZB_MONTHS) * b->climate_grid.cells;
  *temperature =
      zb_cubegrid_sample(&b->climate_grid, b->temperature + off, dir) - 0.0065 * (height > 0.0 ? height : 0.0);
  *precipitation = zb_cubegrid_sample(&b->climate_grid, b->precipitation + off, dir);
}

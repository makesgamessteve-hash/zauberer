/* SPDX-License-Identifier: MIT */
#include "world/terrain.h"

#include <math.h>
#include <string.h>

#include "core/dmath.h"
#include "core/hash.h"
#include "world/bundle.h"
#include "world/dvec.h"
#include "world/noise.h"
#include "world/water.h"

#define MACRO_RES (ZB_COARSE_RES + 2)
#define COARSE_STEP 16 /* stage 1 samples per stage 0 sample */
#define DETAIL_RES 129 /* detail noise grid per chunk edge (2 m apart) */
#define THRESHOLD_SAMPLES 2048

/* Independent noise streams. */
enum {
  SEED_CONTINENT = 1,
  SEED_HILLS,
  SEED_MOUNTAIN_MASK,
  SEED_RIDGES,
  SEED_DETAIL,
  SEED_JITTER,
  SEED_CRAGS,
  SEED_RAIN,
  SEED_PLANTS
};

static uint64_t stream(const zb_terrain *t, int which) {
  return zb_mix64(t->seed ^ ((uint64_t)(uint32_t)which * 0x9e3779b97f4a7c15ull));
}

void zb_terrain_default_params(zb_terrain_params *out) {
  out->land_fraction = 0.35;
  out->ocean_depth = 4000.0;
  out->lowland_height = 600.0;
  out->hill_height = 120.0;
  out->mountain_height = 3500.0;
  out->detail_height = 2.5;
}

static double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

static double smoothstep(double e0, double e1, double x) {
  double t = clamp01((x - e0) / (e1 - e0));
  return t * t * (3.0 - 2.0 * t);
}

/* Continent field: frequencies relative to the radius, so any planet size
 * gets a handful of continents. */
static double continent_field(const zb_terrain *t, zb_dvec3 d) {
  return zb_fbm3(stream(t, SEED_CONTINENT), d.x * 1.6, d.y * 1.6, d.z * 1.6, 9, 0.5);
}

static void sort_doubles(double *v, size_t n) {
  /* Shell sort: deterministic and allocation-free. */
  static const size_t gaps[] = {701, 301, 132, 57, 23, 10, 4, 1};
  for (size_t g = 0; g < sizeof gaps / sizeof gaps[0]; ++g) {
    size_t gap = gaps[g];
    for (size_t i = gap; i < n; ++i) {
      double x = v[i];
      size_t j = i;
      while (j >= gap && v[j - gap] > x) {
        v[j] = v[j - gap];
        j -= gap;
      }
      v[j] = x;
    }
  }
}

void zb_terrain_init(zb_terrain *t, const zb_planet *p, uint64_t seed, const zb_terrain_params *params) {
  t->planet = *p;
  t->seed = seed;
  if (params) {
    t->params = *params;
  } else {
    zb_terrain_default_params(&t->params);
  }
  /* Sea level: the continent-field quantile that leaves land_fraction above it,
   * measured on a Fibonacci sphere. */
  double samples[THRESHOLD_SAMPLES];
  const double golden_angle = ZB_DPI * (3.0 - sqrt(5.0));
  for (int i = 0; i < THRESHOLD_SAMPLES; ++i) {
    double z = 1.0 - (2.0 * i + 1.0) / THRESHOLD_SAMPLES;
    double r = sqrt(1.0 - z * z);
    double phi = golden_angle * i;
    zb_dvec3 d = {r * zb_dcos(phi), r * zb_dsin(phi), z};
    samples[i] = continent_field(t, d);
  }
  sort_doubles(samples, THRESHOLD_SAMPLES);
  int k = (int)((1.0 - t->params.land_fraction) * (THRESHOLD_SAMPLES - 1) + 0.5);
  k = k < 0 ? 0 : (k >= THRESHOLD_SAMPLES ? THRESHOLD_SAMPLES - 1 : k);
  t->continent_threshold = samples[k];
  t->bundle = NULL;
  t->flora = NULL;
}

void zb_terrain_use_bundle(zb_terrain *t, const zb_world_bundle *bundle) { t->bundle = bundle; }

void zb_terrain_use_flora(zb_terrain *t, const zb_flora *flora) { t->flora = flora; }

void zb_terrain_chunk_climate(const zb_terrain *t, zb_chunk c, double *mean, double *coldest, double *warmest,
                              double *precipitation) {
  zb_dvec3 d = zb_planet_chunk_dir(&t->planet, c);
  if (t->bundle) {
    double sum = 0.0, rain = 0.0, lo = 1e9, hi = -1e9;
    for (int m = 0; m < ZB_MONTHS; ++m) {
      double temp, p;
      zb_world_bundle_climate(t->bundle, d, m, 0.0, &temp, &p);
      sum += temp;
      rain += p;
      lo = temp < lo ? temp : lo;
      hi = temp > hi ? temp : hi;
    }
    *mean = sum / ZB_MONTHS;
    *coldest = lo;
    *warmest = hi;
    *precipitation = rain;
    return;
  }
  /* The noise world: warm at the equator, seasons growing poleward, and
   * rainy and dry regions a few hundred kilometres across. */
  double s2 = d.z * d.z, r = t->planet.radius / 400000.0;
  double wet = 0.5 + 0.5 * zb_fbm3(stream(t, SEED_RAIN), d.x * r, d.y * r, d.z * r, 3, 0.5);
  double swing = 4.0 + 30.0 * fabs(d.z);
  *mean = 27.0 - 42.0 * s2;
  *coldest = *mean - 0.5 * swing;
  *warmest = *mean + 0.5 * swing;
  *precipitation = (150.0 + 2200.0 * wet * wet) * (1.25 - 0.85 * s2);
}

/* Terrain v2: the created world's macro grid with hills and crags below its
 * 6 km cells; both fade into river valleys. rs holds the rivers nearby. */
static double bundle_height(const zb_terrain *t, zb_dvec3 d, const zb_river_set *rs) {
  const zb_terrain_params *pr = &t->params;
  double h = zb_world_bundle_height(t->bundle, d);
  double px = d.x * t->planet.radius, py = d.y * t->planet.radius, pz = d.z * t->planet.radius;
  double relief = 1.0;
  zb_river_hit hit;
  if (rs && rs->count && zb_rivers_nearest(rs, t->planet.radius, d, 1e30, &hit)) {
    relief = zb_river_hill_factor(&hit);
  }
  double land = smoothstep(-20.0, 40.0, h);
  h += relief * (0.3 + 0.7 * land) * pr->hill_height *
       zb_fbm3(stream(t, SEED_HILLS), px / 6000.0, py / 6000.0, pz / 6000.0, 6, 0.5);
  double rugged = relief * smoothstep(800.0, 2800.0, h);
  if (rugged > 0.0) {
    double r = zb_ridged3(stream(t, SEED_CRAGS), px / 4000.0, py / 4000.0, pz / 4000.0, 5);
    h += rugged * 450.0 * (r - 0.4);
  }
  return h;
}

/* Rivers within radius metres of d (none in the noise world). Point queries
 * ignore a failure (memory ran out: fewer rivers); chunks report it. */
static zb_result gather_rivers(const zb_terrain *t, zb_dvec3 d, double radius, zb_river_set *rs,
                               const zb_allocator *a) {
  zb_river_set_init(rs, a);
  return t->bundle ? zb_rivers_gather(t->bundle, d, radius, rs) : ZB_OK;
}

/* The widest reach of any river in the set. */
static double max_reach(const zb_river_set *rs) {
  double m = 0.0;
  for (size_t i = 0; i < rs->count; ++i) {
    double r = 2.0 * rs->piece[i].half_width + 4.0;
    m = r > m ? r : m;
  }
  return m;
}

static double carve(const zb_terrain *t, const zb_river_set *rs, double reach, zb_dvec3 d, double height) {
  zb_river_hit hit;
  if (rs->count && zb_rivers_nearest(rs, t->planet.radius, d, reach, &hit)) {
    return zb_river_carve(&hit, height);
  }
  return height;
}

double zb_terrain_macro_height(const zb_terrain *t, zb_dvec3 d) {
  if (t->bundle) {
    zb_river_set rs;
    (void)gather_rivers(t, d, 0.0, &rs, zb_heap_allocator());
    double h = bundle_height(t, d, &rs);
    zb_river_set_free(&rs);
    return h;
  }
  const zb_terrain_params *pr = &t->params;
  double e = continent_field(t, d) - t->continent_threshold;
  double px = d.x * t->planet.radius, py = d.y * t->planet.radius, pz = d.z * t->planet.radius;
  double h;
  if (e < 0.0) {
    h = -pr->ocean_depth * smoothstep(0.0, 0.3, -e);
  } else {
    h = pr->lowland_height * smoothstep(0.0, 0.35, e);
  }
  /* Hills reach slightly below sea level to break up coastlines. */
  double land = smoothstep(-0.03, 0.05, e);
  if (land > 0.0) {
    h += land * pr->hill_height * zb_fbm3(stream(t, SEED_HILLS), px / 6000.0, py / 6000.0, pz / 6000.0, 6, 0.5);
  }
  /* Mountain ranges: ridged noise in belts given by a broad mask. */
  double inland = smoothstep(0.0, 0.12, e);
  if (inland > 0.0) {
    double mask = smoothstep(
        0.1, 0.5, zb_fbm3(stream(t, SEED_MOUNTAIN_MASK), px / 700000.0, py / 700000.0, pz / 700000.0, 3, 0.5));
    if (mask > 0.0) {
      double r = zb_ridged3(stream(t, SEED_RIDGES), px / 50000.0, py / 50000.0, pz / 50000.0, 7);
      h += inland * mask * pr->mountain_height * r * r;
    }
  }
  return h;
}

static double detail_height(const zb_terrain *t, double px, double py, double pz) {
  return t->params.detail_height * zb_fbm3(stream(t, SEED_DETAIL), px / 40.0, py / 40.0, pz / 40.0, 3, 0.45);
}

/* The full height with the rivers in rs (terrain v2). */
static double height_with(const zb_terrain *t, zb_dvec3 d, const zb_river_set *rs) {
  double r = t->planet.radius;
  double h = bundle_height(t, d, rs) + detail_height(t, d.x * r, d.y * r, d.z * r);
  return carve(t, rs, max_reach(rs), d, h);
}

double zb_terrain_height(const zb_terrain *t, zb_dvec3 d) {
  double r = t->planet.radius;
  if (t->bundle) {
    zb_river_set rs;
    (void)gather_rivers(t, d, 0.0, &rs, zb_heap_allocator());
    double h = height_with(t, d, &rs);
    zb_river_set_free(&rs);
    return h;
  }
  return zb_terrain_macro_height(t, d) + detail_height(t, d.x * r, d.y * r, d.z * r);
}

zb_material zb_terrain_material_rule(double height, double slope, double sin_lat, double jitter) {
  double polar = fabs(sin_lat);
  double snowline = 3200.0 - 3000.0 * polar * polar;
  if (height < -25.0) {
    return ZB_MAT_MUD;
  }
  if (height < 2.0 + jitter) {
    return ZB_MAT_SAND;
  }
  if (slope > 0.78) { /* steeper than about 38 degrees */
    return ZB_MAT_ROCK;
  }
  if (height > snowline + 80.0 * jitter) {
    return slope > 0.6 ? ZB_MAT_ROCK : ZB_MAT_SNOW;
  }
  if (polar > 0.96) { /* above about 74 degrees latitude */
    return ZB_MAT_ICE;
  }
  if (slope > 0.58) { /* scree between about 30 and 38 degrees */
    return ZB_MAT_GRAVEL;
  }
  return ZB_MAT_SOIL;
}

zb_material zb_terrain_biome_material(int biome, double height, double slope, double jitter) {
  if (height < -25.0) {
    return ZB_MAT_MUD;
  }
  if (height < 2.0 + jitter) {
    return ZB_MAT_SAND;
  }
  if (slope > 0.78) {
    return ZB_MAT_ROCK;
  }
  switch ((zb_biome)biome) {
    case ZB_BIOME_SAND_DESERT:
    case ZB_BIOME_DUNES:
    case ZB_BIOME_BEACH:
    case ZB_BIOME_SALT_FLAT: return ZB_MAT_SAND;
    case ZB_BIOME_ROCKY_DESERT:
    case ZB_BIOME_SCREE:
    case ZB_BIOME_CLIFFS:
    case ZB_BIOME_VOLCANIC_SLOPES:
    case ZB_BIOME_ASHLANDS:
    case ZB_BIOME_POLAR_DESERT: return slope > 0.4 + 0.1 * jitter ? ZB_MAT_ROCK : ZB_MAT_GRAVEL;
    case ZB_BIOME_GLACIER:
    case ZB_BIOME_ICE_SHEET:
    case ZB_BIOME_SEA_ICE: return slope > 0.6 ? ZB_MAT_ROCK : ZB_MAT_ICE;
    case ZB_BIOME_MARSH:
    case ZB_BIOME_SWAMP_FOREST:
    case ZB_BIOME_PEAT_BOG:
    case ZB_BIOME_FLOODPLAIN:
    case ZB_BIOME_MANGROVE:
    case ZB_BIOME_SALT_MARSH:
    case ZB_BIOME_ESTUARY:
    case ZB_BIOME_MIREFEN: return ZB_MAT_MUD;
    case ZB_BIOME_ALPINE_MEADOW:
    case ZB_BIOME_TUNDRA:
      return height > 3600.0 + 120.0 * jitter ? ZB_MAT_SNOW : (slope > 0.58 ? ZB_MAT_GRAVEL : ZB_MAT_SOIL);
    default: return slope > 0.58 ? ZB_MAT_GRAVEL : ZB_MAT_SOIL;
  }
}

const char *zb_material_name(zb_material m) {
  static const char *const names[ZB_MAT_COUNT] = {"none", "soil", "sand", "rock", "gravel",
                                                  "snow", "ice",  "mud",  "ash"};
  return (unsigned)m < ZB_MAT_COUNT ? names[m] : "none";
}

void zb_chunk_terrain_init(zb_chunk_terrain *ct, zb_chunk c) {
  memset(ct, 0, sizeof *ct);
  ct->chunk = c;
  ct->stage = ZB_STAGE_NONE;
}

void zb_chunk_terrain_release(const zb_allocator *a, zb_chunk_terrain *ct) {
  zb_free(a, ct->height);
  zb_free(a, ct->material);
  zb_free(a, ct->plants);
  zb_chunk_terrain_init(ct, ct->chunk);
}

static void update_range(zb_chunk_terrain *ct, const float *v, int n);

static void coarse_range(zb_chunk_terrain *ct) {
  ct->min_height = ct->max_height = ct->macro[MACRO_RES + 1];
  for (int y = 0; y < ZB_COARSE_RES; ++y) {
    update_range(ct, &ct->macro[(y + 1) * MACRO_RES + 1], ZB_COARSE_RES);
  }
}

void zb_chunk_terrain_trim(const zb_allocator *a, zb_chunk_terrain *ct, int stage) {
  if (stage < ZB_STAGE_COARSE) {
    zb_chunk_terrain_release(a, ct);
    return;
  }
  if (stage < ZB_STAGE_ECOLOGY && ct->plants) {
    zb_free(a, ct->plants);
    ct->plants = NULL;
    ct->plant_count = 0;
  }
  if (stage < ZB_STAGE_SURFACE && ct->material) {
    zb_free(a, ct->material);
    ct->material = NULL;
  }
  if (stage < ZB_STAGE_TERRAIN && ct->height) {
    zb_free(a, ct->height);
    ct->height = NULL;
    coarse_range(ct);
  }
  if (ct->stage > stage) {
    ct->stage = stage;
  }
}

size_t zb_chunk_terrain_bytes(const zb_chunk_terrain *ct) {
  size_t n = sizeof *ct;
  if (ct->height) {
    n += sizeof(float) * ZB_HEIGHT_RES * ZB_HEIGHT_RES;
  }
  if (ct->material) {
    n += (size_t)ZB_SURFACE_RES * ZB_SURFACE_RES;
  }
  n += sizeof(zb_plant) * ct->plant_count;
  return n;
}

/* Tangents of the face coordinate of n samples along one chunk axis: the
 * first at `first` nominal metres from the chunk's edge (negative: outside),
 * then every `step` metres. */
static void axis_tangents(const zb_terrain *t, int32_t cell, double first, double step, int n, double *out) {
  for (int i = 0; i < n; ++i) {
    double u = -1.0 + ((double)cell + (first + i * step) / ZB_CHUNK_SIZE) * t->planet.chunk_uv;
    out[i] = zb_dtan(u * (ZB_DPI / 4.0));
  }
}

typedef struct face_frame {
  zb_dvec3 n, u, v;
} face_frame;

static face_frame frame_of(int face) {
  face_frame f;
  zb_planet_face_axes(face, &f.n, &f.u, &f.v);
  return f;
}

static zb_dvec3 sample_dir(const face_frame *f, double a, double b) {
  double x = f->n.x + a * f->u.x + b * f->v.x;
  double y = f->n.y + a * f->u.y + b * f->v.y;
  double z = f->n.z + a * f->u.z + b * f->v.z;
  double inv = 1.0 / sqrt(x * x + y * y + z * z);
  zb_dvec3 d = {x * inv, y * inv, z * inv};
  return d;
}

/* River pieces per tile of RIVER_TILE x RIVER_TILE samples, for one band of
 * rows at a time, so that a sample checks only the pieces that can reach its
 * tile. A piece within max_distance of a sample lies within max_distance plus
 * the tile's radius of the tile's centre (the triangle inequality; a tile's
 * samples lie in the spherical cap through its corners), so the nearest piece
 * within max_distance is the same as over the whole set. */
#define RIVER_TILE 16
#define RIVER_TILES_MAX (ZB_HEIGHT_RES / RIVER_TILE + 1)

typedef struct river_tiles {
  zb_river_set tile[RIVER_TILES_MAX];
} river_tiles;

/* Room for the pieces of every tile of a band, in bytes. */
static size_t river_tiles_size(const zb_river_set *rs) { return sizeof(zb_river_piece) * RIVER_TILES_MAX * rs->count; }

/* Fills the tiles of the band of rows y0.. for samples at tangents ua x ub
 * (n per edge); buf has river_tiles_size bytes. */
static void river_tiles_band(river_tiles *rt, const zb_terrain *t, const face_frame *f, const zb_river_set *rs,
                             zb_river_piece *buf, const double *ua, const double *ub, int n, int y0,
                             double max_distance) {
  int y1 = y0 + RIVER_TILE < n ? y0 + RIVER_TILE : n;
  for (int i = 0, x0 = 0; x0 < n; ++i, x0 += RIVER_TILE) {
    int x1 = x0 + RIVER_TILE < n ? x0 + RIVER_TILE : n;
    zb_dvec3 c = sample_dir(f, 0.5 * (ua[x0] + ua[x1 - 1]), 0.5 * (ub[y0] + ub[y1 - 1]));
    double reach = 0.0;
    const double ca[2] = {ua[x0], ua[x1 - 1]}, cb[2] = {ub[y0], ub[y1 - 1]};
    for (int k = 0; k < 4; ++k) {
      double d = zb_dv_len(zb_dv_sub(sample_dir(f, ca[k & 1], cb[k >> 1]), c)) * t->planet.radius;
      reach = d > reach ? d : reach;
    }
    zb_river_set *tile = &rt->tile[i];
    *tile = *rs;
    tile->piece = buf + (size_t)i * rs->count;
    tile->cap = rs->count;
    tile->count = zb_rivers_select(rs, t->planet.radius, c, max_distance + reach + 1.0, tile->piece);
  }
}

/* Material-border jitter: value noise on a lattice every JITTER_CELL metres of
 * the face grid (continuous across chunk borders within a face), so borders
 * wander smoothly instead of speckling. */
#define JITTER_CELL 32
#define JITTER_RES (ZB_SURFACE_RES / JITTER_CELL + 1)

static double lattice_value(const zb_terrain *t, int face, int64_t gx, int64_t gy) {
  uint64_t h = zb_mix64(stream(t, SEED_JITTER) ^ ((uint64_t)(uint32_t)face << 61) ^
                        ((uint64_t)gx * 0x9e3779b97f4a7c15ull) ^ ((uint64_t)gy * 0xc2b2ae3d27d4eb4full));
  return (double)(h >> 11) * (2.0 / 9007199254740992.0) - 1.0;
}

static void jitter_lattice(const zb_terrain *t, zb_chunk c, double out[JITTER_RES * JITTER_RES]) {
  int64_t bx = (int64_t)c.x * (ZB_SURFACE_RES / JITTER_CELL), by = (int64_t)c.y * (ZB_SURFACE_RES / JITTER_CELL);
  for (int y = 0; y < JITTER_RES; ++y) {
    for (int x = 0; x < JITTER_RES; ++x) {
      out[y * JITTER_RES + x] = lattice_value(t, c.face, bx + x, by + y);
    }
  }
}

/* Jitter at a cell (x, y in 0..255 of a chunk), smoothly interpolated. */
static double jitter_at(const double *lat, double x, double y) {
  double fx = x / JITTER_CELL, fy = y / JITTER_CELL;
  int ix = (int)fx, iy = (int)fy;
  ix = ix >= JITTER_RES - 1 ? JITTER_RES - 2 : ix;
  iy = iy >= JITTER_RES - 1 ? JITTER_RES - 2 : iy;
  double tx = fx - ix, ty = fy - iy;
  tx = tx * tx * (3.0 - 2.0 * tx);
  ty = ty * ty * (3.0 - 2.0 * ty);
  const double *r0 = &lat[iy * JITTER_RES + ix], *r1 = r0 + JITTER_RES;
  double a = r0[0] + (r0[1] - r0[0]) * tx, b = r1[0] + (r1[1] - r1[0]) * tx;
  return a + (b - a) * ty;
}

static void update_range(zb_chunk_terrain *ct, const float *v, int n) {
  for (int i = 0; i < n; ++i) {
    if (v[i] < ct->min_height) {
      ct->min_height = v[i];
    }
    if (v[i] > ct->max_height) {
      ct->max_height = v[i];
    }
  }
}

/* Rivers near a chunk and its one-sample border. */
static zb_result chunk_rivers(const zb_terrain *t, const zb_allocator *a, zb_chunk c, zb_river_set *rs) {
  double w, h;
  zb_planet_chunk_size(&t->planet, c, &w, &h);
  zb_result r =
      gather_rivers(t, zb_planet_chunk_dir(&t->planet, c), 0.5 * sqrt(w * w + h * h) + 2.0 * COARSE_STEP, rs, a);
  if (r != ZB_OK) {
    zb_river_set_free(rs);
  }
  return r;
}

/* Only the rivers whose banks reach into the chunk (margin: extra metres). */
static void chunk_rivers_near(const zb_terrain *t, zb_chunk c, zb_river_set *rs, double margin) {
  double w, h;
  zb_planet_chunk_size(&t->planet, c, &w, &h);
  zb_rivers_keep_near(rs, t->planet.radius, zb_planet_chunk_dir(&t->planet, c), 0.5 * sqrt(w * w + h * h), margin);
}

/* The created world's biome at every 16 m sample of a chunk (zeros for the noise world). */
static void coarse_biomes(const zb_terrain *t, zb_chunk c, uint8_t out[ZB_COARSE_RES * ZB_COARSE_RES]) {
  if (!t->bundle) {
    memset(out, 0, ZB_COARSE_RES * ZB_COARSE_RES);
    return;
  }
  face_frame f = frame_of(c.face);
  double ta[ZB_COARSE_RES], tb[ZB_COARSE_RES];
  axis_tangents(t, c.x, 0.0, COARSE_STEP, ZB_COARSE_RES, ta);
  axis_tangents(t, c.y, 0.0, COARSE_STEP, ZB_COARSE_RES, tb);
  for (int y = 0; y < ZB_COARSE_RES; ++y) {
    for (int x = 0; x < ZB_COARSE_RES; ++x) {
      out[y * ZB_COARSE_RES + x] = (uint8_t)zb_world_bundle_biome(t->bundle, sample_dir(&f, ta[x], tb[y]));
    }
  }
}

/* ---- Plants ------------------------------------------------------------------ */

#define LAPSE 0.0065 /* deg C per metre */

/* The biome of a place: the created world's, or for the noise world from its
 * climate. */
static int site_biome(const zb_terrain *t, int bundle_biome, double temperature, double precipitation, double warmest,
                      int material) {
  if (t->bundle) {
    return bundle_biome;
  }
  return (int)zb_biome_classify(temperature, precipitation, warmest, 0.35, material == ZB_MAT_ROCK ? 1.0 : 0.0);
}

/* Stage 0: how much of each 32 m of the chunk is under trees, and which far-
 * forest impostor suits it best. */
static void stage0_canopy(const zb_terrain *t, zb_chunk_terrain *ct, const uint8_t *biomes, double cell_w,
                          double cell_h) {
  memset(ct->canopy, 0, sizeof ct->canopy);
  ct->clump = 0;
  const zb_flora *f = t->flora;
  if (!f) {
    return;
  }
  double mean, coldest, warmest, rain;
  zb_terrain_chunk_climate(t, ct->chunk, &mean, &coldest, &warmest, &rain);
  double best[ZB_FLORA_MAX];
  memset(best, 0, sizeof best);
  for (int j = 0; j < ZB_CANOPY_RES; ++j) {
    for (int i = 0; i < ZB_CANOPY_RES; ++i) {
      int x = 2 * i, y = 2 * j;
      const float *m = &ct->macro[(y + 1) * MACRO_RES + x + 1];
      double h = (double)m[0];
      if (h < 0.0) {
        continue;
      }
      double gx = ((double)m[1] - (double)m[-1]) / (2.0 * cell_w),
             gy = ((double)m[MACRO_RES] - (double)m[-MACRO_RES]) / (2.0 * cell_h);
      double slope = sqrt(gx * gx + gy * gy), lapse = LAPSE * h;
      int material = ct->coarse_material[y * ZB_COARSE_RES + x];
      int biome = site_biome(t, biomes[y * ZB_COARSE_RES + x], mean - lapse, rain, warmest - lapse, material);
      bool wet = zb_biome_is_wet(biome);
      double cover = zb_biome_cover(biome, ZB_PLANT_TREE), vigour = 0.0;
      for (int s = 0; s < f->count; ++s) {
        const zb_species *sp = &f->species[s];
        if (sp->layer != ZB_PLANT_TREE || !zb_species_ground_ok(sp, material, slope, wet)) {
          continue;
        }
        double v = zb_species_climate(sp, mean - lapse, coldest - lapse, rain) * (double)sp->density;
        vigour += v;
        best[s] += v * cover;
      }
      double c = cover * (vigour < 1.0 ? vigour : 1.0);
      ct->canopy[j * ZB_CANOPY_RES + i] = (uint8_t)(255.0 * (c < 1.0 ? c : 1.0) + 0.5);
    }
  }
  int top = -1;
  for (int s = 0; s < f->count; ++s) {
    if (f->species[s].clump >= 0 && best[s] > 0.0 && (top < 0 || best[s] > best[top])) {
      top = s;
    }
  }
  ct->clump = (uint8_t)(top >= 0 ? top + 1 : 0);
}

static zb_result generate_coarse(const zb_terrain *t, const zb_allocator *a, zb_chunk_terrain *ct) {
  face_frame f = frame_of(ct->chunk.face);
  double ta[MACRO_RES], tb[MACRO_RES];
  axis_tangents(t, ct->chunk.x, -COARSE_STEP, COARSE_STEP, MACRO_RES, ta);
  axis_tangents(t, ct->chunk.y, -COARSE_STEP, COARSE_STEP, MACRO_RES, tb);
  zb_river_set rs;
  if (chunk_rivers(t, a, ct->chunk, &rs) != ZB_OK) {
    return ZB_ERR_NOMEM;
  }
  for (int y = 0; y < MACRO_RES; ++y) {
    for (int x = 0; x < MACRO_RES; ++x) {
      zb_dvec3 d = sample_dir(&f, ta[x], tb[y]);
      ct->macro[y * MACRO_RES + x] = (float)(t->bundle ? bundle_height(t, d, &rs) : zb_terrain_macro_height(t, d));
    }
  }
  zb_river_set_free(&rs);
  /* Coarse materials from the macro grid (slope over 16 m). */
  double w, h;
  zb_planet_chunk_size(&t->planet, ct->chunk, &w, &h);
  double sx = 2.0 * COARSE_STEP * w / ZB_CHUNK_SIZE, sy = 2.0 * COARSE_STEP * h / ZB_CHUNK_SIZE;
  double sin_lat = zb_planet_chunk_dir(&t->planet, ct->chunk).z;
  coarse_range(ct);
  double jitter[JITTER_RES * JITTER_RES];
  jitter_lattice(t, ct->chunk, jitter);
  uint8_t biomes[ZB_COARSE_RES * ZB_COARSE_RES];
  coarse_biomes(t, ct->chunk, biomes);
  for (int y = 0; y < ZB_COARSE_RES; ++y) {
    const float *row = &ct->macro[(y + 1) * MACRO_RES + 1];
    for (int x = 0; x < ZB_COARSE_RES; ++x) {
      double gx = (row[x + 1] - row[x - 1]) / sx;
      double gy = (row[x + MACRO_RES] - row[x - MACRO_RES]) / sy;
      double slope = sqrt(gx * gx + gy * gy);
      double jit = jitter_at(jitter, x * 16.0, y * 16.0);
      if (t->bundle) {
        ct->coarse_material[y * ZB_COARSE_RES + x] =
            (uint8_t)zb_terrain_biome_material(biomes[y * ZB_COARSE_RES + x], row[x], slope, jit);
      } else {
        ct->coarse_material[y * ZB_COARSE_RES + x] = (uint8_t)zb_terrain_material_rule(row[x], slope, sin_lat, jit);
      }
    }
  }
  stage0_canopy(t, ct, biomes, 0.5 * sx, 0.5 * sy);
  ct->stage = ZB_STAGE_COARSE;
  return ZB_OK;
}

/* Catmull-Rom weights for t in [0, 1). */
static void catmull_rom(double t, double w[4]) {
  double t2 = t * t, t3 = t2 * t;
  w[0] = 0.5 * (-t3 + 2.0 * t2 - t);
  w[1] = 0.5 * (3.0 * t3 - 5.0 * t2 + 2.0);
  w[2] = 0.5 * (-3.0 * t3 + 4.0 * t2 + t);
  w[3] = 0.5 * (t3 - t2);
}

static zb_result generate_terrain(const zb_terrain *t, const zb_allocator *a, zb_chunk_terrain *ct) {
  float *height = zb_alloc(a, sizeof(float) * ZB_HEIGHT_RES * ZB_HEIGHT_RES);
  /* Row-interpolated macro grid (heap: worker threads have small stacks). */
  double(*rows)[ZB_HEIGHT_RES] = zb_alloc(a, sizeof(double) * MACRO_RES * ZB_HEIGHT_RES);
  double *detail = zb_alloc(a, sizeof(double) * DETAIL_RES * DETAIL_RES);
  if (!height || !rows || !detail) {
    zb_free(a, height);
    zb_free(a, rows);
    zb_free(a, detail);
    return ZB_ERR_NOMEM;
  }
  /* Macro shape: separable Catmull-Rom from the 16 m grid. Rows first. */
  static const int R = ZB_HEIGHT_RES;
  double weights[COARSE_STEP][4];
  for (int k = 0; k < COARSE_STEP; ++k) {
    catmull_rom((double)k / COARSE_STEP, weights[k]);
  }
  for (int my = 0; my < MACRO_RES; ++my) {
    const float *src = &ct->macro[my * MACRO_RES];
    for (int x = 0; x < R; ++x) {
      int cell = x / COARSE_STEP, k = x % COARSE_STEP;
      if (cell == ZB_COARSE_RES - 1) { /* x == 256: the last sample */
        cell -= 1;
        k = COARSE_STEP;
      }
      if (k == COARSE_STEP) {
        rows[my][x] = src[cell + 2];
      } else {
        const double *w = weights[k];
        rows[my][x] = w[0] * src[cell] + w[1] * src[cell + 1] + w[2] * src[cell + 2] + w[3] * src[cell + 3];
      }
    }
  }
  /* Detail noise on a 2 m grid, bilinearly refined to 1 m: its finest octave
   * has a 10 m wavelength, so the refinement error stays below 5 cm. */
  face_frame f = frame_of(ct->chunk.face);
  double ta[DETAIL_RES], tb[DETAIL_RES];
  axis_tangents(t, ct->chunk.x, 0.0, 2.0, DETAIL_RES, ta);
  axis_tangents(t, ct->chunk.y, 0.0, 2.0, DETAIL_RES, tb);
  double radius = t->planet.radius;
  for (int y = 0; y < DETAIL_RES; ++y) {
    for (int x = 0; x < DETAIL_RES; ++x) {
      zb_dvec3 d = sample_dir(&f, ta[x], tb[y]);
      detail[y * DETAIL_RES + x] = detail_height(t, d.x * radius, d.y * radius, d.z * radius);
    }
  }
  for (int y = 0; y < R; ++y) {
    int cell = y / COARSE_STEP, k = y % COARSE_STEP;
    if (cell == ZB_COARSE_RES - 1) {
      cell -= 1;
      k = COARSE_STEP;
    }
    const double *w = k < COARSE_STEP ? weights[k] : NULL;
    const double *d0 = &detail[(y / 2) * DETAIL_RES];
    const double *d1 = (y & 1) ? d0 + DETAIL_RES : d0;
    for (int x = 0; x < R; ++x) {
      double macro =
          w ? w[0] * rows[cell][x] + w[1] * rows[cell + 1][x] + w[2] * rows[cell + 2][x] + w[3] * rows[cell + 3][x]
            : rows[cell + 2][x];
      int dx0 = x / 2, dx1 = dx0 + (x & 1);
      double fine = 0.25 * (d0[dx0] + d0[dx1] + d1[dx0] + d1[dx1]);
      height[y * R + x] = (float)(macro + fine);
    }
  }
  zb_free(a, rows);
  zb_free(a, detail);
  /* River channels and banks, at full resolution. */
  zb_river_set rs;
  if (chunk_rivers(t, a, ct->chunk, &rs) != ZB_OK) {
    zb_free(a, height);
    return ZB_ERR_NOMEM;
  }
  chunk_rivers_near(t, ct->chunk, &rs, 1.0);
  double *ua = rs.count ? zb_alloc(a, sizeof(double) * 2 * ZB_HEIGHT_RES + river_tiles_size(&rs)) : NULL;
  if (rs.count && !ua) {
    zb_river_set_free(&rs);
    zb_free(a, height);
    return ZB_ERR_NOMEM;
  }
  if (ua) {
    double reach = max_reach(&rs), *ub = ua + ZB_HEIGHT_RES;
    river_tiles rt;
    axis_tangents(t, ct->chunk.x, 0.0, 1.0, R, ua);
    axis_tangents(t, ct->chunk.y, 0.0, 1.0, R, ub);
    for (int y = 0; y < R; ++y) {
      if (y % RIVER_TILE == 0) {
        river_tiles_band(&rt, t, &f, &rs, (zb_river_piece *)(ub + ZB_HEIGHT_RES), ua, ub, R, y, reach);
      }
      for (int x = 0; x < R; ++x) {
        const zb_river_set *tile = &rt.tile[x / RIVER_TILE];
        if (tile->count) {
          float *hp = &height[y * R + x];
          *hp = (float)carve(t, tile, reach, sample_dir(&f, ua[x], ub[y]), (double)*hp);
        }
      }
    }
    zb_free(a, ua);
  }
  zb_river_set_free(&rs);
  ct->height = height;
  ct->min_height = ct->max_height = height[0];
  update_range(ct, height, R * R);
  ct->stage = ZB_STAGE_TERRAIN;
  return ZB_OK;
}

/* Water-shaped materials of a created world: river beds (gravel where the
 * current is fast, mud where it is slow), sandy banks, and lake beds and
 * beaches. Returns ZB_MAT_NONE where the land's own rule applies. */
static zb_material water_material(const zb_terrain *t, const zb_river_set *rs, double reach, zb_dvec3 d, double height,
                                  double lake) {
  zb_river_hit hit;
  if (rs->count && zb_rivers_nearest(rs, t->planet.radius, d, reach, &hit) && !(height < 0.0 && hit.surface < 1.0)) {
    if (hit.distance < hit.half_width) {
      return hit.speed > 1.2 ? ZB_MAT_GRAVEL : ZB_MAT_MUD;
    }
    if (height < hit.surface + 1.5) {
      return ZB_MAT_SAND;
    }
  }
  if (height < lake - 1.0) {
    return ZB_MAT_MUD;
  }
  if (height < lake + 1.0) {
    return ZB_MAT_SAND;
  }
  return ZB_MAT_NONE;
}

static zb_result generate_surface(const zb_terrain *t, const zb_allocator *a, zb_chunk_terrain *ct) {
  uint8_t *mat = zb_alloc(a, (size_t)ZB_SURFACE_RES * ZB_SURFACE_RES);
  if (!mat) {
    return ZB_ERR_NOMEM;
  }
  zb_river_set rs;
  if (chunk_rivers(t, a, ct->chunk, &rs) != ZB_OK) {
    zb_free(a, mat);
    return ZB_ERR_NOMEM;
  }
  chunk_rivers_near(t, ct->chunk, &rs, 1.0);
  double reach = max_reach(&rs);
  double jitter[JITTER_RES * JITTER_RES];
  jitter_lattice(t, ct->chunk, jitter);
  uint8_t biomes[ZB_COARSE_RES * ZB_COARSE_RES];
  coarse_biomes(t, ct->chunk, biomes);
  face_frame f = frame_of(ct->chunk.face);
  /* Lake levels at the 16 m samples (no lake: far below the ground). */
  float lakes[ZB_COARSE_RES * ZB_COARSE_RES];
  double ca[ZB_COARSE_RES], cb[ZB_COARSE_RES];
  axis_tangents(t, ct->chunk.x, 0.0, COARSE_STEP, ZB_COARSE_RES, ca);
  axis_tangents(t, ct->chunk.y, 0.0, COARSE_STEP, ZB_COARSE_RES, cb);
  for (int y = 0; y < ZB_COARSE_RES; ++y) {
    for (int x = 0; x < ZB_COARSE_RES; ++x) {
      double level = -1e9;
      if (t->bundle && !zb_lake_level(t->bundle, sample_dir(&f, ca[x], cb[y]), &level)) {
        level = -1e9;
      }
      lakes[y * ZB_COARSE_RES + x] = (float)level;
    }
  }
  double *fa = rs.count ? zb_alloc(a, sizeof(double) * 2 * ZB_SURFACE_RES + river_tiles_size(&rs)) : NULL;
  double *fb = fa ? fa + ZB_SURFACE_RES : NULL;
  if (rs.count && !fa) {
    zb_river_set_free(&rs);
    zb_free(a, mat);
    return ZB_ERR_NOMEM;
  }
  if (fa) {
    axis_tangents(t, ct->chunk.x, 0.5, 1.0, ZB_SURFACE_RES, fa);
    axis_tangents(t, ct->chunk.y, 0.5, 1.0, ZB_SURFACE_RES, fb);
  }
  double w, h;
  zb_planet_chunk_size(&t->planet, ct->chunk, &w, &h);
  double cw = w / ZB_SURFACE_RES, ch = h / ZB_SURFACE_RES;
  double sin_lat = zb_planet_chunk_dir(&t->planet, ct->chunk).z;
  const float *hf = ct->height;
  river_tiles rt;
  for (int y = 0; y < ZB_SURFACE_RES; ++y) {
    const float *r0 = &hf[y * ZB_HEIGHT_RES], *r1 = r0 + ZB_HEIGHT_RES;
    if (fa && y % RIVER_TILE == 0) {
      river_tiles_band(&rt, t, &f, &rs, (zb_river_piece *)(fb + ZB_SURFACE_RES), fa, fb, ZB_SURFACE_RES, y, reach);
    }
    for (int x = 0; x < ZB_SURFACE_RES; ++x) {
      double gx = ((double)r0[x + 1] + r1[x + 1] - r0[x] - r1[x]) / (2.0 * cw);
      double gy = ((double)r1[x] + r1[x + 1] - r0[x] - r0[x + 1]) / (2.0 * ch);
      double avg = ((double)r0[x] + r0[x + 1] + r1[x] + r1[x + 1]) * 0.25;
      double jit = jitter_at(jitter, x + 0.5, y + 0.5), slope = sqrt(gx * gx + gy * gy);
      if (t->bundle) {
        /* The nearest coarse sample's biome, its border dithered by the jitter. */
        int bx = (int)((x + 8.0 + 6.0 * jit) / 16.0), by = (int)((y + 8.0 - 6.0 * jit) / 16.0);
        bx = bx < 0 ? 0 : (bx >= ZB_COARSE_RES ? ZB_COARSE_RES - 1 : bx);
        by = by < 0 ? 0 : (by >= ZB_COARSE_RES ? ZB_COARSE_RES - 1 : by);
        zb_dvec3 d = fa ? sample_dir(&f, fa[x], fb[y]) : f.n;
        zb_material m = water_material(t, fa ? &rt.tile[x / RIVER_TILE] : &rs, reach, d, avg,
                                       (double)lakes[by * ZB_COARSE_RES + bx]);
        mat[y * ZB_SURFACE_RES + x] =
            (uint8_t)(m != ZB_MAT_NONE ? m
                                       : zb_terrain_biome_material(biomes[by * ZB_COARSE_RES + bx], avg, slope, jit));
      } else {
        mat[y * ZB_SURFACE_RES + x] = (uint8_t)zb_terrain_material_rule(avg, slope, sin_lat, jit);
      }
    }
  }
  zb_free(a, fa);
  zb_river_set_free(&rs);
  ct->material = mat;
  ct->stage = ZB_STAGE_SURFACE;
  return ZB_OK;
}

/* Stage 3: plants on jittered grids, one per layer (trees about every 6 m,
 * shrubs 4 m, ground cover 2.5 m), each kept with a probability from the
 * biome's cover and how well the species there suit the climate and ground,
 * and the species drawn by suitability. Nothing grows under water. */
static const int LAYER_CELLS[ZB_PLANT_LAYERS] = {42, 64, 100};
static const double LAYER_FILL[ZB_PLANT_LAYERS] = {0.6, 0.25, 0.35};

static double unit(uint64_t h, int k) { return (double)((h >> (k * 16)) & 0xffff) / 65536.0; }

static zb_result generate_ecology(const zb_terrain *t, const zb_allocator *a, zb_chunk_terrain *ct) {
  const zb_flora *f = t->flora;
  if (!f || f->count == 0) {
    ct->stage = ZB_STAGE_ECOLOGY;
    return ZB_OK;
  }
  enum { SAMPLES = ZB_COARSE_RES * ZB_COARSE_RES, WATER = 33 };
  float *weight = zb_alloc(a, sizeof(float) * SAMPLES * (size_t)f->count);
  zb_plant *out = zb_alloc(a, sizeof(zb_plant) * ZB_PLANTS_MAX);
  uint8_t *wkind = zb_alloc(a, WATER * WATER);
  float *wsurf = zb_alloc(a, sizeof(float) * WATER * WATER);
  zb_result r = weight && out && wkind && wsurf ? zb_terrain_water_grid(t, a, ct, 8, wkind, wsurf) : ZB_ERR_NOMEM;
  if (r != ZB_OK) {
    zb_free(a, weight);
    zb_free(a, out);
    zb_free(a, wkind);
    zb_free(a, wsurf);
    return r;
  }
  /* The climate's part of every species' suitability at the 16 m samples. */
  double mean, coldest, warmest, rain;
  zb_terrain_chunk_climate(t, ct->chunk, &mean, &coldest, &warmest, &rain);
  uint8_t biomes[SAMPLES], sample_biome[SAMPLES];
  coarse_biomes(t, ct->chunk, biomes);
  for (int k = 0; k < SAMPLES; ++k) {
    double h = (double)zb_chunk_coarse_height(ct, k % ZB_COARSE_RES, k / ZB_COARSE_RES);
    double lapse = LAPSE * (h > 0.0 ? h : 0.0);
    sample_biome[k] = (uint8_t)site_biome(t, biomes[k], mean - lapse, rain, warmest - lapse, ct->coarse_material[k]);
    for (int s = 0; s < f->count; ++s) {
      const zb_species *sp = &f->species[s];
      weight[k * f->count + s] =
          (float)(zb_species_climate(sp, mean - lapse, coldest - lapse, rain) * (double)sp->density);
    }
  }
  bool oasis =
      t->bundle && (t->bundle->cell_flags[zb_world_bundle_cell(t->bundle, zb_planet_chunk_dir(&t->planet, ct->chunk))] &
                    ZB_CELL_OASIS);
  double w, h;
  zb_planet_chunk_size(&t->planet, ct->chunk, &w, &h);
  double mx = w / ZB_CHUNK_SIZE, my = h / ZB_CHUNK_SIZE; /* metres per nominal metre */
  uint64_t base = zb_mix64(stream(t, SEED_PLANTS) ^ zb_chunk_key(ct->chunk));
  uint32_t n = 0;
  float picks[ZB_FLORA_MAX];
  for (int layer = 0; layer < ZB_PLANT_LAYERS; ++layer) {
    int cells = LAYER_CELLS[layer];
    double size = ZB_CHUNK_SIZE / cells;
    for (int j = 0; j < cells; ++j) {
      for (int i = 0; i < cells; ++i) {
        uint64_t r1 = zb_mix64(base ^ ((uint64_t)(uint32_t)layer << 56) ^ ((uint64_t)(uint32_t)j << 28) ^
                               ((uint64_t)(uint32_t)i * 0x9e3779b97f4a7c15ull));
        uint64_t r2 = zb_mix64(r1);
        double x = (i + unit(r1, 0)) * size, y = (j + unit(r1, 1)) * size;
        int k = (int)(y / COARSE_STEP + 0.5) * ZB_COARSE_RES + (int)(x / COARSE_STEP + 0.5);
        double cover = zb_biome_cover(sample_biome[k], (zb_plant_layer)layer);
        if (cover <= 0.0) {
          continue;
        }
        int px = (int)x, py = (int)y;
        px = px > ZB_SURFACE_RES - 1 ? ZB_SURFACE_RES - 1 : px;
        py = py > ZB_SURFACE_RES - 1 ? ZB_SURFACE_RES - 1 : py;
        const float *hp = &ct->height[py * ZB_HEIGHT_RES + px];
        double ground = (double)*hp;
        double gx = ((double)hp[1] - ground) / mx, gy = ((double)hp[ZB_HEIGHT_RES] - ground) / my;
        double slope = sqrt(gx * gx + gy * gy);
        int vx = (int)(x / 8.0 + 0.5), vy = (int)(y / 8.0 + 0.5);
        int wv = vy * WATER + vx;
        if (ground < 0.0 || (wkind[wv] != ZB_WATER_NONE && ground < (double)wsurf[wv] + 0.2)) {
          continue; /* under water */
        }
        bool wet = oasis || zb_biome_is_wet(sample_biome[k]);
        for (int dy = -1; dy <= 1 && !wet; ++dy) {
          for (int dx = -1; dx <= 1 && !wet; ++dx) {
            int ux = vx + dx, uy = vy + dy;
            wet = ux >= 0 && uy >= 0 && ux < WATER && uy < WATER &&
                  (wkind[uy * WATER + ux] == ZB_WATER_RIVER || wkind[uy * WATER + ux] == ZB_WATER_LAKE);
          }
        }
        int material = ct->material[py * ZB_SURFACE_RES + px];
        double total = 0.0;
        for (int s = 0; s < f->count; ++s) {
          const zb_species *sp = &f->species[s];
          bool ok = (int)sp->layer == layer && zb_species_ground_ok(sp, material, slope, wet);
          picks[s] = ok ? weight[k * f->count + s] : 0.0f;
          total += (double)picks[s];
        }
        double p = total * LAYER_FILL[layer] * cover;
        if (unit(r1, 2) >= (p < 0.95 ? p : 0.95) || n == ZB_PLANTS_MAX) {
          continue;
        }
        double target = unit(r1, 3) * total;
        int chosen = -1;
        for (int s = 0; s < f->count && chosen < 0; ++s) {
          target -= (double)picks[s];
          chosen = picks[s] > 0.0f && target < 0.0 ? s : -1;
        }
        if (chosen < 0) { /* rounding at the very end of the range */
          for (int s = f->count - 1; s >= 0 && chosen < 0; --s) {
            chosen = picks[s] > 0.0f ? s : -1;
          }
        }
        zb_plant *pl = &out[n++];
        pl->x = (uint16_t)(x * 256.0 > 65535.0 ? 65535.0 : x * 256.0);
        pl->y = (uint16_t)(y * 256.0 > 65535.0 ? 65535.0 : y * 256.0);
        pl->species = (uint8_t)chosen;
        pl->size = (uint8_t)(unit(r2, 0) * 256.0);
        pl->variant = (uint8_t)(r2 >> 56);
        pl->layer = (uint8_t)layer;
      }
    }
  }
  zb_free(a, weight);
  zb_free(a, wkind);
  zb_free(a, wsurf);
  if (n == 0) {
    zb_free(a, out);
    out = NULL;
  } else {
    zb_plant *fit = zb_realloc(a, out, sizeof(zb_plant) * n);
    out = fit ? fit : out;
  }
  ct->plants = out;
  ct->plant_count = n;
  ct->stage = ZB_STAGE_ECOLOGY;
  return ZB_OK;
}

zb_result zb_terrain_generate(const zb_terrain *t, const zb_allocator *a, zb_chunk_terrain *ct, int stage) {
  if (stage > ZB_STAGE_ECOLOGY || !zb_chunk_valid(&t->planet, ct->chunk)) {
    return ZB_ERR_INVALID;
  }
  if (ct->stage < ZB_STAGE_COARSE && stage >= ZB_STAGE_COARSE) {
    zb_result r = generate_coarse(t, a, ct);
    if (r != ZB_OK) {
      return r;
    }
  }
  if (ct->stage < ZB_STAGE_TERRAIN && stage >= ZB_STAGE_TERRAIN) {
    zb_result r = generate_terrain(t, a, ct);
    if (r != ZB_OK) {
      return r;
    }
  }
  if (ct->stage < ZB_STAGE_SURFACE && stage >= ZB_STAGE_SURFACE) {
    zb_result r = generate_surface(t, a, ct);
    if (r != ZB_OK) {
      return r;
    }
  }
  if (ct->stage < ZB_STAGE_ECOLOGY && stage >= ZB_STAGE_ECOLOGY) {
    return generate_ecology(t, a, ct);
  }
  return ZB_OK;
}

float zb_chunk_coarse_height(const zb_chunk_terrain *ct, int x, int y) {
  return ct->macro[(y + 1) * MACRO_RES + x + 1];
}

float zb_chunk_height_at(const zb_chunk_terrain *ct, float lx, float ly) {
  bool fine = ct->height != NULL;
  float scale = fine ? 1.0f : 1.0f / COARSE_STEP;
  int last = fine ? ZB_HEIGHT_RES - 1 : ZB_COARSE_RES - 1;
  float fx = zb_clampf(lx * scale, 0.0f, (float)last), fy = zb_clampf(ly * scale, 0.0f, (float)last);
  int x0 = (int)fx, y0 = (int)fy;
  x0 = x0 >= last ? last - 1 : x0;
  y0 = y0 >= last ? last - 1 : y0;
  float tx = fx - (float)x0, ty = fy - (float)y0;
  float h00, h10, h01, h11;
  if (fine) {
    const float *r = &ct->height[y0 * ZB_HEIGHT_RES + x0];
    h00 = r[0];
    h10 = r[1];
    h01 = r[ZB_HEIGHT_RES];
    h11 = r[ZB_HEIGHT_RES + 1];
  } else {
    h00 = zb_chunk_coarse_height(ct, x0, y0);
    h10 = zb_chunk_coarse_height(ct, x0 + 1, y0);
    h01 = zb_chunk_coarse_height(ct, x0, y0 + 1);
    h11 = zb_chunk_coarse_height(ct, x0 + 1, y0 + 1);
  }
  return zb_lerpf(zb_lerpf(h00, h10, tx), zb_lerpf(h01, h11, tx), ty);
}

uint64_t zb_chunk_terrain_hash(const zb_chunk_terrain *ct) {
  zb_hasher h;
  zb_hasher_init(&h, zb_chunk_key(ct->chunk));
  zb_hasher_u64(&h, (uint64_t)(int64_t)ct->stage);
  if (ct->stage >= ZB_STAGE_COARSE) {
    zb_hasher_bytes(&h, ct->macro, sizeof ct->macro);
    zb_hasher_bytes(&h, ct->coarse_material, sizeof ct->coarse_material);
    zb_hasher_bytes(&h, ct->canopy, sizeof ct->canopy);
    zb_hasher_u64(&h, ct->clump);
  }
  if (ct->height) {
    zb_hasher_bytes(&h, ct->height, sizeof(float) * ZB_HEIGHT_RES * ZB_HEIGHT_RES);
  }
  if (ct->material) {
    zb_hasher_bytes(&h, ct->material, (size_t)ZB_SURFACE_RES * ZB_SURFACE_RES);
  }
  for (uint32_t i = 0; i < ct->plant_count; ++i) {
    const zb_plant *pl = &ct->plants[i];
    uint64_t v = (uint64_t)pl->x | (uint64_t)pl->y << 16 | (uint64_t)pl->species << 32 | (uint64_t)pl->size << 40 |
                 (uint64_t)pl->variant << 48 | (uint64_t)pl->layer << 56;
    zb_hasher_u64(&h, v);
  }
  return zb_hasher_final(&h);
}

/* ---- Water ------------------------------------------------------------------ */

/* Lakes first (a river crossing a lake is part of it), then rivers, then the
 * sea. widen: how far beyond its banks a river still counts; raise: lift
 * rivers above ground that is not carved (coarse heights). */
static void classify_water(const zb_terrain *t, const zb_river_set *rs, zb_dvec3 d, double ground, double widen,
                           bool raise, zb_water_sample *out) {
  memset(out, 0, sizeof *out);
  double level;
  if (t->bundle && zb_lake_level(t->bundle, d, &level) && ground < level) {
    out->kind = ZB_WATER_LAKE;
    out->surface = level;
    out->depth = level - ground;
    return;
  }
  zb_river_hit hit;
  /* A river's mouth: once it reaches the sea below sea level, it is the sea. */
  bool sea = ground < 0.0;
  if (rs->count && zb_rivers_nearest(rs, t->planet.radius, d, max_reach(rs) + widen, &hit) &&
      hit.distance < hit.half_width + widen && (raise || ground < hit.surface) && !(sea && hit.surface < 1.0)) {
    out->kind = ZB_WATER_RIVER;
    out->surface = raise && ground + 0.5 > hit.surface ? ground + 0.5 : hit.surface;
    out->depth = out->surface - ground;
    /* A parabolic profile, fastest mid-stream and still at the banks, and a
     * pull towards the middle that carries floating things round bends. */
    double u = hit.distance / hit.half_width;
    u = u < 1.0 ? u : 1.0;
    out->flow = zb_dv_add(zb_dv_scale(hit.along, 1.5 * hit.speed * (1.0 - u * u)),
                          zb_dv_scale(hit.to_centre, 0.5 * hit.speed * u));
    return;
  }
  if (sea) {
    out->kind = ZB_WATER_OCEAN;
    out->depth = -ground;
  }
}

void zb_terrain_water(const zb_terrain *t, zb_dvec3 dir, zb_water_sample *out) {
  zb_river_set rs;
  (void)gather_rivers(t, dir, 0.0, &rs, zb_heap_allocator());
  double ground = t->bundle ? height_with(t, dir, &rs) : zb_terrain_height(t, dir);
  classify_water(t, &rs, dir, ground, 0.0, false, out);
  zb_river_set_free(&rs);
}

zb_result zb_terrain_water_grid(const zb_terrain *t, const zb_allocator *a, const zb_chunk_terrain *ct, int step,
                                uint8_t *kind, float *surface) {
  if (step < 1 || step > 64 || (step & (step - 1)) != 0 || ct->stage < ZB_STAGE_COARSE) {
    return ZB_ERR_INVALID;
  }
  int n = (int)ZB_CHUNK_SIZE / step + 1;
  zb_river_set rs;
  if (chunk_rivers(t, a, ct->chunk, &rs) != ZB_OK) {
    return ZB_ERR_NOMEM;
  }
  chunk_rivers_near(t, ct->chunk, &rs, 0.5 * step + 1.0);
  double *ua = zb_alloc(a, sizeof(double) * 2 * (size_t)n);
  if (!ua) {
    zb_river_set_free(&rs);
    return ZB_ERR_NOMEM;
  }
  double *ub = ua + n;
  axis_tangents(t, ct->chunk.x, 0.0, (double)step, n, ua);
  axis_tangents(t, ct->chunk.y, 0.0, (double)step, n, ub);
  face_frame f = frame_of(ct->chunk.face);
  bool raise = ct->height == NULL;
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      double ground = (double)zb_chunk_height_at(ct, (float)(i * step), (float)(j * step));
      zb_water_sample w;
      classify_water(t, &rs, sample_dir(&f, ua[i], ub[j]), ground, 0.5 * step, raise, &w);
      kind[j * n + i] = (uint8_t)w.kind;
      surface[j * n + i] = (float)w.surface;
    }
  }
  zb_free(a, ua);
  zb_river_set_free(&rs);
  return ZB_OK;
}

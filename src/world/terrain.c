/* SPDX-License-Identifier: MIT */
#include "world/terrain.h"

#include <math.h>
#include <string.h>

#include "core/dmath.h"
#include "core/hash.h"
#include "world/noise.h"

#define MACRO_RES (ZB_COARSE_RES + 2)
#define COARSE_STEP 16 /* stage 1 samples per stage 0 sample */
#define DETAIL_RES 129 /* detail noise grid per chunk edge (2 m apart) */
#define THRESHOLD_SAMPLES 2048

/* Independent noise streams. */
enum { SEED_CONTINENT = 1, SEED_HILLS, SEED_MOUNTAIN_MASK, SEED_RIDGES, SEED_DETAIL, SEED_JITTER };

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
}

double zb_terrain_macro_height(const zb_terrain *t, zb_dvec3 d) {
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

double zb_terrain_height(const zb_terrain *t, zb_dvec3 d) {
  double r = t->planet.radius;
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

const char *zb_material_name(zb_material m) {
  static const char *const names[ZB_MAT_COUNT] = {"none", "soil", "sand", "rock", "gravel", "snow", "ice", "mud"};
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

static void generate_coarse(const zb_terrain *t, zb_chunk_terrain *ct) {
  face_frame f = frame_of(ct->chunk.face);
  double ta[MACRO_RES], tb[MACRO_RES];
  axis_tangents(t, ct->chunk.x, -COARSE_STEP, COARSE_STEP, MACRO_RES, ta);
  axis_tangents(t, ct->chunk.y, -COARSE_STEP, COARSE_STEP, MACRO_RES, tb);
  for (int y = 0; y < MACRO_RES; ++y) {
    for (int x = 0; x < MACRO_RES; ++x) {
      ct->macro[y * MACRO_RES + x] = (float)zb_terrain_macro_height(t, sample_dir(&f, ta[x], tb[y]));
    }
  }
  /* Coarse materials from the macro grid (slope over 16 m). */
  double w, h;
  zb_planet_chunk_size(&t->planet, ct->chunk, &w, &h);
  double sx = 2.0 * COARSE_STEP * w / ZB_CHUNK_SIZE, sy = 2.0 * COARSE_STEP * h / ZB_CHUNK_SIZE;
  double sin_lat = zb_planet_chunk_dir(&t->planet, ct->chunk).z;
  coarse_range(ct);
  double jitter[JITTER_RES * JITTER_RES];
  jitter_lattice(t, ct->chunk, jitter);
  for (int y = 0; y < ZB_COARSE_RES; ++y) {
    const float *row = &ct->macro[(y + 1) * MACRO_RES + 1];
    for (int x = 0; x < ZB_COARSE_RES; ++x) {
      double gx = (row[x + 1] - row[x - 1]) / sx;
      double gy = (row[x + MACRO_RES] - row[x - MACRO_RES]) / sy;
      double slope = sqrt(gx * gx + gy * gy);
      ct->coarse_material[y * ZB_COARSE_RES + x] =
          (uint8_t)zb_terrain_material_rule(row[x], slope, sin_lat, jitter_at(jitter, x * 16.0, y * 16.0));
    }
  }
  ct->stage = ZB_STAGE_COARSE;
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
  ct->height = height;
  ct->min_height = ct->max_height = height[0];
  update_range(ct, height, R * R);
  ct->stage = ZB_STAGE_TERRAIN;
  return ZB_OK;
}

static zb_result generate_surface(const zb_terrain *t, const zb_allocator *a, zb_chunk_terrain *ct) {
  uint8_t *mat = zb_alloc(a, (size_t)ZB_SURFACE_RES * ZB_SURFACE_RES);
  if (!mat) {
    return ZB_ERR_NOMEM;
  }
  double jitter[JITTER_RES * JITTER_RES];
  jitter_lattice(t, ct->chunk, jitter);
  double w, h;
  zb_planet_chunk_size(&t->planet, ct->chunk, &w, &h);
  double cw = w / ZB_SURFACE_RES, ch = h / ZB_SURFACE_RES;
  double sin_lat = zb_planet_chunk_dir(&t->planet, ct->chunk).z;
  const float *hf = ct->height;
  for (int y = 0; y < ZB_SURFACE_RES; ++y) {
    const float *r0 = &hf[y * ZB_HEIGHT_RES], *r1 = r0 + ZB_HEIGHT_RES;
    for (int x = 0; x < ZB_SURFACE_RES; ++x) {
      double gx = ((double)r0[x + 1] + r1[x + 1] - r0[x] - r1[x]) / (2.0 * cw);
      double gy = ((double)r1[x] + r1[x + 1] - r0[x] - r0[x + 1]) / (2.0 * ch);
      double avg = ((double)r0[x] + r0[x + 1] + r1[x] + r1[x + 1]) * 0.25;
      mat[y * ZB_SURFACE_RES + x] =
          (uint8_t)zb_terrain_material_rule(avg, sqrt(gx * gx + gy * gy), sin_lat, jitter_at(jitter, x + 0.5, y + 0.5));
    }
  }
  ct->material = mat;
  ct->stage = ZB_STAGE_SURFACE;
  return ZB_OK;
}

zb_result zb_terrain_generate(const zb_terrain *t, const zb_allocator *a, zb_chunk_terrain *ct, int stage) {
  if (stage > ZB_STAGE_SURFACE || !zb_chunk_valid(&t->planet, ct->chunk)) {
    return ZB_ERR_INVALID;
  }
  if (ct->stage < ZB_STAGE_COARSE && stage >= ZB_STAGE_COARSE) {
    generate_coarse(t, ct);
  }
  if (ct->stage < ZB_STAGE_TERRAIN && stage >= ZB_STAGE_TERRAIN) {
    zb_result r = generate_terrain(t, a, ct);
    if (r != ZB_OK) {
      return r;
    }
  }
  if (ct->stage < ZB_STAGE_SURFACE && stage >= ZB_STAGE_SURFACE) {
    return generate_surface(t, a, ct);
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
  }
  if (ct->height) {
    zb_hasher_bytes(&h, ct->height, sizeof(float) * ZB_HEIGHT_RES * ZB_HEIGHT_RES);
  }
  if (ct->material) {
    zb_hasher_bytes(&h, ct->material, (size_t)ZB_SURFACE_RES * ZB_SURFACE_RES);
  }
  return zb_hasher_final(&h);
}

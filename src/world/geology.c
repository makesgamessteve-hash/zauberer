/* SPDX-License-Identifier: MIT */
/* Deep-time geology: the starting world, the tectonic time step and the
 * driver that runs the phases (see world/geology.h). */
#include "world/geology_internal.h"

#include <stdio.h>
#include <string.h>

#include "world/dvec.h"
#include "world/heap.h"
#include "world/noise.h"

#define OCEANIC_THICKNESS 7.0f
#define CONTINENTAL_THICKNESS 35.0f
#define MAX_CONTINENTAL_THICKNESS 75.0f
#define MAX_OCEANIC_THICKNESS 24.0f
#define BUOYANT_THICKNESS 15.0f   /* oceanic crust this thick (arcs, plateaus) no longer subducts */
#define ACCRETION_THICKNESS 18.0f /* island-arc crust this thick has matured into continental crust */
#define COLLAPSE_THICKNESS 55.0f  /* thicker crust spreads onto the ocean floor beside it */
#define EROSION_RATE 0.0087       /* per Myr: mountains lose half their height in about 80 Myr */
#define AGE_OF_COAL 360.0         /* Myr before present: land plants make coal swamps after this */
#define SIN_30 0.5
#define SIN_12 0.2079
#define SIN_38 0.6157
#define SIN_52 0.7880
#define SIN_55 0.8192
#define SIN_60 0.8660

/* ---- Target presets ----------------------------------------------------------- */

typedef struct preset {
  int plates, cratons, hotspots, min_plates;
  double continental; /* share of the surface with continental crust at the start */
  double rift_share;  /* a plate holding this share of all continental crust rifts */
  double rift_chance; /* chance per 4 Myr that a large continental plate rifts */
  double temp_offset; /* climate offset (ice world < 0 < hothouse) */
} preset;

static const preset PRESETS[ZB_GEO_TARGET_COUNT] = {
    {12, 6, 8, 7, 0.46, 0.45, 0.010, 0.0},    /* Earth */
    {9, 3, 6, 4, 0.46, 2.00, 0.000, 0.0},     /* Pangaea: no rifting */
    {20, 24, 14, 16, 0.30, 0.06, 0.100, 0.0}, /* Archipelago: many small cratons, frequent rifts */
    {12, 6, 8, 7, 0.46, 0.45, 0.010, -3.5},   /* Ice world */
    {12, 6, 8, 7, 0.46, 0.45, 0.010, 2.5},    /* Hothouse */
};

/* ---- Crust buffers --------------------------------------------------------------- */

zb_result zb_geo_crust_alloc(const zb_allocator *a, zb_geo_crust *c, int n) {
  size_t un = (size_t)n;
  memset(c, 0, sizeof *c);
  c->plate = zb_alloc(a, un);
  c->continental = zb_alloc(a, un);
  c->basement = zb_alloc(a, un);
  c->thickness = zb_alloc(a, sizeof(float) * un);
  c->born = zb_alloc(a, sizeof(float) * un);
  c->orogeny = zb_alloc(a, sizeof(float) * un);
  c->glacial = zb_alloc(a, sizeof(float) * un);
  c->flags = zb_alloc(a, sizeof(uint16_t) * un);
  c->strata = zb_calloc(a, sizeof(zb_stratum) * un * ZB_STRATA);
  if (!c->plate || !c->continental || !c->basement || !c->thickness || !c->born || !c->orogeny || !c->glacial ||
      !c->flags || !c->strata) {
    zb_geo_crust_free(a, c);
    return ZB_ERR_NOMEM;
  }
  return ZB_OK;
}

void zb_geo_crust_free(const zb_allocator *a, zb_geo_crust *c) {
  zb_free(a, c->plate);
  zb_free(a, c->continental);
  zb_free(a, c->basement);
  zb_free(a, c->thickness);
  zb_free(a, c->born);
  zb_free(a, c->orogeny);
  zb_free(a, c->glacial);
  zb_free(a, c->flags);
  zb_free(a, c->strata);
  memset(c, 0, sizeof *c);
}

void zb_geo_crust_copy(zb_geo_crust *dst, const zb_geo_crust *src, int n) {
  size_t un = (size_t)n;
  memcpy(dst->plate, src->plate, un);
  memcpy(dst->continental, src->continental, un);
  memcpy(dst->basement, src->basement, un);
  memcpy(dst->thickness, src->thickness, sizeof(float) * un);
  memcpy(dst->born, src->born, sizeof(float) * un);
  memcpy(dst->orogeny, src->orogeny, sizeof(float) * un);
  memcpy(dst->glacial, src->glacial, sizeof(float) * un);
  memcpy(dst->flags, src->flags, sizeof(uint16_t) * un);
  memcpy(dst->strata, src->strata, sizeof(zb_stratum) * un * ZB_STRATA);
}

/* Copies point j of src to point i of dst. */
static void crust_move(zb_geo_crust *dst, int i, const zb_geo_crust *src, int j) {
  dst->plate[i] = src->plate[j];
  dst->continental[i] = src->continental[j];
  dst->basement[i] = src->basement[j];
  dst->thickness[i] = src->thickness[j];
  dst->born[i] = src->born[j];
  dst->orogeny[i] = src->orogeny[j];
  dst->glacial[i] = src->glacial[j];
  dst->flags[i] = src->flags[j];
  memcpy(&dst->strata[(size_t)i * ZB_STRATA], &src->strata[(size_t)j * ZB_STRATA], sizeof(zb_stratum) * ZB_STRATA);
}

/* ---- Strata ------------------------------------------------------------------------ */

static uint16_t clamp_u16(double v) { return (uint16_t)(v < 0.0 ? 0.0 : (v > 65535.0 ? 65535.0 : v + 0.5)); }

void zb_geo_add_stratum(zb_stratum *col, zb_rock rock, zb_rock_env env, double metres, double myr_ago) {
  if (metres < 0.5) {
    return;
  }
  zb_stratum *top = &col[0];
  if (top->thickness > 0 && top->rock == rock && top->env == env && (double)top->age - myr_ago < 60.0) {
    top->thickness = clamp_u16((double)top->thickness + metres);
    top->age = clamp_u16(myr_ago);
    return;
  }
  /* Push down; the oldest stratum merges into the one above it when full. */
  if (col[ZB_STRATA - 1].thickness > 0) {
    col[ZB_STRATA - 2].thickness =
        clamp_u16((double)col[ZB_STRATA - 2].thickness + (double)col[ZB_STRATA - 1].thickness);
  }
  memmove(&col[1], &col[0], sizeof(zb_stratum) * (ZB_STRATA - 1));
  col[0].rock = (uint8_t)rock;
  col[0].env = (uint8_t)env;
  col[0].thickness = clamp_u16(metres);
  col[0].age = clamp_u16(myr_ago);
}

double zb_geo_erode_strata(zb_stratum *col, double metres) {
  double removed = 0.0;
  while (metres > 0.0 && col[0].thickness > 0) {
    double t = (double)col[0].thickness;
    if (t > metres) {
      col[0].thickness = clamp_u16(t - metres);
      removed += metres;
      if (col[0].thickness > 0) {
        return removed;
      }
    } else {
      removed += t;
      metres -= t;
    }
    memmove(&col[0], &col[1], sizeof(zb_stratum) * (ZB_STRATA - 1));
    memset(&col[ZB_STRATA - 1], 0, sizeof(zb_stratum));
  }
  return removed;
}

void zb_geo_metamorphose(zb_stratum *col) {
  for (int k = 0; k < ZB_STRATA && col[k].thickness > 0; ++k) {
    switch (col[k].rock) {
      case ZB_ROCK_SHALE:
      case ZB_ROCK_PELAGIC_CLAY: col[k].rock = ZB_ROCK_SCHIST; break;
      case ZB_ROCK_LIMESTONE:
      case ZB_ROCK_CHALK: col[k].rock = ZB_ROCK_MARBLE; break;
      case ZB_ROCK_SANDSTONE:
      case ZB_ROCK_RED_SANDSTONE:
      case ZB_ROCK_CONGLOMERATE: col[k].rock = ZB_ROCK_QUARTZITE; break;
      case ZB_ROCK_GRANITE: col[k].rock = ZB_ROCK_GNEISS; break;
      default: continue;
    }
    col[k].env = ZB_ENV_MOUNTAIN_ROOT;
  }
}

double zb_geo_sediment_thickness(const zb_stratum *col) {
  double t = 0.0;
  for (int k = 0; k < ZB_STRATA; ++k) {
    switch (col[k].rock) {
      case ZB_ROCK_SANDSTONE:
      case ZB_ROCK_RED_SANDSTONE:
      case ZB_ROCK_CONGLOMERATE:
      case ZB_ROCK_SHALE:
      case ZB_ROCK_LIMESTONE:
      case ZB_ROCK_CHALK:
      case ZB_ROCK_COAL:
      case ZB_ROCK_SALT:
      case ZB_ROCK_GYPSUM:
      case ZB_ROCK_TILL:
      case ZB_ROCK_PELAGIC_CLAY: t += (double)col[k].thickness; break;
      default: break;
    }
  }
  return t;
}

/* ---- Helpers ------------------------------------------------------------------------- */

static zb_dvec3 random_unit(zb_rng *r) {
  double z = 2.0 * zb_rng_next_double(r) - 1.0;
  double phi = 2.0 * ZB_GEO_PI * zb_rng_next_double(r);
  double s = sqrt(1.0 - z * z);
  return zb_dv(s * zb_dcos(phi), s * zb_dsin(phi), z);
}

static double rand_range(zb_rng *r, double lo, double hi) { return lo + (hi - lo) * zb_rng_next_double(r); }

/* Plate speed in cm/yr to radians per Myr at the Earth-scale reference radius. */
static double speed_to_rate(double cm_per_year) { return cm_per_year * 10.0 / ZB_GEO_REF_RADIUS_KM; }

static zb_stratum *column(zb_geo_crust *c, int i) { return &c->strata[(size_t)i * ZB_STRATA]; }

static double now_myr(const zb_geology *g) { return g->opt.span_myr - (double)g->step * g->dt; }

/* ---- Elevation ------------------------------------------------------------------------- */

static float crust_elevation(bool continental, double thickness, double age) {
  double e;
  if (continental) { /* Airy isostasy: 0.18 km of relief per km of crust */
    e = 400.0 + (thickness - 35.0) * 180.0;
  } else { /* the ocean floor deepens as it cools; plateaus and arcs stand higher */
    e = -(2600.0 + 340.0 * sqrt(age > 0.0 ? age : 0.0));
    if (e < -6400.0) {
      e = -6400.0;
    }
    e += (thickness - 7.0) * 120.0;
  }
  return (float)(e < -11000.0 ? -11000.0 : (e > 9000.0 ? 9000.0 : e));
}

void zb_geo_update_elevation(zb_geology *g) {
  double now = now_myr(g);
  for (int i = 0; i < g->n; ++i) {
    float e = crust_elevation(g->cur.continental[i] != 0, (double)g->cur.thickness[i], (double)g->cur.born[i] - now);
    if (g->cur.flags[i] & ZB_GEO_TRENCH) {
      e -= 2500.0f;
      if (e < -11000.0f) {
        e = -11000.0f;
      }
    }
    g->elevation[i] = e;
  }
}

float zb_geo_level_for_fraction(double *hist, const float *values, const float *weights, size_t n, double fraction) {
  memset(hist, 0, sizeof(double) * ZB_GEO_HIST_BINS);
  double total = 0.0;
  for (size_t i = 0; i < n; ++i) {
    int b = (int)floor((double)values[i]) - ZB_GEO_HIST_LO;
    b = b < 0 ? 0 : (b >= ZB_GEO_HIST_BINS ? ZB_GEO_HIST_BINS - 1 : b);
    double w = weights ? (double)weights[i] : 1.0;
    hist[b] += w;
    total += w;
  }
  double want = fraction * total, acc = 0.0;
  for (int b = ZB_GEO_HIST_BINS - 1; b >= 0; --b) {
    acc += hist[b];
    if (acc >= want) {
      return (float)(b + ZB_GEO_HIST_LO);
    }
  }
  return (float)ZB_GEO_HIST_LO;
}

/* ---- The starting world --------------------------------------------------------------- */

static void grow_plates(zb_geology *g) {
  const zb_lattice *l = &g->lat;
  int n = g->n, plates = g->opt.plates;
  double weight[ZB_GEO_MAX_PLATES];
  zb_heap h;
  zb_heap_init(&h, g->a);
  memset(g->cur.plate, ZB_GEO_NO_PLATE, (size_t)n);
  for (int k = 0; k < plates; ++k) {
    weight[k] = rand_range(&g->rng, 0.6, 1.6);
    int seed_point = (int)zb_rng_range(&g->rng, 0, n - 1);
    zb_heap_push(&h, 0.0, seed_point, k);
  }
  uint64_t ns = zb_hash_combine(g->seed, 0x706c617465ull);
  zb_heap_item it;
  while (zb_heap_pop(&h, &it)) {
    if (g->cur.plate[it.id] != ZB_GEO_NO_PLATE) {
      continue;
    }
    g->cur.plate[it.id] = (uint8_t)it.aux;
    for (int k = 0; k < ZB_LATTICE_K; ++k) {
      int j = zb_lattice_nbr(l, it.id, k);
      if (j >= 0 && g->cur.plate[j] == ZB_GEO_NO_PLATE) {
        zb_dvec3 p = l->p[j];
        double rough = 1.0 + 0.5 * zb_fbm3(ns, p.x * 3.0, p.y * 3.0, p.z * 3.0, 3, 0.5);
        zb_heap_push(&h, it.key + zb_dv_angle(l->p[it.id], p) * weight[it.aux] * rough, j, it.aux);
      }
    }
  }
  zb_heap_free(&h);
}

void zb_geo_init_world(zb_geology *g) {
  const zb_lattice *l = &g->lat;
  const preset *pr = &PRESETS[g->opt.target];
  int n = g->n;
  double span = g->opt.span_myr;
  if (g->opt.plates <= 0) {
    g->opt.plates = pr->plates;
  }
  g->min_plates = pr->min_plates < g->opt.plates ? pr->min_plates : g->opt.plates;
  g->rift_share = pr->rift_share;
  g->rift_chance = pr->rift_chance;
  g->temp_offset = pr->temp_offset;
  for (int k = 0; k < 4; ++k) {
    g->cycle[k] = rand_range(&g->rng, 0.0, 2.0 * ZB_GEO_PI);
  }
  grow_plates(g);

  /* Continental cores. */
  zb_dvec3 centre[24];
  double radius[24], wsum = 0.0, w[24];
  int cratons = pr->cratons;
  for (int c = 0; c < cratons; ++c) {
    centre[c] = random_unit(&g->rng);
    w[c] = rand_range(&g->rng, 0.4, 1.6);
    wsum += w[c];
  }
  for (int c = 0; c < cratons; ++c) {
    double area = pr->continental * w[c] / wsum; /* share of the sphere */
    radius[c] = zb_dacos(1.0 - 2.0 * area);
  }
  uint64_t s1 = zb_hash_combine(g->seed, 1), s2 = zb_hash_combine(g->seed, 2), s3 = zb_hash_combine(g->seed, 3);
  for (int i = 0; i < n; ++i) {
    zb_dvec3 p = l->p[i];
    double f1 = zb_fbm3(s1, p.x * 2.5, p.y * 2.5, p.z * 2.5, 4, 0.5);
    double f2 = zb_fbm3(s2, p.x * 4.0, p.y * 4.0, p.z * 4.0, 3, 0.5);
    double f3 = zb_fbm3(s3, p.x * 6.0, p.y * 6.0, p.z * 6.0, 3, 0.5);
    int inside = -1;
    double core = 0.0;
    for (int c = 0; c < cratons && inside < 0; ++c) {
      double d = zb_dv_angle(p, centre[c]);
      double r = radius[c] * (1.0 + 0.35 * f1);
      if (d < r) {
        inside = c;
        core = 1.0 - d / r;
      }
    }
    zb_stratum *col = column(&g->cur, i);
    memset(col, 0, sizeof(zb_stratum) * ZB_STRATA);
    g->cur.glacial[i] = -1.0f;
    if (inside >= 0) {
      g->cur.continental[i] = 1;
      g->cur.thickness[i] = (float)(36.0 + 3.0 * f2);
      g->cur.born[i] = (float)(span + 1200.0 + 900.0 * (1.0 + f2));
      g->cur.flags[i] = core > 0.45 ? ZB_GEO_CRATON : 0;
      g->cur.basement[i] = core > 0.45 ? ZB_ROCK_GNEISS : ZB_ROCK_GRANITE;
      /* Old mountain belts that predate the simulated span. */
      g->cur.orogeny[i] = f3 > 0.35 ? (float)(span + 80.0 + 200.0 * f3) : -1.0f;
      if (f3 > 0.35) {
        g->cur.thickness[i] += (float)(6.0 * (f3 - 0.35));
      }
      if (f2 > -0.1) { /* platform cover */
        zb_geo_add_stratum(col, ZB_ROCK_SANDSTONE, ZB_ENV_RIVER, 250.0 + 300.0 * (f2 + 0.1), span + 60.0);
        zb_geo_add_stratum(col, ZB_ROCK_LIMESTONE, ZB_ENV_SHALLOW_SEA, 150.0 + 200.0 * (f1 + 1.0) * 0.5, span + 20.0);
      }
    } else {
      g->cur.continental[i] = 0;
      g->cur.thickness[i] = OCEANIC_THICKNESS;
      g->cur.born[i] = (float)(span + 75.0 + 70.0 * f2);
      g->cur.flags[i] = 0;
      g->cur.basement[i] = ZB_ROCK_BASALT;
      g->cur.orogeny[i] = -1.0f;
      zb_geo_add_stratum(col, ZB_ROCK_PELAGIC_CLAY, ZB_ENV_DEEP_SEA, 40.0, span + 10.0);
    }
  }

  /* Plate motions: continental plates drift slower. */
  int counts[ZB_GEO_MAX_PLATES] = {0}, cont[ZB_GEO_MAX_PLATES] = {0};
  for (int i = 0; i < n; ++i) {
    counts[g->cur.plate[i]]++;
    cont[g->cur.plate[i]] += g->cur.continental[i];
  }
  memset(g->plates, 0, sizeof g->plates);
  for (int k = 0; k < g->opt.plates; ++k) {
    zb_geo_plate *pl = &g->plates[k];
    pl->alive = counts[k] > 0;
    pl->axis = random_unit(&g->rng);
    bool continental = counts[k] > 0 && cont[k] * 2 > counts[k];
    pl->rate = speed_to_rate(continental ? rand_range(&g->rng, 1.5, 3.5) : rand_range(&g->rng, 3.0, 8.0));
    pl->last_rift = 0;
  }

  /* Hotspots: about half are still active today. */
  g->hotspot_count = pr->hotspots < ZB_GEO_MAX_HOTSPOTS ? pr->hotspots : ZB_GEO_MAX_HOTSPOTS;
  for (int h = 0; h < g->hotspot_count; ++h) {
    zb_geo_hotspot *hs = &g->hotspots[h];
    hs->dir = random_unit(&g->rng);
    if (h % 2 == 0) {
      hs->start = (float)rand_range(&g->rng, 40.0, span);
      hs->end = -1.0f;
    } else {
      hs->start = (float)rand_range(&g->rng, 0.3 * span, span);
      hs->end = (float)((double)hs->start - rand_range(&g->rng, 60.0, 250.0));
    }
  }
  g->step = 0;
  g->continental_target = 0;
  for (int i = 0; i < n; ++i) {
    g->continental_target += g->cur.continental[i];
  }
  zb_geo_update_elevation(g);
  g->sea_level = zb_geo_level_for_fraction(g->hist, g->elevation, NULL, (size_t)n, g->opt.land_fraction);
}

/* ---- Rifting ------------------------------------------------------------------------------ */

static zb_dvec3 plate_centroid(const zb_geology *g, int plate, int *count) {
  zb_dvec3 c = zb_dv(0, 0, 0);
  int k = 0;
  for (int i = 0; i < g->n; ++i) {
    if (g->cur.plate[i] == plate) {
      c = zb_dv_add(c, g->lat.p[i]);
      k++;
    }
  }
  *count = k;
  return zb_dv_norm(c);
}

bool zb_geo_rift_plate(zb_geology *g, int plate, zb_dvec3 cut_normal) {
  int f = -1;
  for (int k = 0; k < ZB_GEO_MAX_PLATES && f < 0; ++k) {
    if (!g->plates[k].alive) {
      f = k;
    }
  }
  int count;
  zb_dvec3 c = plate_centroid(g, plate, &count);
  if (f < 0 || count < 2) {
    return false;
  }
  zb_dvec3 m = zb_dv_norm(zb_dv_sub(cut_normal, zb_dv_scale(c, zb_dv_dot(cut_normal, c))));
  int moved = 0;
  for (int i = 0; i < g->n; ++i) {
    moved += g->cur.plate[i] == plate && zb_dv_dot(g->lat.p[i], m) > 0.0;
  }
  if (moved == 0 || moved == count) {
    return false; /* the cut leaves one side empty */
  }
  double band = zb_dsin(1.5 * g->lat.spacing);
  for (int i = 0; i < g->n; ++i) {
    if (g->cur.plate[i] != plate) {
      continue;
    }
    double side = zb_dv_dot(g->lat.p[i], m);
    if (side > 0.0) {
      g->cur.plate[i] = (uint8_t)f;
    }
    if (g->cur.continental[i] && side > -band && side < band) {
      g->cur.flags[i] |= ZB_GEO_RIFT;
      g->cur.thickness[i] *= 0.85f;
      if (zb_rng_next_double(&g->rng) < 0.3) {
        g->cur.flags[i] |= ZB_GEO_FLOOD_BASALT;
        zb_geo_add_stratum(column(&g->cur, i), ZB_ROCK_BASALT, ZB_ENV_VOLCANIC, 300.0, now_myr(g));
      }
    }
  }
  /* The halves move apart, each towards its own side of the cut. */
  zb_dvec3 apart = zb_dv_norm(zb_dv_cross(c, m));
  double rate = speed_to_rate(rand_range(&g->rng, 2.5, 6.0));
  g->plates[f].alive = true;
  g->plates[f].axis = apart;
  g->plates[f].rate = rate;
  g->plates[f].last_rift = g->step;
  g->plates[plate].axis = zb_dv_scale(apart, -1.0);
  g->plates[plate].rate = rate * rand_range(&g->rng, 0.6, 1.0);
  g->plates[plate].last_rift = g->step;
  return true;
}

static void supercontinent_cycle(zb_geology *g, bool final_phase) {
  int counts[ZB_GEO_MAX_PLATES] = {0}, cont[ZB_GEO_MAX_PLATES] = {0}, total_cont = 0, alive = 0;
  for (int i = 0; i < g->n; ++i) {
    counts[g->cur.plate[i]]++;
    cont[g->cur.plate[i]] += g->cur.continental[i];
    total_cont += g->cur.continental[i];
  }
  for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
    g->plates[k].alive = g->plates[k].alive && counts[k] > 0;
    alive += g->plates[k].alive;
  }
  if (g->opt.target == ZB_GEO_TARGET_PANGAEA && final_phase) {
    return; /* the supercontinent holds together to the present */
  }
  for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
    if (!g->plates[k].alive || total_cont == 0) {
      continue;
    }
    double share = (double)cont[k] / (double)total_cont;
    double since = (double)(g->step - g->plates[k].last_rift) * g->dt;
    bool big = share > g->rift_share && since > 120.0;
    bool chance = share > 0.15 && since > 150.0 && zb_rng_next_double(&g->rng) < g->rift_chance * g->dt / 4.0;
    if (big || chance) {
      zb_geo_rift_plate(g, k, random_unit(&g->rng));
      alive++;
    }
  }
  while (alive < g->min_plates) {
    int largest = -1;
    for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
      if (g->plates[k].alive && (largest < 0 || counts[k] > counts[largest])) {
        largest = k;
      }
    }
    if (largest < 0 || !zb_geo_rift_plate(g, largest, random_unit(&g->rng))) {
      break;
    }
    alive++;
    counts[largest] /= 2;
  }
}

/* ---- The time step ---------------------------------------------------------------------- */

typedef struct cap {
  zb_dvec3 centre;
  double min_dot;
  zb_drot inverse; /* rotates a present position back to where its crust was a step ago */
} cap;

static void plate_caps(zb_geology *g, cap *caps) {
  zb_dvec3 sum[ZB_GEO_MAX_PLATES];
  memset(sum, 0, sizeof sum);
  for (int i = 0; i < g->n; ++i) {
    sum[g->cur.plate[i]] = zb_dv_add(sum[g->cur.plate[i]], g->lat.p[i]);
  }
  for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
    caps[k].centre = zb_dv_norm(sum[k]);
    caps[k].min_dot = 2.0;
    caps[k].inverse = zb_drot_make(g->plates[k].axis, -g->plates[k].rate * g->dt);
  }
  for (int i = 0; i < g->n; ++i) {
    cap *c = &caps[g->cur.plate[i]];
    double d = zb_dv_dot(g->lat.p[i], c->centre);
    if (d < c->min_dot) {
      c->min_dot = d;
    }
  }
  for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
    if (caps[k].min_dot <= 1.0) {
      double angle = zb_dacos(caps[k].min_dot < -1.0 ? -1.0 : caps[k].min_dot) + 2.0 * g->lat.spacing;
      caps[k].min_dot = angle >= ZB_GEO_PI ? -2.0 : zb_dcos(angle);
    }
  }
}

/* How strongly crust resists subduction. */
static double buoyancy(const zb_geo_crust *c, int j, double now) {
  if (c->continental[j]) {
    return 3.0;
  }
  if (c->thickness[j] > BUOYANT_THICKNESS) {
    return 2.0;
  }
  double age = (double)c->born[j] - now;
  return 1.0 - (age > 0.0 ? age : 0.0) / 1000.0;
}

static void advect(zb_geology *g, double now, double after, int32_t *collided_with) {
  const zb_lattice *l = &g->lat;
  cap caps[ZB_GEO_MAX_PLATES];
  plate_caps(g, caps);
  zb_geo_crust *cur = &g->cur, *next = &g->next;
  for (int i = 0; i < g->n; ++i) {
    int cand_plate[ZB_GEO_MAX_PLATES], cand_src[ZB_GEO_MAX_PLATES], nc = 0;
    for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
      if (!g->plates[k].alive) {
        continue;
      }
      zb_dvec3 q = zb_drot_apply(&caps[k].inverse, l->p[i]);
      if (zb_dv_dot(q, caps[k].centre) < caps[k].min_dot) {
        continue;
      }
      int j = zb_lattice_nearest(l, q, i);
      if (cur->plate[j] == k) {
        cand_plate[nc] = k;
        cand_src[nc] = j;
        nc++;
      }
    }
    collided_with[i] = -1;
    if (nc == 0) { /* divergence: new ocean floor */
      next->plate[i] = cur->plate[i];
      next->continental[i] = 0;
      next->basement[i] = ZB_ROCK_BASALT;
      next->thickness[i] = OCEANIC_THICKNESS;
      next->born[i] = (float)after;
      next->orogeny[i] = -1.0f;
      next->glacial[i] = -1.0f;
      next->flags[i] = ZB_GEO_RIDGE;
      memset(column(next, i), 0, sizeof(zb_stratum) * ZB_STRATA);
      continue;
    }
    int w = 0;
    for (int c = 1; c < nc; ++c) {
      if (buoyancy(cur, cand_src[c], now) > buoyancy(cur, cand_src[w], now)) {
        w = c;
      }
    }
    crust_move(next, i, cur, cand_src[w]);
    next->plate[i] = (uint8_t)cand_plate[w];
    next->flags[i] &= (uint16_t) ~(ZB_GEO_TRENCH | ZB_GEO_BOUNDARY | ZB_GEO_ACTIVE_VOLCANO);
    bool winner_buoyant = next->continental[i] || next->thickness[i] > BUOYANT_THICKNESS;
    for (int c = 0; c < nc; ++c) {
      if (c == w) {
        continue;
      }
      int j = cand_src[c];
      bool loser_buoyant = cur->continental[j] || cur->thickness[j] > BUOYANT_THICKNESS;
      if (winner_buoyant && loser_buoyant) {                     /* collision: the crust thickens into mountains */
        float t = next->thickness[i] + 0.9f * cur->thickness[j]; /* crust is conserved, not consumed */
        next->thickness[i] = t > MAX_CONTINENTAL_THICKNESS ? MAX_CONTINENTAL_THICKNESS : t;
        next->continental[i] = 1;
        next->flags[i] |= ZB_GEO_COLLISION;
        next->orogeny[i] = (float)after;
        zb_geo_metamorphose(column(next, i));
        if (next->basement[i] == ZB_ROCK_GRANITE) {
          next->basement[i] = ZB_ROCK_GNEISS;
        }
        collided_with[i] = cand_plate[c];
        g->collisions[cand_plate[w] * ZB_GEO_MAX_PLATES + cand_plate[c]]++;
      } else { /* subduction: a volcanic arc on the overriding plate */
        float cap_t = next->continental[i] ? MAX_CONTINENTAL_THICKNESS - 10.0f : MAX_OCEANIC_THICKNESS;
        float t = next->thickness[i] + (next->continental[i] ? 1.2f : 1.6f);
        next->thickness[i] = t > cap_t ? cap_t : t;
        next->flags[i] |= ZB_GEO_ARC | ZB_GEO_ACTIVE_VOLCANO;
        next->orogeny[i] = (float)after;
        zb_geo_add_stratum(column(next, i), ZB_ROCK_ANDESITE, ZB_ENV_VOLCANIC, 250.0, after);
      }
    }
  }
  zb_geo_crust tmp = g->cur;
  g->cur = g->next;
  g->next = tmp;
}

/* Plates that keep colliding weld into one; their seam becomes a suture. */
static void weld(zb_geology *g, const int32_t *collided_with) {
  int counts[ZB_GEO_MAX_PLATES] = {0};
  for (int i = 0; i < g->n; ++i) {
    counts[g->cur.plate[i]]++;
  }
  for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
    for (int m = 0; m < ZB_GEO_MAX_PLATES; ++m) {
      int hits = g->collisions[k * ZB_GEO_MAX_PLATES + m] + g->collisions[m * ZB_GEO_MAX_PLATES + k];
      if (k >= m || hits == 0 || !g->plates[k].alive || !g->plates[m].alive || counts[k] == 0 || counts[m] == 0) {
        continue;
      }
      int smaller = counts[k] < counts[m] ? counts[k] : counts[m];
      if (hits < 3 || hits * 60 < smaller) {
        continue;
      }
      int keep = counts[k] >= counts[m] ? k : m, gone = keep == k ? m : k;
      for (int i = 0; i < g->n; ++i) {
        if ((collided_with[i] == gone && g->cur.plate[i] == keep) ||
            (collided_with[i] == keep && g->cur.plate[i] == gone)) {
          g->cur.flags[i] |= ZB_GEO_SUTURE;
        }
        if (g->cur.plate[i] == gone) {
          g->cur.plate[i] = (uint8_t)keep;
        }
      }
      g->plates[gone].alive = false;
      counts[keep] += counts[gone];
      counts[gone] = 0;
    }
  }
}

/* Boundaries, trenches and arc uplift inland of the arc. */
static void boundaries(zb_geology *g, double after) {
  const zb_lattice *l = &g->lat;
  for (int i = 0; i < g->n; ++i) {
    int p = g->cur.plate[i];
    for (int k = 0; k < ZB_LATTICE_K; ++k) {
      int j = zb_lattice_nbr(l, i, k);
      if (j >= 0 && g->cur.plate[j] != p) {
        g->cur.flags[i] |= ZB_GEO_BOUNDARY;
        if (!g->cur.continental[i] && (g->cur.flags[j] & ZB_GEO_ACTIVE_VOLCANO)) {
          g->cur.flags[i] |= ZB_GEO_TRENCH;
        }
      }
    }
  }
  for (int i = 0; i < g->n; ++i) {
    if (!(g->cur.flags[i] & ZB_GEO_ACTIVE_VOLCANO)) {
      continue;
    }
    for (int k = 0; k < 6; ++k) {
      int j = zb_lattice_nbr(l, i, k);
      if (j >= 0 && g->cur.plate[j] == g->cur.plate[i] && !(g->cur.flags[j] & ZB_GEO_ACTIVE_VOLCANO) &&
          !(g->cur.flags[j] & ZB_GEO_TRENCH)) {
        float t = g->cur.thickness[j] + 0.3f;
        float cap_t = g->cur.continental[j] ? MAX_CONTINENTAL_THICKNESS - 10.0f : MAX_OCEANIC_THICKNESS;
        g->cur.thickness[j] = t > cap_t ? cap_t : t;
        if (g->cur.continental[j]) {
          g->cur.orogeny[j] = (float)after;
        }
      }
    }
  }
}

/* Continental crust flows a little: no single-point spikes. */
static void relax_thickness(zb_geology *g) {
  const zb_lattice *l = &g->lat;
  float *out = g->scratch_f;
  for (int i = 0; i < g->n; ++i) {
    out[i] = g->cur.thickness[i];
    if (!g->cur.continental[i]) {
      continue;
    }
    double sum = 0.0;
    int count = 0;
    for (int k = 0; k < 6; ++k) {
      int j = zb_lattice_nbr(l, i, k);
      if (j >= 0 && g->cur.continental[j]) {
        sum += (double)g->cur.thickness[j];
        count++;
      }
    }
    if (count > 0) {
      out[i] = (float)((double)g->cur.thickness[i] + 0.12 * (sum / count - (double)g->cur.thickness[i]));
    }
  }
  memcpy(g->cur.thickness, out, sizeof(float) * (size_t)g->n);
}

/* Mature island arcs become continental crust; overthickened mountain belts
 * spread onto the ocean floor beside them (crust is conserved). */
static void accrete(zb_geology *g, double after) {
  const zb_lattice *l = &g->lat;
  for (int i = 0; i < g->n; ++i) {
    if (!g->cur.continental[i] && g->cur.thickness[i] > ACCRETION_THICKNESS) {
      g->cur.continental[i] = 1;
      g->cur.basement[i] = ZB_ROCK_ANDESITE;
    }
  }
  /* Margins grow by sedimentation and accreted terranes while collisions have
   * shortened the continents below their budget: the shallowest ocean floor
   * beside a continent of the same plate joins it, a little each step. */
  int count = 0;
  for (int i = 0; i < g->n; ++i) {
    count += g->cur.continental[i];
  }
  int deficit = g->continental_target - count;
  int32_t *cand = g->scratch_a;
  int nc = 0;
  for (int i = 0; i < g->n && deficit > 0; ++i) {
    if (g->cur.continental[i]) {
      continue;
    }
    for (int k = 0; k < 6; ++k) {
      int j = zb_lattice_nbr(l, i, k);
      if (j >= 0 && g->cur.continental[j] && g->cur.plate[j] == g->cur.plate[i]) {
        cand[nc++] = i;
        break;
      }
    }
  }
  int grow = deficit > 0 ? (deficit + 3) / 4 : 0;
  for (int c = 0; c < grow && c < nc; ++c) { /* partial selection: highest floor first */
    int best = c;
    for (int d = c + 1; d < nc; ++d) {
      if (g->elevation[cand[d]] > g->elevation[cand[best]]) {
        best = d;
      }
    }
    int32_t t = cand[c];
    cand[c] = cand[best];
    cand[best] = t;
    int i = cand[c];
    g->cur.continental[i] = 1;
    g->cur.thickness[i] = 30.0f;
    g->cur.basement[i] = ZB_ROCK_SCHIST;
    zb_geo_add_stratum(column(&g->cur, i), ZB_ROCK_SANDSTONE, ZB_ENV_SHALLOW_SEA, 200.0, after);
  }
  for (int i = 0; i < g->n; ++i) {
    if (!g->cur.continental[i] || g->cur.thickness[i] <= COLLAPSE_THICKNESS) {
      continue;
    }
    for (int k = 0; k < 6; ++k) {
      int j = zb_lattice_nbr(l, i, k);
      if (j < 0 || g->cur.continental[j] || g->cur.plate[j] != g->cur.plate[i]) {
        continue;
      }
      g->cur.continental[j] = 1;
      g->cur.thickness[j] = 28.0f;
      g->cur.thickness[i] -= 21.0f;
      g->cur.born[j] = (float)after;
      g->cur.orogeny[j] = (float)after;
      g->cur.basement[j] = ZB_ROCK_GNEISS;
      g->cur.flags[j] = (uint16_t)((g->cur.flags[j] & ~ZB_GEO_RIDGE) | ZB_GEO_COLLISION);
      memcpy(column(&g->cur, j), column(&g->cur, i), sizeof(zb_stratum) * ZB_STRATA);
      break;
    }
  }
}

static void climate_era(zb_geology *g, double after, zb_geology_era *era) {
  int polar = 0, cont = 0;
  for (int i = 0; i < g->n; ++i) {
    if (g->cur.continental[i]) {
      cont++;
      polar += fabs(g->lat.p[i].z) > SIN_60;
    }
  }
  double polar_share = cont > 0 ? (double)polar / (double)cont : 0.0;
  double t = 0.8 * zb_dsin(2.0 * ZB_GEO_PI * after / 150.0 + g->cycle[0]) +
             0.4 * zb_dsin(2.0 * ZB_GEO_PI * after / 41.0 + g->cycle[1]) + 0.3 * (zb_rng_next_double(&g->rng) - 0.5) -
             2.5 * (polar_share - 0.08) + g->temp_offset;
  double h = 0.8 * zb_dsin(2.0 * ZB_GEO_PI * after / 90.0 + g->cycle[2]) + 0.3 * (zb_rng_next_double(&g->rng) - 0.5);
  era->myr_ago = (float)after;
  era->temperature = (float)t;
  era->humidity = (float)h;
  era->ice_age = t < -0.55;
}

/* Erosion of land and deposition downhill, with the environment deciding the rock. */
static void erode_and_deposit(zb_geology *g, const zb_geology_era *era, double after) {
  const zb_lattice *l = &g->lat;
  int32_t *recv = g->scratch_a;
  double sea = (double)g->sea_level;
  bool arid = era->humidity < -0.35f, humid = era->humidity > 0.35f;
  for (int i = 0; i < g->n; ++i) {
    recv[i] = -1;
    float lowest = g->elevation[i];
    for (int k = 0; k < 6; ++k) {
      int j = zb_lattice_nbr(l, i, k);
      if (j >= 0 && g->elevation[j] < lowest) {
        lowest = g->elevation[j];
        recv[i] = j;
      }
    }
  }
  for (int i = 0; i < g->n; ++i) {
    double rel = (double)g->elevation[i] - sea;
    if (rel <= 0.0) {
      continue;
    }
    double factor = (era->ice_age && g->cur.glacial[i] == (float)after) ? 1.6 : (arid ? 0.7 : 1.0);
    double e = EROSION_RATE * rel / 190.0 * g->dt * factor; /* km of crust */
    g->cur.thickness[i] -= (float)e;
    zb_geo_erode_strata(column(&g->cur, i), e * 1000.0);
    /* Follow the slope to where the sediment settles. */
    int t = i, last_land = i, guard = 0;
    while (recv[t] >= 0 && guard++ < 64) {
      t = recv[t];
      if ((double)g->elevation[t] <= sea) {
        break;
      }
      last_land = t;
    }
    if (t == i) {
      continue;
    }
    double s = e * 0.4; /* km of thickness gained where it settles (spread and compacted) */
    /* Coal swamps on the coastal lowland where a river meets the sea. */
    if (humid && after < AGE_OF_COAL && last_land != i && (double)g->elevation[t] <= sea &&
        (double)g->elevation[last_land] - sea < 300.0 && fabs(l->p[last_land].z) < SIN_55) {
      double peat = s * 200.0 < 40.0 ? s * 200.0 : 40.0;
      zb_geo_add_stratum(column(&g->cur, last_land), ZB_ROCK_COAL, ZB_ENV_SWAMP, peat, after);
    }
    g->cur.thickness[t] += (float)s;
    double metres = s * 1000.0;
    double depth = sea - (double)g->elevation[t];
    double z = fabs(l->p[t].z);
    zb_stratum *col = column(&g->cur, t);
    if (depth > 0.0) {
      bool warm = z < SIN_30;
      if (arid && depth < 200.0 && z > SIN_12 && z < SIN_38) { /* sabkhas and restricted shallow seas */
        zb_geo_add_stratum(col, ZB_ROCK_GYPSUM, ZB_ENV_SALT_BASIN, metres * 0.4, after);
        zb_geo_add_stratum(col, ZB_ROCK_SALT, ZB_ENV_SALT_BASIN, metres * 0.3, after);
      } else if (depth < 400.0) {
        zb_geo_add_stratum(col, warm ? ZB_ROCK_LIMESTONE : ZB_ROCK_SHALE, ZB_ENV_SHALLOW_SEA, metres, after);
      } else {
        zb_geo_add_stratum(col, ZB_ROCK_SHALE, ZB_ENV_DEEP_SEA, metres, after);
      }
    } else if (arid && z > SIN_12 && z < SIN_38 && recv[t] < 0) {
      zb_geo_add_stratum(col, ZB_ROCK_SALT, ZB_ENV_SALT_BASIN, metres * 0.5, after);
      zb_geo_add_stratum(col, ZB_ROCK_GYPSUM, ZB_ENV_SALT_BASIN, metres * 0.2, after);
    } else if (arid) {
      zb_geo_add_stratum(col, ZB_ROCK_RED_SANDSTONE, ZB_ENV_DESERT, metres, after);
    } else if (humid && after < AGE_OF_COAL && -depth < 400.0 && z < SIN_55) {
      zb_geo_add_stratum(col, ZB_ROCK_SANDSTONE, ZB_ENV_RIVER, metres * 0.7, after);
      zb_geo_add_stratum(col, ZB_ROCK_COAL, ZB_ENV_SWAMP, metres * 0.3 < 60.0 ? metres * 0.3 : 60.0, after);
    } else {
      zb_geo_add_stratum(col, rel > 2000.0 ? ZB_ROCK_CONGLOMERATE : ZB_ROCK_SANDSTONE, ZB_ENV_RIVER, metres, after);
    }
  }
  /* The slow rain of the deep sea. */
  for (int i = 0; i < g->n; ++i) {
    double depth = sea - (double)g->elevation[i];
    if (!g->cur.continental[i] && depth > 1000.0) {
      zb_rock rock = depth > 4500.0 ? ZB_ROCK_PELAGIC_CLAY : (after < 145.0 ? ZB_ROCK_CHALK : ZB_ROCK_LIMESTONE);
      zb_geo_add_stratum(column(&g->cur, i), rock, ZB_ENV_DEEP_SEA, 3.0 * g->dt / 4.0, after);
    }
  }
}

static void glaciate(zb_geology *g, double after) {
  double sea = (double)g->sea_level;
  for (int i = 0; i < g->n; ++i) {
    double z = fabs(g->lat.p[i].z), rel = (double)g->elevation[i] - sea;
    if (rel > 0.0 && (z > SIN_52 || (rel > 2500.0 && z > SIN_30))) {
      g->cur.flags[i] |= ZB_GEO_GLACIATED;
      g->cur.glacial[i] = (float)after;
    }
  }
}

/* Retreating ice leaves till behind (after the ice has scoured the ground). */
static void deposit_till(zb_geology *g, double after) {
  for (int i = 0; i < g->n; ++i) {
    if (g->cur.glacial[i] == (float)after) {
      zb_geo_add_stratum(column(&g->cur, i), ZB_ROCK_TILL, ZB_ENV_GLACIAL, 15.0, after);
    }
  }
}

static void hotspots(zb_geology *g, double after) {
  for (int h = 0; h < g->hotspot_count; ++h) {
    const zb_geo_hotspot *hs = &g->hotspots[h];
    if ((double)hs->start < after || (double)hs->end > after) {
      continue;
    }
    int i = zb_lattice_nearest(&g->lat, hs->dir, -1);
    zb_stratum *col = column(&g->cur, i);
    if (g->cur.continental[i]) {
      g->cur.flags[i] |= ZB_GEO_FLOOD_BASALT | ZB_GEO_HOTSPOT;
      g->cur.thickness[i] += 0.8f;
      zb_geo_add_stratum(col, ZB_ROCK_BASALT, ZB_ENV_VOLCANIC, 300.0, after);
    } else {
      float t = g->cur.thickness[i] + 2.5f;
      g->cur.thickness[i] = t > MAX_OCEANIC_THICKNESS ? MAX_OCEANIC_THICKNESS : t;
      g->cur.flags[i] |= ZB_GEO_HOTSPOT;
      zb_geo_add_stratum(col, ZB_ROCK_BASALT, ZB_ENV_VOLCANIC, 400.0, after);
    }
    if (hs->end < 0.0f && after <= g->dt) {
      g->cur.flags[i] |= ZB_GEO_ACTIVE_VOLCANO;
    }
  }
}

static void perturb_plates(zb_geology *g) {
  for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
    zb_geo_plate *pl = &g->plates[k];
    if (!pl->alive) {
      continue;
    }
    pl->axis = zb_dv_norm(zb_dv_add(pl->axis, zb_dv_scale(random_unit(&g->rng), 0.06)));
    double r = pl->rate * (1.0 + 0.08 * (zb_rng_next_double(&g->rng) - 0.5));
    double lo = speed_to_rate(1.0), hi = speed_to_rate(10.0);
    pl->rate = r < lo ? lo : (r > hi ? hi : r);
  }
}

void zb_geo_step(zb_geology *g) {
  double now = now_myr(g), after = now - g->dt;
  if (after < 0.0) {
    after = 0.0;
  }
  zb_geology_era *era = &g->eras[g->step];
  memset(era, 0, sizeof *era);
  perturb_plates(g);
  memset(g->collisions, 0, sizeof(uint16_t) * ZB_GEO_MAX_PLATES * ZB_GEO_MAX_PLATES);
  advect(g, now, after, g->scratch_b);
  weld(g, g->scratch_b);
  boundaries(g, after);
  relax_thickness(g);
  accrete(g, after);
  g->step++;
  zb_geo_update_elevation(g);
  climate_era(g, after, era);
  /* Sea level: fitted to the land target, raised and lowered by slow cycles and ice. */
  double eustatic = 120.0 * zb_dsin(2.0 * ZB_GEO_PI * after / 250.0 + g->cycle[3]) - (era->ice_age ? 100.0 : 0.0);
  g->sea_level =
      (float)((double)zb_geo_level_for_fraction(g->hist, g->elevation, NULL, (size_t)g->n, g->opt.land_fraction) +
              eustatic);
  if (era->ice_age) {
    glaciate(g, after);
  }
  erode_and_deposit(g, era, after);
  if (era->ice_age) {
    deposit_till(g, after);
  }
  hotspots(g, after);
  zb_geo_update_elevation(g);
  supercontinent_cycle(g, g->step > g->final_start);
  int alive = 0, land = 0;
  for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
    alive += g->plates[k].alive;
  }
  for (int i = 0; i < g->n; ++i) {
    land += g->elevation[i] > g->sea_level;
  }
  era->sea_level = g->sea_level;
  era->plates = (uint8_t)alive;
  era->land_fraction = (float)land / (float)g->n;
}

/* ---- Options and lifetime ----------------------------------------------------------------- */

void zb_geology_default_options(zb_geology_options *o, double radius) {
  memset(o, 0, sizeof *o);
  o->radius = radius;
  o->points = 40000;
  o->steps = 125;
  o->span_myr = 500.0;
  o->plates = 0;
  o->land_fraction = 0.35;
  o->target = ZB_GEO_TARGET_EARTH;
  o->max_retries = 3;
  o->macro_res = 0;
  o->erosion_iterations = 12;
}

static const char *check_options(const zb_geology_options *o) {
  if (!(o->radius >= ZB_PLANET_MIN_RADIUS && o->radius <= ZB_PLANET_MAX_RADIUS)) {
    return "radius out of range";
  }
  if (o->points < 500 || o->points > ZB_LATTICE_MAX_POINTS) {
    return "points must be 500..1000000";
  }
  if (o->steps < 10 || o->steps > 2000) {
    return "steps must be 10..2000";
  }
  if (!(o->span_myr >= 50.0 && o->span_myr <= 4000.0)) {
    return "span must be 50..4000 Myr";
  }
  if (o->plates != 0 && (o->plates < 2 || o->plates > ZB_GEO_MAX_PLATES - 8)) {
    return "plates must be 2..24";
  }
  if (!(o->land_fraction >= 0.05 && o->land_fraction <= 0.95)) {
    return "land fraction must be 0.05..0.95";
  }
  if ((int)o->target < 0 || o->target >= ZB_GEO_TARGET_COUNT) {
    return "unknown target";
  }
  if (o->max_retries < 0 || o->max_retries > 10) {
    return "retries must be 0..10";
  }
  if (o->macro_res != 0 && (o->macro_res < 8 || o->macro_res > 2048)) {
    return "macro resolution must be 8..2048";
  }
  if (o->erosion_iterations < 0 || o->erosion_iterations > 200) {
    return "erosion iterations must be 0..200";
  }
  return NULL;
}

static zb_result snapshot_alloc(zb_geology *g, zb_geo_snapshot *s) {
  if (zb_geo_crust_alloc(g->a, &s->crust, g->n) != ZB_OK) {
    return ZB_ERR_NOMEM;
  }
  s->elevation = zb_alloc(g->a, sizeof(float) * (size_t)g->n);
  return s->elevation ? ZB_OK : ZB_ERR_NOMEM;
}

static void snapshot_free(zb_geology *g, zb_geo_snapshot *s) {
  zb_geo_crust_free(g->a, &s->crust);
  zb_free(g->a, s->elevation);
  s->elevation = NULL;
}

static void snapshot_save(zb_geology *g, zb_geo_snapshot *s) {
  zb_geo_crust_copy(&s->crust, &g->cur, g->n);
  memcpy(s->elevation, g->elevation, sizeof(float) * (size_t)g->n);
  memcpy(s->plates, g->plates, sizeof g->plates);
  s->step = g->step;
  s->sea_level = g->sea_level;
  s->valid = true;
}

static void snapshot_restore(zb_geology *g, const zb_geo_snapshot *s) {
  zb_geo_crust_copy(&g->cur, &s->crust, g->n);
  memcpy(g->elevation, s->elevation, sizeof(float) * (size_t)g->n);
  memcpy(g->plates, s->plates, sizeof g->plates);
  g->step = s->step;
  g->sea_level = s->sea_level;
}

zb_geology *zb_geology_create(const zb_allocator *a, uint64_t seed, const zb_geology_options *o, char *err,
                              size_t err_len) {
  const char *why = check_options(o);
  if (why) {
    snprintf(err, err_len, "%s", why);
    return NULL;
  }
  zb_geology *g = zb_calloc(a, sizeof *g);
  if (!g) {
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  g->a = a;
  g->seed = seed;
  g->opt = *o;
  g->n = o->points;
  g->dt = o->span_myr / (double)o->steps;
  g->final_start = (int)((double)o->steps * 0.75);
  g->rng = zb_rng_make(seed, 0x67656f6cull);
  g->best_score = 1e300;
  if (g->opt.macro_res == 0) {
    int res = (int)(o->radius * ZB_GEO_PI / 2.0 / 6000.0 + 0.5);
    g->opt.macro_res = res < 16 ? 16 : (res > 1024 ? 1024 : res);
  }
  size_t n = (size_t)g->n;
  bool ok = zb_lattice_init(&g->lat, a, g->n) == ZB_OK && zb_geo_crust_alloc(a, &g->cur, g->n) == ZB_OK &&
            zb_geo_crust_alloc(a, &g->next, g->n) == ZB_OK && snapshot_alloc(g, &g->start) == ZB_OK &&
            snapshot_alloc(g, &g->best) == ZB_OK;
  if (ok) {
    g->elevation = zb_alloc(a, sizeof(float) * n);
    g->minerals = zb_calloc(a, sizeof(uint32_t) * n);
    g->scratch_a = zb_alloc(a, sizeof(int32_t) * n);
    g->scratch_b = zb_alloc(a, sizeof(int32_t) * n);
    g->scratch_f = zb_alloc(a, sizeof(float) * n);
    g->hist = zb_alloc(a, sizeof(double) * ZB_GEO_HIST_BINS);
    g->collisions = zb_alloc(a, sizeof(uint16_t) * ZB_GEO_MAX_PLATES * ZB_GEO_MAX_PLATES);
    g->eras = zb_calloc(a, sizeof(zb_geology_era) * (size_t)o->steps);
    g->best_eras = zb_calloc(a, sizeof(zb_geology_era) * (size_t)o->steps);
    ok = g->elevation && g->minerals && g->scratch_a && g->scratch_b && g->scratch_f && g->hist && g->collisions &&
         g->eras && g->best_eras;
  }
  if (!ok) {
    zb_geology_destroy(g);
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  zb_geo_init_world(g);
  g->phase = ZB_GEO_PHASE_TECTONICS;
  return g;
}

void zb_geology_destroy(zb_geology *g) {
  if (!g) {
    return;
  }
  zb_geo_macro_free(g);
  zb_lattice_free(&g->lat);
  zb_geo_crust_free(g->a, &g->cur);
  zb_geo_crust_free(g->a, &g->next);
  snapshot_free(g, &g->start);
  snapshot_free(g, &g->best);
  zb_free(g->a, g->elevation);
  zb_free(g->a, g->minerals);
  zb_free(g->a, g->scratch_a);
  zb_free(g->a, g->scratch_b);
  zb_free(g->a, g->scratch_f);
  zb_free(g->a, g->hist);
  zb_free(g->a, g->collisions);
  zb_free(g->a, g->eras);
  zb_free(g->a, g->best_eras);
  zb_free(g->a, g->ley_nodes);
  zb_free(g->a, g->ley_edges);
  zb_free(g->a, g->ley_path);
  zb_free(g->a, g);
}

/* ---- The driver --------------------------------------------------------------------------- */

/* Makes the present sea level the datum (elevations become heights above sea level). */
static void fit_present_sea_level(zb_geology *g) {
  float sea = zb_geo_level_for_fraction(g->hist, g->elevation, NULL, (size_t)g->n, g->opt.land_fraction);
  for (int i = 0; i < g->n; ++i) {
    g->elevation[i] -= sea;
  }
  for (int s = 0; s < g->opt.steps; ++s) {
    g->eras[s].sea_level -= sea;
  }
  g->sea_level = 0.0f;
}

static void accept_lattice(zb_geology *g) {
  for (int i = 0; i < g->n; ++i) {
    if (g->cur.flags[i] & ZB_GEO_ACTIVE_VOLCANO && g->elevation[i] < -2000.0f) {
      g->cur.flags[i] &= (uint16_t)~ZB_GEO_ACTIVE_VOLCANO; /* deep seamounts are no volcanoes to visit */
    }
  }
  zb_geo_build_ley(g);
  zb_geo_compute_minerals(g);
  g->phase = ZB_GEO_PHASE_MACRO;
}

static void end_of_tectonics(zb_geology *g) {
  zb_geo_update_elevation(g);
  fit_present_sea_level(g);
  zb_geo_compute_metrics(g, &g->metrics);
  g->metrics.attempts = g->attempt + 1;
  if (g->metrics.passed || g->attempt >= g->opt.max_retries) {
    if (!g->metrics.passed && g->best.valid && g->best_score < g->metrics.score) {
      snapshot_restore(g, &g->best); /* an earlier attempt came closer */
      memcpy(g->eras, g->best_eras, sizeof(zb_geology_era) * (size_t)g->opt.steps);
      zb_geo_compute_metrics(g, &g->metrics);
      g->metrics.attempts = g->attempt + 1;
    }
    accept_lattice(g);
    return;
  }
  if (g->metrics.score < g->best_score) {
    g->best_score = g->metrics.score;
    snapshot_save(g, &g->best);
    memcpy(g->best_eras, g->eras, sizeof(zb_geology_era) * (size_t)g->opt.steps);
  }
  /* Steer the next attempt: more rifting for too few continents, less for too many. */
  if (g->metrics.continents < 3 || g->metrics.largest_landmass > 0.6 * g->metrics.land_fraction) {
    g->rift_chance = (g->rift_chance > 0.0 ? g->rift_chance : 0.01) * 3.0;
    g->rift_share *= 0.6;
  } else if (g->metrics.continents > 8) {
    g->rift_chance /= 3.0;
    g->rift_share = g->rift_share < 0.5 ? g->rift_share * 1.5 : g->rift_share;
  }
  g->attempt++;
  snapshot_restore(g, &g->start);
  g->rng = zb_rng_make(zb_hash_combine(g->seed, (uint64_t)g->attempt), 0x7265747279ull);
}

zb_geology_phase zb_geology_advance(zb_geology *g) {
  switch (g->phase) {
    case ZB_GEO_PHASE_TECTONICS:
      if (g->step == g->final_start && !g->start.valid) {
        snapshot_save(g, &g->start);
      }
      zb_geo_step(g);
      if (g->step >= g->opt.steps) {
        end_of_tectonics(g);
      }
      break;
    case ZB_GEO_PHASE_MACRO:
      if (!g->macro.height && zb_geo_macro_alloc(g) != ZB_OK) {
        g->phase = ZB_GEO_PHASE_DONE; /* out of memory: the lattice stays usable */
        break;
      }
      if (zb_geo_macro_amplify(g, g->opt.macro_res)) {
        g->phase = ZB_GEO_PHASE_EROSION;
      }
      break;
    case ZB_GEO_PHASE_EROSION:
      if (g->erosion_done < g->opt.erosion_iterations) {
        zb_geo_macro_erode(g);
        g->erosion_done++;
      }
      if (g->erosion_done >= g->opt.erosion_iterations) {
        g->phase = ZB_GEO_PHASE_FINISH;
      }
      break;
    case ZB_GEO_PHASE_FINISH:
      zb_geo_macro_route(g);
      g->phase = ZB_GEO_PHASE_DONE;
      break;
    case ZB_GEO_PHASE_DONE: break;
  }
  return g->phase;
}

void zb_geology_run(zb_geology *g) {
  while (zb_geology_advance(g) != ZB_GEO_PHASE_DONE) {
  }
}

void zb_geology_get_progress(const zb_geology *g, zb_geology_progress *out) {
  memset(out, 0, sizeof *out);
  out->phase = g->phase;
  out->step = g->step;
  out->steps = g->opt.steps;
  out->attempt = g->attempt;
  out->erosion = g->erosion_done;
  out->erosion_iterations = g->opt.erosion_iterations;
  out->myr_ago = now_myr(g);
  /* Weights: tectonics 70 %, amplification 10 %, erosion 20 %. */
  double rows = 6.0 * (double)g->opt.macro_res;
  double tect = (double)g->step / (double)g->opt.steps;
  double amp = rows > 0.0 ? (double)g->macro_rows_done / rows : 0.0;
  double ero = g->opt.erosion_iterations > 0 ? (double)g->erosion_done / (double)g->opt.erosion_iterations : 1.0;
  switch (g->phase) {
    case ZB_GEO_PHASE_TECTONICS: out->fraction = 0.7 * tect; break;
    case ZB_GEO_PHASE_MACRO: out->fraction = 0.7 + 0.1 * amp; break;
    case ZB_GEO_PHASE_EROSION:
    case ZB_GEO_PHASE_FINISH: out->fraction = 0.8 + 0.2 * ero; break;
    case ZB_GEO_PHASE_DONE: out->fraction = 1.0; break;
  }
}

/* ---- Accessors ----------------------------------------------------------------------------- */

const zb_lattice *zb_geology_lattice(const zb_geology *g) { return &g->lat; }

void zb_geology_get_points(const zb_geology *g, zb_geology_points *out) {
  out->n = g->n;
  out->plate = g->cur.plate;
  out->continental = g->cur.continental;
  out->thickness = g->cur.thickness;
  out->born = g->cur.born;
  out->elevation = g->elevation;
  out->orogeny = g->cur.orogeny;
  out->flags = g->cur.flags;
  out->minerals = g->minerals;
  out->strata = g->cur.strata;
  out->basement = g->cur.basement;
}

const zb_geology_era *zb_geology_eras(const zb_geology *g, int *count) {
  *count = g->step;
  return g->eras;
}

const zb_geology_metrics *zb_geology_get_metrics(const zb_geology *g) { return &g->metrics; }

const zb_geology_macro *zb_geology_get_macro(const zb_geology *g) {
  return g->phase == ZB_GEO_PHASE_DONE && g->macro.height ? &g->macro : NULL;
}

void zb_geology_get_ley(const zb_geology *g, zb_ley_network *out) {
  out->nodes = g->ley_node_count;
  out->edges = g->ley_edge_count;
  out->node = g->ley_nodes;
  out->edge = g->ley_edges;
  out->path = g->ley_path;
}

int zb_geology_point_at(const zb_geology *g, zb_dvec3 dir) { return zb_lattice_nearest(&g->lat, dir, -1); }

uint64_t zb_geology_hash(const zb_geology *g) {
  zb_hasher h;
  zb_hasher_init(&h, 0x67656f6c6f6779ull);
  size_t n = (size_t)g->n;
  zb_hasher_bytes(&h, g->cur.plate, n);
  zb_hasher_bytes(&h, g->cur.continental, n);
  zb_hasher_bytes(&h, g->cur.thickness, sizeof(float) * n);
  zb_hasher_bytes(&h, g->elevation, sizeof(float) * n);
  zb_hasher_bytes(&h, g->cur.flags, sizeof(uint16_t) * n);
  zb_hasher_bytes(&h, g->cur.strata, sizeof(zb_stratum) * n * ZB_STRATA);
  zb_hasher_bytes(&h, g->minerals, sizeof(uint32_t) * n);
  if (g->macro.height && g->phase == ZB_GEO_PHASE_DONE) {
    size_t cells = (size_t)6 * (size_t)g->macro.res * (size_t)g->macro.res;
    zb_hasher_bytes(&h, g->macro.height, sizeof(float) * cells);
    zb_hasher_bytes(&h, g->macro.drainage, sizeof(float) * cells);
  }
  for (int e = 0; e < g->ley_edge_count; ++e) {
    zb_hasher_u64(&h, (uint64_t)g->ley_edges[e].a << 32 | (uint64_t)(uint32_t)g->ley_edges[e].b);
  }
  return zb_hasher_final(&h);
}

/* ---- Names and fossils ------------------------------------------------------------------- */

const char *zb_rock_name(zb_rock r) {
  static const char *const names[ZB_ROCK_COUNT] = {
      "none",     "granite", "gneiss",    "schist",        "marble",       "quartzite",   "basalt",
      "andesite", "gabbro",  "sandstone", "red sandstone", "conglomerate", "shale",       "limestone",
      "chalk",    "coal",    "rock salt", "gypsum",        "glacial till", "pelagic clay"};
  return (int)r >= 0 && r < ZB_ROCK_COUNT ? names[r] : "?";
}

const char *zb_rock_env_name(zb_rock_env e) {
  static const char *const names[ZB_ENV_COUNT] = {"unknown", "deep sea",   "shallow sea", "river",   "swamp",
                                                  "desert",  "salt basin", "volcanic",    "glacial", "mountain root"};
  return (int)e >= 0 && e < ZB_ENV_COUNT ? names[e] : "?";
}

const char *zb_fossil_name(zb_fossil f) {
  static const char *const names[ZB_FOSSIL_COUNT] = {"none",  "sea creatures", "giant beasts",
                                                     "amber", "plants",        "mana fossils"};
  return (int)f >= 0 && f < ZB_FOSSIL_COUNT ? names[f] : "?";
}

const char *zb_mineral_name(uint32_t bit) {
  static const char *const names[ZB_MIN_COUNT] = {
      "copper",  "gold",    "silver",       "tin",       "iron",      "salt",         "gypsum", "sulfur",   "coal",
      "naphtha", "bitumen", "lapis lazuli", "turquoise", "carnelian", "mana crystal", "clay",   "limestone"};
  for (int k = 0; k < ZB_MIN_COUNT; ++k) {
    if (bit == (1u << k)) {
      return names[k];
    }
  }
  return "?";
}

const char *zb_geology_target_name(zb_geology_target t) {
  static const char *const names[ZB_GEO_TARGET_COUNT] = {"earth", "pangaea", "archipelago", "ice world", "hothouse"};
  return (int)t >= 0 && t < ZB_GEO_TARGET_COUNT ? names[t] : "?";
}

zb_fossil zb_stratum_fossil(const zb_stratum *s, uint64_t salt) {
  if (s->thickness == 0) {
    return ZB_FOSSIL_NONE;
  }
  double roll = (double)(zb_mix64(salt ^ ((uint64_t)s->rock << 16 | s->age)) >> 11) * (1.0 / 9007199254740992.0);
  bool sediment = false;
  zb_fossil f = ZB_FOSSIL_NONE;
  switch (s->rock) {
    case ZB_ROCK_LIMESTONE:
    case ZB_ROCK_CHALK:
    case ZB_ROCK_SHALE:
      sediment = true;
      f = s->env == ZB_ENV_SHALLOW_SEA || s->env == ZB_ENV_DEEP_SEA ? ZB_FOSSIL_SEA_CREATURES : ZB_FOSSIL_NONE;
      break;
    case ZB_ROCK_SANDSTONE:
    case ZB_ROCK_RED_SANDSTONE:
    case ZB_ROCK_CONGLOMERATE:
      sediment = true;
      f = s->age >= 66 && s->age <= 250 ? ZB_FOSSIL_GIANT_BEASTS : ZB_FOSSIL_NONE;
      break;
    case ZB_ROCK_COAL:
      sediment = true;
      f = s->age <= 150 ? ZB_FOSSIL_AMBER : ZB_FOSSIL_PLANTS;
      break;
    default: break;
  }
  /* The Age of Mana: sediments older than 300 Myr sometimes keep a glow. */
  if (sediment && s->age > 300 && roll < 0.35) {
    return ZB_FOSSIL_MANA;
  }
  return f;
}

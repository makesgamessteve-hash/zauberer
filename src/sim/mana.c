/* SPDX-License-Identifier: MIT */
#include "sim/mana.h"

#include "core/dmath.h"
#include "core/hash.h"
#include "sim/active.h"
#include "world/delta.h"
#include "world/dvec.h"
#include "world/ley.h"
#include "world/stream.h"

#include <math.h>
#include <string.h>

#define N ZB_MANA_RES
#define CELLS (N * N)
#define COLORS ZB_MANA_COLORS
#define REFRESH_STEPS 4       /* a chunk looks at the world again every 2 s */
#define DIFFUSE (1.0 / 600.0) /* per second, between neighbouring cells */
#define LEY_FLOW (1.0 / 20.0) /* per second, between cells on a ley line */
#define STRAIN_MEMORY 300.0   /* seconds over which drainings count towards blight */
#define FRESH_BURN (2.0 * 86400.0)
#define OLD_BURN (90.0 * 86400.0)

typedef struct mana_cell {
  float stored[COLORS], cap[COLORS];
  float base[COLORS]; /* richness from the ground, plants, climate and ley lines */
  float dyn[COLORS];  /* richness from fire, water, wind, weather and daylight */
  float strain;       /* times drained empty lately (fades over five minutes) */
  float ley, nexus;   /* metres */
  uint8_t ley_colors[2];
  bool blighted;
} mana_cell;

typedef struct mana_chunk {
  zb_chunk chunk;
  mana_cell cell[CELLS];
  float mean_height; /* for the sky */
} mana_chunk;

struct zb_mana {
  const zb_allocator *a;
  zb_active grid;
  zb_mana_sources src;
  zb_weather weather;
  double since_survey, pending, drawn;
  uint64_t steps;
  float delta[ZB_ACTIVE_MAX * CELLS * COLORS]; /* diffusion scratch */
};

static mana_chunk *MC(const zb_mana *m, int s) { return m->grid.slot[s]->data; }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ---- Richness from the ground (when a chunk comes in) ---- */

typedef struct ground_mix {
  double share[ZB_MAT_COUNT];
  double height, slope;
  double trees, shrubs, cover;
} ground_mix;

static void ground_of(const zb_chunk_terrain *ct, int cx, int cy, ground_mix *g) {
  memset(g, 0, sizeof *g);
  const int span = ZB_SURFACE_RES / N; /* 32 one-metre cells */
  for (int y = 0; ct->material && y < span; ++y) {
    for (int x = 0; x < span; ++x) {
      int m = ct->material[(cy * span + y) * ZB_SURFACE_RES + cx * span + x];
      g->share[m < ZB_MAT_COUNT ? m : 0] += 1.0 / (span * span);
    }
  }
  if (ct->height) {
    double lo = 1e30, hi = -1e30, sum = 0.0;
    for (int y = 0; y <= span; y += 4) {
      for (int x = 0; x <= span; x += 4) {
        double h = ct->height[(cy * span + y) * ZB_HEIGHT_RES + cx * span + x];
        lo = h < lo ? h : lo;
        hi = h > hi ? h : hi;
        sum += h;
      }
    }
    g->height = sum / 81.0;
    g->slope = (hi - lo) / span;
  }
  for (uint32_t k = 0; ct->plants && k < ct->plant_count; ++k) {
    const zb_plant *p = &ct->plants[k];
    if ((int)(p->x * N / 65536u) != cx || (int)(p->y * N / 65536u) != cy) {
      continue;
    }
    g->trees += p->layer == ZB_PLANT_TREE ? 1.0 : 0.0;
    g->shrubs += p->layer == ZB_PLANT_SHRUB ? 1.0 : 0.0;
    g->cover += p->layer == ZB_PLANT_GROUND ? 1.0 : 0.0;
  }
}

static void base_richness(zb_mana *m, mana_chunk *c, const zb_chunk_terrain *ct) {
  zb_world *w = m->grid.world;
  double mean, coldest, warmest, rain;
  zb_terrain_chunk_climate(zb_world_terrain(w), c->chunk, &mean, &coldest, &warmest, &rain);
  double warm = clampd((mean - 12.0) / 15.0, 0.0, 1.2), wet = clampd(rain / 2000.0, 0.0, 1.0);
  const zb_ley *ley = zb_world_ley(w);
  const zb_planet *p = zb_world_planet(w);
  double sum_h = 0.0;
  for (int cy = 0; cy < N; ++cy) {
    for (int cx = 0; cx < N; ++cx) {
      mana_cell *cell = &c->cell[cy * N + cx];
      ground_mix g;
      ground_of(ct, cx, cy, &g);
      sum_h += g.height;
      zb_planet_pos at = {c->chunk, (float)((cx + 0.5) * 256.0 / N), (float)((cy + 0.5) * 256.0 / N), 0.0f};
      zb_ley_sample ls;
      zb_ley_sample_at(ley, zb_dv_norm(zb_planet_to_cart(p, &at)), &ls);
      cell->ley = (float)ls.line_distance;
      cell->nexus = (float)ls.nexus_distance;
      int la, lb;
      zb_ley_line_colors(ley, ls.line, &la, &lb);
      cell->ley_colors[0] = (uint8_t)la;
      cell->ley_colors[1] = (uint8_t)lb;
      double snow = g.share[ZB_MAT_SNOW] + g.share[ZB_MAT_ICE], biomass = g.trees + 0.3 * g.shrubs + 0.05 * g.cover;
      double flow = ley->strength * exp(-ls.line_distance / ZB_MANA_LEY_REACH);
      double node = ley->strength * ley->nexus[ls.nexus].power * exp(-ls.nexus_distance / 300.0);
      float *b = cell->base;
      b[ZB_MANA_FIRE] = (float)(warm * 0.6 + g.share[ZB_MAT_SAND] * warm * 0.4 + g.share[ZB_MAT_ASH] * 0.5);
      b[ZB_MANA_STORM] = (float)(0.1 + clampd(g.height / 1500.0, 0.0, 1.5) + clampd(g.slope, 0.0, 1.0) * 0.4);
      b[ZB_MANA_LIGHT] = (float)(0.1 + snow * 1.2 + g.share[ZB_MAT_SAND] * 0.2);
      b[ZB_MANA_LIFE] = (float)(clampd(biomass / 12.0, 0.0, 3.0) + g.share[ZB_MAT_SOIL] * 0.2);
      b[ZB_MANA_WATER] = (float)(wet * 0.6 + g.share[ZB_MAT_MUD] * 0.6 + snow * 0.4);
      b[ZB_MANA_EARTH] = (float)(g.share[ZB_MAT_ROCK] + g.share[ZB_MAT_GRAVEL] * 0.6 + g.share[ZB_MAT_SAND] * 0.3 +
                                 clampd(g.height / 2000.0, 0.0, 1.0) * 0.8 + clampd(g.slope, 0.0, 1.0) * 0.5);
      b[ZB_MANA_VOID] = (float)(1.5 * flow + 2.0 * node);
      b[ZB_MANA_DECAY] = (float)(0.05 + g.share[ZB_MAT_MUD] * 0.8 + g.share[ZB_MAT_ASH] * 0.6);
      b[la] += (float)flow;
      b[lb] += (float)flow;
      int dom = ley->nexus[ls.nexus].colors[0], sub = ley->nexus[ls.nexus].colors[1];
      b[dom] += (float)(2.0 * node);
      b[sub] += (float)node;
    }
  }
  c->mean_height = (float)(sum_h / CELLS);
}

/* ---- Richness from what happens (every few seconds) ---- */

static void burns_and_blight(const zb_mana *m, mana_chunk *c, double fresh[CELLS], double old[CELLS]) {
  size_t n;
  const zb_edit *e = zb_deltas_get(zb_world_deltas(m->grid.world), c->chunk, &n);
  double now = zb_world_time(m->grid.world);
  const int per = ZB_BURN_RES / N;
  for (size_t k = 0; k < n; ++k) {
    double age = now - e[k].index * 60.0;
    if (e[k].kind == ZB_EDIT_BURNT && e[k].x < ZB_BURN_RES && e[k].y < ZB_BURN_RES) {
      int i = (e[k].y / per) * N + e[k].x / per;
      if (age < FRESH_BURN) {
        fresh[i] += 1.0 / (per * per);
      } else if (age < OLD_BURN) {
        old[i] += 1.0 / (per * per);
      }
    } else if (e[k].kind == ZB_EDIT_BLIGHT && e[k].x < N && e[k].y < N && age < ZB_BLIGHT_SECONDS) {
      c->cell[e[k].y * N + e[k].x].blighted = true;
    }
  }
}

static void dynamic_richness(zb_mana *m, mana_chunk *c) {
  zb_world *w = m->grid.world;
  const zb_planet *p = zb_world_planet(w);
  zb_planet_pos mid = {c->chunk, 128.0f, 128.0f, c->mean_height};
  zb_sky_place sky;
  zb_calendar_sky(zb_world_calendar(w), zb_world_time(w), zb_dv_norm(zb_planet_to_cart(p, &mid)), &sky);
  const zb_weather *wt = &m->weather;
  double fresh[CELLS] = {0}, old[CELLS] = {0};
  for (int i = 0; i < CELLS; ++i) {
    c->cell[i].blighted = false;
  }
  burns_and_blight(m, c, fresh, old);
  double mean, coldest, warmest, rain;
  zb_terrain_chunk_climate(zb_world_terrain(w), c->chunk, &mean, &coldest, &warmest, &rain);
  double warm = clampd((wt->temperature - 15.0) / 15.0, 0.0, 1.5), dry = clampd(1.0 - rain / 800.0, 0.0, 1.0);
  for (int cy = 0; cy < N; ++cy) {
    for (int cx = 0; cx < N; ++cx) {
      mana_cell *cell = &c->cell[cy * N + cx];
      double burning = 0.0, wet = 0.0;
      for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
          zb_planet_pos at = {c->chunk, (float)((cx + (sx + 0.5) / 4.0) * 256.0 / N),
                              (float)((cy + (sy + 0.5) / 4.0) * 256.0 / N), 0.0f};
          if (m->src.fire) {
            zb_fire_cell fc;
            zb_fire_cell_at(m->src.fire, &at, &fc);
            burning += fc.burning ? 1.0 / 16.0 : 0.0;
          }
          if (m->src.water) {
            zb_water_cell wc;
            zb_surface_water_cell_at(m->src.water, &at, &wc);
            wet += wc.fixed || wc.depth > 0.05 ? 1.0 / 16.0 : 0.0;
          }
        }
      }
      double wind = sqrt(wt->wind_east * wt->wind_east + wt->wind_north * wt->wind_north);
      if (m->src.air) {
        zb_air_sample as;
        zb_planet_pos at = {c->chunk, (float)((cx + 0.5) * 256.0 / N), (float)((cy + 0.5) * 256.0 / N), 0.0f};
        zb_air_sample_at(m->src.air, &at, &as);
        wind = as.loaded ? sqrt(as.east * as.east + as.north * as.north) : wind;
      }
      float *d = cell->dyn;
      d[ZB_MANA_FIRE] =
          (float)(3.0 * burning + 1.5 * fresh[cy * N + cx] + sky.daylight * warm * dry * (1.0 - wt->cloud) * 0.6);
      d[ZB_MANA_STORM] = (float)(wind / 8.0 + 2.0 * wt->storm);
      d[ZB_MANA_LIGHT] = (float)(sky.daylight * (1.0 - 0.7 * wt->cloud) +
                                 (1.0 - sky.daylight) * sky.moon_light * 0.3 * (1.0 - wt->cloud));
      d[ZB_MANA_LIFE] = 0.0f;
      d[ZB_MANA_WATER] = (float)(2.0 * wet + (wt->rain + wt->snow) / 20.0);
      d[ZB_MANA_EARTH] = 0.0f;
      d[ZB_MANA_VOID] = (float)((1.0 - sky.daylight) * 0.6);
      d[ZB_MANA_DECAY] = (float)(1.2 * old[cy * N + cx]);
    }
  }
}

static void set_caps(mana_cell *cell) {
  for (int k = 0; k < COLORS; ++k) {
    double r = cell->base[k] + cell->dyn[k];
    if (cell->blighted) {
      r = k == ZB_MANA_LIFE ? r * 0.25 : (k == ZB_MANA_DECAY ? r + 2.0 : r);
    }
    cell->cap[k] = (float)(ZB_MANA_CAP * (r > 0.0 ? r : 0.0));
  }
}

static void *take_in(void *ctx, const zb_active_chunk *ac, const zb_chunk_terrain *ct) {
  zb_mana *m = ctx;
  mana_chunk *c = zb_calloc(m->a, sizeof *c);
  if (!c) {
    return NULL;
  }
  c->chunk = ac->chunk;
  base_richness(m, c, ct);
  dynamic_richness(m, c);
  for (int i = 0; i < CELLS; ++i) {
    set_caps(&c->cell[i]);
    memcpy(c->cell[i].stored, c->cell[i].cap, sizeof c->cell[i].stored); /* found full */
  }
  return c;
}

static void let_go(void *ctx, void *data) {
  zb_mana *m = ctx;
  zb_free(m->a, data);
}

void zb_mana_set_radius(zb_mana *m, double radius) { m->grid.radius = radius; }

zb_mana *zb_mana_create(const zb_allocator *a, zb_world *world, double radius) {
  zb_mana *m = zb_calloc(a, sizeof *m);
  if (!m) {
    return NULL;
  }
  m->a = a;
  m->weather.cloud = 0.3;
  m->weather.temperature = 15.0;
  zb_active_hooks hooks = {m, take_in, let_go};
  zb_active_init(&m->grid, a, world, radius, ZB_STAGE_ECOLOGY, N, &hooks);
  return m;
}

void zb_mana_destroy(zb_mana *m) {
  if (m) {
    zb_active_clear(&m->grid, NULL);
    zb_free(m->a, m);
  }
}

void zb_mana_set_world(zb_mana *m, zb_world *world) { zb_active_clear(&m->grid, world); }
void zb_mana_set_sources(zb_mana *m, const zb_mana_sources *s) { m->src = *s; }
void zb_mana_set_weather(zb_mana *m, const zb_weather *w) { m->weather = *w; }

void zb_mana_ground_changed(zb_mana *m, zb_chunk c) {
  int s = zb_active_slot(&m->grid, c);
  const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(m->grid.world), c);
  if (s >= 0 && ct) {
    mana_chunk *mc = MC(m, s);
    base_richness(m, mc, ct);
    for (int i = 0; i < CELLS; ++i) {
      set_caps(&mc->cell[i]);
    }
  }
}

/* ---- Stepping ---- */

static void spread(zb_mana *m, double dt) {
  float *delta = m->delta;
  memset(delta, 0, sizeof(float) * (size_t)m->grid.count * CELLS * COLORS);
  for (int s = 0; s < m->grid.count; ++s) {
    mana_chunk *c = MC(m, s);
    for (int y = 0; y < N; ++y) {
      for (int x = 0; x < N; ++x) {
        int32_t here = s * CELLS + y * N + x;
        const mana_cell *a = &c->cell[y * N + x];
        /* Each pair once: to the east and to the north. */
        for (int k = 0; k < 2; ++k) {
          int32_t there = zb_active_locate(&m->grid, s, x + (k == 0), y + (k == 1));
          if (there < 0) {
            continue;
          }
          const mana_cell *b = &MC(m, there / CELLS)->cell[there % CELLS];
          bool ley = a->ley < ZB_MANA_LEY_REACH * 0.5 && b->ley < ZB_MANA_LEY_REACH * 0.5;
          double rate = (ley ? LEY_FLOW : DIFFUSE) * dt;
          for (int col = 0; col < COLORS; ++col) {
            float f = (float)(rate * (b->stored[col] - a->stored[col]));
            delta[here * COLORS + col] += f;
            delta[there * COLORS + col] -= f;
          }
        }
      }
    }
  }
  for (int s = 0; s < m->grid.count; ++s) {
    mana_chunk *c = MC(m, s);
    for (int i = 0; i < CELLS; ++i) {
      for (int col = 0; col < COLORS; ++col) {
        c->cell[i].stored[col] += delta[(s * CELLS + i) * COLORS + col];
      }
    }
  }
}

static void step(zb_mana *m) {
  double k = ZB_MANA_STEP / ZB_MANA_REFILL, fade = 1.0 - ZB_MANA_STEP / STRAIN_MEMORY;
  for (int s = 0; s < m->grid.count; ++s) {
    mana_chunk *c = MC(m, s);
    bool refresh = (m->steps + (uint64_t)s) % REFRESH_STEPS == 0;
    if (refresh) {
      dynamic_richness(m, c);
    }
    for (int i = 0; i < CELLS; ++i) {
      mana_cell *cell = &c->cell[i];
      if (refresh) {
        set_caps(cell);
      }
      for (int col = 0; col < COLORS; ++col) {
        cell->stored[col] += (float)((cell->cap[col] - cell->stored[col]) * k);
      }
      cell->strain *= (float)fade;
    }
  }
  spread(m, ZB_MANA_STEP);
  m->steps++;
}

void zb_mana_update(zb_mana *m, const zb_planet_pos *focus, int focus_count, double seconds) {
  m->since_survey += seconds;
  if (m->since_survey >= 1.0 || m->grid.count == 0) {
    m->since_survey = 0.0;
    zb_active_survey(&m->grid, focus, focus_count);
  }
  m->pending += seconds;
  for (int k = 0; k < 4 && m->pending >= ZB_MANA_STEP; ++k) {
    m->pending -= ZB_MANA_STEP;
    step(m);
  }
  if (m->pending > ZB_MANA_STEP) {
    m->pending = ZB_MANA_STEP;
  }
}

/* ---- Outside access ---- */

static mana_cell *cell_at(const zb_mana *m, const zb_planet_pos *pos, int32_t *out) {
  int32_t cell = zb_active_cell_at(&m->grid, pos);
  if (out) {
    *out = cell;
  }
  return cell < 0 ? NULL : &MC(m, cell / CELLS)->cell[cell % CELLS];
}

void zb_mana_sample_at(const zb_mana *m, const zb_planet_pos *pos, zb_mana_sample *out) {
  memset(out, 0, sizeof *out);
  const mana_cell *c = cell_at(m, pos, NULL);
  if (!c) {
    return;
  }
  out->loaded = true;
  for (int k = 0; k < COLORS; ++k) {
    out->stored[k] = c->stored[k];
    out->cap[k] = c->cap[k];
  }
  out->ley_distance = c->ley;
  out->nexus_distance = c->nexus;
  out->blighted = c->blighted;
}

typedef struct gather {
  const zb_mana *m;
  int32_t cells[512];
  int n;
} gather;

static bool gather_fn(void *ctx, int32_t cell) {
  gather *g = ctx;
  if (g->n < 512) {
    g->cells[g->n++] = cell;
    return true;
  }
  return false;
}

static void blight(zb_mana *m, int32_t cell) {
  mana_chunk *c = MC(m, cell / CELLS);
  int i = cell % CELLS;
  if (!c->cell[i].blighted) {
    c->cell[i].blighted = true;
    zb_world_blight(m->grid.world, c->chunk, i % N, i / N, zb_world_time(m->grid.world));
    set_caps(&c->cell[i]);
  }
}

double zb_mana_draw(zb_mana *m, const zb_planet_pos *pos, zb_mana_color color, double amount, double radius) {
  if (color < 0 || color >= COLORS || !(amount > 0.0)) {
    return 0.0;
  }
  gather g;
  g.m = m;
  g.n = 0;
  int32_t centre;
  if (!cell_at(m, pos, &centre)) {
    return 0.0;
  }
  if (radius > 0.5 * 256.0 / N) {
    zb_active_around(&m->grid, pos, radius, gather_fn, &g);
  }
  if (g.n == 0) {
    g.cells[g.n++] = centre;
  }
  double avail = 0.0;
  for (int k = 0; k < g.n; ++k) {
    avail += MC(m, g.cells[k] / CELLS)->cell[g.cells[k] % CELLS].stored[color];
  }
  double take = amount < avail ? amount : avail;
  if (take <= 0.0) {
    return 0.0;
  }
  for (int k = 0; k < g.n; ++k) {
    mana_cell *c = &MC(m, g.cells[k] / CELLS)->cell[g.cells[k] % CELLS];
    double share = take * c->stored[color] / avail;
    c->stored[color] -= (float)share;
    if (share > 0.0 && c->stored[color] < 0.02f * c->cap[color]) {
      c->strain += amount > 2.0 * avail ? 2.0f : 1.0f; /* drained, and asked for far more */
      if (c->strain >= ZB_MANA_BLIGHT_STRAIN) {
        blight(m, g.cells[k]);
      }
    }
  }
  m->drawn += take;
  return take;
}

void zb_mana_release(zb_mana *m, const zb_planet_pos *pos, zb_mana_color color, double amount) {
  mana_cell *c = cell_at(m, pos, NULL);
  if (c && color >= 0 && color < COLORS && amount > 0.0) {
    c->stored[color] += (float)amount;
  }
}

void zb_mana_get_stats(const zb_mana *m, zb_mana_stats *out) {
  memset(out, 0, sizeof *out);
  out->chunks = m->grid.count;
  out->drawn = m->drawn;
  for (int s = 0; s < m->grid.count; ++s) {
    const mana_chunk *c = MC(m, s);
    for (int i = 0; i < CELLS; ++i) {
      out->blighted += c->cell[i].blighted ? 1 : 0;
      for (int k = 0; k < COLORS; ++k) {
        out->stored[k] += c->cell[i].stored[k];
        out->cap[k] += c->cell[i].cap[k];
      }
    }
  }
}

uint64_t zb_mana_hash(const zb_mana *m) {
  uint64_t h = zb_hash64(&m->steps, sizeof m->steps, 0x6d616e61);
  for (int s = 0; s < m->grid.count; ++s) {
    const mana_chunk *c = MC(m, s);
    h = zb_hash64(&c->chunk, sizeof c->chunk, h);
    for (int i = 0; i < CELLS; ++i) {
      h = zb_hash64(c->cell[i].stored, sizeof c->cell[i].stored, h);
    }
  }
  return h;
}

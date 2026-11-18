/* SPDX-License-Identifier: MIT */
#include "sim/water.h"

#include <math.h>
#include <string.h>

#include "core/hash.h"
#include "world/stream.h"

#define N ZB_ACTIVE_RES
#define CELLS ZB_ACTIVE_CELLS
#define GRAVITY 9.81
#define DAMPING 0.99   /* per step: bed friction, which lets sloshing settle */
#define STILL 1e-5     /* m^3/s: slower flows count as still */
#define STILL_STEPS 40 /* still steps before a chunk sleeps */
#define WET 0.01       /* metres: a wet cell */
#define RAIN_BATCH 1.0 /* seconds of rain given to sleeping chunks at once */

/* How fast water soaks into the ground (m/s) and how much the top of it holds
 * (metres of water), by surface material. */
static const struct {
  double rate, capacity;
} SOAK[ZB_MAT_COUNT] = {
    {0.0, 0.0},   /* none */
    {1e-5, 0.10}, /* soil: 36 mm/h */
    {1e-4, 0.08}, /* sand */
    {0.0, 0.0},   /* rock */
    {5e-5, 0.05}, /* gravel */
    {0.0, 0.0},   /* snow (frozen) */
    {0.0, 0.0},   /* ice */
    {1e-6, 0.02}, /* mud: nearly saturated */
    {2e-5, 0.08}, /* ash */
};

enum { FIXED = 1 };

typedef struct water_chunk {
  float *block;
  double *depth; /* metres of water: doubles, as small flows are below a float's resolution */
  float *ground, *soaked, *capacity, *rate, *level;
  float *out; /* 4 per cell: m^3/s leaving through x-, x+, y-, y+ */
  uint8_t *state;
  bool awake;
  int still;        /* steps without flow */
  uint32_t version; /* changes whenever the water moved */
  double area;      /* m^2 of a cell */
  double width[4];  /* metres of each side */
} water_chunk;

struct zb_surface_water {
  const zb_allocator *a;
  zb_active grid;
  double rain; /* mm/h */
  double pending, since_survey, rain_wait;
  uint64_t steps;
  double drained, rained, added, absorbed, left;
};

static water_chunk *WC(const zb_surface_water *w, int s) { return w->grid.slot[s]->data; }

/* ---------------------------------------------------------------- chunks */

static void *take_in(void *ctx, const zb_active_chunk *ac, const zb_chunk_terrain *ct) {
  zb_surface_water *w = ctx;
  water_chunk *c = zb_calloc(w->a, sizeof *c);
  float *block = c ? zb_calloc(w->a, sizeof(float) * CELLS * 10 + CELLS) : NULL;
  double *depth = c ? zb_calloc(w->a, sizeof(double) * CELLS) : NULL;
  uint8_t *kind = c ? zb_alloc(w->a, (N + 1) * (N + 1)) : NULL;
  float *surface = c ? zb_alloc(w->a, sizeof(float) * (N + 1) * (N + 1)) : NULL;
  if (!block || !depth || !kind || !surface ||
      zb_terrain_water_grid(zb_world_terrain(w->grid.world), w->a, ct, 256 / N, kind, surface) != ZB_OK) {
    zb_free(w->a, block);
    zb_free(w->a, depth);
    zb_free(w->a, kind);
    zb_free(w->a, surface);
    zb_free(w->a, c);
    return NULL;
  }
  c->block = block;
  c->depth = depth;
  c->ground = block;
  c->soaked = c->ground + CELLS;
  c->capacity = c->soaked + CELLS;
  c->rate = c->capacity + CELLS;
  c->level = c->rate + CELLS;
  c->out = c->level + CELLS;
  c->state = (uint8_t *)(c->out + 4 * CELLS);
  c->area = ac->cw * ac->ch;
  c->width[0] = c->width[1] = ac->ch;
  c->width[2] = c->width[3] = ac->cw;
  double mean, coldest, warmest, rain;
  zb_terrain_chunk_climate(zb_world_terrain(w->grid.world), ct->chunk, &mean, &coldest, &warmest, &rain);
  double wetness = rain / 2500.0;
  wetness = wetness < 0.1 ? 0.1 : (wetness > 0.9 ? 0.9 : wetness);
  for (int y = 0; y < N; ++y) {
    for (int x = 0; x < N; ++x) {
      int i = y * N + x;
      const float *h0 = &ct->height[(2 * y) * ZB_HEIGHT_RES + 2 * x], *h1 = h0 + ZB_HEIGHT_RES;
      c->ground[i] = 0.25f * (h0[0] + h0[1] + h1[0] + h1[1]);
      int m = ct->material ? ct->material[(2 * y) * ZB_SURFACE_RES + 2 * x] : ZB_MAT_ROCK;
      m = m < ZB_MAT_COUNT ? m : ZB_MAT_ROCK;
      c->rate[i] = (float)SOAK[m].rate;
      c->capacity[i] = (float)SOAK[m].capacity;
      c->soaked[i] = (float)(SOAK[m].capacity * wetness);
      /* A water body of the terrain where most corners are under it. */
      const uint8_t *k0 = &kind[y * (N + 1) + x], *k1 = k0 + N + 1;
      int wet = (k0[0] != 0) + (k0[1] != 0) + (k1[0] != 0) + (k1[1] != 0);
      if (wet >= 2) {
        const float *s0 = &surface[y * (N + 1) + x], *s1 = s0 + N + 1;
        float level = s0[0];
        level = s0[1] > level ? s0[1] : level;
        level = s1[0] > level ? s1[0] : level;
        level = s1[1] > level ? s1[1] : level;
        c->state[i] = FIXED;
        c->level[i] = level;
      }
    }
  }
  zb_free(w->a, kind);
  zb_free(w->a, surface);
  return c;
}

static void let_go(void *ctx, void *data) {
  zb_surface_water *w = ctx;
  water_chunk *c = data;
  for (int i = 0; i < CELLS; ++i) {
    w->left += c->depth[i] * c->area;
  }
  zb_free(w->a, c->block);
  zb_free(w->a, c->depth);
  zb_free(w->a, c);
}

zb_surface_water *zb_surface_water_create(const zb_allocator *a, zb_world *world, double radius) {
  zb_surface_water *w = zb_calloc(a, sizeof *w);
  if (!w) {
    return NULL;
  }
  w->a = a;
  zb_active_hooks hooks = {w, take_in, let_go};
  zb_active_init(&w->grid, a, world, radius, ZB_STAGE_SURFACE, &hooks);
  return w;
}

void zb_surface_water_destroy(zb_surface_water *w) {
  if (w) {
    zb_active_clear(&w->grid, NULL);
    zb_free(w->a, w);
  }
}

void zb_surface_water_set_world(zb_surface_water *w, zb_world *world) { zb_active_clear(&w->grid, world); }

void zb_surface_water_set_rain(zb_surface_water *w, double mm_per_hour) {
  w->rain = mm_per_hour > 0.0 ? mm_per_hour : 0.0;
}

/* ----------------------------------------------------------------- steps */

static void wake(water_chunk *c) {
  c->awake = true;
  c->still = 0;
  c->version++;
}

/* The water surface of a cell as its neighbours see it. */
static double surface_of(const water_chunk *c, int i) {
  return (c->state[i] & FIXED) ? c->level[i] : (double)c->ground[i] + c->depth[i];
}

/* Rain for seconds on a cell: it soaks in as fast as the ground takes it,
 * the rest stays on the ground; returns true when water is left standing. */
static bool rain_on(zb_surface_water *w, water_chunk *c, int i, double seconds) {
  if (c->state[i] & FIXED) {
    w->rained += w->rain / 3.6e6 * seconds * c->area;
    w->drained += w->rain / 3.6e6 * seconds * c->area;
    return false;
  }
  double fall = w->rain / 3.6e6 * seconds; /* metres */
  w->rained += fall * c->area;
  double room = (double)c->capacity[i] - c->soaked[i], take = c->rate[i] * seconds;
  take = take < room ? take : room;
  take = take < fall ? take : fall;
  take = take > 0.0 ? take : 0.0;
  c->soaked[i] = (float)(c->soaked[i] + take);
  w->absorbed += take * c->area;
  if (fall - take > 0.0) {
    c->depth[i] += fall - take;
    return true;
  }
  return false;
}

/* Outflows of a cell by the differences of the water surfaces, scaled so that
 * no more leaves than it holds. */
static double outflows(zb_surface_water *w, int s, int i) {
  water_chunk *c = WC(w, s);
  float *out = &c->out[4 * i];
  if (c->depth[i] <= 0.0) {
    out[0] = out[1] = out[2] = out[3] = 0.0f;
    return 0.0;
  }
  int x = i % N, y = i / N;
  bool inside = x > 0 && y > 0 && x < N - 1 && y < N - 1;
  double h = (double)c->ground[i] + c->depth[i], sum = 0.0, f[4];
  for (int k = 0; k < 4; ++k) {
    int32_t j = inside ? s * CELLS + i + ZB_ACTIVE_DY[k] * N + ZB_ACTIVE_DX[k]
                       : zb_active_locate(&w->grid, s, x + ZB_ACTIVE_DX[k], y + ZB_ACTIVE_DY[k]);
    f[k] = 0.0;
    if (j >= 0) {
      double hj = surface_of(zb_active_chunk_of(&w->grid, j)->data, j % CELLS);
      f[k] = out[k] * DAMPING + ZB_WATER_STEP * GRAVITY * (h - hj) * c->width[k];
      f[k] = f[k] > 0.0 ? f[k] : 0.0;
    }
    sum += f[k];
  }
  double volume = c->depth[i] * c->area;
  double scale = sum * ZB_WATER_STEP > volume ? volume / (sum * ZB_WATER_STEP) : 1.0;
  double most = 0.0;
  for (int k = 0; k < 4; ++k) {
    out[k] = (float)(f[k] * scale);
    most = out[k] > most ? out[k] : most;
  }
  return most;
}

/* Moves the water that leaves a cell into its neighbours (or the water body
 * it drains into). */
static void move(zb_surface_water *w, int s, int i) {
  water_chunk *c = WC(w, s);
  const float *out = &c->out[4 * i];
  if (out[0] == 0.0f && out[1] == 0.0f && out[2] == 0.0f && out[3] == 0.0f) {
    return;
  }
  int x = i % N, y = i / N;
  double gone = 0.0;
  for (int k = 0; k < 4; ++k) {
    if (out[k] == 0.0f) {
      continue;
    }
    double v = (double)out[k] * ZB_WATER_STEP;
    int32_t j = zb_active_locate(&w->grid, s, x + ZB_ACTIVE_DX[k], y + ZB_ACTIVE_DY[k]);
    water_chunk *t = zb_active_chunk_of(&w->grid, j)->data;
    int ti = j % CELLS;
    if (t->state[ti] & FIXED) {
      w->drained += v;
    } else {
      t->depth[ti] += v / t->area;
      if (!t->awake) {
        wake(t);
      }
    }
    gone += v;
  }
  double d = c->depth[i] - gone / c->area;
  c->depth[i] = d > 0.0 ? d : 0.0;
}

/* Water soaks into the ground under it. */
static void soak(zb_surface_water *w, water_chunk *c, int i) {
  double room = (double)c->capacity[i] - c->soaked[i];
  if (c->depth[i] <= 0.0 || room <= 0.0) {
    return;
  }
  double take = c->rate[i] * ZB_WATER_STEP;
  take = take < room ? take : room;
  take = take < c->depth[i] ? take : c->depth[i];
  c->depth[i] -= take;
  c->soaked[i] = (float)(c->soaked[i] + take);
  w->absorbed += take * c->area;
}

static void step(zb_surface_water *w) {
  int n = w->grid.count;
  double most[ZB_ACTIVE_MAX] = {0};
  for (int s = 0; s < n; ++s) {
    water_chunk *c = WC(w, s);
    for (int i = 0; c->awake && i < CELLS; ++i) {
      if (!(c->state[i] & FIXED)) {
        double m = outflows(w, s, i);
        most[s] = m > most[s] ? m : most[s];
      }
    }
  }
  for (int s = 0; s < n; ++s) {
    water_chunk *c = WC(w, s);
    if (!c->awake) {
      continue;
    }
    for (int i = 0; i < CELLS; ++i) {
      if (!(c->state[i] & FIXED)) {
        move(w, s, i);
      }
    }
  }
  for (int s = 0; s < n; ++s) {
    water_chunk *c = WC(w, s);
    if (!c->awake) {
      continue;
    }
    for (int i = 0; i < CELLS; ++i) {
      if (w->rain > 0.0) {
        rain_on(w, c, i, ZB_WATER_STEP);
      }
      soak(w, c, i);
    }
    c->still = most[s] < STILL ? c->still + 1 : 0;
    c->version++;
    if (c->still >= STILL_STEPS) {
      c->awake = false; /* still water keeps its depths while asleep */
      memset(c->out, 0, sizeof(float) * 4 * CELLS);
    }
  }
  w->steps++;
}

/* Sleeping chunks get their rain in batches; standing water wakes them. */
static void rain_on_sleepers(zb_surface_water *w, double seconds) {
  for (int s = 0; s < w->grid.count; ++s) {
    water_chunk *c = WC(w, s);
    bool standing = false;
    for (int i = 0; !c->awake && i < CELLS; ++i) {
      standing |= rain_on(w, c, i, seconds);
    }
    if (standing) {
      wake(c);
    }
  }
}

void zb_surface_water_update(zb_surface_water *w, const zb_planet_pos *focus, int focus_count, double seconds) {
  w->since_survey += seconds;
  if (w->since_survey >= 1.0 || w->grid.count == 0) {
    w->since_survey = 0.0;
    zb_active_survey(&w->grid, focus, focus_count);
  }
  w->pending += seconds;
  int steps = 0;
  while (w->pending >= ZB_WATER_STEP && steps < 4) {
    w->pending -= ZB_WATER_STEP;
    step(w);
    steps++;
  }
  if (w->pending > ZB_WATER_STEP) {
    w->pending = ZB_WATER_STEP; /* behind: drop time rather than spiral */
  }
  if (w->rain > 0.0) {
    w->rain_wait += seconds;
    if (w->rain_wait >= RAIN_BATCH) {
      rain_on_sleepers(w, w->rain_wait);
      w->rain_wait = 0.0;
    }
  }
}

/* ------------------------------------------------------- outside access */

typedef struct pour {
  zb_surface_water *w;
  double per_cell; /* m^3 */
} pour;

static bool pour_fn(void *ctx, int32_t cell) {
  pour *p = ctx;
  water_chunk *c = zb_active_chunk_of(&p->w->grid, cell)->data;
  int i = cell % CELLS;
  if (c->state[i] & FIXED) {
    return false;
  }
  if (p->per_cell > 0.0) {
    c->depth[i] += p->per_cell / c->area;
    p->w->added += p->per_cell;
    wake(c);
  }
  return true;
}

int zb_surface_water_add(zb_surface_water *w, const zb_planet_pos *pos, double radius, double cubic_metres) {
  pour count = {w, 0.0};
  int n = zb_active_around(&w->grid, pos, radius, pour_fn, &count);
  if (n > 0 && cubic_metres > 0.0) {
    pour p = {w, cubic_metres / n};
    zb_active_around(&w->grid, pos, radius, pour_fn, &p);
  }
  return n;
}

void zb_surface_water_cell_at(const zb_surface_water *w, const zb_planet_pos *pos, zb_water_cell *out) {
  memset(out, 0, sizeof *out);
  int32_t cell = zb_active_cell_at(&w->grid, pos);
  if (cell < 0) {
    return;
  }
  const water_chunk *c = zb_active_chunk_of(&w->grid, cell)->data;
  int i = cell % CELLS;
  out->loaded = true;
  out->fixed = (c->state[i] & FIXED) != 0;
  out->ground = c->ground[i];
  out->depth = out->fixed ? (c->level[i] > c->ground[i] ? c->level[i] - c->ground[i] : 0.0) : c->depth[i];
  out->level = out->ground + out->depth;
  out->soaked = c->soaked[i];
  out->capacity = c->capacity[i];
  const float *o = &c->out[4 * i];
  out->flow = (double)o[0] + o[1] + o[2] + o[3];
}

void zb_surface_water_get_stats(const zb_surface_water *w, zb_water_stats *out) {
  memset(out, 0, sizeof *out);
  out->chunks = w->grid.count;
  for (int s = 0; s < w->grid.count; ++s) {
    const water_chunk *c = WC(w, s);
    out->awake += c->awake ? 1 : 0;
    for (int i = 0; i < CELLS; ++i) {
      out->on_ground += c->depth[i] * c->area;
      out->wet += c->depth[i] > WET ? 1 : 0;
      out->max_depth = c->depth[i] > out->max_depth ? c->depth[i] : out->max_depth;
      const float *o = &c->out[4 * i];
      double flow = (double)o[0] + o[1] + o[2] + o[3];
      out->max_flow = flow > out->max_flow ? flow : out->max_flow;
    }
  }
  out->soaked = w->absorbed;
  out->drained = w->drained;
  out->rained = w->rained;
  out->added = w->added;
}

uint64_t zb_surface_water_hash(const zb_surface_water *w) {
  uint64_t h = zb_hash64(&w->steps, sizeof w->steps, 0x77617465);
  for (int s = 0; s < w->grid.count; ++s) {
    const water_chunk *c = WC(w, s);
    h = zb_hash64(&w->grid.slot[s]->chunk, sizeof(zb_chunk), h);
    h = zb_hash64(c->depth, sizeof(double) * CELLS, h);
    h = zb_hash64(c->soaked, sizeof(float) * CELLS, h);
    h = zb_hash64(c->out, sizeof(float) * 4 * CELLS, h);
  }
  return h;
}

void zb_surface_water_each_wet(const zb_surface_water *w, double min_depth, zb_water_cell_fn fn, void *ctx) {
  for (int s = 0; s < w->grid.count; ++s) {
    const water_chunk *c = WC(w, s);
    for (int i = 0; i < CELLS; ++i) {
      if (!(c->state[i] & FIXED) && c->depth[i] > min_depth) {
        zb_planet_pos p = zb_active_cell_pos(&w->grid, s * CELLS + i, (float)(c->ground[i] + c->depth[i]));
        fn(ctx, &p, c->depth[i]);
      }
    }
  }
}

int zb_surface_water_chunk_count(const zb_surface_water *w) { return w->grid.count; }

void zb_surface_water_chunk(const zb_surface_water *w, int index, zb_water_chunk_view *out) {
  const water_chunk *c = WC(w, index);
  out->chunk = w->grid.slot[index]->chunk;
  out->ground = c->ground;
  out->depth = c->depth;
  out->version = c->version;
}

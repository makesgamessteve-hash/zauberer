/* SPDX-License-Identifier: MIT */
#include "sim/air.h"

#include <math.h>
#include <string.h>

#include "core/hash.h"
#include "world/dvec.h"
#include "world/stream.h"

#define R ZB_AIR_RES
#define CELLS (ZB_AIR_RES * ZB_AIR_RES)
#define SPREAD 2.0    /* m^2/s: turbulent spreading */
#define TRACE 1e-9    /* kg/m^3: less is no gas */
#define QUIET_STEPS 8 /* steps without gas before a chunk sleeps */

/* How fast each gas leaves the lowest air: smoke rises and thins, steam
 * condenses, scent fades, fumes linger (1/s). */
static const double LOSS[ZB_GASES] = {1.0 / 120.0, 1.0 / 20.0, 1.0 / 300.0, 1.0 / 200.0};
static const char *const NAMES[ZB_GASES] = {"smoke", "steam", "scent", "fumes"};

typedef struct air_chunk {
  float *block;
  float *gas[ZB_GASES];
  float *next;
  float *factor;   /* the ground's effect on wind speed */
  float *gx, *gy;  /* the ground's slope (rise per metre) along the cell axes */
  float *wx, *wy;  /* the local wind along the cell axes, m/s */
  zb_dvec3 ax, ay; /* the cell axes */
  zb_dvec3 east, north;
  double mean_east, mean_north; /* the local wind over the chunk */
  double cw, ch;
  bool awake;
  int quiet;
} air_chunk;

struct zb_air {
  const zb_allocator *a;
  zb_active grid;
  double wind_east, wind_north;
  double pending, since_survey;
  uint64_t steps;
  double emitted[ZB_GASES];
};

const char *zb_gas_name(zb_gas gas) { return gas >= 0 && gas < ZB_GASES ? NAMES[gas] : "?"; }

static air_chunk *AC(const zb_air *air, int s) { return air->grid.slot[s]->data; }

/* The local wind from the weather's: turned along slopes rather than up them,
 * and faster or slower by the ground. */
static void local_wind(const zb_air *air, air_chunk *c) {
  zb_dvec3 b = zb_dv_add(zb_dv_scale(c->east, air->wind_east), zb_dv_scale(c->north, air->wind_north));
  double bx = zb_dv_dot(b, c->ax), by = zb_dv_dot(b, c->ay), sx = 0.0, sy = 0.0;
  for (int i = 0; i < CELLS; ++i) {
    double vx = bx, vy = by, g = sqrt((double)c->gx[i] * c->gx[i] + (double)c->gy[i] * c->gy[i]);
    if (g > 0.05) {
      double ux = c->gx[i] / g, uy = c->gy[i] / g, up = vx * ux + vy * uy;
      double turn = g * 2.0 > 0.8 ? 0.8 : g * 2.0;
      if (up > 0.0) {
        vx -= turn * up * ux;
        vy -= turn * up * uy;
      }
    }
    c->wx[i] = (float)(vx * c->factor[i]);
    c->wy[i] = (float)(vy * c->factor[i]);
    sx += c->wx[i];
    sy += c->wy[i];
  }
  zb_dvec3 mean = zb_dv_add(zb_dv_scale(c->ax, sx / CELLS), zb_dv_scale(c->ay, sy / CELLS));
  c->mean_east = zb_dv_dot(mean, c->east);
  c->mean_north = zb_dv_dot(mean, c->north);
}

static void *take_in(void *ctx, const zb_active_chunk *ac, const zb_chunk_terrain *ct) {
  zb_air *air = ctx;
  air_chunk *c = zb_calloc(air->a, sizeof *c);
  float *block = c ? zb_calloc(air->a, sizeof(float) * CELLS * (ZB_GASES + 8)) : NULL;
  if (!block) {
    zb_free(air->a, c);
    return NULL;
  }
  c->block = block;
  for (int g = 0; g < ZB_GASES; ++g) {
    c->gas[g] = block + g * CELLS;
  }
  c->next = block + ZB_GASES * CELLS;
  c->factor = c->next + CELLS;
  c->gx = c->factor + CELLS;
  c->gy = c->gx + CELLS;
  c->wx = c->gy + CELLS;
  c->wy = c->wx + CELLS;
  c->cw = ac->cw;
  c->ch = ac->ch;
  const zb_planet *p = zb_world_planet(air->grid.world);
  zb_planet_pos o = {ct->chunk, 128.0f, 128.0f, 0.0f}, px = {ct->chunk, 129.0f, 128.0f, 0.0f},
                py = {ct->chunk, 128.0f, 129.0f, 0.0f};
  zb_dvec3 at = zb_planet_to_cart(p, &o), up;
  c->ax = zb_dv_norm(zb_dv_sub(zb_planet_to_cart(p, &px), at));
  c->ay = zb_dv_norm(zb_dv_sub(zb_planet_to_cart(p, &py), at));
  zb_planet_frame(zb_dv_norm(at), &c->east, &c->north, &up);
  /* Heights at the cell centres (the 1 m grid's), then exposure and slope. */
  float h[CELLS];
  const int step = 256 / R;
  for (int y = 0; y < R; ++y) {
    for (int x = 0; x < R; ++x) {
      h[y * R + x] = ct->height[(y * step + step / 2) * ZB_HEIGHT_RES + x * step + step / 2];
    }
  }
  for (int y = 0; y < R; ++y) {
    for (int x = 0; x < R; ++x) {
      int i = y * R + x;
      double sum = 0.0;
      int n = 0;
      for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
          int xx = x + dx, yy = y + dy;
          if (xx >= 0 && yy >= 0 && xx < R && yy < R) {
            sum += h[yy * R + xx];
            n++;
          }
        }
      }
      double exposure = h[i] - sum / n; /* above the ground around: a crest */
      double f = 1.0 + exposure / 40.0;
      f = f < 0.5 ? 0.5 : (f > 1.6 ? 1.6 : f);
      int cx = x * ZB_CANOPY_RES / R, cy = y * ZB_CANOPY_RES / R;
      f *= 1.0 - 0.5 * ct->canopy[cy * ZB_CANOPY_RES + cx] / 255.0; /* forest shelters */
      c->factor[i] = (float)f;
      int x0 = x > 0 ? x - 1 : x, x1 = x < R - 1 ? x + 1 : x, y0 = y > 0 ? y - 1 : y, y1 = y < R - 1 ? y + 1 : y;
      c->gx[i] = (float)((h[y * R + x1] - h[y * R + x0]) / ((x1 - x0) * c->cw));
      c->gy[i] = (float)((h[y1 * R + x] - h[y0 * R + x]) / ((y1 - y0) * c->ch));
    }
  }
  local_wind(air, c);
  return c;
}

static void let_go(void *ctx, void *data) {
  zb_air *air = ctx;
  air_chunk *c = data;
  zb_free(air->a, c->block);
  zb_free(air->a, c);
}

zb_air *zb_air_create(const zb_allocator *a, zb_world *world, double radius) {
  zb_air *air = zb_calloc(a, sizeof *air);
  if (!air) {
    return NULL;
  }
  air->a = a;
  zb_active_hooks hooks = {air, take_in, let_go};
  zb_active_init(&air->grid, a, world, radius, ZB_STAGE_SURFACE, R, &hooks);
  return air;
}

void zb_air_destroy(zb_air *air) {
  if (air) {
    zb_active_clear(&air->grid, NULL);
    zb_free(air->a, air);
  }
}

void zb_air_set_world(zb_air *air, zb_world *world) { zb_active_clear(&air->grid, world); }

void zb_air_set_wind(zb_air *air, double east, double north) {
  air->wind_east = east;
  air->wind_north = north;
  for (int s = 0; s < air->grid.count; ++s) {
    local_wind(air, AC(air, s));
  }
}

/* ----------------------------------------------------------------- steps */

static void wake(air_chunk *c) {
  c->awake = true;
  c->quiet = 0;
}

/* A gas's concentration at cell (x, y) of a slot's chunk, one beyond its edges
 * included (none outside the tier or in a sleeping chunk). */
static float gas_at(const zb_air *air, int s, int x, int y, int g) {
  int32_t cell = zb_active_locate(&air->grid, s, x, y);
  if (cell < 0) {
    return 0.0f;
  }
  const air_chunk *c = zb_active_chunk_of(&air->grid, cell)->data;
  return c->awake ? c->gas[g][cell % CELLS] : 0.0f;
}

/* The gas arriving at a cell: traced back along the wind, spread a little and
 * lost at the gas's rate. */
static void carry(zb_air *air, int s, int g) {
  air_chunk *c = AC(air, s);
  const float *now = c->gas[g];
  double keep = 1.0 - LOSS[g] * ZB_AIR_STEP;
  double kx = SPREAD * ZB_AIR_STEP / (c->cw * c->cw), ky = SPREAD * ZB_AIR_STEP / (c->ch * c->ch);
  for (int y = 0; y < R; ++y) {
    for (int x = 0; x < R; ++x) {
      int i = y * R + x;
      double px = x - c->wx[i] * ZB_AIR_STEP / c->cw, py = y - c->wy[i] * ZB_AIR_STEP / c->ch;
      int x0 = (int)floor(px), y0 = (int)floor(py);
      double fx = px - x0, fy = py - y0;
      bool inside = x0 >= 0 && y0 >= 0 && x0 + 1 < R && y0 + 1 < R;
      double a00 = inside ? now[y0 * R + x0] : gas_at(air, s, x0, y0, g);
      double a10 = inside ? now[y0 * R + x0 + 1] : gas_at(air, s, x0 + 1, y0, g);
      double a01 = inside ? now[(y0 + 1) * R + x0] : gas_at(air, s, x0, y0 + 1, g);
      double a11 = inside ? now[(y0 + 1) * R + x0 + 1] : gas_at(air, s, x0 + 1, y0 + 1, g);
      double v = (a00 * (1 - fx) + a10 * fx) * (1 - fy) + (a01 * (1 - fx) + a11 * fx) * fy;
      bool in4 = x > 0 && y > 0 && x < R - 1 && y < R - 1;
      double lr =
          in4 ? (double)now[i - 1] + now[i + 1] : (double)gas_at(air, s, x - 1, y, g) + gas_at(air, s, x + 1, y, g);
      double du =
          in4 ? (double)now[i - R] + now[i + R] : (double)gas_at(air, s, x, y - 1, g) + gas_at(air, s, x, y + 1, g);
      v += kx * (lr - 2.0 * now[i]) + ky * (du - 2.0 * now[i]);
      v *= keep;
      c->next[i] = (float)(v > TRACE ? v : 0.0);
    }
  }
}

static void step(zb_air *air) {
  int n = air->grid.count;
  /* Each gas in turn: every awake chunk computes from the old values, then all
   * take the new ones (the next buffer is per chunk, so copy back after). */
  float *fresh[ZB_ACTIVE_MAX][ZB_GASES];
  for (int g = 0; g < ZB_GASES; ++g) {
    for (int s = 0; s < n; ++s) {
      air_chunk *c = AC(air, s);
      fresh[s][g] = NULL;
      if (!c->awake) {
        continue;
      }
      carry(air, s, g);
      /* Swap: next holds the new values; the old array becomes the scratch. */
      fresh[s][g] = c->next;
      c->next = c->gas[g];
    }
    for (int s = 0; s < n; ++s) {
      if (fresh[s][g]) {
        AC(air, s)->gas[g] = fresh[s][g];
      }
    }
  }
  for (int s = 0; s < n; ++s) {
    air_chunk *c = AC(air, s);
    if (!c->awake) {
      continue;
    }
    bool any = false, edge = false;
    for (int g = 0; g < ZB_GASES; ++g) {
      for (int i = 0; i < CELLS; ++i) {
        if (c->gas[g][i] > 0.0f) {
          any = true;
          int x = i % R, y = i / R;
          edge = edge || x == 0 || y == 0 || x == R - 1 || y == R - 1;
        }
      }
    }
    c->quiet = any ? 0 : c->quiet + 1;
    if (c->quiet >= QUIET_STEPS) {
      c->awake = false;
    }
    for (int k = 0; k < 8 && edge; ++k) { /* gas at an edge drifts into the neighbours */
      int nb = air->grid.slot[s]->near[k];
      if (nb >= 0 && !AC(air, nb)->awake) {
        wake(AC(air, nb));
      }
    }
  }
  air->steps++;
}

void zb_air_update(zb_air *air, const zb_planet_pos *focus, int focus_count, double seconds) {
  air->since_survey += seconds;
  if (air->since_survey >= 1.0 || air->grid.count == 0) {
    air->since_survey = 0.0;
    zb_active_survey(&air->grid, focus, focus_count);
  }
  air->pending += seconds;
  int steps = 0;
  while (air->pending >= ZB_AIR_STEP && steps < 2) {
    air->pending -= ZB_AIR_STEP;
    step(air);
    steps++;
  }
  if (air->pending > ZB_AIR_STEP) {
    air->pending = ZB_AIR_STEP;
  }
}

/* ------------------------------------------------------- outside access */

void zb_air_emit(zb_air *air, const zb_planet_pos *pos, zb_gas gas, double kg) {
  int32_t cell = zb_active_cell_at(&air->grid, pos);
  if (cell < 0 || gas < 0 || gas >= ZB_GASES || kg <= 0.0) {
    return;
  }
  air_chunk *c = zb_active_chunk_of(&air->grid, cell)->data;
  if (!c->awake) {
    wake(c); /* a chunk sleeps only when it holds no gas */
  }
  c->gas[gas][cell % CELLS] += (float)(kg / (c->cw * c->ch * ZB_AIR_LAYER));
  air->emitted[gas] += kg;
}

void zb_air_sample_at(const zb_air *air, const zb_planet_pos *pos, zb_air_sample *out) {
  memset(out, 0, sizeof *out);
  int32_t cell = zb_active_cell_at(&air->grid, pos);
  if (cell < 0) {
    out->east = air->wind_east;
    out->north = air->wind_north;
    return;
  }
  const air_chunk *c = zb_active_chunk_of(&air->grid, cell)->data;
  int i = cell % CELLS;
  zb_dvec3 v = zb_dv_add(zb_dv_scale(c->ax, c->wx[i]), zb_dv_scale(c->ay, c->wy[i]));
  out->loaded = true;
  out->east = zb_dv_dot(v, c->east);
  out->north = zb_dv_dot(v, c->north);
  for (int g = 0; g < ZB_GASES; ++g) {
    out->gas[g] = c->awake ? c->gas[g][i] : 0.0f;
  }
}

void zb_air_chunk_wind(const zb_air *air, zb_chunk chunk, double *east, double *north) {
  int s = zb_active_slot(&air->grid, chunk);
  *east = s < 0 ? air->wind_east : AC(air, s)->mean_east;
  *north = s < 0 ? air->wind_north : AC(air, s)->mean_north;
}

void zb_air_get_stats(const zb_air *air, zb_air_stats *out) {
  memset(out, 0, sizeof *out);
  out->chunks = air->grid.count;
  for (int s = 0; s < air->grid.count; ++s) {
    const air_chunk *c = AC(air, s);
    out->awake += c->awake ? 1 : 0;
    for (int g = 0; g < ZB_GASES && c->awake; ++g) {
      for (int i = 0; i < CELLS; ++i) {
        out->mass[g] += (double)c->gas[g][i] * c->cw * c->ch * ZB_AIR_LAYER;
      }
    }
  }
  memcpy(out->emitted, air->emitted, sizeof out->emitted);
}

uint64_t zb_air_hash(const zb_air *air) {
  uint64_t h = zb_hash64(&air->steps, sizeof air->steps, 0x616972);
  for (int s = 0; s < air->grid.count; ++s) {
    const air_chunk *c = AC(air, s);
    h = zb_hash64(&air->grid.slot[s]->chunk, sizeof(zb_chunk), h);
    for (int g = 0; g < ZB_GASES && c->awake; ++g) {
      h = zb_hash64(c->gas[g], sizeof(float) * CELLS, h);
    }
    h = zb_hash64(c->wx, sizeof(float) * CELLS, h);
  }
  return h;
}

void zb_air_each(const zb_air *air, zb_gas gas, double min_kg_m3, zb_air_cell_fn fn, void *ctx) {
  for (int s = 0; s < air->grid.count; ++s) {
    const air_chunk *c = AC(air, s);
    for (int i = 0; c->awake && i < CELLS; ++i) {
      if (c->gas[gas][i] > min_kg_m3) {
        fn(ctx,
           &(zb_planet_pos){air->grid.slot[s]->chunk, (float)(((i % R) + 0.5) * 256.0 / R),
                            (float)(((i / R) + 0.5) * 256.0 / R), 0.0f},
           c->gas[gas][i]);
      }
    }
  }
}

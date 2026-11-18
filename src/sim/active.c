/* SPDX-License-Identifier: MIT */
#include "sim/active.h"

#include <math.h>
#include <string.h>

#include "world/stream.h"

#define N ZB_ACTIVE_RES
#define CELLS ZB_ACTIVE_CELLS

const int ZB_ACTIVE_DX[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
const int ZB_ACTIVE_DY[8] = {0, 0, -1, 1, -1, -1, 1, 1};

void zb_active_init(zb_active *g, const zb_allocator *a, zb_world *world, double radius, int stage,
                    const zb_active_hooks *hooks) {
  memset(g, 0, sizeof *g);
  g->a = a;
  g->world = world;
  g->radius = radius;
  g->stage = stage;
  g->hooks = *hooks;
}

static void let_go(zb_active *g, int s) {
  g->hooks.let_go(g->hooks.ctx, g->slot[s]->data);
  zb_free(g->a, g->slot[s]);
  memmove(&g->slot[s], &g->slot[s + 1], sizeof g->slot[0] * (size_t)(g->count - s - 1));
  g->count--;
}

void zb_active_clear(zb_active *g, zb_world *world) {
  while (g->count > 0) {
    let_go(g, g->count - 1);
  }
  if (world) {
    g->world = world;
  }
}

static const zb_planet *planet(const zb_active *g) { return zb_world_planet(g->world); }

int zb_active_slot(const zb_active *g, zb_chunk c) {
  for (int i = 0; i < g->count; ++i) {
    if (zb_chunk_equal(g->slot[i]->chunk, c)) {
      return i;
    }
  }
  return -1;
}

int32_t zb_active_find(const zb_active *g, zb_chunk c, double x, double y) {
  zb_planet_pos p = {c, (float)((x + 0.5) * 256.0 / N), (float)((y + 0.5) * 256.0 / N), 0.0f};
  zb_planet_pos q = zb_planet_from_cart(planet(g), zb_planet_to_cart(planet(g), &p));
  int s = zb_active_slot(g, q.chunk);
  if (s < 0) {
    return -1;
  }
  int cx = (int)(q.lx * N / 256.0f), cy = (int)(q.ly * N / 256.0f);
  cx = cx < 0 ? 0 : (cx >= N ? N - 1 : cx);
  cy = cy < 0 ? 0 : (cy >= N ? N - 1 : cy);
  return s * CELLS + cy * N + cx;
}

int32_t zb_active_locate(const zb_active *g, int s, int x, int y) {
  const zb_active_chunk *c = g->slot[s];
  bool in_x = x >= 0 && x < N, in_y = y >= 0 && y < N;
  if (in_x && in_y) {
    return s * CELLS + y * N + x;
  }
  if (in_y) {
    return c->edge[x < 0 ? 0 : 1][y];
  }
  if (in_x) {
    return c->edge[y < 0 ? 2 : 3][x];
  }
  return c->corner[(x < 0 ? 0 : 1) + (y < 0 ? 0 : 2)];
}

/* The cells beyond every chunk's edges and corners. A neighbour on the same
 * face lines up cell for cell; across a cube-face seam the planet finds them. */
static void link(zb_active *g) {
  static const int side[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  for (int s = 0; s < g->count; ++s) {
    zb_active_chunk *c = g->slot[s];
    for (int e = 0; e < 4; ++e) {
      zb_chunk nb;
      bool same = zb_planet_neighbor(planet(g), c->chunk, side[e][0], side[e][1], &nb) && nb.face == c->chunk.face;
      int t = same ? zb_active_slot(g, nb) : -1;
      for (int i = 0; i < N; ++i) {
        int x = e == 0 ? -1 : (e == 1 ? N : i), y = e == 2 ? -1 : (e == 3 ? N : i);
        c->edge[e][i] =
            !same ? zb_active_find(g, c->chunk, x, y) : (t < 0 ? -1 : t * CELLS + ((y + N) % N) * N + (x + N) % N);
      }
    }
    c->corner[0] = zb_active_find(g, c->chunk, -1, -1);
    c->corner[1] = zb_active_find(g, c->chunk, N, -1);
    c->corner[2] = zb_active_find(g, c->chunk, -1, N);
    c->corner[3] = zb_active_find(g, c->chunk, N, N);
    for (int k = 0; k < 8; ++k) {
      int dx = ZB_ACTIVE_DX[k], dy = ZB_ACTIVE_DY[k];
      int32_t cell = zb_active_locate(g, s, dx < 0 ? -1 : (dx > 0 ? N : N / 2), dy < 0 ? -1 : (dy > 0 ? N : N / 2));
      c->near[k] = cell < 0 ? -1 : cell / CELLS;
    }
  }
}

typedef struct survey {
  zb_active *g;
  const zb_planet_pos *focus;
  int focus_count;
  const zb_chunk_terrain *want[ZB_ACTIVE_MAX];
  int n;
} survey;

static void consider(void *ctx, const zb_chunk_terrain *ct, uint32_t version, int target) {
  (void)version;
  (void)target;
  survey *sv = ctx;
  if (ct->stage < sv->g->stage || sv->n >= ZB_ACTIVE_MAX) {
    return;
  }
  zb_planet_pos centre = zb_planet_chunk_center(ct->chunk);
  for (int i = 0; i < sv->focus_count; ++i) {
    /* A chunk is about 256 m across: its far corner is 182 m from its centre. */
    if (zb_planet_distance(planet(sv->g), &centre, &sv->focus[i]) <= sv->g->radius + 182.0) {
      sv->want[sv->n++] = ct;
      return;
    }
  }
}

static bool take_in(zb_active *g, const zb_chunk_terrain *ct) {
  zb_active_chunk *c = zb_calloc(g->a, sizeof *c);
  if (!c) {
    return false;
  }
  c->chunk = ct->chunk;
  zb_planet_chunk_size(planet(g), ct->chunk, &c->cw, &c->ch);
  c->cw /= N;
  c->ch /= N;
  for (int k = 0; k < 8; ++k) {
    c->near[k] = -1;
  }
  c->data = g->hooks.take_in(g->hooks.ctx, c, ct);
  if (!c->data) {
    zb_free(g->a, c);
    return false;
  }
  g->slot[g->count++] = c;
  return true;
}

bool zb_active_survey(zb_active *g, const zb_planet_pos *focus, int focus_count) {
  survey sv = {.g = g, .focus = focus, .focus_count = focus_count};
  zb_stream_each(zb_world_stream(g->world), consider, &sv);
  bool changed = false;
  for (int i = 0; i < g->count;) {
    bool keep = false;
    for (int k = 0; k < sv.n && !keep; ++k) {
      keep = zb_chunk_equal(sv.want[k]->chunk, g->slot[i]->chunk);
    }
    if (keep) {
      ++i;
    } else {
      let_go(g, i);
      changed = true;
    }
  }
  for (int k = 0; k < sv.n; ++k) {
    if (zb_active_slot(g, sv.want[k]->chunk) < 0 && take_in(g, sv.want[k])) {
      changed = true;
    }
  }
  if (changed) {
    link(g);
  }
  return changed;
}

int32_t zb_active_cell_at(const zb_active *g, const zb_planet_pos *pos) {
  int s = zb_active_slot(g, pos->chunk);
  if (s < 0) {
    return -1;
  }
  int x = (int)(pos->lx * N / 256.0f), y = (int)(pos->ly * N / 256.0f);
  x = x < 0 ? 0 : (x >= N ? N - 1 : x);
  y = y < 0 ? 0 : (y >= N ? N - 1 : y);
  return s * CELLS + y * N + x;
}

zb_planet_pos zb_active_cell_pos(const zb_active *g, int32_t cell, float alt) {
  int i = cell % CELLS;
  zb_planet_pos p = {zb_active_chunk_of(g, cell)->chunk, (float)(((i % N) + 0.5) * 256.0 / N),
                     (float)(((i / N) + 0.5) * 256.0 / N), alt};
  return p;
}

int zb_active_around(const zb_active *g, const zb_planet_pos *pos, double radius, bool (*fn)(void *ctx, int32_t cell),
                     void *ctx) {
  int s = zb_active_slot(g, pos->chunk);
  if (s < 0) {
    return 0;
  }
  const zb_active_chunk *c = g->slot[s];
  int cx = (int)(pos->lx * N / 256.0f), cy = (int)(pos->ly * N / 256.0f), hits = 0;
  int rx = (int)(radius / c->cw) + 1, ry = (int)(radius / c->ch) + 1;
  for (int dy = -ry; dy <= ry; ++dy) {
    for (int dx = -rx; dx <= rx; ++dx) {
      double mx = dx * c->cw, my = dy * c->ch;
      if (mx * mx + my * my > radius * radius + 0.5 * c->cw * c->ch) {
        continue;
      }
      int x = cx + dx, y = cy + dy;
      int32_t cell = x >= 0 && y >= 0 && x < N && y < N ? s * CELLS + y * N + x : zb_active_find(g, c->chunk, x, y);
      if (cell >= 0 && fn(ctx, cell)) {
        hits++;
      }
    }
  }
  return hits;
}

/* SPDX-License-Identifier: MIT */
#include "world/water.h"

#include <string.h>

#include "core/hash.h"
#include "world/dvec.h"

#define BANK_MIN 4.0          /* metres of bank beside the smallest channel */
#define LEVEE 0.3             /* banks stand this much above the water */
#define VALLEY_MIN 150.0      /* metres over which hills return beside a river */
#define VALLEY_PER_HALF 6.0   /* ... plus this many half widths */
#define MEANDER_WAVELENGTH 14 /* channel widths per meander */

void zb_river_geometry(double q, double *width, double *depth, double *speed) {
  q = q > 0.01 ? q : 0.01;
  /* Leopold and Maddock's at-a-station exponents (b 0.5, f 0.4, m 0.1). */
  *width = 3.0 * zb_dpow(q, 0.5);
  *depth = 0.3 * zb_dpow(q, 0.4);
  *speed = q / (*width * *depth);
}

double zb_water_cell_level(const zb_world_bundle *b, int32_t cell) {
  if (b->cell_flags[cell] & ZB_CELL_OCEAN) {
    return 0.0;
  }
  return (double)b->height[cell] + (double)b->lake[cell];
}

bool zb_water_cell_has_channel(const zb_world_bundle *b, int32_t cell) {
  return !(b->cell_flags[cell] & ZB_CELL_OCEAN) && b->receiver[cell] >= 0 &&
         (double)b->discharge[cell] >= ZB_RIVER_MIN_DISCHARGE;
}

void zb_river_set_init(zb_river_set *s, const zb_allocator *a) {
  memset(s, 0, sizeof *s);
  s->a = a;
}

void zb_river_set_free(zb_river_set *s) {
  zb_free(s->a, s->piece);
  s->piece = NULL;
  s->count = s->cap = 0;
}

static bool push_piece(zb_river_set *s, const zb_river_piece *p) {
  if (s->count == s->cap) {
    size_t cap = s->cap ? s->cap * 2 : 64;
    zb_river_piece *n = zb_realloc(s->a, s->piece, sizeof(zb_river_piece) * cap);
    if (!n) {
      return false;
    }
    s->piece = n;
    s->cap = cap;
  }
  s->piece[s->count++] = *p;
  return true;
}

/* The tributary that carries the most water into a cell, or -1. */
static int32_t main_upstream(const zb_world_bundle *b, int32_t cell) {
  int32_t best = -1;
  for (int k = 0; k < ZB_CUBEGRID_NBRS; ++k) {
    int32_t n = zb_cubegrid_nbr(&b->grid, cell, k);
    if (n >= 0 && b->receiver[n] == cell && zb_water_cell_has_channel(b, n) &&
        (best < 0 || b->discharge[n] > b->discharge[best])) {
      best = n;
    }
  }
  return best;
}

static zb_dvec3 catmull_rom(zb_dvec3 p0, zb_dvec3 p1, zb_dvec3 p2, zb_dvec3 p3, double t) {
  double t2 = t * t, t3 = t2 * t;
  double w0 = -0.5 * t3 + t2 - 0.5 * t, w1 = 1.5 * t3 - 2.5 * t2 + 1.0, w2 = -1.5 * t3 + 2.0 * t2 + 0.5 * t,
         w3 = 0.5 * t3 - 0.5 * t2;
  return zb_dv_add(zb_dv_add(zb_dv_scale(p0, w0), zb_dv_scale(p1, w1)),
                   zb_dv_add(zb_dv_scale(p2, w2), zb_dv_scale(p3, w3)));
}

zb_result zb_rivers_add_cell(const zb_world_bundle *b, int32_t cell, zb_river_set *out) {
  if (!zb_water_cell_has_channel(b, cell)) {
    return ZB_OK;
  }
  double radius = b->options.radius;
  int32_t r = b->receiver[cell];
  zb_dvec3 p1 = zb_cubegrid_dir(&b->grid, cell), p2 = zb_cubegrid_dir(&b->grid, r);
  int32_t up = main_upstream(b, cell);
  zb_dvec3 p0 = up >= 0 ? zb_cubegrid_dir(&b->grid, up) : zb_dv_sub(zb_dv_scale(p1, 2.0), p2);
  int32_t rr = zb_water_cell_has_channel(b, r) ? b->receiver[r] : -1;
  zb_dvec3 p3 = rr >= 0 ? zb_cubegrid_dir(&b->grid, rr) : zb_dv_sub(zb_dv_scale(p2, 2.0), p1);
  double width, depth, speed;
  zb_river_geometry((double)b->discharge[cell], &width, &depth, &speed);
  double length = zb_dv_angle(p1, p2) * radius;
  /* Meanders: a few waves between the cell centres, which stay fixed so that
   * tributaries meet the river there. */
  uint64_t h = zb_mix64(b->options.seed ^ ((uint64_t)(uint32_t)cell * 0x9e3779b97f4a7c15ull) ^ 0x7269766572ull);
  double phase = (double)(h & 0xffff) / 65536.0 * 2.0 * ZB_DPI;
  double waves = length / (MEANDER_WAVELENGTH * width) + 0.5;
  waves = waves < 1.0 ? 1.0 : (waves > 3.0 ? 3.0 : (double)(int)waves);
  double amplitude = 0.08 * length < 6.0 * width ? 0.08 * length : 6.0 * width;
  amplitude *= 0.5 + 0.5 * (double)((h >> 16) & 0xffff) / 65536.0;
  double s1 = zb_water_cell_level(b, cell), s2 = zb_water_cell_level(b, r);
  s2 = s2 < s1 ? s2 : s1; /* water never climbs */
  /* Into the sea: down to sea level by the coast, half-way to the sea cell. */
  bool to_sea = (b->cell_flags[r] & ZB_CELL_OCEAN) != 0 && s1 > 0.0;
  zb_dvec3 prev = p1;
  double prev_level = s1;
  for (int k = 1; k <= ZB_RIVER_PIECES; ++k) {
    double t = (double)k / ZB_RIVER_PIECES;
    zb_dvec3 p = k == ZB_RIVER_PIECES ? p2 : zb_dv_norm(catmull_rom(p0, p1, p2, p3, t));
    if (k < ZB_RIVER_PIECES) {
      zb_dvec3 ahead = zb_dv_norm(catmull_rom(p0, p1, p2, p3, t + 0.5 / ZB_RIVER_PIECES));
      zb_dvec3 side = zb_dv_norm(zb_dv_cross(p, zb_dv_sub(ahead, p)));
      double env = zb_dsin(ZB_DPI * t);
      double offset = amplitude * env * env * zb_dsin(2.0 * ZB_DPI * waves * t + phase);
      p = zb_dv_norm(zb_dv_add(p, zb_dv_scale(side, offset / radius)));
    }
    /* A river without a tributary springs up: it widens over its first half. */
    double grow = up >= 0 ? 1.0 : 0.15 + 0.85 * (2.0 * t < 1.0 ? 2.0 * t : 1.0);
    double level = to_sea ? s1 * (1.0 - 2.0 * t > 0.0 ? 1.0 - 2.0 * t : 0.0) : s1 + (s2 - s1) * t;
    zb_river_piece pc = {prev, p, prev_level, level, 0.5 * width * grow, depth * grow, speed};
    prev_level = level;
    if (!push_piece(out, &pc)) {
      return ZB_ERR_NOMEM;
    }
    prev = p;
  }
  return ZB_OK;
}

/* Distance in metres from dir to a piece, and where along it (0..1). */
static double piece_distance(const zb_river_piece *p, double radius, zb_dvec3 dir, double *t) {
  zb_dvec3 ab = zb_dv_sub(p->b, p->a);
  double len2 = zb_dv_dot(ab, ab);
  double u = len2 > 0.0 ? zb_dv_dot(zb_dv_sub(dir, p->a), ab) / len2 : 0.0;
  u = u < 0.0 ? 0.0 : (u > 1.0 ? 1.0 : u);
  *t = u;
  return zb_dv_len(zb_dv_sub(dir, zb_dv_add(p->a, zb_dv_scale(ab, u)))) * radius;
}

static double piece_zone(const zb_river_piece *p) {
  return 2.0 * p->half_width + BANK_MIN + VALLEY_MIN + VALLEY_PER_HALF * p->half_width;
}

zb_result zb_rivers_gather(const zb_world_bundle *b, zb_dvec3 dir, double radius, zb_river_set *out) {
  out->count = 0;
  /* The cell and two rings of neighbours: river pieces leave a cell's centre
   * and end at a neighbour's, so nothing farther reaches this cell. */
  int32_t cells[64];
  int n = 0;
  cells[n++] = zb_cubegrid_cell_at(&b->grid, dir);
  for (int ring = 0, from = 0; ring < 2; ++ring) {
    int to = n;
    for (int i = from; i < to; ++i) {
      for (int k = 0; k < ZB_CUBEGRID_NBRS; ++k) {
        int32_t c = zb_cubegrid_nbr(&b->grid, cells[i], k);
        bool seen = c < 0;
        for (int j = 0; j < n && !seen; ++j) {
          seen = cells[j] == c;
        }
        if (!seen && n < 64) {
          cells[n++] = c;
        }
      }
    }
    from = to;
  }
  zb_river_set tmp;
  zb_river_set_init(&tmp, out->a);
  for (int i = 0; i < n; ++i) {
    tmp.count = 0;
    if (zb_rivers_add_cell(b, cells[i], &tmp) != ZB_OK) {
      zb_river_set_free(&tmp);
      return ZB_ERR_NOMEM;
    }
    for (size_t k = 0; k < tmp.count; ++k) {
      double t;
      if (piece_distance(&tmp.piece[k], b->options.radius, dir, &t) <= radius + piece_zone(&tmp.piece[k]) &&
          !push_piece(out, &tmp.piece[k])) {
        zb_river_set_free(&tmp);
        return ZB_ERR_NOMEM;
      }
    }
  }
  zb_river_set_free(&tmp);
  return ZB_OK;
}

void zb_rivers_keep_near(zb_river_set *s, double planet_radius, zb_dvec3 dir, double radius, double margin) {
  size_t n = 0;
  for (size_t i = 0; i < s->count; ++i) {
    double t;
    double reach = 2.0 * s->piece[i].half_width + BANK_MIN + margin;
    if (piece_distance(&s->piece[i], planet_radius, dir, &t) <= radius + reach) {
      s->piece[n++] = s->piece[i];
    }
  }
  s->count = n;
}

bool zb_rivers_nearest(const zb_river_set *s, double planet_radius, zb_dvec3 dir, double max_distance,
                       zb_river_hit *out) {
  const zb_river_piece *best = NULL;
  double best_d = max_distance, best_t = 0.0;
  for (size_t i = 0; i < s->count; ++i) {
    double t, d = piece_distance(&s->piece[i], planet_radius, dir, &t);
    if (d < best_d || (!best && d <= best_d)) {
      best = &s->piece[i];
      best_d = d;
      best_t = t;
    }
  }
  if (!best) {
    return false;
  }
  out->distance = best_d;
  out->surface = best->surface_a + (best->surface_b - best->surface_a) * best_t;
  out->half_width = best->half_width;
  out->depth = best->depth;
  out->speed = best->speed;
  zb_dvec3 ab = zb_dv_sub(best->b, best->a);
  out->along = zb_dv_norm(zb_dv_sub(ab, zb_dv_scale(dir, zb_dv_dot(ab, dir))));
  zb_dvec3 to = zb_dv_sub(zb_dv_add(best->a, zb_dv_scale(ab, best_t)), dir);
  to = zb_dv_sub(to, zb_dv_scale(dir, zb_dv_dot(to, dir)));
  double tl = zb_dv_len(to);
  out->to_centre = tl > 1e-15 ? zb_dv_scale(to, 1.0 / tl) : zb_dv(0.0, 0.0, 0.0);
  return true;
}

double zb_river_reach(const zb_river_hit *h) { return 2.0 * h->half_width + BANK_MIN; }

double zb_river_carve(const zb_river_hit *h, double height) {
  double r = h->distance, half = h->half_width;
  if (r < half) {
    /* A parabolic channel whose mean depth is the river's depth. */
    double u = r / half;
    double bed = h->surface - 1.5 * h->depth * (1.0 - u * u);
    return height < bed ? height : bed;
  }
  double bank = half + BANK_MIN;
  if (r < half + bank && height >= 0.0) { /* banks raise land, never the sea floor */
    double u = (r - half) / bank, k = u * u * (3.0 - 2.0 * u);
    double top = h->surface + LEVEE;
    return top + (height - top) * k;
  }
  return height;
}

double zb_river_hill_factor(const zb_river_hit *h) {
  double zone = VALLEY_MIN + VALLEY_PER_HALF * h->half_width;
  double u = (h->distance - zb_river_reach(h)) / zone;
  u = u < 0.0 ? 0.0 : (u > 1.0 ? 1.0 : u);
  return u * u * (3.0 - 2.0 * u);
}

bool zb_lake_level(const zb_world_bundle *b, zb_dvec3 dir, double *level) {
  int32_t c = zb_cubegrid_cell_at(&b->grid, dir);
  bool found = false;
  double best = 0.0;
  if (b->cell_flags[c] & ZB_CELL_LAKE) {
    best = zb_water_cell_level(b, c);
    found = true;
  }
  for (int k = 0; k < ZB_CUBEGRID_NBRS; ++k) {
    int32_t n = zb_cubegrid_nbr(&b->grid, c, k);
    if (n < 0 || !(b->cell_flags[n] & ZB_CELL_LAKE)) {
      continue;
    }
    double l = zb_water_cell_level(b, n);
    /* The lake reaches into this cell only where it is shore, not the outlet. */
    bool shore = (b->cell_flags[c] & ZB_CELL_LAKE) || (double)b->height[c] >= l;
    if (shore && (!found || l > best)) {
      best = l;
      found = true;
    }
  }
  *level = best;
  return found;
}

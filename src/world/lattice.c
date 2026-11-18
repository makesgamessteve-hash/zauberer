/* SPDX-License-Identifier: MIT */
#include "world/lattice.h"

#include <string.h>

#include "world/dvec.h"

#define GOLDEN_CONJ 0.6180339887498948482 /* (sqrt(5) - 1) / 2 */
#define TWO_PI 6.283185307179586477
#define SEARCH_RADIUS 3 /* buckets searched around a point for its neighbours */

static int bucket_of(const zb_lattice *l, zb_dvec3 d) {
  int face = zb_planet_dir_face(d);
  double u, v;
  zb_planet_dir_uv(face, d, &u, &v);
  int bx = (int)((u + 1.0) * 0.5 * (double)l->grid);
  int by = (int)((v + 1.0) * 0.5 * (double)l->grid);
  bx = bx < 0 ? 0 : (bx >= l->grid ? l->grid - 1 : bx);
  by = by < 0 ? 0 : (by >= l->grid ? l->grid - 1 : by);
  return (face * l->grid + by) * l->grid + bx;
}

/* Buckets within `radius` of bucket b (across cube edges too); returns the count. */
static int buckets_around(const zb_lattice *l, int b, int radius, int32_t *out, int cap) {
  int g = l->grid;
  int face = b / (g * g), by = (b / g) % g, bx = b % g;
  int count = 0;
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      int nx = bx + dx, ny = by + dy, nb;
      if (nx >= 0 && nx < g && ny >= 0 && ny < g) {
        nb = (face * g + ny) * g + nx;
      } else {
        double u = -1.0 + ((double)nx + 0.5) * 2.0 / (double)g;
        double v = -1.0 + ((double)ny + 0.5) * 2.0 / (double)g;
        if (u <= -1.9 || u >= 1.9 || v <= -1.9 || v >= 1.9) {
          continue;
        }
        nb = bucket_of(l, zb_planet_face_dir(face, u, v));
      }
      bool seen = false;
      for (int i = 0; i < count && !seen; ++i) {
        seen = out[i] == nb;
      }
      if (!seen && count < cap) {
        out[count++] = nb;
      }
    }
  }
  return count;
}

static void build_buckets(zb_lattice *l, int32_t *bucket) {
  int nb = 6 * l->grid * l->grid;
  memset(l->start, 0, sizeof(int32_t) * (size_t)(nb + 1));
  for (int i = 0; i < l->n; ++i) {
    bucket[i] = bucket_of(l, l->p[i]);
    l->start[bucket[i] + 1]++;
  }
  for (int b = 0; b < nb; ++b) {
    l->start[b + 1] += l->start[b];
  }
  for (int i = 0; i < l->n; ++i) { /* start[] serves as the fill cursor, then shifts back */
    l->items[l->start[bucket[i]]++] = i;
  }
  for (int b = nb; b > 0; --b) {
    l->start[b] = l->start[b - 1];
  }
  l->start[0] = 0;
}

/* Keeps the K largest dot products (nearest points) in order. */
static void insert_nearest(int32_t *best, double *best_dot, int j, double d) {
  if (d <= best_dot[ZB_LATTICE_K - 1]) {
    return;
  }
  int k = ZB_LATTICE_K - 1;
  while (k > 0 && best_dot[k - 1] < d) {
    best[k] = best[k - 1];
    best_dot[k] = best_dot[k - 1];
    --k;
  }
  best[k] = j;
  best_dot[k] = d;
}

static void build_neighbours(zb_lattice *l) {
  int32_t around[(2 * SEARCH_RADIUS + 1) * (2 * SEARCH_RADIUS + 1) * 2];
  double angle_sum = 0.0;
  for (int i = 0; i < l->n; ++i) {
    int32_t *best = &l->nbr[(size_t)i * ZB_LATTICE_K];
    double best_dot[ZB_LATTICE_K];
    for (int k = 0; k < ZB_LATTICE_K; ++k) {
      best[k] = -1;
      best_dot[k] = -2.0;
    }
    int count =
        buckets_around(l, bucket_of(l, l->p[i]), SEARCH_RADIUS, around, (int)(sizeof around / sizeof around[0]));
    for (int c = 0; c < count; ++c) {
      for (int32_t t = l->start[around[c]]; t < l->start[around[c] + 1]; ++t) {
        int j = l->items[t];
        if (j != i) {
          insert_nearest(best, best_dot, j, zb_dv_dot(l->p[i], l->p[j]));
        }
      }
    }
    angle_sum += zb_dv_angle(l->p[i], l->p[best[0]]);
  }
  l->spacing = angle_sum / (double)l->n;
}

zb_result zb_lattice_init(zb_lattice *l, const zb_allocator *a, int n) {
  memset(l, 0, sizeof *l);
  if (n < ZB_LATTICE_MIN_POINTS || n > ZB_LATTICE_MAX_POINTS) {
    return ZB_ERR_INVALID;
  }
  l->a = a;
  l->n = n;
  l->grid = (int)sqrt((double)n / 6.0);
  if (l->grid < 1) {
    l->grid = 1;
  }
  size_t nb = (size_t)6 * (size_t)l->grid * (size_t)l->grid;
  l->p = zb_alloc(a, sizeof(zb_dvec3) * (size_t)n);
  l->nbr = zb_alloc(a, sizeof(int32_t) * (size_t)n * ZB_LATTICE_K);
  l->start = zb_alloc(a, sizeof(int32_t) * (nb + 1));
  l->items = zb_alloc(a, sizeof(int32_t) * (size_t)n);
  int32_t *bucket = zb_alloc(a, sizeof(int32_t) * (size_t)n);
  if (!l->p || !l->nbr || !l->start || !l->items || !bucket) {
    zb_free(a, bucket);
    zb_lattice_free(l);
    return ZB_ERR_NOMEM;
  }
  for (int i = 0; i < n; ++i) {
    double z = 1.0 - (2.0 * (double)i + 1.0) / (double)n;
    double r = sqrt(1.0 - z * z);
    double t = (double)i * GOLDEN_CONJ;
    double phi = TWO_PI * (t - floor(t));
    l->p[i] = zb_dv(r * zb_dcos(phi), r * zb_dsin(phi), z);
  }
  build_buckets(l, bucket);
  zb_free(a, bucket);
  build_neighbours(l);
  return ZB_OK;
}

void zb_lattice_free(zb_lattice *l) {
  if (!l->a) {
    return;
  }
  zb_free(l->a, l->p);
  zb_free(l->a, l->nbr);
  zb_free(l->a, l->start);
  zb_free(l->a, l->items);
  memset(l, 0, sizeof *l);
}

/* Greedy descent over the neighbour graph, then over neighbours of neighbours. */
static int descend(const zb_lattice *l, zb_dvec3 d, int cur) {
  double cur_dot = zb_dv_dot(l->p[cur], d);
  for (;;) {
    int best = cur;
    double best_dot = cur_dot;
    for (int k = 0; k < ZB_LATTICE_K; ++k) {
      int j = zb_lattice_nbr(l, cur, k);
      if (j >= 0) {
        double dj = zb_dv_dot(l->p[j], d);
        if (dj > best_dot) {
          best = j;
          best_dot = dj;
        }
      }
    }
    if (best == cur) {
      for (int k = 0; k < ZB_LATTICE_K && best == cur; ++k) {
        int j = zb_lattice_nbr(l, cur, k);
        for (int m = 0; j >= 0 && m < ZB_LATTICE_K; ++m) {
          int q = zb_lattice_nbr(l, j, m);
          if (q >= 0) {
            double dq = zb_dv_dot(l->p[q], d);
            if (dq > best_dot) {
              best = q;
              best_dot = dq;
            }
          }
        }
      }
      if (best == cur) {
        return cur;
      }
    }
    cur = best;
    cur_dot = best_dot;
  }
}

int zb_lattice_nearest(const zb_lattice *l, zb_dvec3 dir, int hint) {
  if (hint < 0 || hint >= l->n) {
    int32_t around[9 * 2];
    int count = buckets_around(l, bucket_of(l, dir), 1, around, 18);
    double best_dot = -2.0;
    hint = 0;
    for (int c = 0; c < count; ++c) {
      for (int32_t t = l->start[around[c]]; t < l->start[around[c] + 1]; ++t) {
        double dj = zb_dv_dot(l->p[l->items[t]], dir);
        if (dj > best_dot) {
          best_dot = dj;
          hint = l->items[t];
        }
      }
    }
  }
  return descend(l, dir, hint);
}

double zb_lattice_cell_area(const zb_lattice *l) { return 4.0 * 3.141592653589793 / (double)l->n; }

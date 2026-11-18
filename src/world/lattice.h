/* SPDX-License-Identifier: MIT */
/* A Fibonacci lattice on the unit sphere: n nearly evenly spaced points with
 * their nearest neighbours and a nearest-point lookup. The deep-time geology
 * simulation runs on it. Deterministic on every platform. */
#ifndef ZB_WORLD_LATTICE_H
#define ZB_WORLD_LATTICE_H

#include "core/alloc.h"
#include "world/planet.h"

#define ZB_LATTICE_K 8 /* neighbours kept per point, nearest first */
#define ZB_LATTICE_MIN_POINTS 64
#define ZB_LATTICE_MAX_POINTS 1000000

typedef struct zb_lattice {
  const zb_allocator *a;
  int n;
  zb_dvec3 *p;    /* unit positions */
  int32_t *nbr;   /* ZB_LATTICE_K per point, -1 where fewer exist */
  double spacing; /* mean angle to the nearest neighbour (radians) */
  int grid;       /* buckets per cube-face edge */
  int32_t *start; /* bucket b holds items[start[b] .. start[b + 1]) */
  int32_t *items;
} zb_lattice;

/* Builds the lattice; ZB_ERR_INVALID outside the point limits, ZB_ERR_NOMEM. */
zb_result zb_lattice_init(zb_lattice *l, const zb_allocator *a, int n);
void zb_lattice_free(zb_lattice *l);
/* The point nearest to a unit direction. hint (a point index or -1) speeds up
 * the search when it is close to the answer. */
int zb_lattice_nearest(const zb_lattice *l, zb_dvec3 dir, int hint);
/* The k-th neighbour of a point (-1 if none). */
static inline int zb_lattice_nbr(const zb_lattice *l, int i, int k) {
  return l->nbr[(size_t)i * ZB_LATTICE_K + (size_t)k];
}
/* Solid angle per point (4 pi / n). */
double zb_lattice_cell_area(const zb_lattice *l);

#endif

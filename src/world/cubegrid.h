/* SPDX-License-Identifier: MIT */
/* A cube-sphere grid of res x res equi-angular cells per face: cell indices,
 * centre directions, solid-angle areas and the eight neighbours of every
 * cell, across cube edges too. Used by the macro grid and the climate grid. */
#ifndef ZB_WORLD_CUBEGRID_H
#define ZB_WORLD_CUBEGRID_H

#include "core/alloc.h"
#include "world/planet.h"

typedef struct zb_cubegrid {
  const zb_allocator *a;
  int res;
  size_t cells;      /* 6 * res * res; cell (face, x, y) is (face * res + y) * res + x */
  float *area;       /* km^2 per cell of one face (the same on every face), res * res */
  int32_t *edge_nbr; /* neighbours of perimeter cells: 6 * 4 * res * 8 */
} zb_cubegrid;

/* Neighbour order: E, W, N, S (k < 4), then NE, NW, SE, SW; -1 is the missing
 * diagonal at a cube corner. */
#define ZB_CUBEGRID_NBRS 8

zb_result zb_cubegrid_init(zb_cubegrid *g, const zb_allocator *a, int res, double radius_km);
void zb_cubegrid_free(zb_cubegrid *g);
int32_t zb_cubegrid_nbr(const zb_cubegrid *g, int32_t cell, int k);
zb_dvec3 zb_cubegrid_dir(const zb_cubegrid *g, int32_t cell);
int32_t zb_cubegrid_cell_at(const zb_cubegrid *g, zb_dvec3 dir);
static inline float zb_cubegrid_area(const zb_cubegrid *g, int32_t cell) {
  return g->area[(size_t)cell % ((size_t)g->res * (size_t)g->res)];
}
/* Bilinear sample of a per-cell field at a direction (clamped at face edges). */
double zb_cubegrid_sample(const zb_cubegrid *g, const float *field, zb_dvec3 dir);
/* Bilinear sample that continues across cube edges into the neighbouring face,
 * so the field has no seams (cube corners fall back to the nearest cell). */
double zb_cubegrid_sample_smooth(const zb_cubegrid *g, const float *field, zb_dvec3 dir);

#endif

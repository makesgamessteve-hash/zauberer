/* SPDX-License-Identifier: MIT */
/* The active tier as a grid (concept 5.2): every loaded chunk with the data
 * a simulation needs (surface or plants) within a radius of a focus point, each cut into ZB_ACTIVE_RES^2
 * cells of about 2 m. The simulations of the active tier (heat and fire,
 * surface water) keep their own data per chunk; this keeps the set of chunks
 * and finds a cell's neighbours across chunk edges and cube-face seams.
 *
 * A cell is named by one integer: slot * ZB_ACTIVE_CELLS + y * ZB_ACTIVE_RES + x.
 * Slots move when chunks leave, so cell numbers last until the next survey. */
#ifndef ZB_SIM_ACTIVE_H
#define ZB_SIM_ACTIVE_H

#include "world/world.h"

#define ZB_ACTIVE_RES 128
#define ZB_ACTIVE_CELLS (ZB_ACTIVE_RES * ZB_ACTIVE_RES)
#define ZB_ACTIVE_MAX 64

typedef struct zb_active_chunk {
  zb_chunk chunk;
  double cw, ch;                  /* cell size, metres */
  int32_t edge[4][ZB_ACTIVE_RES]; /* the cell beyond each edge (x-, x+, y-, y+), -1 for none */
  int32_t corner[4];              /* beyond (-1,-1), (N,-1), (-1,N), (N,N) */
  int near[8];                    /* the neighbouring chunks' slots, -1 for none */
  void *data;                     /* the simulation's */
} zb_active_chunk;

/* The neighbour directions: four sides, then four corners. */
extern const int ZB_ACTIVE_DX[8], ZB_ACTIVE_DY[8];

typedef struct zb_active_hooks {
  void *ctx;
  /* A chunk enters with its terrain (at least the grid's stage): its data, or
   * NULL to leave it out (out of memory). */
  void *(*take_in)(void *ctx, const zb_active_chunk *c, const zb_chunk_terrain *ct);
  void (*let_go)(void *ctx, void *data);
} zb_active_hooks;

typedef struct zb_active {
  const zb_allocator *a;
  zb_world *world;
  double radius;
  int stage; /* the chunk data it needs (ZB_STAGE_SURFACE or ZB_STAGE_ECOLOGY) */
  zb_active_hooks hooks;
  int count;
  zb_active_chunk *slot[ZB_ACTIVE_MAX];
} zb_active;

void zb_active_init(zb_active *g, const zb_allocator *a, zb_world *world, double radius, int stage,
                    const zb_active_hooks *hooks);
/* Lets every chunk go (and follows another world when world is not NULL). */
void zb_active_clear(zb_active *g, zb_world *world);
/* Lets go of chunks that are no longer wanted or loaded, takes in new ones,
 * and relinks the cells; true when the set changed. */
bool zb_active_survey(zb_active *g, const zb_planet_pos *focus, int focus_count);

int zb_active_slot(const zb_active *g, zb_chunk c);
static inline zb_active_chunk *zb_active_chunk_of(const zb_active *g, int32_t cell) {
  return g->slot[cell / ZB_ACTIVE_CELLS];
}
/* The cell (x, y) of a slot's chunk, x and y at most one beyond its edges;
 * -1 when that lies outside the active tier. */
int32_t zb_active_locate(const zb_active *g, int slot, int x, int y);
/* The cell at (x, y) of a chunk's grid, anywhere (through the planet). */
int32_t zb_active_find(const zb_active *g, zb_chunk c, double x, double y);
/* The cell at a position, or -1. */
int32_t zb_active_cell_at(const zb_active *g, const zb_planet_pos *pos);
/* The centre of a cell, at altitude alt. */
zb_planet_pos zb_active_cell_pos(const zb_active *g, int32_t cell, float alt);
/* Calls fn for the cells within radius metres of pos; returns how many it accepted. */
int zb_active_around(const zb_active *g, const zb_planet_pos *pos, double radius, bool (*fn)(void *ctx, int32_t cell),
                     void *ctx);

#endif

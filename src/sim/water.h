/* SPDX-License-Identifier: MIT */
/* Surface water in the active tier (concept 5.7): puddles, runoff, floods and
 * water from spells, on the 2 m cells of the active grid. A shallow-water
 * pipe model moves water between neighbouring cells by the difference of
 * their water surfaces; volume is conserved, so every cubic metre is either
 * on the ground, soaked into it, drained into the sea, a lake or a river (the
 * static water bodies of the terrain), or rained in or added from outside.
 *
 * Rain soaks into the ground as fast as its material lets it (sand quickly,
 * rock not at all) until the ground is saturated, and the rest runs off.
 * Chunks whose water is still go to sleep. Everything is deterministic. */
#ifndef ZB_SIM_WATER_H
#define ZB_SIM_WATER_H

#include "sim/active.h"

#define ZB_WATER_STEP 0.05 /* seconds per step (20 Hz) */

typedef struct zb_surface_water zb_surface_water;

typedef struct zb_water_cell {
  bool loaded;     /* the place is in the active tier */
  bool fixed;      /* sea, lake or river: a water body of the terrain */
  double ground;   /* metres above sea level */
  double depth;    /* metres of water on it */
  double level;    /* the water's surface (the ground when dry) */
  double soaked;   /* metres of water held in the ground */
  double capacity; /* metres the ground can hold */
  double flow;     /* m^3/s leaving the cell */
  double vx, vy;   /* m/s of the water along the chunk's x and y */
  bool frozen;     /* ice: it neither flows nor takes water in, and can be walked on */
} zb_water_cell;

/* Where the water is, in m^3. on_ground + soaked + drained + left + removed
 * = rained + added (up to float rounding). */
typedef struct zb_water_stats {
  int chunks, awake;
  int wet; /* cells with more than a centimetre of water */
  double on_ground, soaked, drained, rained, added;
  double left;      /* standing in chunks when they left the active tier */
  double removed;   /* taken away (magic drying it up) */
  double max_depth; /* metres */
  double max_flow;  /* m^3/s out of the busiest cell */
} zb_water_stats;

zb_surface_water *zb_surface_water_create(const zb_allocator *a, zb_world *world, double radius);
void zb_surface_water_destroy(zb_surface_water *w);
void zb_surface_water_set_world(zb_surface_water *w, zb_world *world);
void zb_surface_water_set_rain(zb_surface_water *w, double mm_per_hour);
/* The ground of a chunk changed (dug, raised): its cells take the new heights
 * and the water on them moves again. */
void zb_surface_water_ground_changed(zb_surface_water *w, const zb_chunk_terrain *ct);
/* Takes in chunks near the focus points and advances by whole steps (real seconds). */
void zb_surface_water_update(zb_surface_water *w, const zb_planet_pos *focus, int focus_count, double seconds);
/* Pours cubic metres of water over the cells within radius metres (a spell,
 * a spring, a broken dam); returns the cells that got some. */
int zb_surface_water_add(zb_surface_water *w, const zb_planet_pos *pos, double radius, double cubic_metres);
void zb_surface_water_cell_at(const zb_surface_water *w, const zb_planet_pos *pos, zb_water_cell *out);
/* Takes up to cubic_metres of standing water from the cells within radius
 * (in proportion to what they hold); returns how much it took. */
double zb_surface_water_remove(zb_surface_water *w, const zb_planet_pos *pos, double radius, double cubic_metres);
/* Freezes the water (standing or a water body) within radius for seconds of
 * simulated time, or thaws it; returns the cells changed. */
int zb_surface_water_freeze(zb_surface_water *w, const zb_planet_pos *pos, double radius, double seconds);
int zb_surface_water_thaw(zb_surface_water *w, const zb_planet_pos *pos, double radius);
void zb_surface_water_get_stats(const zb_surface_water *w, zb_water_stats *out);
uint64_t zb_surface_water_hash(const zb_surface_water *w);
/* Calls fn for every cell with more than min_depth metres of water (not the
 * terrain's water bodies): its centre on the water's surface and its depth. */
typedef void (*zb_water_cell_fn)(void *ctx, const zb_planet_pos *pos, double depth);
void zb_surface_water_each_wet(const zb_surface_water *w, double min_depth, zb_water_cell_fn fn, void *ctx);

/* A chunk's water, for drawing: its cells' ground and depth (row-major,
 * ZB_ACTIVE_RES per row; depth 0 on the terrain's water bodies), and a version
 * that changes whenever its water moved. */
typedef struct zb_water_chunk_view {
  zb_chunk chunk;
  const float *ground;
  const double *depth;
  const float *level; /* the water bodies' surface (ground where there is none) */
  const uint8_t *ice; /* 1 for frozen cells; NULL when none is */
  uint32_t version;
} zb_water_chunk_view;

int zb_surface_water_chunk_count(const zb_surface_water *w);
void zb_surface_water_chunk(const zb_surface_water *w, int index, zb_water_chunk_view *out);

#endif

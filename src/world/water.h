/* SPDX-License-Identifier: MIT */
/* Water bodies of a created world: lakes at the level their basins fill to,
 * and rivers along the macro grid's drainage. A river runs from a macro cell
 * to its receiver as a smooth, meandering centre line through the cell
 * centres (so tributaries meet it), with a channel whose width, depth and
 * speed follow its discharge, and a water surface that falls downstream. All
 * of it is computed on demand from the world bundle: nothing is stored. */
#ifndef ZB_WORLD_WATER_H
#define ZB_WORLD_WATER_H

#include "world/bundle.h"

typedef enum zb_water_kind { ZB_WATER_NONE, ZB_WATER_OCEAN, ZB_WATER_LAKE, ZB_WATER_RIVER } zb_water_kind;

#define ZB_RIVER_MIN_DISCHARGE 8.0 /* m^3/s: the smallest channel, as climate's rivers */
#define ZB_RIVER_PIECES 24         /* straight pieces per macro segment */

/* One straight piece of a river's centre line. */
typedef struct zb_river_piece {
  zb_dvec3 a, b;               /* unit directions of its ends, a upstream */
  double surface_a, surface_b; /* water level at the ends, metres above sea level */
  double half_width, depth, speed;
} zb_river_piece;

/* The river pieces near some place, gathered once and queried many times. */
typedef struct zb_river_set {
  const zb_allocator *a;
  zb_river_piece *piece;
  size_t count, cap;
} zb_river_set;

/* The nearest river centre line to a direction. */
typedef struct zb_river_hit {
  double distance; /* metres from the centre line */
  double surface;  /* water level there */
  double half_width, depth, speed;
  zb_dvec3 along;     /* unit tangent, downstream */
  zb_dvec3 to_centre; /* unit tangent towards the centre line (zero on it) */
} zb_river_hit;

/* Hydraulic geometry of a channel carrying q m^3/s: width and mean depth in
 * metres, mean speed in m/s (width x depth x speed = q). */
void zb_river_geometry(double q, double *width, double *depth, double *speed);
/* The water level of a macro cell: the fill level on land and in lakes, 0 at sea. */
double zb_water_cell_level(const zb_world_bundle *b, int32_t cell);
/* True when a channel leaves the cell (enough discharge, not the sea). */
bool zb_water_cell_has_channel(const zb_world_bundle *b, int32_t cell);

void zb_river_set_init(zb_river_set *s, const zb_allocator *a);
void zb_river_set_free(zb_river_set *s);
/* Appends the pieces of the channel leaving one cell. */
zb_result zb_rivers_add_cell(const zb_world_bundle *b, int32_t cell, zb_river_set *out);
/* Replaces the set with every piece that comes within radius metres of dir. */
zb_result zb_rivers_gather(const zb_world_bundle *b, zb_dvec3 dir, double radius, zb_river_set *out);
/* Keeps only the pieces whose banks (plus margin metres) come within radius
 * metres of dir: the ones that can carve or flood around there. */
void zb_rivers_keep_near(zb_river_set *s, double planet_radius, zb_dvec3 dir, double radius, double margin);
/* Copies to out (room for s->count pieces) the pieces whose centre line comes
 * within radius metres of dir; returns how many. */
size_t zb_rivers_select(const zb_river_set *s, double planet_radius, zb_dvec3 dir, double radius, zb_river_piece *out);
/* The nearest centre line within max_distance metres; false when there is none. */
bool zb_rivers_nearest(const zb_river_set *s, double planet_radius, zb_dvec3 dir, double max_distance,
                       zb_river_hit *out);

/* How a river shapes the land around it (heights in metres above sea level). */
double zb_river_reach(const zb_river_hit *h); /* half width plus its banks */
double zb_river_carve(const zb_river_hit *h, double height);
double zb_river_hill_factor(const zb_river_hit *h); /* 0 at the river .. 1 away from its valley */

/* The level of a lake that reaches dir (its own cell or a neighbouring lake
 * cell whose shore rises above the level); false when there is none. */
bool zb_lake_level(const zb_world_bundle *b, zb_dvec3 dir, double *level);

#endif

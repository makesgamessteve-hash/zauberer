/* SPDX-License-Identifier: MIT */
/* The ley network (concept 3.2): nexuses on high ground, each with two mana
 * colours, joined by ley lines along great circles to their nearest
 * neighbours. It is made once per world from its seed and macro heights, so
 * every peer and every save has the same network. */
#ifndef ZB_WORLD_LEY_H
#define ZB_WORLD_LEY_H

#include "world/terrain.h"

#define ZB_LEY_NEXUSES 64
#define ZB_LEY_MAX_LINES (3 * ZB_LEY_NEXUSES)

typedef struct zb_ley_nexus {
  zb_dvec3 dir;      /* unit vector from the planet's centre */
  uint8_t colors[2]; /* zb_mana_color values of its mana */
  float power;       /* 0.5 .. 1 */
} zb_ley_nexus;

typedef struct zb_ley_line {
  uint8_t a, b; /* nexus indices */
} zb_ley_line;

typedef struct zb_ley {
  double radius; /* the planet's, metres */
  zb_ley_nexus nexus[ZB_LEY_NEXUSES];
  zb_ley_line line[ZB_LEY_MAX_LINES];
  int lines;
  double strength; /* 0 .. 1: the network's strength (it weakens over the story) */
} zb_ley;

typedef struct zb_ley_sample {
  double line_distance;  /* metres to the nearest ley line */
  int line;              /* its index */
  double nexus_distance; /* metres to the nearest nexus */
  int nexus;             /* its index */
} zb_ley_sample;

/* Builds the network of a world (radius in metres). */
void zb_ley_build(zb_ley *l, const zb_terrain *t, double radius, uint64_t seed);
/* The nearest line and nexus to a direction (a unit vector). */
void zb_ley_sample_at(const zb_ley *l, zb_dvec3 dir, zb_ley_sample *out);
/* The two colours a line carries: those of its nexuses' first colours. */
void zb_ley_line_colors(const zb_ley *l, int line, int *a, int *b);

#endif

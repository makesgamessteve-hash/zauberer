/* SPDX-License-Identifier: MIT */
/* The active tier (concept 5.2, 5.3): its simulations (heat and fire with the
 * material registry, surface water, air), stepped at their own rates around
 * the world's focus points and coupled to each other:
 *
 * - the weather in use (the world's own at the first focus point, once a
 *   second, unless the scripts set one) drives them all, and storms send
 *   lightning;
 * - fire smokes into the air and leans with the air's local wind;
 * - standing water puts fire out and soaks its fuel;
 * - rigid bodies float in the water and wake when the ground under them
 *   changes;
 * - the mana field draws its richness from all of them and the weather;
 * - creatures sense fire, water, scent and bodies, and are hurt by them.
 *
 * It times every simulation against the budget for the whole tier. */
#ifndef ZB_SIM_TIER_H
#define ZB_SIM_TIER_H

#include "sim/air.h"
#include "sim/bodies.h"
#include "sim/creatures.h"
#include "sim/fire.h"
#include "sim/mana.h"
#include "sim/water.h"
#include "world/weather.h"

#define ZB_TIER_BUDGET_MS 6.0 /* per 60 Hz tick, desktop (concept 5.15) */

typedef enum zb_tier_system {
  ZB_TIER_FIRE,
  ZB_TIER_WATER,
  ZB_TIER_AIR,
  ZB_TIER_WEATHER,
  ZB_TIER_BODIES,
  ZB_TIER_MANA,
  ZB_TIER_CREATURES,
  ZB_TIER_SYSTEMS
} zb_tier_system;

typedef struct zb_tier_config {
  const zb_allocator *a;
  zb_world *world;
  const zb_materials *materials;   /* NULL: no fire and no bodies */
  double radius;                   /* metres around each focus point */
  uint64_t seed;                   /* lightning and creatures */
  uint64_t (*clock_ns)(void *ctx); /* for timing; NULL: not timed */
  void *clock_ctx;
} zb_tier_config;

typedef struct zb_tier_stats {
  double ms[ZB_TIER_SYSTEMS]; /* per tick, averaged over the last second */
  double total_ms;
  double budget_ms;
  int over_budget; /* seconds over the budget so far */
} zb_tier_stats;

typedef struct zb_tier zb_tier;

zb_tier *zb_tier_create(const zb_tier_config *cfg);
void zb_tier_destroy(zb_tier *t);
void zb_tier_set_world(zb_tier *t, zb_world *world);
zb_fire *zb_tier_fire(const zb_tier *t);
zb_surface_water *zb_tier_water(const zb_tier *t);
zb_air *zb_tier_air(const zb_tier *t);
zb_bodies *zb_tier_bodies(const zb_tier *t); /* NULL without materials */
zb_mana *zb_tier_mana(const zb_tier *t);
zb_creatures *zb_tier_creatures(const zb_tier *t);
zb_weather_control *zb_tier_weather(zb_tier *t);
/* Advances everything by one tick of seconds around the world's focus points. */
void zb_tier_update(zb_tier *t, double seconds);
/* Applies a weather to the simulations now (the scripts' weather). */
void zb_tier_apply_weather(zb_tier *t, const zb_weather *w);
/* Lightning at a place: fire in the dry fuel there, counted; the cells that caught. */
int zb_tier_lightning(zb_tier *t, const zb_planet_pos *pos);
void zb_tier_get_stats(const zb_tier *t, zb_tier_stats *out);
/* The radius simulated around each focus point (several areas, one per
 * separated heir, share the budget by shrinking it). Returns the old one. */
double zb_tier_set_radius(zb_tier *t, double radius);
double zb_tier_radius(const zb_tier *t);
/* The ground of these chunks changed (zb_world_deform): the simulations take
 * the new heights, and water moves again. */
void zb_tier_ground_changed(zb_tier *t, const zb_chunk *chunks, int count);

#endif

/* SPDX-License-Identifier: MIT */
/* Mana in the active tier (concept 3.2): a field of eight colours on 32 m
 * cells (8 per chunk edge).
 *
 * Each cell's richness per colour comes from the physical state, so changing
 * the world changes the mana:
 * - Fire from warm climates, hot dry sand at midday, fresh ash and burning fuel;
 * - Storm from wind, height, exposed slopes and thunderstorms;
 * - Light from daylight under a clear sky, the moon, snow and ice;
 * - Life from living plants;
 * - Water from water bodies, standing water, rain and wet climates;
 * - Earth from rock, gravel, mountains and steep ground;
 * - Void from the night and from ley lines and nexuses;
 * - Decay from mud, old burn scars and blight.
 * Ley lines also carry their nexuses' colours.
 *
 * A cell stores up to ZB_MANA_CAP mana per unit of richness and fills again
 * over about ZB_MANA_REFILL seconds. Mana spreads slowly between cells and
 * fast along ley lines. Drawing mana takes it from the cells around a place;
 * draining a cell empty again and again (ZB_MANA_BLIGHT_STRAIN times within
 * a few minutes) leaves it blighted for ZB_BLIGHT_SECONDS: little Life, much
 * Decay, and the blight is kept with the world. */
#ifndef ZB_SIM_MANA_H
#define ZB_SIM_MANA_H

#include "sim/air.h"
#include "sim/fire.h"
#include "sim/material.h"
#include "sim/water.h"
#include "world/weather.h"

#define ZB_MANA_RES 8             /* cells per chunk edge (32 m) */
#define ZB_MANA_STEP 0.5          /* seconds per step */
#define ZB_MANA_CAP 20.0          /* mana a cell holds per unit of richness */
#define ZB_MANA_REFILL 300.0      /* seconds: a drained cell fills again */
#define ZB_MANA_LEY_REACH 80.0    /* metres: how far a ley line's flow reaches */
#define ZB_MANA_BLIGHT_STRAIN 5.0 /* times a cell is drained empty (fading over five minutes) that blight it */

typedef struct zb_mana zb_mana;

typedef struct zb_mana_sample {
  bool loaded;
  double stored[ZB_MANA_COLORS]; /* mana in the cell */
  double cap[ZB_MANA_COLORS];    /* what it holds when full */
  double ley_distance;           /* metres to the nearest ley line */
  double nexus_distance;         /* metres to the nearest nexus */
  bool blighted;
} zb_mana_sample;

typedef struct zb_mana_stats {
  int chunks;
  int blighted;                                       /* cells */
  double stored[ZB_MANA_COLORS], cap[ZB_MANA_COLORS]; /* over the active tier */
  double drawn;                                       /* mana drawn so far */
} zb_mana_stats;

/* The other simulations it reads (any may be NULL). */
typedef struct zb_mana_sources {
  const zb_fire *fire;
  const zb_surface_water *water;
  const zb_air *air;
} zb_mana_sources;

zb_mana *zb_mana_create(const zb_allocator *a, zb_world *world, double radius);
/* Changes the radius around each focus point that is simulated. */
void zb_mana_set_radius(zb_mana *m, double radius);
void zb_mana_destroy(zb_mana *m);
void zb_mana_set_world(zb_mana *m, zb_world *world);
void zb_mana_set_sources(zb_mana *m, const zb_mana_sources *s);
/* The weather in use (clouds dim Light, storms feed Storm, rain feeds Water). */
void zb_mana_set_weather(zb_mana *m, const zb_weather *w);
void zb_mana_update(zb_mana *m, const zb_planet_pos *focus, int focus_count, double seconds);
/* The ground of a chunk changed: its cells look at it again. */
void zb_mana_ground_changed(zb_mana *m, zb_chunk c);
void zb_mana_sample_at(const zb_mana *m, const zb_planet_pos *pos, zb_mana_sample *out);
/* Takes up to amount mana of a colour from the cells within radius metres
 * (the nearer the more); returns how much it got. */
double zb_mana_draw(zb_mana *m, const zb_planet_pos *pos, zb_mana_color color, double amount, double radius);
/* Gives mana back to the field (motes dissolving, spells fading). */
void zb_mana_release(zb_mana *m, const zb_planet_pos *pos, zb_mana_color color, double amount);
void zb_mana_get_stats(const zb_mana *m, zb_mana_stats *out);
uint64_t zb_mana_hash(const zb_mana *m);

#endif

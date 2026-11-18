/* SPDX-License-Identifier: MIT */
/* Heat and fire in the active tier (concept 5.5, 5.6): a grid of 2 m cells
 * over every loaded chunk near a focus point. Each cell holds:
 *
 * - its temperature above the ambient air (the local climate, season and
 *   time of day give the ambient);
 * - the fuel on it: plants and litter, with their material from the registry;
 * - the fuel's moisture.
 *
 * Heat flows between neighbours, leaning downwind and uphill, and is lost to
 * the air. A cell ignites when it is dry and hot enough for its fuel, burns
 * its fuel at the material's rate, and heats its neighbours. Embers fly
 * downwind and start spot fires. Rain and water put fires out, and rivers,
 * lakes and bare ground stop them.
 *
 * A burnt-out cell leaves a scar in the world's deltas: ash for a while, and
 * plants that grow back by layer (ground cover first, trees last). Chunks
 * with nothing above ambient cost nothing. Everything is deterministic. */
#ifndef ZB_SIM_FIRE_H
#define ZB_SIM_FIRE_H

#include "sim/material.h"
#include "world/world.h"

#define ZB_FIRE_RES 128       /* cells per chunk edge (about 2 m): the active grid's */
#define ZB_FIRE_MAX_CHUNKS 64 /* chunks simulated at once */
#define ZB_FIRE_STEP 0.05     /* seconds per step (20 Hz) */

typedef struct zb_fire zb_fire;

typedef struct zb_fire_weather {
  double wind_east, wind_north; /* m/s */
  double rain;                  /* mm/h */
} zb_fire_weather;

typedef struct zb_fire_stats {
  int chunks;             /* chunks in the simulation */
  int awake;              /* of those, chunks with heat or fire */
  int burning;            /* cells on fire */
  int burnt;              /* cells burnt out since the start */
  int embers;             /* embers in the air */
  int spots;              /* spot fires embers started */
  double max_temperature; /* deg C, the hottest cell */
} zb_fire_stats;

/* What a cell holds, for scripts and tests. */
typedef struct zb_fire_cell {
  bool loaded; /* the place is in the simulation */
  bool burning, burnt, water;
  double temperature; /* deg C */
  double fuel;        /* kg/m^2 */
  double moisture;    /* kg of water per kg of fuel */
  int material;       /* the fuel's material, -1 for none */
} zb_fire_cell;

/* materials must outlive the simulation; radius: the active tier in metres. */
zb_fire *zb_fire_create(const zb_allocator *a, zb_world *world, const zb_materials *materials, double radius);
void zb_fire_destroy(zb_fire *f);
void zb_fire_set_radius(zb_fire *f, double radius);
/* Drops every chunk and follows another world (a created world replaced the
 * one being played). */
void zb_fire_set_world(zb_fire *f, zb_world *world);
void zb_fire_set_weather(zb_fire *f, const zb_fire_weather *w);
const zb_fire_weather *zb_fire_weather_get(const zb_fire *f);
/* Takes in chunks near the focus points that have their plants, lets go of
 * the others, and advances the simulation by whole steps. Fire runs in real
 * seconds (physical time, like flight); scars are dated in game time. */
void zb_fire_update(zb_fire *f, const zb_planet_pos *focus, int focus_count, double seconds);
/* Sets fire to the fuel within radius metres (lightning, a spell, a torch);
 * returns the cells that caught. */
int zb_fire_ignite(zb_fire *f, const zb_planet_pos *pos, double radius);
/* Adds heat (J) spread over radius metres. */
void zb_fire_add_heat(zb_fire *f, const zb_planet_pos *pos, double radius, double joules);
void zb_fire_cell_at(const zb_fire *f, const zb_planet_pos *pos, zb_fire_cell *out);
void zb_fire_get_stats(const zb_fire *f, zb_fire_stats *out);
/* A hash of the whole state, for determinism checks. */
uint64_t zb_fire_hash(const zb_fire *f);

/* Calls fn for every burning cell: where it is (on the ground), its
 * intensity in W/m^2 and the fuel left in kg/m^2. */
typedef void (*zb_fire_cell_fn)(void *ctx, const zb_planet_pos *pos, double intensity, double fuel);
void zb_fire_each_burning(const zb_fire *f, zb_fire_cell_fn fn, void *ctx);

#endif

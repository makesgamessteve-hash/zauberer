/* SPDX-License-Identifier: MIT */
/* Air in the active tier (concept 5.8): the local wind and the gases it
 * carries, on 8 m cells (32 per chunk edge) of an active grid.
 *
 * The wind is the weather's wind shaped by the ground: faster over ridges and
 * open crests, slower in hollows and under forest canopy, and turned along
 * slopes rather than up them. Gases (smoke, steam, scent and fumes) are
 * concentrations in kg/m^3 of the lowest air layer, taken along by that wind
 * (semi-Lagrangian), spread a little, and lost upwards or condensed at their
 * own rates. Sources emit kilograms per second; chunks with no gas sleep. */
#ifndef ZB_SIM_AIR_H
#define ZB_SIM_AIR_H

#include "sim/active.h"

#define ZB_AIR_RES 32     /* cells per chunk edge (8 m) */
#define ZB_AIR_STEP 0.25  /* seconds per step (4 Hz) */
#define ZB_AIR_LAYER 20.0 /* metres: the air layer a gas mixes into */

typedef enum zb_gas { ZB_GAS_SMOKE, ZB_GAS_STEAM, ZB_GAS_SCENT, ZB_GAS_FUMES, ZB_GASES } zb_gas;

typedef struct zb_air zb_air;

typedef struct zb_air_sample {
  bool loaded;
  double east, north;   /* the local wind, m/s */
  double gas[ZB_GASES]; /* kg/m^3 */
} zb_air_sample;

typedef struct zb_air_stats {
  int chunks, awake;
  double mass[ZB_GASES];    /* kg in the air of the active tier */
  double emitted[ZB_GASES]; /* kg emitted so far */
} zb_air_stats;

zb_air *zb_air_create(const zb_allocator *a, zb_world *world, double radius);
/* Changes the radius around each focus point that is simulated. */
void zb_air_set_radius(zb_air *air, double radius);
void zb_air_destroy(zb_air *air);
void zb_air_set_world(zb_air *air, zb_world *world);
/* The weather's wind (m/s towards east and north). */
void zb_air_set_wind(zb_air *air, double east, double north);
void zb_air_update(zb_air *air, const zb_planet_pos *focus, int focus_count, double seconds);
/* Releases kg of a gas over a second into the air above pos (kg/s x seconds). */
void zb_air_emit(zb_air *air, const zb_planet_pos *pos, zb_gas gas, double kg);
void zb_air_sample_at(const zb_air *air, const zb_planet_pos *pos, zb_air_sample *out);
/* The mean local wind over a chunk (the weather's wind when it is not simulated). */
void zb_air_chunk_wind(const zb_air *air, zb_chunk c, double *east, double *north);
void zb_air_get_stats(const zb_air *air, zb_air_stats *out);
uint64_t zb_air_hash(const zb_air *air);
const char *zb_gas_name(zb_gas gas);
/* Calls fn for every cell with more than min_kg_m3 of a gas: its centre (at
 * the ground) and the concentration. */
typedef void (*zb_air_cell_fn)(void *ctx, const zb_planet_pos *pos, double kg_m3);
void zb_air_each(const zb_air *air, zb_gas gas, double min_kg_m3, zb_air_cell_fn fn, void *ctx);

#endif

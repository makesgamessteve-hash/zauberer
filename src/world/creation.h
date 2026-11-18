/* SPDX-License-Identifier: MIT */
/* World creation: deep-time geology, then the present climate, in bounded
 * steps with progress and cancellation (destroy it), recording a time-lapse
 * of the drifting plates, and finally the world bundle. */
#ifndef ZB_WORLD_CREATION_H
#define ZB_WORLD_CREATION_H

#include "world/bundle.h"

typedef enum zb_creation_stage {
  ZB_CREATION_DEEP_TIME,
  ZB_CREATION_RELIEF, /* amplification and erosion of the macro grid */
  ZB_CREATION_CLIMATE,
  ZB_CREATION_DONE
} zb_creation_stage;

typedef struct zb_creation_progress {
  zb_creation_stage stage;
  const char *label; /* "Drifting continents", ... */
  double fraction;   /* 0..1 of the whole creation */
  double myr_ago;    /* during deep time */
  int frames;        /* time-lapse frames recorded so far */
  int attempt;       /* re-runs of the final phase for Earth-likeness */
} zb_creation_progress;

typedef struct zb_world_creation zb_world_creation;

/* Geology options for a quality preset: quick (mobile), normal, high. */
void zb_world_geology_options(const zb_world_options *o, zb_geology_options *out);
/* NULL and a message on invalid options or when out of memory. */
zb_world_creation *zb_world_creation_create(const zb_allocator *a, const zb_world_options *o, char *err,
                                            size_t err_len);
void zb_world_creation_destroy(zb_world_creation *c);
/* One bounded piece of work; true once everything is done. */
bool zb_world_creation_advance(zb_world_creation *c);
void zb_world_creation_get_progress(const zb_world_creation *c, zb_creation_progress *out);
const zb_world_options *zb_world_creation_options(const zb_world_creation *c);
const zb_geology *zb_world_creation_geology(const zb_world_creation *c);
/* The climate once the climate stage has started (else NULL). */
const zb_climate *zb_world_creation_climate(const zb_world_creation *c);
/* Time-lapse: per lattice point elevation (metres above that era's sea
 * level) and plate for frame 0..frames-1. */
const int16_t *zb_world_creation_frame_elevation(const zb_world_creation *c, int frame);
const uint8_t *zb_world_creation_frame_plates(const zb_world_creation *c, int frame);
/* The bundle, once done. */
zb_result zb_world_creation_bundle(const zb_world_creation *c, const zb_allocator *a, zb_world_bundle *out);

#endif

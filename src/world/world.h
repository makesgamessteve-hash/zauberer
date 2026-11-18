/* SPDX-License-Identifier: MIT */
/* The world: planet, terrain and chunk stream, plus the focus points the
 * stream follows. Created by the engine (or a test) and updated once per frame. */
#ifndef ZB_WORLD_WORLD_H
#define ZB_WORLD_WORLD_H

#include "world/stream.h"

typedef struct zb_world_desc {
  const zb_allocator *a;
  zb_jobs *jobs; /* NULL: inline generation */
  double radius; /* 0: ZB_PLANET_DEFAULT_RADIUS */
  uint64_t seed;
  zb_stream_profile profile;
  uint64_t (*clock_ns)(void *ctx); /* inline time budget */
  void *clock_ctx;
  /* A created world (terrain v2); it must outlive the world, and its radius
   * and seed replace radius and seed. NULL: the noise world. */
  const struct zb_world_bundle *bundle;
} zb_world_desc;

typedef struct zb_world zb_world;

/* Fails (NULL, message in err) for an invalid radius or out of memory. */
zb_world *zb_world_create(const zb_world_desc *desc, char *err, size_t err_len);
void zb_world_destroy(zb_world *w);
const zb_planet *zb_world_planet(const zb_world *w);
const zb_terrain *zb_world_terrain(const zb_world *w);
/* The created world's bundle, or NULL for the noise world. */
const struct zb_world_bundle *zb_world_get_bundle(const zb_world *w);
zb_stream *zb_world_stream(zb_world *w);
/* Sets (focus != NULL) or clears a focus point; index 0..ZB_STREAM_MAX_FOCUS-1. */
bool zb_world_set_focus(zb_world *w, int index, const zb_stream_focus *focus);
/* Streams around the focus points (call once per frame). */
void zb_world_update(zb_world *w);
/* The render origin. zb_world_update moves it to the chunk of focus 1 when
 * that focus is more than ZB_ORIGIN_REBASE_DISTANCE away from it. */
const zb_origin *zb_world_origin(const zb_world *w);
/* Increases whenever the origin moves. */
uint32_t zb_world_origin_version(const zb_world *w);
/* Moves the origin to a chunk now. */
void zb_world_set_origin(zb_world *w, zb_chunk anchor);
/* Ground height (metres above sea level) at a position: from the loaded
 * heightfield when its chunk is at stage 1 or more, else the analytic terrain. */
double zb_world_ground_height(const zb_world *w, const zb_planet_pos *pos);

#endif

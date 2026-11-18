/* SPDX-License-Identifier: MIT */
#include "world/world.h"

#include "world/bundle.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

struct zb_world {
  const zb_allocator *a;
  zb_planet planet;
  zb_terrain terrain;
  zb_stream *stream;
  zb_stream_focus focus[ZB_STREAM_MAX_FOCUS];
  bool active[ZB_STREAM_MAX_FOCUS];
  zb_origin origin;
  uint32_t origin_version;
};

zb_world *zb_world_create(const zb_world_desc *desc, char *err, size_t err_len) {
  zb_world *w = zb_alloc(desc->a, sizeof *w);
  if (!w) {
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  memset(w, 0, sizeof *w);
  w->a = desc->a;
  double radius = desc->radius > 0.0 ? desc->radius : ZB_PLANET_DEFAULT_RADIUS;
  uint64_t seed = desc->seed;
  if (desc->bundle) {
    radius = desc->bundle->options.radius;
    seed = desc->bundle->options.seed;
  }
  if (!zb_planet_init(&w->planet, radius, err, err_len)) {
    zb_free(desc->a, w);
    return NULL;
  }
  zb_terrain_init(&w->terrain, &w->planet, seed, NULL);
  zb_terrain_use_bundle(&w->terrain, desc->bundle);
  zb_terrain_use_flora(&w->terrain, desc->flora);
  zb_chunk start = {ZB_FACE_PX, w->planet.n / 2, w->planet.n / 2};
  zb_origin_init(&w->origin, &w->planet, start);
  zb_stream_config cfg;
  zb_stream_default_config(&cfg, desc->profile);
  cfg.a = desc->a;
  cfg.jobs = desc->jobs;
  cfg.clock_ns = desc->clock_ns;
  cfg.clock_ctx = desc->clock_ctx;
  cfg.ecology_radius = desc->flora ? zb_stream_profile_ecology_radius(desc->profile) : 0.0f;
  w->stream = zb_stream_create(&w->terrain, &cfg);
  if (!w->stream) {
    snprintf(err, err_len, "out of memory");
    zb_free(desc->a, w);
    return NULL;
  }
  return w;
}

void zb_world_destroy(zb_world *w) {
  if (w) {
    zb_stream_destroy(w->stream);
    zb_free(w->a, w);
  }
}

const zb_planet *zb_world_planet(const zb_world *w) { return &w->planet; }
const zb_terrain *zb_world_terrain(const zb_world *w) { return &w->terrain; }
const zb_world_bundle *zb_world_get_bundle(const zb_world *w) { return w->terrain.bundle; }
zb_stream *zb_world_stream(zb_world *w) { return w->stream; }

bool zb_world_set_focus(zb_world *w, int index, const zb_stream_focus *focus) {
  if (index < 0 || index >= ZB_STREAM_MAX_FOCUS) {
    return false;
  }
  w->active[index] = focus != NULL;
  if (focus) {
    w->focus[index] = *focus;
  }
  return true;
}

const zb_origin *zb_world_origin(const zb_world *w) { return &w->origin; }
uint32_t zb_world_origin_version(const zb_world *w) { return w->origin_version; }

void zb_world_set_origin(zb_world *w, zb_chunk anchor) {
  zb_origin_init(&w->origin, &w->planet, anchor);
  w->origin_version++;
}

void zb_world_update(zb_world *w) {
  if (w->active[0]) {
    zb_vec3 l = zb_origin_to_local(&w->origin, &w->planet, &w->focus[0].pos);
    l.y = 0.0f; /* altitude alone never moves the origin */
    if (zb_origin_should_rebase(l, ZB_ORIGIN_REBASE_DISTANCE)) {
      zb_world_set_origin(w, w->focus[0].pos.chunk);
    }
  }
  zb_stream_focus list[ZB_STREAM_MAX_FOCUS];
  int count = 0;
  for (int i = 0; i < ZB_STREAM_MAX_FOCUS; ++i) {
    if (w->active[i]) {
      list[count++] = w->focus[i];
    }
  }
  zb_stream_update(w->stream, list, count);
}

double zb_world_ground_height(const zb_world *w, const zb_planet_pos *pos) {
  const zb_chunk_terrain *ct = zb_stream_find(w->stream, pos->chunk);
  if (ct && ct->stage >= ZB_STAGE_TERRAIN) {
    return (double)zb_chunk_height_at(ct, pos->lx, pos->ly);
  }
  zb_planet_pos ground = *pos;
  ground.alt = 0.0f;
  zb_dvec3 c = zb_planet_to_cart(&w->planet, &ground);
  double len = sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
  zb_dvec3 d = {c.x / len, c.y / len, c.z / len};
  return zb_terrain_height(&w->terrain, d);
}

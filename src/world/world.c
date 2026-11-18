/* SPDX-License-Identifier: MIT */
#include "world/world.h"

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
  if (!zb_planet_init(&w->planet, radius, err, err_len)) {
    zb_free(desc->a, w);
    return NULL;
  }
  zb_terrain_init(&w->terrain, &w->planet, desc->seed, NULL);
  zb_stream_config cfg;
  zb_stream_default_config(&cfg, desc->profile);
  cfg.a = desc->a;
  cfg.jobs = desc->jobs;
  cfg.clock_ns = desc->clock_ns;
  cfg.clock_ctx = desc->clock_ctx;
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

void zb_world_update(zb_world *w) {
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

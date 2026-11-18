/* SPDX-License-Identifier: MIT */
#include "world/world.h"

#include "world/bundle.h"
#include "world/delta.h"

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
  zb_calendar_config calendar;
  double time; /* game seconds */
  zb_deltas *deltas;
};

static bool arrived(void *ctx, zb_chunk_terrain *ct) {
  zb_world *w = ctx;
  return zb_deltas_apply(w->deltas, ct, w->time);
}

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
  zb_calendar_default(&w->calendar);
  if (desc->bundle) {
    w->calendar.axial_tilt = desc->bundle->options.axial_tilt;
  }
  w->time = w->calendar.start;
  zb_chunk start = {ZB_FACE_PX, w->planet.n / 2, w->planet.n / 2};
  zb_origin_init(&w->origin, &w->planet, start);
  zb_stream_config cfg;
  zb_stream_default_config(&cfg, desc->profile);
  cfg.a = desc->a;
  cfg.jobs = desc->jobs;
  cfg.clock_ns = desc->clock_ns;
  cfg.clock_ctx = desc->clock_ctx;
  cfg.ecology_radius = desc->flora ? zb_stream_profile_ecology_radius(desc->profile) : 0.0f;
  cfg.arrived = arrived;
  cfg.arrived_ctx = w;
  w->deltas = zb_deltas_create(desc->a);
  w->stream = w->deltas ? zb_stream_create(&w->terrain, &cfg) : NULL;
  if (!w->stream) {
    zb_deltas_destroy(w->deltas);
    snprintf(err, err_len, "out of memory");
    zb_free(desc->a, w);
    return NULL;
  }
  return w;
}

void zb_world_destroy(zb_world *w) {
  if (w) {
    zb_stream_destroy(w->stream);
    zb_deltas_destroy(w->deltas);
    zb_free(w->a, w);
  }
}

const zb_planet *zb_world_planet(const zb_world *w) { return &w->planet; }
const zb_terrain *zb_world_terrain(const zb_world *w) { return &w->terrain; }
const zb_world_bundle *zb_world_get_bundle(const zb_world *w) { return w->terrain.bundle; }
zb_stream *zb_world_stream(zb_world *w) { return w->stream; }

zb_deltas *zb_world_deltas(zb_world *w) { return w->deltas; }

/* Records an edit and applies it to the chunk if it is loaded. */
static zb_result edit(zb_world *w, zb_chunk c, const zb_edit *e) {
  zb_result r = zb_deltas_add(w->deltas, c, e);
  zb_chunk_terrain *ct = r == ZB_OK ? zb_stream_edit(w->stream, c) : NULL;
  if (ct) {
    zb_deltas_apply(w->deltas, ct, w->time);
  }
  return r;
}

int zb_world_remove_plants(zb_world *w, const zb_planet_pos *pos, double radius) {
  zb_chunk chunks[9];
  chunks[0] = pos->chunk;
  int n = 1 + zb_planet_neighbors(&w->planet, pos->chunk, &chunks[1]), removed = 0;
  for (int k = 0; k < n; ++k) {
    const zb_chunk_terrain *ct = zb_stream_find(w->stream, chunks[k]);
    for (uint32_t i = 0; ct && i < ct->plant_count; ++i) {
      const zb_plant *pl = &ct->plants[i];
      zb_planet_pos at = {chunks[k], (float)pl->x / 256.0f, (float)pl->y / 256.0f, 0.0f};
      if (pl->layer != ZB_PLANT_REMOVED && zb_planet_distance(&w->planet, pos, &at) <= radius) {
        zb_edit e = {ZB_EDIT_PLANT_REMOVED, 0, 0, 0, i};
        if (edit(w, chunks[k], &e) != ZB_OK) {
          return removed;
        }
        ct = zb_stream_find(w->stream, chunks[k]); /* (the same data, re-read after the edit) */
        removed++;
      }
    }
  }
  return removed;
}

zb_result zb_world_burn(zb_world *w, zb_chunk c, const uint16_t *cells, int count) {
  zb_edit e = {ZB_EDIT_BURNT, 0, 0, 0, (uint32_t)(w->time / 60.0)};
  for (int i = 0; i < count; ++i) {
    e.x = (uint16_t)(cells[i] % ZB_BURN_RES);
    e.y = (uint16_t)(cells[i] / ZB_BURN_RES);
    zb_result r = zb_deltas_add(w->deltas, c, &e);
    if (r != ZB_OK) {
      return r;
    }
  }
  zb_chunk_terrain *ct = count > 0 ? zb_stream_edit(w->stream, c) : NULL;
  if (ct) {
    zb_deltas_apply(w->deltas, ct, w->time);
  }
  return ZB_OK;
}

zb_result zb_world_set_material(zb_world *w, const zb_planet_pos *pos, int material) {
  int x = (int)pos->lx, y = (int)pos->ly;
  x = x < 0 ? 0 : (x >= ZB_SURFACE_RES ? ZB_SURFACE_RES - 1 : x);
  y = y < 0 ? 0 : (y >= ZB_SURFACE_RES ? ZB_SURFACE_RES - 1 : y);
  zb_edit e = {ZB_EDIT_MATERIAL, (uint8_t)material, (uint16_t)x, (uint16_t)y, 0};
  return material > ZB_MAT_NONE && material < ZB_MAT_COUNT ? edit(w, pos->chunk, &e) : ZB_ERR_INVALID;
}

const zb_calendar_config *zb_world_calendar(const zb_world *w) { return &w->calendar; }
double zb_world_time(const zb_world *w) { return w->time; }
void zb_world_set_time(zb_world *w, double time) { w->time = time > 0.0 ? time : 0.0; }
void zb_world_advance_time(zb_world *w, double real_seconds) {
  w->time += zb_calendar_game_seconds(&w->calendar, real_seconds);
}

int zb_world_focus(const zb_world *w, zb_planet_pos *out, int max) {
  int n = 0;
  for (int i = 0; i < ZB_STREAM_MAX_FOCUS && n < max; ++i) {
    if (w->active[i]) {
      out[n++] = w->focus[i].pos;
    }
  }
  return n;
}

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

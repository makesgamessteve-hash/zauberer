/* SPDX-License-Identifier: MIT */
#include "world/world.h"

#include "core/dmath.h"
#include "world/bundle.h"
#include "world/delta.h"
#include "world/dvec.h"

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
  double time;      /* game seconds */
  double healed_at; /* when burnt chunks were last looked at (game seconds; < 0: not yet) */
  zb_deltas *deltas;
  zb_ley ley;
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
  zb_ley_build(&w->ley, &w->terrain, radius, seed);
  zb_calendar_default(&w->calendar);
  if (desc->bundle) {
    w->calendar.axial_tilt = desc->bundle->options.axial_tilt;
  }
  w->time = w->calendar.start;
  w->healed_at = -1.0;
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
const zb_ley *zb_world_ley(const zb_world *w) { return &w->ley; }
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

zb_result zb_world_blight(zb_world *w, zb_chunk c, int x, int y, double when) {
  zb_edit e = {ZB_EDIT_BLIGHT, 0, (uint16_t)x, (uint16_t)y, (uint32_t)((when > 0.0 ? when : 0.0) / 60.0)};
  return x >= 0 && y >= 0 ? zb_deltas_add(w->deltas, c, &e) : ZB_ERR_INVALID;
}

zb_result zb_world_burn(zb_world *w, zb_chunk c, const uint16_t *cells, int count, double when) {
  zb_edit e = {ZB_EDIT_BURNT, 0, 0, 0, (uint32_t)((when > 0.0 ? when : 0.0) / 60.0)};
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

static int clamp_sample(double v) { return v < 0.0 ? 0 : (v > ZB_HEIGHT_RES - 1 ? ZB_HEIGHT_RES - 1 : (int)v); }

int zb_world_deform(zb_world *w, const zb_planet_pos *pos, double radius, double amount, zb_chunk *changed, int max) {
  zb_chunk chunks[9];
  chunks[0] = pos->chunk;
  int candidates = 1 + zb_planet_neighbors(&w->planet, pos->chunk, &chunks[1]), n = 0;
  zb_dvec3 at = zb_planet_to_cart(&w->planet, &(zb_planet_pos){pos->chunk, pos->lx, pos->ly, 0.0f});
  for (int k = 0; k < candidates && n < max; ++k) {
    const zb_chunk_terrain *ct = zb_stream_find(w->stream, chunks[k]);
    if (!ct || ct->stage < ZB_STAGE_TERRAIN) {
      continue;
    }
    /* Sample positions across a chunk are a plane to within millimetres. */
    zb_planet_pos p00 = {chunks[k], 0.0f, 0.0f, 0.0f}, p10 = {chunks[k], 256.0f, 0.0f, 0.0f},
                  p01 = {chunks[k], 0.0f, 256.0f, 0.0f};
    zb_dvec3 o = zb_planet_to_cart(&w->planet, &p00);
    zb_dvec3 ux = zb_dv_scale(zb_dv_sub(zb_planet_to_cart(&w->planet, &p10), o), 1.0 / 256.0);
    zb_dvec3 uy = zb_dv_scale(zb_dv_sub(zb_planet_to_cart(&w->planet, &p01), o), 1.0 / 256.0);
    /* Only the samples around the place's projection onto this chunk. */
    zb_dvec3 rel = zb_dv_sub(at, o);
    double xx = zb_dv_dot(ux, ux), xy = zb_dv_dot(ux, uy), yy = zb_dv_dot(uy, uy), rx = zb_dv_dot(rel, ux),
           ry = zb_dv_dot(rel, uy), det = xx * yy - xy * xy; /* the chunk's axes need not be square */
    double cx = (rx * yy - ry * xy) / det, cy = (ry * xx - rx * xy) / det;
    int x0 = clamp_sample(cx - radius - 2.0), x1 = clamp_sample(cx + radius + 2.0);
    int y0 = clamp_sample(cy - radius - 2.0), y1 = clamp_sample(cy + radius + 2.0);
    int edits = 0;
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        zb_dvec3 s = zb_dv_add(o, zb_dv_add(zb_dv_scale(ux, x), zb_dv_scale(uy, y)));
        double d = zb_dv_len(zb_dv_sub(s, at));
        if (d >= radius) {
          continue;
        }
        double c = zb_dcos(0.5 * ZB_DPI * d / radius);
        float h = (float)(ct->height[y * ZB_HEIGHT_RES + x] + amount * c * c);
        uint32_t bits;
        memcpy(&bits, &h, sizeof bits);
        zb_edit e = {ZB_EDIT_HEIGHT, 0, (uint16_t)x, (uint16_t)y, bits};
        if (zb_deltas_add(w->deltas, chunks[k], &e) == ZB_OK) {
          edits++;
        }
      }
    }
    zb_chunk_terrain *edit = edits ? zb_stream_edit(w->stream, chunks[k]) : NULL;
    if (edit) {
      zb_deltas_apply(w->deltas, edit, w->time);
      changed[n++] = chunks[k];
    }
  }
  if (n > 0 && fabs(amount) > 0.5) {
    zb_world_remove_plants(w, pos, 0.8 * radius); /* dug up or buried */
  }
  return n;
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

#define HEAL_CHECK 3600.0 /* game seconds between looks at burnt chunks */

typedef struct heal_span {
  zb_world *w;
  double from, to;
} heal_span;

/* A loaded chunk whose burn turned a corner (ash gone, plants back) is
 * generated again from its surface up, with the edits applied anew. */
static void heal_chunk(void *ctx, zb_chunk c) {
  heal_span *h = ctx;
  if (zb_deltas_burn_due(h->w->deltas, c, h->from, h->to)) {
    zb_stream_regenerate(h->w->stream, c, ZB_STAGE_SURFACE);
  }
}

static void heal(zb_world *w) {
  if (w->healed_at < 0.0 || w->time < w->healed_at) {
    w->healed_at = w->time;
  } else if (w->time - w->healed_at >= HEAL_CHECK) {
    heal_span h = {w, w->healed_at, w->time};
    zb_deltas_each_chunk(w->deltas, heal_chunk, &h);
    w->healed_at = w->time;
  }
}

void zb_world_update(zb_world *w) {
  heal(w);
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

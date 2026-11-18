/* SPDX-License-Identifier: MIT */
#include "world/creation.h"

#include <stdio.h>
#include <string.h>

struct zb_world_creation {
  const zb_allocator *a;
  zb_world_options options;
  zb_geology *geology;
  zb_climate *climate;
  zb_climate_link link;
  zb_creation_stage stage;
  int frames, frame_cap, points;
  int16_t *elevation; /* frame_cap * points */
  uint8_t *plates;
};

void zb_world_geology_options(const zb_world_options *o, zb_geology_options *g) {
  zb_geology_default_options(g, o->radius);
  g->target = o->target;
  g->land_fraction = o->land_fraction;
  double cell_km = 6.0;
  switch (o->quality) {
    case ZB_WORLD_QUICK:
      g->points = 12000;
      g->steps = 80;
      g->erosion_iterations = 8;
      cell_km = 12.0;
      break;
    case ZB_WORLD_HIGH:
      g->points = 80000;
      g->steps = 160;
      g->erosion_iterations = 16;
      cell_km = 4.0;
      break;
    case ZB_WORLD_NORMAL:
    default: break;
  }
  int res = (int)(o->radius * 1.5707963267948966 / (cell_km * 1000.0) + 0.5);
  g->macro_res = res < 16 ? 16 : (res > 1024 ? 1024 : res);
}

static bool valid_options(const zb_world_options *o, char *err, size_t err_len) {
  const char *why = NULL;
  if ((int)o->quality < 0 || o->quality >= ZB_WORLD_QUALITY_COUNT) {
    why = "unknown quality";
  } else if (!(o->temperature_offset >= -10.0 && o->temperature_offset <= 10.0)) {
    why = "temperature offset must be -10..10";
  } else if (!(o->axial_tilt >= 0.0 && o->axial_tilt <= 45.0)) {
    why = "axial tilt must be 0..45 degrees";
  }
  if (why) {
    snprintf(err, err_len, "%s", why);
  }
  return why == NULL;
}

zb_world_creation *zb_world_creation_create(const zb_allocator *a, const zb_world_options *o, char *err,
                                            size_t err_len) {
  if (!valid_options(o, err, err_len)) {
    return NULL;
  }
  zb_geology_options go;
  zb_world_geology_options(o, &go);
  zb_world_creation *c = zb_calloc(a, sizeof *c);
  if (!c) {
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  c->a = a;
  c->options = *o;
  c->geology = zb_geology_create(a, o->seed, &go, err, err_len);
  if (!c->geology) {
    zb_free(a, c);
    return NULL;
  }
  c->points = go.points;
  c->frame_cap = go.steps;
  c->elevation = zb_alloc(a, sizeof(int16_t) * (size_t)c->frame_cap * (size_t)c->points);
  c->plates = zb_alloc(a, (size_t)c->frame_cap * (size_t)c->points);
  if (!c->elevation || !c->plates) {
    zb_world_creation_destroy(c);
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  c->stage = ZB_CREATION_DEEP_TIME;
  return c;
}

void zb_world_creation_destroy(zb_world_creation *c) {
  if (!c) {
    return;
  }
  zb_climate_destroy(c->climate);
  zb_climate_link_free(&c->link);
  zb_geology_destroy(c->geology);
  zb_free(c->a, c->elevation);
  zb_free(c->a, c->plates);
  zb_free(c->a, c);
}

/* Records the state after a tectonic step (re-runs of the final phase overwrite their frames). */
static void record_frame(zb_world_creation *c, int step) {
  if (step < 1 || step > c->frame_cap) {
    return;
  }
  zb_geology_points pts;
  zb_geology_get_points(c->geology, &pts);
  int count;
  const zb_geology_era *eras = zb_geology_eras(c->geology, &count);
  float sea = count >= step ? eras[step - 1].sea_level : 0.0f;
  int16_t *e = c->elevation + (size_t)(step - 1) * (size_t)c->points;
  uint8_t *p = c->plates + (size_t)(step - 1) * (size_t)c->points;
  for (int i = 0; i < c->points; ++i) {
    float h = pts.elevation[i] - sea;
    e[i] = (int16_t)(h > 32000.0f ? 32000.0f : (h < -32000.0f ? -32000.0f : h));
    p[i] = pts.plate[i];
  }
  c->frames = step > c->frames ? step : c->frames;
}

static bool start_climate(zb_world_creation *c) {
  zb_climate_input in;
  if (zb_climate_input_from_geology(&in, &c->link, c->a, c->geology, c->options.seed) != ZB_OK) {
    return false;
  }
  in.axial_tilt = c->options.axial_tilt;
  in.temperature_offset = c->options.temperature_offset;
  char err[128];
  c->climate = zb_climate_create(c->a, &in, err, sizeof err);
  return c->climate != NULL;
}

bool zb_world_creation_advance(zb_world_creation *c) {
  zb_geology_progress gp;
  switch (c->stage) {
    case ZB_CREATION_DEEP_TIME:
    case ZB_CREATION_RELIEF: {
      zb_geology_phase phase = zb_geology_advance(c->geology);
      zb_geology_get_progress(c->geology, &gp);
      if (phase == ZB_GEO_PHASE_TECTONICS || c->stage == ZB_CREATION_DEEP_TIME) {
        record_frame(c, gp.step);
      }
      if (phase == ZB_GEO_PHASE_DONE) {
        if (!zb_geology_get_macro(c->geology) || !start_climate(c)) {
          c->stage = ZB_CREATION_DONE; /* out of memory: no bundle (zb_world_creation_bundle reports it) */
          return true;
        }
        c->stage = ZB_CREATION_CLIMATE;
      } else if (phase != ZB_GEO_PHASE_TECTONICS) {
        c->stage = ZB_CREATION_RELIEF;
      }
      return false;
    }
    case ZB_CREATION_CLIMATE:
      if (zb_climate_advance(c->climate) == ZB_CLIMATE_PHASE_DONE) {
        c->stage = ZB_CREATION_DONE;
      }
      return c->stage == ZB_CREATION_DONE;
    case ZB_CREATION_DONE:
    default: return true;
  }
}

void zb_world_creation_get_progress(const zb_world_creation *c, zb_creation_progress *out) {
  static const char *const labels[] = {"Drifting continents", "Raising mountains, carving valleys",
                                       "Winds, rain and rivers", "Done"};
  zb_geology_progress gp;
  zb_geology_get_progress(c->geology, &gp);
  memset(out, 0, sizeof *out);
  out->stage = c->stage;
  out->label = labels[c->stage];
  out->myr_ago = gp.myr_ago;
  out->frames = c->frames;
  out->attempt = gp.attempt;
  /* Geology takes about 85 % of the time, climate the rest. */
  switch (c->stage) {
    case ZB_CREATION_DEEP_TIME:
    case ZB_CREATION_RELIEF: out->fraction = 0.85 * gp.fraction; break;
    case ZB_CREATION_CLIMATE: out->fraction = 0.85 + 0.15 * zb_climate_progress(c->climate); break;
    case ZB_CREATION_DONE:
    default: out->fraction = 1.0; break;
  }
}

const zb_world_options *zb_world_creation_options(const zb_world_creation *c) { return &c->options; }
const zb_geology *zb_world_creation_geology(const zb_world_creation *c) { return c->geology; }
const zb_climate *zb_world_creation_climate(const zb_world_creation *c) { return c->climate; }

const int16_t *zb_world_creation_frame_elevation(const zb_world_creation *c, int frame) {
  return frame >= 0 && frame < c->frames ? c->elevation + (size_t)frame * (size_t)c->points : NULL;
}

const uint8_t *zb_world_creation_frame_plates(const zb_world_creation *c, int frame) {
  return frame >= 0 && frame < c->frames ? c->plates + (size_t)frame * (size_t)c->points : NULL;
}

zb_result zb_world_creation_bundle(const zb_world_creation *c, const zb_allocator *a, zb_world_bundle *out) {
  memset(out, 0, sizeof *out);
  if (c->stage != ZB_CREATION_DONE || !c->climate) {
    return ZB_ERR_STATE;
  }
  return zb_world_bundle_build(a, &c->options, c->geology, c->climate, out);
}

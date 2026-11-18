/* SPDX-License-Identifier: MIT */
#include "app/daylight.h"

#include <math.h>
#include <string.h>

#include "core/dmath.h"
#include "world/dvec.h"

/* A planet direction in render coordinates (x east, y up, z south). */
static zb_vec3 to_local(const zb_origin *o, zb_dvec3 v) {
  return zb_v3((float)zb_dv_dot(v, o->east), (float)zb_dv_dot(v, o->up), (float)-zb_dv_dot(v, o->north));
}

static zb_vec3 mix3(zb_vec3 a, zb_vec3 b, float t) { return zb_v3_add(a, zb_v3_scale(zb_v3_sub(b, a), t)); }

static float clampf01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

void zb_daylight_compute(const zb_calendar_config *c, double time, const zb_origin *o, zb_vec3 eye, zb_daylight *out) {
  memset(out, 0, sizeof *out);
  zb_dvec3 place = zb_dv_norm(zb_origin_to_cart(o, eye));
  zb_calendar_sky(c, time, place, &out->sky);
  const zb_sky_place *s = &out->sky;
  float day = (float)s->daylight;
  float sun_el = (float)s->sun_elevation, moon_el = (float)s->moon_elevation;
  /* Twilight: the sun within about 17 degrees of the horizon. */
  float twilight = sun_el > -0.3f ? clampf01(1.0f - fabsf(sun_el) / 0.3f) : 0.0f;
  float moon_up = clampf01(sinf(moon_el) * 4.0f) * (float)s->moon_light;
  zb_atmosphere *a = &out->atmosphere;
  a->enabled = true;
  zb_vec3 night_zenith = zb_v3(0.01f + 0.03f * moon_up, 0.015f + 0.04f * moon_up, 0.05f + 0.08f * moon_up);
  zb_vec3 night_horizon = zb_v3(0.03f + 0.05f * moon_up, 0.05f + 0.06f * moon_up, 0.1f + 0.1f * moon_up);
  a->zenith = mix3(night_zenith, zb_v3(0.2f, 0.4f, 0.78f), day);
  a->horizon = mix3(night_horizon, zb_v3(0.72f, 0.8f, 0.88f), day);
  a->horizon = mix3(a->horizon, zb_v3(0.95f, 0.55f, 0.32f), 0.65f * twilight);
  a->zenith = mix3(a->zenith, zb_v3(0.3f, 0.32f, 0.55f), 0.35f * twilight);
  zb_vec3 sun = to_local(o, s->sun), moon = to_local(o, s->moon);
  a->sun_dir = sun;
  a->sun_color =
      zb_v3_scale(mix3(zb_v3(1.0f, 0.95f, 0.8f), zb_v3(1.0f, 0.55f, 0.3f), twilight), sun_el > -0.05f ? 1.0f : 0.0f);
  a->night = 1.0f - day;
  a->moon_dir = moon;
  a->moon_light = (float)s->moon_light;
  /* Stars are fixed in space while the planet turns under them. */
  double spin = 2.0 * ZB_DPI * time / ZB_DAY_SECONDS;
  double cs = zb_dcos(spin), sn = zb_dsin(spin);
  zb_dvec3 axes[3] = {o->east, o->up, zb_dv_scale(o->north, -1.0)};
  zb_mat4 m = zb_m4_identity();
  for (int k = 0; k < 3; ++k) {
    zb_dvec3 v = axes[k];
    m.m[k * 4 + 0] = (float)(v.x * cs + v.y * sn);
    m.m[k * 4 + 1] = (float)(-v.x * sn + v.y * cs);
    m.m[k * 4 + 2] = (float)v.z;
  }
  a->celestial = m;
  /* The light: the sun by day, the moon by night. */
  if (sun_el > -0.05f) {
    out->light_dir = zb_v3_scale(sun, -1.0f);
    out->light_color =
        zb_v3_scale(mix3(zb_v3(1.0f, 0.95f, 0.85f), zb_v3(1.0f, 0.6f, 0.38f), twilight), 0.15f + 0.85f * day);
  } else {
    out->light_dir = zb_v3_scale(moon, -1.0f);
    out->light_color = zb_v3_scale(zb_v3(0.3f, 0.34f, 0.48f), moon_up);
  }
  out->ambient = mix3(zb_v3(0.05f + 0.06f * moon_up, 0.06f + 0.07f * moon_up, 0.1f + 0.1f * moon_up),
                      zb_v3(0.38f, 0.4f, 0.48f), day);
}

float zb_daylight_snow(double mean, double coldest, double warmest, double height, double season) {
  /* The seasonal cycle peaks half-way through summer (season 1.5). */
  double t = mean + 0.5 * (warmest - coldest) * zb_dsin(2.0 * ZB_DPI * (season - 0.5) / 4.0) -
             0.0065 * (height > 0.0 ? height : 0.0);
  return clampf01((float)(-t / 4.0));
}

/* SPDX-License-Identifier: MIT */
#include "render/retro.h"

#include <math.h>

float zb_fog_factor(float distance, float density) {
  float k = distance * density;
  return 1.0f - expf(-k * k);
}

float zb_fog_density_for(float distance, float amount) {
  if (distance <= 0.0f || amount <= 0.0f) {
    return 0.0f;
  }
  amount = amount >= 1.0f ? 0.9999f : amount;
  return sqrtf(-logf(1.0f - amount)) / distance;
}

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

zb_vec3 zb_sky_color(const zb_atmosphere *a, zb_vec3 ray) {
  /* Elevation above the (dipped) horizon, mapped to a gradient. */
  float s = zb_clampf(zb_v3_dot(ray, a->up), -1.0f, 1.0f);
  float elevation = asinf(s) + a->horizon_dip;
  float t = clamp01(elevation / 0.6f);
  t = t * (2.0f - t);
  zb_vec3 c = zb_v3_lerp(a->horizon, a->zenith, t);
  float sun = zb_v3_dot(ray, a->sun_dir);
  if (sun > 0.0f) {
    float glow = sun * sun;
    glow *= glow;
    glow *= glow;
    glow *= glow; /* sun^16 */
    c = zb_v3_add(c, zb_v3_scale(a->sun_color, 0.25f * glow));
  }
  if (sun > 0.9994f) { /* a disc of about 2 degrees */
    c = a->sun_color;
  }
  return zb_v3(clamp01(c.x), clamp01(c.y), clamp01(c.z));
}

float zb_horizon_dip(double radius, double altitude) {
  if (altitude <= 0.0) {
    return 0.0f;
  }
  return (float)acos(radius / (radius + altitude));
}

float zb_vignette_factor(float strength, float dx, float dy) {
  strength = clamp01(strength);
  if (strength <= 0.0f) {
    return 1.0f;
  }
  float t = clamp01((sqrtf(dx * dx + dy * dy) - 0.35f) / 0.75f);
  return 1.0f - strength * t * t * (3.0f - 2.0f * t);
}

void zb_palette_quantize(const zb_image *lut, const zb_image *bayer, float dither, const float rgb[3], int x, int y,
                         unsigned char out[3]) {
  float t = (float)bayer->rgba[(((size_t)(y & 7) * (size_t)bayer->width) + (size_t)(x & 7)) * 4] / 255.0f;
  int q[3];
  for (int i = 0; i < 3; ++i) {
    float c = clamp01(rgb[i] + (t - 0.5f) * dither);
    q[i] = (int)floorf(c * (float)(ZB_LUT_LEVELS - 1) + 0.5f);
  }
  const unsigned char *p = &lut->rgba[((size_t)q[1] * (size_t)lut->width + (size_t)(q[2] * ZB_LUT_LEVELS + q[0])) * 4];
  out[0] = p[0];
  out[1] = p[1];
  out[2] = p[2];
}

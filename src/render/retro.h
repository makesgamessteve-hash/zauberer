/* SPDX-License-Identifier: MIT */
/* The retro look: palette quantisation with ordered dithering, distance fog
 * and the gradient sky. These CPU functions are the reference for the shaders
 * (assets/base/shaders/post.frag, sky.frag and the fog in the scene shaders)
 * and are used by tests to check rendered images. */
#ifndef ZB_RENDER_RETRO_H
#define ZB_RENDER_RETRO_H

#include "core/math.h"
#include "render/image.h"

#define ZB_LUT_LEVELS 32 /* per channel; the LUT image is 1024 x 32 (x = b * 32 + r, y = g) */

typedef struct zb_atmosphere {
  bool enabled;      /* draw the sky and apply fog */
  zb_vec3 zenith;    /* sky colour overhead */
  zb_vec3 horizon;   /* sky colour at the horizon, also the fog colour */
  zb_vec3 up;        /* local up (render coordinates) */
  float horizon_dip; /* radians the horizon lies below the horizontal plane */
  zb_vec3 sun_dir;   /* direction towards the sun */
  zb_vec3 sun_color;
  float fog_density; /* per metre: fog = 1 - exp(-(distance * density)^2) */
  /* Night (the GPU sky only; zb_sky_color ignores them): */
  float night;       /* 0 day .. 1 night: how bright the stars are */
  zb_vec3 moon_dir;  /* towards the moon */
  float moon_light;  /* lit fraction of the moon's disc (0: no moon drawn) */
  zb_mat4 celestial; /* render directions to the star sphere (columns) */
} zb_atmosphere;

/* Fog amount (0 clear .. 1 fully fogged) at a distance. */
float zb_fog_factor(float distance, float density);
/* Density at which fog reaches `amount` at `distance` metres. */
float zb_fog_density_for(float distance, float amount);
/* Sky colour along a unit view ray. */
zb_vec3 zb_sky_color(const zb_atmosphere *a, zb_vec3 ray);
/* Horizon dip below the horizontal plane at an altitude above a sphere. */
float zb_horizon_dip(double radius, double altitude);
/* Palette colour of an RGB value (0..1) at framebuffer pixel (x, y) (origin
 * bottom-left, as gl_FragCoord), dithered with the 8x8 Bayer image. */
/* Brightness factor (0 .. 1) of the comfort vignette at an offset (dx, dy)
 * from a view's optical centre, in half-view units (1 at the view's edge
 * along each axis): 1 inside the clear centre (0.35), falling smoothly to
 * 1 - strength from 1.1 outwards. strength is clamped to 0 .. 1; 0 is off. */
float zb_vignette_factor(float strength, float dx, float dy);
void zb_palette_quantize(const zb_image *lut, const zb_image *bayer, float dither, const float rgb[3], int x, int y,
                         unsigned char out[3]);

#endif

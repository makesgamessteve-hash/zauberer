/* SPDX-License-Identifier: MIT */
/* Day and night for the renderer: from the sun and moon above the camera, the
 * sky's colours, the stars and the moon, the light and the ambient, and the
 * season the plants and snow show. */
#ifndef ZB_APP_DAYLIGHT_H
#define ZB_APP_DAYLIGHT_H

#include "render/retro.h"
#include "world/calendar.h"
#include "world/planet.h"

typedef struct zb_daylight {
  zb_atmosphere atmosphere; /* colours, sun, moon, stars; up, dip and fog are left to the caller */
  zb_vec3 light_dir;        /* direction the light travels (render coordinates) */
  zb_vec3 light_color;
  zb_vec3 ambient;
  zb_sky_place sky; /* the sky at the camera */
} zb_daylight;

/* The daylight at the camera eye (render coordinates of origin o) at a time. */
void zb_daylight_compute(const zb_calendar_config *c, double time, const zb_origin *o, zb_vec3 eye, zb_daylight *out);
/* Clouds over the daylight (0 clear: unchanged .. 1 overcast): a grey sky,
 * no sun disc, and a dimmer, softer light. */
void zb_daylight_overcast(zb_daylight *d, double cloud);
/* How snowy a place is (0 .. 1) in a season, from its climate (annual mean,
 * coldest and warmest month, deg C at sea level) and height in metres. */
float zb_daylight_snow(double mean, double coldest, double warmest, double height, double season);

#endif

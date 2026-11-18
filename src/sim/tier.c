/* SPDX-License-Identifier: MIT */
#include "sim/tier.h"

void zb_tier_apply_weather(const zb_tier *t, const zb_weather *w) {
  if (t->fire) {
    zb_fire_weather fw = {w->wind_east, w->wind_north, w->rain + w->snow};
    zb_fire_set_weather(t->fire, &fw);
  }
  if (t->air) {
    zb_air_set_wind(t->air, w->wind_east, w->wind_north);
  }
  if (t->water) {
    zb_surface_water_set_rain(t->water, w->rain);
  }
}

/* SPDX-License-Identifier: MIT */
/* The active tier's simulations together: what they share from outside,
 * such as the weather. Any of them may be missing (NULL). */
#ifndef ZB_SIM_TIER_H
#define ZB_SIM_TIER_H

#include "sim/air.h"
#include "sim/fire.h"
#include "sim/water.h"
#include "world/weather.h"

typedef struct zb_tier {
  zb_fire *fire;
  zb_surface_water *water;
  zb_air *air;
} zb_tier;

/* The weather the simulations see: the air's wind; rain on the water; rain
 * and snow wetting the fire's fuel. */
void zb_tier_apply_weather(const zb_tier *t, const zb_weather *w);

#endif

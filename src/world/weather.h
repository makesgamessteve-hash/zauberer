/* SPDX-License-Identifier: MIT */
/* Weather (concept 5.9): a pure function of the world, the place and the game
 * time, so every peer and every save agrees without storing it.
 *
 * Weather systems about 1,500 km across drift with the prevailing winds of
 * their latitude (trade winds, westerlies, polar easterlies) and change over a
 * few days. A place's climate decides how often they bring cloud and rain
 * (deserts seldom, rainforests most days); its season and height decide
 * whether it falls as rain or snow. Warm, heavy cloud builds thunderstorms,
 * and storms blow harder. */
#ifndef ZB_WORLD_WEATHER_H
#define ZB_WORLD_WEATHER_H

#include "world/calendar.h"
#include "world/terrain.h"

typedef struct zb_weather {
  double cloud;       /* 0 clear .. 1 overcast */
  double rain;        /* mm/h of rain */
  double snow;        /* mm/h of snow (as water) */
  double storm;       /* 0 .. 1: thunderstorms (lightning) */
  double wind_east;   /* m/s towards the east */
  double wind_north;  /* m/s towards the north */
  double temperature; /* deg C of the air at the place */
} zb_weather;

/* The weather the game is using: the world's own at the player, unless the
 * scripts set one (tests, spells, story moments). */
typedef struct zb_weather_control {
  bool overridden;
  zb_weather now;
  int strikes; /* lightning strikes so far */
} zb_weather_control;

/* The weather at a position at game time (seconds); the calendar gives the season. */
void zb_weather_at(const zb_terrain *t, const zb_calendar_config *cal, const zb_planet_pos *pos, double time,
                   zb_weather *out);
/* The prevailing wind of a latitude (radians): east and north, m/s. */
void zb_weather_prevailing(double latitude, double *east, double *north);

#endif

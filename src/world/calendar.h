/* SPDX-License-Identifier: MIT */
/* World time and the sky above: a calendar of days, seasons and years, the
 * sun's direction from the planet's spin and axial tilt, and a moon with its
 * phases. Time is game seconds since the world began; a game day is 86,400
 * game seconds and, by default, 40 real minutes. Everything is a pure function
 * of the time, so every peer sees the same sky. */
#ifndef ZB_WORLD_CALENDAR_H
#define ZB_WORLD_CALENDAR_H

#include "world/planet.h"

#define ZB_DAY_SECONDS 86400.0
#define ZB_SEASON_DAYS 12
#define ZB_YEAR_DAYS (4 * ZB_SEASON_DAYS)
#define ZB_MOON_DAYS 12.0 /* new moon to new moon: one per season */

typedef struct zb_calendar_config {
  double day_length; /* real seconds per game day (2,400: 40 minutes) */
  double axial_tilt; /* degrees */
  double start;      /* game seconds the world starts at (a spring morning) */
} zb_calendar_config;

typedef struct zb_date {
  int64_t year;         /* from 0 */
  int season;           /* 0 spring, 1 summer, 2 autumn, 3 winter (northern hemisphere) */
  int day;              /* 0 .. ZB_SEASON_DAYS - 1 within the season */
  int hour, minute;     /* game clock at longitude 0 */
  double year_fraction; /* 0 .. 1 from the spring equinox */
  double day_fraction;  /* 0 .. 1 from midnight at longitude 0 */
} zb_date;

/* The sky at a place. Directions are planet-centred unit vectors. */
typedef struct zb_sky_place {
  zb_dvec3 sun;         /* towards the sun */
  zb_dvec3 moon;        /* towards the moon */
  double sun_elevation; /* radians above the horizon (negative at night) */
  double moon_elevation;
  double moon_phase; /* 0 new, 0.5 full, back to 1 */
  double moon_light; /* lit fraction of the disc, 0 .. 1 */
  double season;     /* local season, 0 .. 4 (south of the equator half a year on) */
  double daylight;   /* 0 night .. 1 full day, smooth through twilight */
} zb_sky_place;

void zb_calendar_default(zb_calendar_config *c);
/* Game seconds that pass in dt real seconds. */
double zb_calendar_game_seconds(const zb_calendar_config *c, double real_seconds);
void zb_calendar_date(double time, zb_date *out);
/* The sky above a unit direction (a place on the planet) at a time. */
void zb_calendar_sky(const zb_calendar_config *c, double time, zb_dvec3 place, zb_sky_place *out);

#endif

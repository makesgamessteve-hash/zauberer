/* SPDX-License-Identifier: MIT */
#include "world/calendar.h"

#include <math.h>
#include <string.h>

#include "core/dmath.h"
#include "world/dvec.h"

void zb_calendar_default(zb_calendar_config *c) {
  memset(c, 0, sizeof *c);
  c->day_length = 2400.0;
  c->axial_tilt = 23.0;
  c->start = 8.0 * 3600.0; /* the first spring morning */
}

double zb_calendar_game_seconds(const zb_calendar_config *c, double real_seconds) {
  return real_seconds * ZB_DAY_SECONDS / (c->day_length > 1.0 ? c->day_length : 1.0);
}

void zb_calendar_date(double time, zb_date *out) {
  time = time > 0.0 ? time : 0.0;
  double days = floor(time / ZB_DAY_SECONDS);
  double secs = time - days * ZB_DAY_SECONDS;
  int64_t d = (int64_t)days;
  int of_year = (int)(d % ZB_YEAR_DAYS);
  out->year = d / ZB_YEAR_DAYS;
  out->season = of_year / ZB_SEASON_DAYS;
  out->day = of_year % ZB_SEASON_DAYS;
  out->hour = (int)(secs / 3600.0);
  out->minute = (int)((secs - out->hour * 3600.0) / 60.0);
  out->day_fraction = secs / ZB_DAY_SECONDS;
  out->year_fraction = ((double)of_year + out->day_fraction) / ZB_YEAR_DAYS;
}

static double elevation(zb_dvec3 place, zb_dvec3 dir) {
  double s = zb_dv_dot(place, dir);
  return zb_dasin(s > 1.0 ? 1.0 : (s < -1.0 ? -1.0 : s));
}

static zb_dvec3 body(double declination, double longitude) {
  double cd = zb_dcos(declination);
  return zb_dv(cd * zb_dcos(longitude), cd * zb_dsin(longitude), zb_dsin(declination));
}

void zb_calendar_sky(const zb_calendar_config *c, double time, zb_dvec3 place, zb_sky_place *out) {
  zb_date date;
  zb_calendar_date(time, &date);
  double tilt = c->axial_tilt * ZB_DPI / 180.0;
  /* The sun stands over longitude 0 at noon and 180 degrees at midnight; its
   * declination follows the year from the spring equinox. */
  double sun_lon = ZB_DPI - 2.0 * ZB_DPI * date.day_fraction;
  double sun_dec = zb_dasin(zb_dsin(tilt) * zb_dsin(2.0 * ZB_DPI * date.year_fraction));
  out->sun = body(sun_dec, sun_lon);
  /* The moon: new when it stands with the sun, full when opposite. */
  double phase = time / (ZB_MOON_DAYS * ZB_DAY_SECONDS);
  phase -= floor(phase);
  double moon_lon = sun_lon + 2.0 * ZB_DPI * phase;
  double moon_dec = zb_dasin(zb_dsin(tilt) * zb_dsin(2.0 * ZB_DPI * (date.year_fraction + phase)));
  out->moon = body(moon_dec, moon_lon);
  out->moon_phase = phase;
  out->moon_light = 0.5 * (1.0 - zb_dcos(2.0 * ZB_DPI * phase));
  out->sun_elevation = elevation(place, out->sun);
  out->moon_elevation = elevation(place, out->moon);
  double u = (out->sun_elevation + 0.1) / 0.2;
  u = u < 0.0 ? 0.0 : (u > 1.0 ? 1.0 : u);
  out->daylight = u * u * (3.0 - 2.0 * u);
  double season = 4.0 * date.year_fraction + (place.z < 0.0 ? 2.0 : 0.0);
  out->season = season - 4.0 * floor(season / 4.0);
}

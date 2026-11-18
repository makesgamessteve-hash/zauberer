/* SPDX-License-Identifier: MIT */
#include "world/weather.h"

#include <math.h>

#include "core/dmath.h"
#include "world/calendar.h"
#include "world/noise.h"

#define SEED_SYSTEMS 0x77656174ull
#define SEED_STORMS 0x73746f72ull
#define SEED_GUSTS 0x67757374ull
#define SYSTEM_SIZE 1.5e6 /* metres across a weather system */
#define SYSTEM_DAYS 3.0   /* days for a system to change its shape */

static double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

static double smooth(double a, double b, double x) {
  double t = clamp01((x - a) / (b - a));
  return t * t * (3.0 - 2.0 * t);
}

void zb_weather_prevailing(double latitude, double *east, double *north) {
  /* Easterly trade winds in the tropics, westerlies at mid-latitudes, and
   * a little towards the equator in the tropics, poleward beyond. */
  *east = -6.0 * zb_dcos(3.0 * latitude);
  *north = -1.5 * zb_dsin(6.0 * latitude);
}

void zb_weather_at(const zb_terrain *t, const zb_calendar_config *cal, const zb_planet_pos *pos, double time,
                   zb_weather *out) {
  zb_dvec3 c = zb_planet_to_cart(&t->planet, pos);
  double len = sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
  zb_dvec3 d = {c.x / len, c.y / len, c.z / len};
  double lat = zb_dasin(d.z), lon = zb_datan2(d.y, d.x);
  double pe, pn;
  zb_weather_prevailing(lat, &pe, &pn);
  /* Systems drift east (or west) with the prevailing wind. */
  double r = t->planet.radius, cl = zb_dcos(lat);
  cl = cl > 0.2 ? cl : 0.2;
  double drifted = lon - pe * time / (r * cl);
  double f = r / SYSTEM_SIZE;
  f = f > 1.0 ? f : 1.0;
  double qx = zb_dcos(lat) * zb_dcos(drifted) * f, qy = zb_dcos(lat) * zb_dsin(drifted) * f, qz = d.z * f;
  double evolve = time / (SYSTEM_DAYS * 86400.0);
  double n = zb_fbm3(t->seed ^ SEED_SYSTEMS, qx, qy, qz + evolve, 4, 0.5);
  /* How often the climate brings cloud: deserts seldom, rainforests most days. */
  double mean, coldest, warmest, precipitation;
  zb_terrain_chunk_climate(t, pos->chunk, &mean, &coldest, &warmest, &precipitation);
  double wet = clamp01(precipitation / 2500.0), threshold = 0.35 - 0.6 * wet;
  out->cloud = smooth(threshold - 0.25, threshold + 0.25, n);
  /* The air's temperature: the season and the time of day at this height. */
  zb_sky_place sky;
  zb_calendar_sky(cal, time, d, &sky);
  double temp = mean + 0.5 * (warmest - coldest) * zb_dsin(2.0 * ZB_DPI * (sky.season - 0.5) / 4.0) +
                6.0 * (sky.daylight - 0.5) * (1.0 - 0.6 * out->cloud) - 0.0065 * (pos->alt > 0.0f ? pos->alt : 0.0f);
  out->temperature = temp;
  /* Heavy cloud brings rain, or snow below freezing. */
  double fall = out->cloud > 0.6 ? (out->cloud - 0.6) / 0.4 : 0.0;
  double rate = precipitation / 400.0;
  rate = fall * (rate < 0.3 ? 0.3 : (rate > 8.0 ? 8.0 : rate));
  out->rain = temp >= 0.5 ? rate : 0.0;
  out->snow = temp < 0.5 ? rate : 0.0;
  /* Warm, heavy cloud builds thunderstorms, here and there. */
  double cells = zb_noise3(t->seed ^ SEED_STORMS, qx * 6.0, qy * 6.0, qz * 6.0 + evolve * 4.0);
  out->storm = temp > 12.0 && out->cloud > 0.8
                   ? (out->cloud - 0.8) / 0.2 * clamp01((temp - 12.0) / 10.0) * smooth(0.0, 0.4, cells)
                   : 0.0;
  /* Wind: the prevailing wind, stronger under storms, with gusts by the minute. */
  double gust = zb_noise3(t->seed ^ SEED_GUSTS, qx * 20.0, qy * 20.0, qz * 20.0 + time / 120.0);
  double k = 1.0 + 0.3 * gust + 0.3 * out->cloud + 1.5 * out->storm;
  out->wind_east = pe * k;
  out->wind_north = pn * k;
}

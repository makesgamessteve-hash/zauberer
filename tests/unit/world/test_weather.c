/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/dvec.h"
#include "world/weather.h"

#include <math.h>

ZB_TEST_LEAK_CHECKED

static zb_terrain g_terrain;
static zb_calendar_config g_cal;

static void init(void) {
  static bool done = false;
  if (!done) {
    zb_planet p;
    TEST_ASSERT_TRUE(zb_planet_init(&p, ZB_PLANET_DEFAULT_RADIUS, NULL, 0));
    zb_terrain_init(&g_terrain, &p, 1, NULL);
    zb_calendar_default(&g_cal);
    done = true;
  }
}

static zb_planet_pos place(double lat_deg, double lon_deg) {
  double lat = lat_deg * ZB_DPI / 180.0, lon = lon_deg * ZB_DPI / 180.0;
  zb_dvec3 d = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
  zb_planet_pos p = zb_planet_from_cart(&g_terrain.planet, zb_dv_scale(d, g_terrain.planet.radius));
  p.alt = 0.0f;
  return p;
}

static double precipitation_at(const zb_planet_pos *p) {
  double mean, coldest, warmest, rain;
  zb_terrain_chunk_climate(&g_terrain, p->chunk, &mean, &coldest, &warmest, &rain);
  return rain;
}

/* spec: weather: Prevailing winds by latitude */
static void test_prevailing(void) {
  double e, n;
  zb_weather_prevailing(0.0, &e, &n);
  TEST_ASSERT_TRUE(e < -4.0); /* easterly trade winds */
  zb_weather_prevailing(15.0 * ZB_DPI / 180.0, &e, &n);
  TEST_ASSERT_TRUE(e < 0.0 && n < 0.0); /* towards the equator */
  zb_weather_prevailing(-15.0 * ZB_DPI / 180.0, &e, &n);
  TEST_ASSERT_TRUE(e < 0.0 && n > 0.0);
  zb_weather_prevailing(50.0 * ZB_DPI / 180.0, &e, &n);
  TEST_ASSERT_TRUE(e > 4.0); /* westerlies */
}

/* Rainy hours and mean cloud of a place over two months. */
static void sample(const zb_planet_pos *p, double *rainy, double *cloud, double *storms) {
  int n = 0, wet = 0, stormy = 0;
  double sum = 0.0;
  for (double t = 0.0; t < 60.0 * 86400.0; t += 3600.0, ++n) {
    zb_weather w;
    zb_weather_at(&g_terrain, &g_cal, p, t, &w);
    TEST_ASSERT_TRUE(w.cloud >= 0.0 && w.cloud <= 1.0 && w.rain >= 0.0 && w.snow >= 0.0);
    wet += w.rain + w.snow > 0.0 ? 1 : 0;
    stormy += w.storm > 0.0 ? 1 : 0;
    sum += w.cloud;
  }
  *rainy = (double)wet / n;
  *cloud = sum / n;
  *storms = (double)stormy / n;
}

/* spec: weather: Climate decides the weather */
static void test_climate(void) {
  init();
  /* The driest and the wettest of a row of places in the warm belt. */
  zb_planet_pos dry = place(20.0, 0.0), wet = dry;
  for (int k = 0; k < 72; ++k) {
    zb_planet_pos p = place(20.0, k * 5.0);
    dry = precipitation_at(&p) < precipitation_at(&dry) ? p : dry;
    wet = precipitation_at(&p) > precipitation_at(&wet) ? p : wet;
  }
  TEST_ASSERT_TRUE(precipitation_at(&wet) > 2.0 * precipitation_at(&dry));
  double rd, cd, sd, rw, cw, sw;
  sample(&dry, &rd, &cd, &sd);
  sample(&wet, &rw, &cw, &sw);
  TEST_ASSERT_TRUE_MESSAGE(rw > rd && cw > cd, "the wet place has more cloud and rain");
  TEST_ASSERT_TRUE(rw > 0.05 && rd < 0.3);
}

/* spec: weather: Weather systems drift with the wind */
static void test_drift(void) {
  init();
  /* At 50 degrees north the westerlies carry the clouds east: six hours later
   * the weather is found downwind, not upwind. */
  double lat = 50.0 * ZB_DPI / 180.0, e, n;
  zb_weather_prevailing(lat, &e, &n);
  double dt = 6.0 * 3600.0, shift = e * dt / (g_terrain.planet.radius * zb_dcos(lat)) * 180.0 / ZB_DPI;
  double down = 0.0, up = 0.0;
  for (int k = 0; k < 60; ++k) {
    double lon = k * 6.0;
    zb_planet_pos p = place(50.0, lon), pd = place(50.0, lon + shift), pu = place(50.0, lon - shift);
    zb_weather w0, wd, wu;
    zb_weather_at(&g_terrain, &g_cal, &p, 10.0 * 86400.0, &w0);
    zb_weather_at(&g_terrain, &g_cal, &pd, 10.0 * 86400.0 + dt, &wd);
    zb_weather_at(&g_terrain, &g_cal, &pu, 10.0 * 86400.0 + dt, &wu);
    down += fabs(w0.cloud - wd.cloud);
    up += fabs(w0.cloud - wu.cloud);
  }
  TEST_ASSERT_TRUE_MESSAGE(down < 0.75 * up, "the weather moved downwind (and changed a little)");
}

/* spec: weather: Snow, rain and thunderstorms */
static void test_kinds(void) {
  init();
  /* Somewhere far north in winter it snows rather than rains. */
  int snowy = 0, rainy_cold = 0;
  for (int k = 0; k < 72; ++k) {
    zb_planet_pos p = place(68.0, k * 5.0);
    for (double t = 0.0; t < 48.0 * 86400.0; t += 6.0 * 3600.0) {
      zb_weather w;
      zb_weather_at(&g_terrain, &g_cal, &p, t, &w);
      snowy += w.snow > 0.0 ? 1 : 0;
      rainy_cold += w.rain > 0.0 && w.temperature < 0.5 ? 1 : 0;
      TEST_ASSERT_FALSE(w.snow > 0.0 && w.rain > 0.0);
    }
  }
  TEST_ASSERT_TRUE(snowy > 0);
  TEST_ASSERT_EQUAL_INT(0, rainy_cold);
  /* In the warm, wet tropics some hours are stormy, and storms blow harder. */
  int stormy = 0;
  double calm_wind = 0.0, storm_wind = 0.0;
  int calm_n = 0;
  for (int k = 0; k < 72 && stormy < 20; ++k) {
    zb_planet_pos p = place(5.0, k * 5.0);
    for (double t = 0.0; t < 30.0 * 86400.0; t += 3600.0) {
      zb_weather w;
      zb_weather_at(&g_terrain, &g_cal, &p, t, &w);
      double speed = sqrt(w.wind_east * w.wind_east + w.wind_north * w.wind_north);
      if (w.storm > 0.3) {
        stormy++;
        storm_wind += speed;
      } else if (w.cloud < 0.2) {
        calm_wind += speed;
        calm_n++;
      }
    }
  }
  TEST_ASSERT_TRUE(stormy > 0 && calm_n > 0);
  TEST_ASSERT_TRUE(storm_wind / stormy > 1.3 * calm_wind / calm_n);
  /* A pure function: the same place and time, the same weather. */
  zb_planet_pos p = place(5.0, 30.0);
  zb_weather a, b;
  zb_weather_at(&g_terrain, &g_cal, &p, 123456.0, &a);
  zb_weather_at(&g_terrain, &g_cal, &p, 123456.0, &b);
  TEST_ASSERT_TRUE(a.cloud == b.cloud && a.rain == b.rain && a.wind_east == b.wind_east);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_prevailing);
  RUN_TEST(test_climate);
  RUN_TEST(test_drift);
  RUN_TEST(test_kinds);
  return UNITY_END();
}

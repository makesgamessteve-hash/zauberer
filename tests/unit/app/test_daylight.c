/* SPDX-License-Identifier: MIT */
#include "app/daylight.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/dvec.h"

#include <math.h>

ZB_TEST_LEAK_CHECKED

static zb_planet g_planet;
static zb_origin g_origin;

static void init(void) {
  static bool done = false;
  if (!done) {
    TEST_ASSERT_TRUE(zb_planet_init(&g_planet, ZB_PLANET_DEFAULT_RADIUS, NULL, 0));
    /* A chunk on the equator at longitude 0 (face +X). */
    zb_origin_init(&g_origin, &g_planet, zb_planet_from_cart(&g_planet, zb_dv(g_planet.radius, 0, 0)).chunk);
    done = true;
  }
}

static float brightness(zb_vec3 c) { return c.x + c.y + c.z; }

/* A direction through a matrix (columns), as the sky shader does. */
static zb_vec3 rotate(zb_mat4 m, zb_vec3 v) {
  return zb_v3(m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z, m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z,
               m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z);
}

/* spec: sky-and-time: Day, twilight and night */
static void test_day_and_night(void) {
  init();
  zb_calendar_config c;
  zb_calendar_default(&c);
  zb_vec3 eye = zb_v3(0, 100, 0);
  zb_daylight noon, sunset, night_full, night_new;
  double day = 2.0 * ZB_DAY_SECONDS; /* a few days in, near the equinox */
  zb_daylight_compute(&c, day + 12.0 * 3600.0, &g_origin, eye, &noon);
  zb_daylight_compute(&c, day + 18.0 * 3600.0, &g_origin, eye, &sunset);
  /* Full moon at midnight half a moon cycle in; new moon a cycle later. */
  zb_daylight_compute(&c, 6.0 * ZB_DAY_SECONDS, &g_origin, eye, &night_full);
  zb_daylight_compute(&c, 12.0 * ZB_DAY_SECONDS, &g_origin, eye, &night_new);
  /* Noon: a bright blue sky, the sun high, its light coming down. */
  TEST_ASSERT_TRUE(noon.atmosphere.enabled);
  TEST_ASSERT_TRUE(noon.atmosphere.zenith.z > noon.atmosphere.zenith.x);
  TEST_ASSERT_TRUE(brightness(noon.atmosphere.horizon) > 2.0f);
  TEST_ASSERT_TRUE(noon.atmosphere.sun_dir.y > 0.9f);
  TEST_ASSERT_TRUE(noon.light_dir.y < -0.9f);
  TEST_ASSERT_TRUE(noon.atmosphere.night < 0.01f);
  /* Sunset: an orange horizon. */
  TEST_ASSERT_TRUE(sunset.atmosphere.horizon.x > sunset.atmosphere.horizon.z);
  TEST_ASSERT_TRUE(sunset.sky.daylight > 0.2 && sunset.sky.daylight < 0.8);
  /* Midnight: dark with stars, lit by the moon, more under a full moon. */
  TEST_ASSERT_TRUE(night_full.atmosphere.night > 0.99f && night_new.atmosphere.night > 0.99f);
  TEST_ASSERT_TRUE(brightness(night_full.atmosphere.zenith) < 0.5f);
  TEST_ASSERT_TRUE(night_full.atmosphere.moon_light > 0.99f);
  TEST_ASSERT_TRUE(night_full.light_dir.y < 0.0f); /* the moon above */
  TEST_ASSERT_TRUE(brightness(night_full.light_color) > brightness(night_new.light_color));
  TEST_ASSERT_TRUE(brightness(night_full.ambient) > brightness(night_new.ambient));
  TEST_ASSERT_TRUE(brightness(night_full.ambient) < brightness(noon.ambient));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, brightness(night_full.atmosphere.sun_color));
  /* The star sphere turns with time: the same ray maps elsewhere an hour later. */
  zb_daylight later;
  zb_daylight_compute(&c, 6.0 * ZB_DAY_SECONDS + 3600.0, &g_origin, eye, &later);
  zb_vec3 up = rotate(night_full.atmosphere.celestial, zb_v3(0, 1, 0));
  zb_vec3 up_later = rotate(later.atmosphere.celestial, zb_v3(0, 1, 0));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, zb_v3_length(up));
  TEST_ASSERT_TRUE(zb_v3_length(zb_v3_sub(up, up_later)) > 0.2f);
}

/* spec: sky-and-time: Snow in winter */
static void test_snow(void) {
  /* A temperate place: 10 deg C on average, -2 in winter, 22 in summer. */
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_daylight_snow(10.0, -2.0, 22.0, 100.0, 1.5)); /* midsummer */
  float winter = zb_daylight_snow(10.0, -2.0, 22.0, 100.0, 3.5);
  TEST_ASSERT_TRUE(winter > 0.0f && winter < 1.0f);
  TEST_ASSERT_TRUE(zb_daylight_snow(10.0, -2.0, 22.0, 1500.0, 3.5) > winter);  /* higher up */
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_daylight_snow(27.0, 25.0, 29.0, 0.0, 3.5)); /* the tropics */
  TEST_ASSERT_EQUAL_FLOAT(1.0f, zb_daylight_snow(-30.0, -45.0, -10.0, 0.0, 3.5));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_day_and_night);
  RUN_TEST(test_snow);
  return UNITY_END();
}

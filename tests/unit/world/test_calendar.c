/* SPDX-License-Identifier: MIT */
#include "core/hash.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/calendar.h"
#include "world/dvec.h"

#include <math.h>
#include <stdio.h>

ZB_TEST_LEAK_CHECKED

static zb_dvec3 at(double lat_deg, double lon_deg) {
  double lat = lat_deg * ZB_DPI / 180.0, lon = lon_deg * ZB_DPI / 180.0;
  return zb_dv(zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat));
}

static double deg(double rad) { return rad * 180.0 / ZB_DPI; }

/* spec: sky-and-time: A calendar of days and seasons */
static void test_calendar(void) {
  zb_calendar_config c;
  zb_calendar_default(&c);
  TEST_ASSERT_EQUAL_DOUBLE(2400.0, c.day_length);
  TEST_ASSERT_EQUAL_DOUBLE(36.0, zb_calendar_game_seconds(&c, 1.0)); /* 40 minutes a day */
  zb_date d;
  zb_calendar_date(c.start, &d);
  TEST_ASSERT_EQUAL_INT64(0, d.year);
  TEST_ASSERT_EQUAL_INT(0, d.season);
  TEST_ASSERT_EQUAL_INT(0, d.day);
  TEST_ASSERT_EQUAL_INT(8, d.hour);
  zb_calendar_date(ZB_DAY_SECONDS * (ZB_YEAR_DAYS + 2 * ZB_SEASON_DAYS + 5) + 13.5 * 3600.0, &d);
  TEST_ASSERT_EQUAL_INT64(1, d.year);
  TEST_ASSERT_EQUAL_INT(2, d.season); /* autumn */
  TEST_ASSERT_EQUAL_INT(5, d.day);
  TEST_ASSERT_EQUAL_INT(13, d.hour);
  TEST_ASSERT_EQUAL_INT(30, d.minute);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 13.5 / 24.0, d.day_fraction);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, (29.0 + 13.5 / 24.0) / 48.0, d.year_fraction);
  zb_calendar_date(-5.0, &d); /* before the beginning: the beginning */
  TEST_ASSERT_EQUAL_INT64(0, d.year);
  TEST_ASSERT_EQUAL_INT(0, d.hour);
  c.day_length = 0.0;
  TEST_ASSERT_EQUAL_DOUBLE(ZB_DAY_SECONDS, zb_calendar_game_seconds(&c, 1.0));
}

/* spec: sky-and-time: The sun crosses the sky */
static void test_sun(void) {
  zb_calendar_config c;
  zb_calendar_default(&c);
  zb_sky_place s;
  double noon = 12.0 * 3600.0, midnight = 0.0;
  /* The first day, near the equinox, at the equator: overhead at noon (the
   * declination has grown 1.5 degrees by then), below at midnight, rising at 6. */
  zb_calendar_sky(&c, noon, at(0, 0), &s);
  TEST_ASSERT_DOUBLE_WITHIN(2.0, 90.0, deg(s.sun_elevation));
  TEST_ASSERT_EQUAL_DOUBLE(1.0, s.daylight);
  zb_calendar_sky(&c, midnight, at(0, 0), &s);
  TEST_ASSERT_DOUBLE_WITHIN(0.5, -90.0, deg(s.sun_elevation));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, s.daylight);
  zb_calendar_sky(&c, 6.0 * 3600.0, at(0, 0), &s);
  TEST_ASSERT_DOUBLE_WITHIN(0.5, 0.0, deg(s.sun_elevation));
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.5, s.daylight); /* twilight */
  /* Noon moves west with longitude: at 90 degrees east it is noon 6 hours earlier. */
  zb_calendar_sky(&c, 6.0 * 3600.0, at(0, 90), &s);
  TEST_ASSERT_DOUBLE_WITHIN(2.0, 90.0, deg(s.sun_elevation));
  /* Midsummer (northern): the sun stands 23 degrees north, polar day at 80 N,
   * polar night at 80 S, and the south has winter. */
  double midsummer = ZB_DAY_SECONDS * (ZB_SEASON_DAYS * 1.0);
  zb_calendar_sky(&c, midsummer + noon, at(23, 0), &s);
  TEST_ASSERT_DOUBLE_WITHIN(0.5, 90.0, deg(s.sun_elevation));
  for (int h = 0; h < 24; h += 3) {
    zb_calendar_sky(&c, midsummer + h * 3600.0, at(80, 0), &s);
    TEST_ASSERT_TRUE(s.sun_elevation > 0.0);
    zb_calendar_sky(&c, midsummer + h * 3600.0, at(-80, 0), &s);
    TEST_ASSERT_TRUE(s.sun_elevation < 0.0);
  }
  zb_calendar_sky(&c, midsummer, at(45, 0), &s);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, s.season);
  zb_calendar_sky(&c, midsummer, at(-45, 0), &s);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, s.season);
  /* No tilt, no seasons of daylight: the equinox all year. */
  c.axial_tilt = 0.0;
  zb_calendar_sky(&c, midsummer + noon, at(0, 0), &s);
  TEST_ASSERT_DOUBLE_WITHIN(0.5, 90.0, deg(s.sun_elevation));
}

/* spec: sky-and-time: The moon and its phases */
static void test_moon(void) {
  zb_calendar_config c;
  zb_calendar_default(&c);
  zb_sky_place s;
  zb_calendar_sky(&c, 0.0, at(0, 0), &s);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, s.moon_phase);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, s.moon_light); /* new: beside the sun */
  TEST_ASSERT_TRUE(zb_dv_dot(s.moon, s.sun) > 0.99);
  zb_calendar_sky(&c, 0.5 * ZB_MOON_DAYS * ZB_DAY_SECONDS, at(0, 0), &s);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, s.moon_phase);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, s.moon_light); /* full: opposite the sun */
  TEST_ASSERT_TRUE(zb_dv_dot(s.moon, s.sun) < -0.8);
  /* The full moon is up at midnight. */
  zb_calendar_sky(&c, 0.5 * ZB_MOON_DAYS * ZB_DAY_SECONDS, at(0, 0), &s);
  double t = 6.0 * ZB_DAY_SECONDS; /* midnight, full moon */
  zb_calendar_sky(&c, t, at(0, 0), &s);
  TEST_ASSERT_TRUE(s.sun_elevation < 0.0 && s.moon_elevation > 0.5);
}

/* spec: sky-and-time: Deterministic sky */
static void test_golden(void) {
  zb_calendar_config c;
  zb_calendar_default(&c);
  zb_hasher h;
  zb_hasher_init(&h, 0x736b79);
  for (int k = 0; k < 200; ++k) {
    zb_sky_place s;
    zb_calendar_sky(&c, 7919.0 * k * k, at(-80.0 + 0.8 * k, 1.7 * k), &s);
    zb_hasher_f64(&h, s.sun_elevation);
    zb_hasher_f64(&h, s.moon_elevation);
    zb_hasher_f64(&h, s.daylight);
    zb_hasher_f64(&h, s.season);
  }
  char hex[17];
  zb_hash_hex(zb_hasher_final(&h), hex);
  printf("sky golden hash: %s\n", hex);
  TEST_ASSERT_EQUAL_STRING("0befef1c5bd5f061", hex); /* the same on every platform */
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_calendar);
  RUN_TEST(test_sun);
  RUN_TEST(test_moon);
  RUN_TEST(test_golden);
  return UNITY_END();
}

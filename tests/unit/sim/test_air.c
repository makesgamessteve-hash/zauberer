/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "sim/air.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

typedef struct scene {
  zb_world *world;
  zb_air *air;
  zb_planet_pos centre;
  zb_dvec3 ax; /* the chunk's x axis */
} scene;

typedef enum ground { FLAT, RIDGE, FOREST } ground;

/* Flat ground, a ridge running along y at x = 128 (nominal metres), or flat
 * ground under full canopy. */
static void reshape(zb_chunk_terrain *ct, ground g) {
  for (int y = 0; y < ZB_HEIGHT_RES; ++y) {
    for (int x = 0; x < ZB_HEIGHT_RES; ++x) {
      double d = (x - 128.0) / 24.0;
      ct->height[y * ZB_HEIGHT_RES + x] = (float)(g == RIDGE ? 50.0 + 60.0 * exp(-d * d) : 50.0);
    }
  }
  memset(ct->canopy, g == FOREST ? 255 : 0, sizeof ct->canopy);
}

static void scene_open(scene *s, ground g) {
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  char err[128];
  s->world = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(s->world, err);
  const zb_planet *p = zb_world_planet(s->world);
  double lat = 46.0 * ZB_DPI / 180.0, lon = 0.3;
  zb_dvec3 dir = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos = zb_planet_from_cart(p, zb_dv_scale(dir, p->radius));
  f.pos.lx = f.pos.ly = 128.0f;
  s->centre = f.pos;
  zb_world_set_focus(s->world, 0, &f);
  for (int i = 0; i < 2000; ++i) {
    zb_world_update(s->world);
    const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(s->world), f.pos.chunk);
    if (ct && ct->stage >= ZB_STAGE_SURFACE) {
      break;
    }
  }
  reshape(zb_stream_edit(zb_world_stream(s->world), f.pos.chunk), g);
  zb_planet_pos b = s->centre;
  b.lx += 1.0f;
  s->ax = zb_dv_norm(zb_dv_sub(zb_planet_to_cart(p, &b), zb_planet_to_cart(p, &s->centre)));
  s->air = zb_air_create(zb_test_alloc(), s->world, 1.0); /* the reshaped chunk alone */
  TEST_ASSERT_NOT_NULL(s->air);
  zb_air_update(s->air, &s->centre, 1, 0.0);
}

/* A wind of speed m/s along the chunk's x axis. */
static void wind_along_x(scene *s, double speed) {
  zb_dvec3 east, north, up;
  zb_planet_frame(zb_dv_norm(zb_planet_to_cart(zb_world_planet(s->world), &s->centre)), &east, &north, &up);
  zb_air_set_wind(s->air, speed * zb_dv_dot(s->ax, east), speed * zb_dv_dot(s->ax, north));
}

static void scene_close(scene *s) {
  zb_air_destroy(s->air);
  zb_world_destroy(s->world);
}

static zb_planet_pos at(const scene *s, float x, float y) {
  zb_planet_pos p = s->centre;
  p.lx = x;
  p.ly = y;
  return p;
}

static double speed_at(const scene *s, float x, float y) {
  zb_air_sample a;
  zb_planet_pos p = at(s, x, y);
  zb_air_sample_at(s->air, &p, &a);
  return sqrt(a.east * a.east + a.north * a.north);
}

static void run(scene *s, double seconds) {
  for (double t = 0.0; t < seconds; t += 0.25) {
    zb_air_update(s->air, &s->centre, 1, 0.25);
  }
}

/* spec: air-and-gases: The ground shapes the wind */
static void test_wind(void) {
  scene s;
  scene_open(&s, RIDGE);
  wind_along_x(&s, 8.0);
  double crest = speed_at(&s, 128.0f, 128.0f), plain = speed_at(&s, 20.0f, 128.0f);
  TEST_ASSERT_TRUE_MESSAGE(crest > 1.2 * plain, "faster over the crest");
  /* On the windward slope the wind is turned along it: slower across. */
  TEST_ASSERT_TRUE(speed_at(&s, 104.0f, 128.0f) < crest);
  double e, n;
  zb_air_chunk_wind(s.air, s.centre.chunk, &e, &n);
  TEST_ASSERT_TRUE(sqrt(e * e + n * n) > 1.0);
  zb_chunk far = s.centre.chunk;
  far.x += 40;
  zb_air_chunk_wind(s.air, far, &e, &n); /* outside: the weather's wind */
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 8.0, sqrt(e * e + n * n));
  scene_close(&s);
  scene_open(&s, FOREST);
  wind_along_x(&s, 8.0);
  TEST_ASSERT_TRUE_MESSAGE(speed_at(&s, 128.0f, 128.0f) < 0.6 * 8.0, "sheltered under the canopy");
  scene_close(&s);
}

static void count(void *ctx, const zb_planet_pos *pos, double kg_m3) {
  TEST_ASSERT_TRUE(kg_m3 > 0.0 && pos->lx >= 0.0f);
  (*(int *)ctx)++;
}

/* spec: air-and-gases: Gases drift downwind and fade */
static void test_drift(void) {
  scene s;
  scene_open(&s, FLAT);
  zb_air_stats st;
  zb_air_get_stats(s.air, &st);
  TEST_ASSERT_EQUAL_INT(0, st.awake);
  wind_along_x(&s, 5.0);
  zb_planet_pos src = at(&s, 60.0f, 128.0f);
  for (int k = 0; k < 40; ++k) { /* 10 s of smoke at 2 kg/s */
    zb_air_emit(s.air, &src, ZB_GAS_SMOKE, 0.5);
    run(&s, 0.25);
  }
  zb_air_sample down, up;
  zb_planet_pos d = at(&s, 100.0f, 128.0f), u = at(&s, 20.0f, 128.0f);
  zb_air_sample_at(s.air, &d, &down);
  zb_air_sample_at(s.air, &u, &up);
  TEST_ASSERT_TRUE(down.loaded && down.gas[ZB_GAS_SMOKE] > 1e-6);
  TEST_ASSERT_TRUE_MESSAGE(down.gas[ZB_GAS_SMOKE] > 20.0 * up.gas[ZB_GAS_SMOKE], "downwind, not upwind");
  zb_air_get_stats(s.air, &st);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 20.0, st.emitted[ZB_GAS_SMOKE]);
  TEST_ASSERT_TRUE(st.mass[ZB_GAS_SMOKE] > 5.0 && st.mass[ZB_GAS_SMOKE] < 20.0);
  TEST_ASSERT_EQUAL_INT(1, st.awake);
  int cells = 0;
  zb_air_each(s.air, ZB_GAS_SMOKE, 1e-6, count, &cells);
  TEST_ASSERT_TRUE(cells > 5);
  /* In still air, steam condenses fast and scent lingers. */
  zb_air_set_wind(s.air, 0.0, 0.0);
  zb_air_emit(s.air, &src, ZB_GAS_STEAM, 5.0);
  zb_air_emit(s.air, &src, ZB_GAS_SCENT, 5.0);
  run(&s, 90.0);
  zb_air_get_stats(s.air, &st);
  TEST_ASSERT_TRUE(st.mass[ZB_GAS_STEAM] < 0.2 && st.mass[ZB_GAS_SCENT] > 2.0);
  /* With no source the air clears and the chunk sleeps. */
  run(&s, 3600.0); /* scent lingers longest */
  zb_air_get_stats(s.air, &st);
  TEST_ASSERT_EQUAL_INT(0, st.awake);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, st.mass[ZB_GAS_SMOKE]);
  TEST_ASSERT_EQUAL_STRING("fumes", zb_gas_name(ZB_GAS_FUMES));
  TEST_ASSERT_EQUAL_STRING("?", zb_gas_name(ZB_GASES));
  scene_close(&s);
}

/* spec: air-and-gases: Deterministic air */
static void test_deterministic(void) {
  uint64_t h[2];
  for (int k = 0; k < 2; ++k) {
    scene s;
    scene_open(&s, RIDGE);
    wind_along_x(&s, 6.0);
    zb_planet_pos src = at(&s, 40.0f, 100.0f);
    for (int i = 0; i < 20; ++i) {
      zb_air_emit(s.air, &src, ZB_GAS_FUMES, 1.0);
      run(&s, 0.25);
    }
    h[k] = zb_air_hash(s.air);
    zb_planet_pos away = s.centre;
    away.chunk.x += 40;
    zb_air_sample a;
    zb_air_sample_at(s.air, &away, &a);
    TEST_ASSERT_FALSE(a.loaded);
    zb_air_emit(s.air, &away, ZB_GAS_SMOKE, 1.0); /* nowhere to go */
    zb_air_emit(s.air, &src, ZB_GASES, 1.0);      /* no such gas */
    scene_close(&s);
  }
  TEST_ASSERT_EQUAL_UINT64(h[0], h[1]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_wind);
  RUN_TEST(test_drift);
  RUN_TEST(test_deterministic);
  return UNITY_END();
}

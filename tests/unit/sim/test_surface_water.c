/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "sim/water.h"
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
  zb_surface_water *water;
  zb_planet_pos centre;
} scene;

/* Ground shapes, in nominal metres of the chunk (x east-ish, 0..256). */
typedef enum ground { SLOPE, BASIN, SEA_EDGE, FLAT_SOIL, FLAT_ROCK } ground;

static float height_of(ground g, int x, int y) {
  double dx = x - 128.0, dy = y - 128.0;
  switch (g) {
    case SLOPE: return (float)(50.0 + 0.1 * x);
    case BASIN: return (float)(50.0 + 0.002 * (dx * dx + dy * dy));
    case SEA_EDGE: return x < 60 ? -5.0f : (float)(1.0 + 0.05 * (x - 60)); /* the sea west of x = 60 */
    default: return 50.0f;
  }
}

static void reshape(zb_chunk_terrain *ct, ground g) {
  for (int y = 0; y < ZB_HEIGHT_RES; ++y) {
    for (int x = 0; x < ZB_HEIGHT_RES; ++x) {
      ct->height[y * ZB_HEIGHT_RES + x] = height_of(g, x, y);
    }
  }
  memset(ct->material, g == FLAT_SOIL ? ZB_MAT_SOIL : ZB_MAT_ROCK, ZB_SURFACE_RES * ZB_SURFACE_RES);
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
  zb_chunk_terrain *ct = zb_stream_edit(zb_world_stream(s->world), f.pos.chunk);
  TEST_ASSERT_NOT_NULL(ct);
  reshape(ct, g);
  s->water = zb_surface_water_create(zb_test_alloc(), s->world, 1.0); /* the reshaped chunk alone */
  TEST_ASSERT_NOT_NULL(s->water);
  zb_surface_water_update(s->water, &s->centre, 1, 0.0);
}

static void scene_close(scene *s) {
  zb_surface_water_destroy(s->water);
  zb_world_destroy(s->world);
}

static void run(scene *s, double seconds) {
  for (double t = 0.0; t < seconds; t += 0.25) {
    zb_surface_water_update(s->water, &s->centre, 1, 0.25);
  }
}

static zb_planet_pos at(const scene *s, float x, float y) {
  zb_planet_pos p = s->centre;
  p.lx = x;
  p.ly = y;
  return p;
}

static zb_water_cell cell(const scene *s, float x, float y) {
  zb_water_cell c;
  zb_planet_pos p = at(s, x, y);
  zb_surface_water_cell_at(s->water, &p, &c);
  return c;
}

/* Every cubic metre is accounted for. */
static void assert_balanced(const scene *s) {
  zb_water_stats st;
  zb_surface_water_get_stats(s->water, &st);
  double in = st.rained + st.added, held = st.on_ground + st.soaked + st.drained;
  TEST_ASSERT_DOUBLE_WITHIN(1e-3 * (in > 1.0 ? in : 1.0), in, held);
}

/* spec: shallow-water: Water runs downhill */
static void test_downhill(void) {
  scene s;
  scene_open(&s, SLOPE);
  zb_water_stats st;
  zb_surface_water_get_stats(s.water, &st);
  TEST_ASSERT_TRUE(st.chunks >= 1);
  TEST_ASSERT_EQUAL_INT(0, st.awake);
  zb_planet_pos high = at(&s, 180.0f, 128.0f);
  TEST_ASSERT_TRUE(zb_surface_water_add(s.water, &high, 4.0, 40.0) > 0);
  TEST_ASSERT_TRUE(cell(&s, 180.0f, 128.0f).depth > 0.1);
  run(&s, 40.0);
  zb_water_cell top = cell(&s, 180.0f, 128.0f), bottom = cell(&s, 0.5f, 128.0f); /* against the tier's edge */
  TEST_ASSERT_TRUE_MESSAGE(top.depth < 0.01, "the source drained away");
  TEST_ASSERT_TRUE_MESSAGE(bottom.depth > 0.01, "it gathered at the foot of the slope");
  TEST_ASSERT_TRUE(bottom.level < top.level);
  zb_surface_water_get_stats(s.water, &st);
  TEST_ASSERT_TRUE(st.awake >= 1 && st.wet > 0);
  assert_balanced(&s);
  scene_close(&s);
}

/* spec: shallow-water: A basin fills flat and goes still */
static void test_basin(void) {
  scene s;
  scene_open(&s, BASIN);
  zb_planet_pos centre = at(&s, 128.0f, 128.0f);
  zb_surface_water_add(s.water, &centre, 6.0, 600.0);
  run(&s, 240.0);
  double lo = 1e9, hi = -1e9;
  for (float r = 0.0f; r <= 20.0f; r += 4.0f) {
    zb_water_cell c = cell(&s, 128.0f + r, 128.0f);
    if (c.depth > 0.05) {
      lo = c.level < lo ? c.level : lo;
      hi = c.level > hi ? c.level : hi;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(hi > lo - 1.0 && hi - lo < 0.02, "a flat surface");
  zb_water_stats st;
  zb_surface_water_get_stats(s.water, &st);
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, st.awake, "still water sleeps");
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 600.0, st.on_ground); /* rock: nothing soaks in */
  assert_balanced(&s);
  scene_close(&s);
}

/* spec: shallow-water: Water drains into the sea */
static void test_drains(void) {
  scene s;
  scene_open(&s, SEA_EDGE);
  TEST_ASSERT_TRUE(cell(&s, 30.0f, 128.0f).fixed);
  zb_planet_pos sea = at(&s, 30.0f, 128.0f), shore = at(&s, 100.0f, 128.0f);
  TEST_ASSERT_EQUAL_INT(0, zb_surface_water_add(s.water, &sea, 3.0, 10.0)); /* the sea takes none */
  zb_surface_water_add(s.water, &shore, 4.0, 50.0);
  run(&s, 90.0);
  zb_water_stats st;
  zb_surface_water_get_stats(s.water, &st);
  TEST_ASSERT_TRUE(st.drained > 40.0);
  assert_balanced(&s);
  scene_close(&s);
}

/* spec: shallow-water: Rain soaks in or runs off */
static void test_rain(void) {
  scene s;
  scene_open(&s, FLAT_SOIL);
  zb_surface_water_set_rain(s.water, 20.0); /* less than soil takes */
  run(&s, 60.0);
  zb_water_stats st;
  zb_surface_water_get_stats(s.water, &st);
  TEST_ASSERT_TRUE(st.rained > 0.0 && st.soaked > 0.9 * st.rained);
  TEST_ASSERT_EQUAL_INT(0, st.wet);
  assert_balanced(&s);
  scene_close(&s);
  scene_open(&s, FLAT_ROCK);
  zb_surface_water_set_rain(s.water, 200.0); /* a cloudburst on rock */
  run(&s, 60.0);
  zb_surface_water_get_stats(s.water, &st);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, st.soaked);
  TEST_ASSERT_TRUE(st.on_ground > 0.9 * st.rained && cell(&s, 128.0f, 128.0f).depth > 0.002);
  zb_surface_water_set_rain(s.water, -1.0); /* stops */
  assert_balanced(&s);
  scene_close(&s);
}

static void count_wet(void *ctx, const zb_planet_pos *pos, double depth) {
  TEST_ASSERT_TRUE(depth > 0.01 && pos->alt > 0.0f);
  (*(int *)ctx)++;
}

/* spec: shallow-water: Deterministic water */
static void test_deterministic(void) {
  uint64_t h[2];
  for (int k = 0; k < 2; ++k) {
    scene s;
    scene_open(&s, SLOPE);
    zb_planet_pos p = at(&s, 200.0f, 100.0f);
    zb_surface_water_add(s.water, &p, 5.0, 80.0);
    run(&s, 20.0);
    h[k] = zb_surface_water_hash(s.water);
    int wet = 0;
    zb_surface_water_each_wet(s.water, 0.01, count_wet, &wet);
    TEST_ASSERT_TRUE(wet > 0);
    zb_planet_pos away = s.centre;
    away.chunk.x += 40;
    TEST_ASSERT_TRUE(cell(&s, 0.0f, 0.0f).loaded);
    zb_water_cell c;
    zb_surface_water_cell_at(s.water, &away, &c);
    TEST_ASSERT_FALSE(c.loaded);
    TEST_ASSERT_EQUAL_INT(0, zb_surface_water_add(s.water, &away, 5.0, 1.0));
    scene_close(&s);
  }
  TEST_ASSERT_EQUAL_UINT64(h[0], h[1]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_downhill);
  RUN_TEST(test_basin);
  RUN_TEST(test_drains);
  RUN_TEST(test_rain);
  RUN_TEST(test_deterministic);
  return UNITY_END();
}

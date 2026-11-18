/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/delta.h"
#include "world/dvec.h"
#include "world/stream.h"
#include "world/world.h"

#include <string.h>

ZB_TEST_LEAK_CHECKED

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

/* A world with the chunks around a land place loaded to their heights. */
static zb_world *open_world(zb_planet_pos *centre) {
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  char err[128];
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(w, err);
  const zb_planet *p = zb_world_planet(w);
  double lat = 46.0 * ZB_DPI / 180.0, lon = 0.3;
  zb_dvec3 dir = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos = zb_planet_from_cart(p, zb_dv_scale(dir, p->radius));
  f.pos.lx = f.pos.ly = 128.0f;
  *centre = f.pos;
  zb_world_set_focus(w, 0, &f);
  for (int i = 0; i < 2000; ++i) {
    zb_world_update(w);
  }
  return w;
}

/* spec: terrain-deformation: Craters and mounds */
static void test_crater(void) {
  zb_planet_pos c;
  zb_world *w = open_world(&c);
  const zb_stream *s = zb_world_stream(w);
  zb_chunk east;
  TEST_ASSERT_TRUE(zb_planet_neighbor(zb_world_planet(w), c.chunk, 1, 0, &east));
  const zb_chunk_terrain *a = zb_stream_find(s, c.chunk), *b = zb_stream_find(s, east);
  TEST_ASSERT_TRUE(a && b && a->stage >= ZB_STAGE_TERRAIN && b->stage >= ZB_STAGE_TERRAIN);
  zb_planet_pos edge = c;
  edge.lx = 250.0f;
  float before = a->height[128 * ZB_HEIGHT_RES + 250], far = a->height[128 * ZB_HEIGHT_RES + 200];
  float seam = b->height[128 * ZB_HEIGHT_RES + 0];
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, a->height[128 * ZB_HEIGHT_RES + 256], seam);
  zb_chunk changed[9];
  int n = zb_world_deform(w, &edge, 20.0, -5.0, changed, 9);
  TEST_ASSERT_TRUE_MESSAGE(n >= 2, "the crater crosses into the next chunk");
  a = zb_stream_find(s, c.chunk);
  b = zb_stream_find(s, east);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, before - 5.0f, a->height[128 * ZB_HEIGHT_RES + 250]); /* the bottom */
  TEST_ASSERT_EQUAL_FLOAT(far, a->height[128 * ZB_HEIGHT_RES + 200]);                   /* outside: untouched */
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, a->height[128 * ZB_HEIGHT_RES + 256], b->height[128 * ZB_HEIGHT_RES + 0]);
  TEST_ASSERT_TRUE(b->height[128 * ZB_HEIGHT_RES + 0] < seam - 1.0f);
  TEST_ASSERT_TRUE(a->min_height <= a->height[128 * ZB_HEIGHT_RES + 250]);
  TEST_ASSERT_FLOAT_WITHIN(0.05, before - 5.0, zb_world_ground_height(w, &edge));
  size_t edits;
  zb_deltas_get(zb_world_deltas(w), c.chunk, &edits);
  TEST_ASSERT_TRUE(edits > 500);
  /* A mound elsewhere, and nothing where the ground is not loaded. */
  zb_planet_pos hill = c;
  hill.lx = 60.0f;
  float base = a->height[128 * ZB_HEIGHT_RES + 60];
  TEST_ASSERT_EQUAL_INT(1, zb_world_deform(w, &hill, 10.0, 3.0, changed, 9));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, base + 3.0f, zb_stream_find(s, c.chunk)->height[128 * ZB_HEIGHT_RES + 60]);
  zb_planet_pos away = c;
  away.chunk.x += 40;
  TEST_ASSERT_EQUAL_INT(0, zb_world_deform(w, &away, 10.0, 3.0, changed, 9));
  zb_world_destroy(w);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_crater);
  return UNITY_END();
}

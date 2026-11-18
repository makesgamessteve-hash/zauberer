/* SPDX-License-Identifier: MIT */
/* Surface water budget (release builds on Linux): a flood spreading over a
 * chunk, stepped at 20 Hz. */
#define _POSIX_C_SOURCE 199309L
#include "core/dmath.h"
#include "sim/water.h"
#include "unity.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STEP_BUDGET_MS 2.0
#define SAMPLES 60

void setUp(void) {}
void tearDown(void) {}

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static int cmp(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : (x > y);
}

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

/* spec: shallow-water: Water budget */
static void test_water_budget(void) {
  char err[160];
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_heap_allocator();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL(w);
  const zb_planet *p = zb_world_planet(w);
  double lat = 46.0 * ZB_DPI / 180.0, lon = 0.3;
  zb_dvec3 dir = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos = zb_planet_from_cart(p, zb_dv_scale(dir, p->radius));
  f.pos.lx = f.pos.ly = 128.0f;
  zb_world_set_focus(w, 0, &f);
  for (int i = 0; i < 2000; ++i) {
    zb_world_update(w);
    const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(w), f.pos.chunk);
    if (ct && ct->stage >= ZB_STAGE_SURFACE) {
      break;
    }
  }
  zb_surface_water *water = zb_surface_water_create(zb_heap_allocator(), w, 1.0);
  zb_surface_water_update(water, &f.pos, 1, 0.0);
  /* A burst dam: 20,000 m^3 let go in the middle, spreading for half a minute. */
  zb_surface_water_add(water, &f.pos, 30.0, 20000.0);
  for (int i = 0; i < 30 * 20; ++i) {
    zb_surface_water_update(water, &f.pos, 1, ZB_WATER_STEP);
  }
  zb_water_stats st;
  zb_surface_water_get_stats(water, &st);
  double t[SAMPLES];
  for (int i = 0; i < SAMPLES; ++i) {
    double t0 = now_ms();
    zb_surface_water_update(water, &f.pos, 1, ZB_WATER_STEP);
    t[i] = now_ms() - t0;
  }
  qsort(t, SAMPLES, sizeof(double), cmp);
  printf(
      "surface water: %d wet cells in %d awake chunk(s), %.0f m^3 on the ground; step median %.2f ms (max %.2f, "
      "budget %.1f)\n",
      st.wet, st.awake, st.on_ground, t[SAMPLES / 2], t[SAMPLES - 1], STEP_BUDGET_MS);
  TEST_ASSERT_TRUE_MESSAGE(st.wet > 2000 && st.awake == 1, "a flood");
  TEST_ASSERT_TRUE(t[SAMPLES / 2] <= STEP_BUDGET_MS);
  zb_surface_water_destroy(water);
  zb_world_destroy(w);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_water_budget);
  return UNITY_END();
}

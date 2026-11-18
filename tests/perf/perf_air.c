/* SPDX-License-Identifier: MIT */
/* Air budget (release builds on Linux): smoke filling a chunk in a wind,
 * stepped at 4 Hz. */
#define _POSIX_C_SOURCE 199309L
#include "core/dmath.h"
#include "sim/air.h"
#include "unity.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STEP_BUDGET_MS 1.0
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

/* spec: air-and-gases: Air budget */
static void test_air_budget(void) {
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
  zb_air *air = zb_air_create(zb_heap_allocator(), w, 1.0);
  zb_air_update(air, &f.pos, 1, 0.0);
  zb_air_set_wind(air, 3.0, 2.0);
  /* A smoky fire line across the chunk for a minute: every gas, every cell. */
  for (int i = 0; i < 4 * 60; ++i) {
    for (float y = 4.0f; y < 256.0f; y += 8.0f) {
      zb_planet_pos at = f.pos;
      at.lx = 10.0f;
      at.ly = y;
      for (int g = 0; g < ZB_GASES; ++g) {
        zb_air_emit(air, &at, (zb_gas)g, 0.2);
      }
    }
    zb_air_update(air, &f.pos, 1, ZB_AIR_STEP);
  }
  zb_air_stats st;
  zb_air_get_stats(air, &st);
  double t[SAMPLES];
  for (int i = 0; i < SAMPLES; ++i) {
    double t0 = now_ms();
    zb_air_update(air, &f.pos, 1, ZB_AIR_STEP);
    t[i] = now_ms() - t0;
  }
  qsort(t, SAMPLES, sizeof(double), cmp);
  printf("air: %d awake chunk(s), %.0f kg of smoke in the air; step median %.3f ms (max %.3f, budget %.1f)\n", st.awake,
         st.mass[ZB_GAS_SMOKE], t[SAMPLES / 2], t[SAMPLES - 1], STEP_BUDGET_MS);
  TEST_ASSERT_TRUE_MESSAGE(st.awake == 1 && st.mass[ZB_GAS_SMOKE] > 100.0, "a smoky chunk");
  TEST_ASSERT_TRUE(t[SAMPLES / 2] <= STEP_BUDGET_MS);
  zb_air_destroy(air);
  zb_world_destroy(w);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_air_budget);
  return UNITY_END();
}

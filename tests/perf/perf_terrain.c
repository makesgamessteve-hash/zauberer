/* SPDX-License-Identifier: MIT */
/* Terrain generation budget (release builds on Linux; see tests/perf/CMakeLists.txt). */
#define _POSIX_C_SOURCE 199309L
#include "support/test_support.h"
#include "unity.h"
#include "world/terrain.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

ZB_TEST_LEAK_CHECKED

#define CHUNKS 32
#define BUDGET_MS 5.0

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec * 1e-6;
}

static int cmp_double(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : (x > y);
}

/* spec: terrain-generation: Measured time */
static void test_stage_1_and_2_budget(void) {
  zb_planet p;
  TEST_ASSERT_TRUE(zb_planet_init(&p, ZB_PLANET_DEFAULT_RADIUS, NULL, 0));
  zb_terrain t;
  zb_terrain_init(&t, &p, 1, NULL);
  double times[CHUNKS];
  for (int i = 0; i < CHUNKS; ++i) {
    zb_chunk_terrain ct;
    zb_chunk_terrain_init(&ct, (zb_chunk){i % 6, 1000 + i * 37, 2000 + i * 53});
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&t, zb_test_alloc(), &ct, ZB_STAGE_COARSE));
    double t0 = now_ms();
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&t, zb_test_alloc(), &ct, ZB_STAGE_SURFACE));
    times[i] = now_ms() - t0;
    zb_chunk_terrain_release(zb_test_alloc(), &ct);
  }
  qsort(times, CHUNKS, sizeof times[0], cmp_double);
  double median = (times[CHUNKS / 2 - 1] + times[CHUNKS / 2]) * 0.5;
  printf("terrain stages 1-2: median %.2f ms, min %.2f ms, max %.2f ms (budget %.1f ms)\n", median, times[0],
         times[CHUNKS - 1], BUDGET_MS);
  TEST_ASSERT_TRUE(median <= BUDGET_MS);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_stage_1_and_2_budget);
  return UNITY_END();
}

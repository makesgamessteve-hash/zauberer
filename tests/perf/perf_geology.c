/* SPDX-License-Identifier: MIT */
/* Deep-time geology budget: a default world (40,000 points, 125 steps, the
 * 6 km macro grid of a 2,000 km planet, 12 erosion passes) on the desktop
 * (release builds on Linux; see tests/perf/CMakeLists.txt). */
#define _POSIX_C_SOURCE 199309L
#include "support/test_support.h"
#include "unity.h"
#include "world/geology.h"

#include <stdio.h>
#include <time.h>

ZB_TEST_LEAK_CHECKED

#define BUDGET_S 60.0

static double now_s(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* spec: deep-time-geology: Time budget */
static void test_default_world_budget(void) {
  zb_geology_options o;
  zb_geology_default_options(&o, ZB_PLANET_DEFAULT_RADIUS);
  char err[128];
  double t0 = now_s();
  zb_geology *g = zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(g, err);
  zb_geology_progress p;
  double t_macro = 0.0;
  while (zb_geology_advance(g) != ZB_GEO_PHASE_DONE) {
    zb_geology_get_progress(g, &p);
    if (p.phase != ZB_GEO_PHASE_TECTONICS && t_macro == 0.0) {
      t_macro = now_s();
    }
  }
  double t1 = now_s();
  const zb_geology_metrics *m = zb_geology_get_metrics(g);
  printf("geology: %.1f s (tectonics %.1f s in %d attempt(s), macro grid and erosion %.1f s); budget %.0f s\n", t1 - t0,
         t_macro - t0, m->attempts, t1 - t_macro, BUDGET_S);
  TEST_ASSERT_TRUE(m->passed);
  TEST_ASSERT_TRUE_MESSAGE(t1 - t0 < BUDGET_S, "deep time took longer than the desktop budget");
  zb_geology_destroy(g);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_default_world_budget);
  return UNITY_END();
}

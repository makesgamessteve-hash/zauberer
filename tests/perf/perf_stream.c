/* SPDX-License-Identifier: MIT */
/* Ring refresh budget of the chunk stream (release builds on Linux). */
#define _POSIX_C_SOURCE 199309L
#include "support/test_support.h"
#include "unity.h"
#include "world/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

ZB_TEST_LEAK_CHECKED

#define UPDATES 40
#define BUDGET_MS 2.0

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec * 1e-6;
}

static int cmp_double(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : (x > y);
}

/* spec: chunk-streaming: Ring refresh cost */
static void test_refresh_budget(void) {
  zb_planet p;
  TEST_ASSERT_TRUE(zb_planet_init(&p, ZB_PLANET_DEFAULT_RADIUS, NULL, 0));
  zb_terrain t;
  zb_terrain_init(&t, &p, 1, NULL);
  zb_stream_config c;
  zb_stream_default_config(&c, ZB_STREAM_DESKTOP);
  c.a = zb_test_alloc();
  c.max_jobs = 0; /* bookkeeping only */
  zb_stream *s = zb_stream_create(&t, &c);
  TEST_ASSERT_NOT_NULL(s);
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos.chunk = (zb_chunk){0, 6000, 6000};
  f.pos.alt = 100.0f;
  zb_stream_update(s, &f, 1); /* creates the ring entries */
  double times[UPDATES];
  for (int i = 0; i < UPDATES; ++i) {
    f.pos.lx += 40.0f; /* beyond the 32 m refresh distance */
    if (f.pos.lx >= 256.0f) {
      f.pos.lx -= 256.0f;
      f.pos.chunk.x++;
    }
    double t0 = now_ms();
    zb_stream_update(s, &f, 1);
    times[i] = now_ms() - t0;
  }
  zb_stream_stats st;
  zb_stream_get_stats(s, &st);
  TEST_ASSERT_TRUE(st.refreshes >= UPDATES);
  qsort(times, UPDATES, sizeof times[0], cmp_double);
  double median = (times[UPDATES / 2 - 1] + times[UPDATES / 2]) * 0.5;
  printf("ring refresh (%zu chunks wanted): median %.2f ms, max %.2f ms (budget %.1f ms)\n", st.wanted, median,
         times[UPDATES - 1], BUDGET_MS);
  TEST_ASSERT_TRUE(median <= BUDGET_MS);
  zb_stream_destroy(s);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_refresh_budget);
  return UNITY_END();
}

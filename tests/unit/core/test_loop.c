/* SPDX-License-Identifier: MIT */
#include "core/log.h"
#include "core/loop.h"
#include "support/test_support.h"
#include "unity.h"

#include <string.h>

static int g_warnings;

static void count_warn(zb_log_level level, const char *msg, void *user) {
  (void)msg;
  (void)user;
  if (level == ZB_LOG_WARN) {
    g_warnings++;
  }
}

void setUp(void) {
  zb_test_begin();
  zb_log_reset();
  zb_log_set_stderr(false);
  zb_log_add_sink(count_warn, NULL);
  g_warnings = 0;
}

void tearDown(void) {
  zb_log_reset();
  zb_test_end();
}

/* Simulates frames at a fixed frame rate for one second; returns ticks. */
static uint64_t run_one_second(zb_loop *l, unsigned fps) {
  uint64_t ticks = 0;
  uint64_t start = l->last_ns;
  for (unsigned f = 1; f <= fps; ++f) {
    /* exact frame boundaries so the frames sum to one second */
    uint64_t now = start + (ZB_NS_PER_SEC * f) / fps;
    unsigned n = zb_loop_advance(l, now);
    TEST_ASSERT_TRUE(n <= ZB_MAX_CATCHUP_TICKS);
    ticks += n;
  }
  return ticks;
}

/* spec: game-loop: Low frame rate */
static void test_low_frame_rate(void) {
  zb_loop l;
  zb_loop_init(&l);
  TEST_ASSERT_EQUAL_UINT(0, zb_loop_advance(&l, 5000)); /* first call starts the clock */
  TEST_ASSERT_EQUAL_UINT64(60, run_one_second(&l, 30));
  TEST_ASSERT_EQUAL_UINT64(60, l.tick);
}

/* spec: game-loop: High frame rate */
static void test_high_frame_rate(void) {
  zb_loop l;
  zb_loop_init(&l);
  zb_loop_advance(&l, 0);
  TEST_ASSERT_EQUAL_UINT64(60, run_one_second(&l, 144));
  TEST_ASSERT_EQUAL_UINT64(60, run_one_second(&l, 144));
  TEST_ASSERT_EQUAL_UINT64(120, l.tick);
}

/* spec: game-loop: Two-second stall */
static void test_two_second_stall(void) {
  zb_loop l;
  zb_loop_init(&l);
  zb_loop_advance(&l, 0);
  run_one_second(&l, 60);
  TEST_ASSERT_EQUAL_INT(0, g_warnings);
  unsigned n = zb_loop_advance(&l, l.last_ns + 2 * ZB_NS_PER_SEC);
  TEST_ASSERT_EQUAL_UINT(ZB_MAX_CATCHUP_TICKS, n);
  TEST_ASSERT_EQUAL_INT(1, g_warnings);
  TEST_ASSERT_EQUAL_UINT(1, l.stalls);
  TEST_ASSERT_EQUAL_UINT64(120 - ZB_MAX_CATCHUP_TICKS, l.dropped);
  /* Normal frames afterwards: no further warnings. */
  run_one_second(&l, 60);
  TEST_ASSERT_EQUAL_INT(1, g_warnings);
}

/* spec: game-loop: Frame between ticks */
static void test_frame_between_ticks(void) {
  zb_loop l;
  zb_loop_init(&l);
  zb_loop_advance(&l, 0);
  uint64_t tick_ns = ZB_NS_PER_SEC / ZB_TICK_HZ;
  TEST_ASSERT_EQUAL_UINT(1, zb_loop_advance(&l, tick_ns + tick_ns / 2));
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.5, zb_loop_alpha(&l));
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0 / 60.0, zb_loop_tick_seconds());
}

/* spec: game-loop: Paused game keeps rendering */
static void test_pause(void) {
  zb_loop l;
  zb_loop_init(&l);
  zb_loop_advance(&l, 0);
  zb_loop_set_paused(&l, true);
  TEST_ASSERT_EQUAL_UINT64(0, run_one_second(&l, 60));
  zb_loop_set_paused(&l, false);
  TEST_ASSERT_EQUAL_UINT64(60, run_one_second(&l, 60));
}

static void test_time_scale_and_clock_going_backwards(void) {
  zb_loop l;
  zb_loop_init(&l);
  zb_loop_advance(&l, 1000);
  zb_loop_set_time_scale(&l, 2.0);
  TEST_ASSERT_EQUAL_UINT64(120, run_one_second(&l, 60));
  zb_loop_set_time_scale(&l, 0.01);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.1, l.time_scale);
  zb_loop_set_time_scale(&l, 50.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, l.time_scale);
  zb_loop_set_time_scale(&l, 1.0);
  uint64_t before = l.tick;
  TEST_ASSERT_EQUAL_UINT(0, zb_loop_advance(&l, 5)); /* clock went backwards */
  TEST_ASSERT_EQUAL_UINT64(before, l.tick);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_low_frame_rate);
  RUN_TEST(test_high_frame_rate);
  RUN_TEST(test_two_second_stall);
  RUN_TEST(test_frame_between_ticks);
  RUN_TEST(test_pause);
  RUN_TEST(test_time_scale_and_clock_going_backwards);
  return UNITY_END();
}

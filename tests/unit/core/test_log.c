/* SPDX-License-Identifier: MIT */
#include "core/log.h"
#include "support/test_support.h"
#include "unity.h"

#include <string.h>

typedef struct capture {
  int count;
  zb_log_level last_level;
  char last[256];
} capture;

static void sink(zb_log_level level, const char *msg, void *user) {
  capture *c = user;
  c->count++;
  c->last_level = level;
  strncpy(c->last, msg, sizeof c->last - 1);
  c->last[sizeof c->last - 1] = '\0';
}

void setUp(void) {
  zb_test_begin();
  zb_log_reset();
  zb_log_set_stderr(false);
}

void tearDown(void) {
  zb_log_reset();
  zb_test_end();
}

static void test_levels_and_sinks(void) {
  capture c = {0};
  TEST_ASSERT_TRUE(zb_log_add_sink(sink, &c));
  zb_debug("hidden %d", 1);
  TEST_ASSERT_EQUAL_INT(0, c.count);
  zb_info("shown %d", 2);
  TEST_ASSERT_EQUAL_INT(1, c.count);
  TEST_ASSERT_EQUAL_STRING("shown 2", c.last);
  zb_log_set_level(ZB_LOG_TRACE);
  TEST_ASSERT_EQUAL_INT(ZB_LOG_TRACE, zb_log_get_level());
  zb_trace("t");
  zb_debug("d");
  zb_warn("w");
  zb_error("e");
  TEST_ASSERT_EQUAL_INT(5, c.count);
  TEST_ASSERT_EQUAL_INT(ZB_LOG_ERROR, c.last_level);
  zb_log(ZB_LOG_OFF, "never");
  TEST_ASSERT_EQUAL_INT(5, c.count);
  zb_log_remove_sink(sink, &c);
  zb_error("gone");
  TEST_ASSERT_EQUAL_INT(5, c.count);
}

static void test_level_names(void) {
  TEST_ASSERT_EQUAL_STRING("warn", zb_log_level_name(ZB_LOG_WARN));
  TEST_ASSERT_EQUAL_STRING("off", zb_log_level_name(ZB_LOG_OFF));
  TEST_ASSERT_EQUAL_STRING("?", zb_log_level_name((zb_log_level)42));
}

static void test_sink_slots_full(void) {
  capture c[ZB_LOG_MAX_SINKS + 1];
  memset(c, 0, sizeof c);
  for (int i = 0; i < ZB_LOG_MAX_SINKS; ++i) {
    TEST_ASSERT_TRUE(zb_log_add_sink(sink, &c[i]));
  }
  TEST_ASSERT_FALSE(zb_log_add_sink(sink, &c[ZB_LOG_MAX_SINKS]));
}

static void test_once(void) {
  capture c = {0};
  zb_log_add_sink(sink, &c);
  TEST_ASSERT_TRUE(zb_log_once(ZB_LOG_WARN, "missing:textures/x.png", "missing %s", "x"));
  TEST_ASSERT_FALSE(zb_log_once(ZB_LOG_WARN, "missing:textures/x.png", "missing %s", "x"));
  TEST_ASSERT_TRUE(zb_log_once(ZB_LOG_WARN, "missing:textures/y.png", "missing %s", "y"));
  TEST_ASSERT_EQUAL_INT(2, c.count);
  char key[32];
  for (int i = 0; i < ZB_LOG_MAX_ONCE + 5; ++i) { /* overflow still logs */
    snprintf(key, sizeof key, "k%d", i);
    zb_log_once(ZB_LOG_WARN, key, "x");
  }
}

static void test_stderr_sink(void) {
  zb_log_set_stderr(true);
  zb_info("printed to stderr by the built-in sink");
  zb_log_set_stderr(false);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_levels_and_sinks);
  RUN_TEST(test_level_names);
  RUN_TEST(test_sink_slots_full);
  RUN_TEST(test_once);
  RUN_TEST(test_stderr_sink);
  return UNITY_END();
}

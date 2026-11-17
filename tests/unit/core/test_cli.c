/* SPDX-License-Identifier: MIT */
#include "core/cli.h"
#include "support/test_support.h"
#include "unity.h"

#include <stdarg.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static zb_result parse(zb_options *o, char *err, int argc, ...) {
  char *argv[32];
  va_list ap;
  va_start(ap, argc);
  argv[0] = "zauberer";
  for (int i = 1; i < argc; ++i) {
    argv[i] = va_arg(ap, char *);
  }
  va_end(ap);
  return zb_options_parse(o, argc, argv, err, 256);
}

static void test_defaults(void) {
  zb_options o;
  char err[256];
  TEST_ASSERT_EQUAL_INT(ZB_OK, parse(&o, err, 1));
  TEST_ASSERT_NULL(o.assets_dir);
  TEST_ASSERT_FALSE(o.headless);
  TEST_ASSERT_EQUAL_INT64(-1, o.ticks);
  TEST_ASSERT_EQUAL_INT(1, o.debug_views);
}

static void test_all_options(void) {
  zb_options o;
  char err[256];
  TEST_ASSERT_EQUAL_INT(ZB_OK, parse(&o, err, 25, "--headless", "--ticks", "600", "--assets", "a",
                                     "--user-dir", "u", "--test", "t", "--scenario", "s.lua",
                                     "--junit", "j.xml", "--record", "r.bin", "--replay", "p.bin",
                                     "--debug-views", "2", "--lua-coverage", "c.out", "--seed", "0x2A",
                                     "--help"));
  TEST_ASSERT_TRUE(o.headless);
  TEST_ASSERT_EQUAL_INT64(600, o.ticks);
  TEST_ASSERT_EQUAL_STRING("a", o.assets_dir);
  TEST_ASSERT_EQUAL_STRING("u", o.user_dir);
  TEST_ASSERT_EQUAL_STRING("t", o.test_path);
  TEST_ASSERT_EQUAL_STRING("s.lua", o.scenario_path);
  TEST_ASSERT_EQUAL_STRING("j.xml", o.junit_path);
  TEST_ASSERT_EQUAL_STRING("r.bin", o.record_path);
  TEST_ASSERT_EQUAL_STRING("p.bin", o.replay_path);
  TEST_ASSERT_EQUAL_INT(2, o.debug_views);
  TEST_ASSERT_EQUAL_STRING("c.out", o.lua_coverage_path);
  TEST_ASSERT_EQUAL_UINT64(42, o.seed);
  TEST_ASSERT_TRUE(o.seed_set);
  TEST_ASSERT_TRUE(o.help);
  TEST_ASSERT_EQUAL_INT(ZB_OK, parse(&o, err, 2, "--version"));
  TEST_ASSERT_TRUE(o.version);
}

static void test_errors(void) {
  zb_options o;
  char err[256];
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse(&o, err, 2, "--bogus"));
  TEST_ASSERT_NOT_NULL(strstr(err, "unknown option '--bogus'"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse(&o, err, 2, "--ticks"));
  TEST_ASSERT_NOT_NULL(strstr(err, "needs a value"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse(&o, err, 3, "--ticks", "-5"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse(&o, err, 3, "--ticks", "12x"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse(&o, err, 3, "--ticks", ""));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse(&o, err, 3, "--debug-views", "3"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse(&o, err, 3, "--seed", "-1"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse(&o, err, 3, "--seed", "abc"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse(&o, err, 3, "--seed", ""));
  TEST_ASSERT_NOT_NULL(strstr(zb_options_usage(), "--headless"));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_all_options);
  RUN_TEST(test_errors);
  return UNITY_END();
}

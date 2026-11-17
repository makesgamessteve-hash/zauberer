/* SPDX-License-Identifier: MIT */
#include "core/base.h"
#include "support/test_support.h"
#include "unity.h"

#include <string.h>

ZB_TEST_LEAK_CHECKED

static int g_fired;
static const char *g_expr;

static void capture(const char *expr, const char *file, int line) {
  (void)file;
  (void)line;
  g_fired++;
  g_expr = expr;
}

static void test_result_names(void) {
  TEST_ASSERT_EQUAL_STRING("ok", zb_result_str(ZB_OK));
  TEST_ASSERT_EQUAL_STRING("out of memory", zb_result_str(ZB_ERR_NOMEM));
  TEST_ASSERT_EQUAL_STRING("cancelled", zb_result_str(ZB_ERR_CANCELLED));
  TEST_ASSERT_EQUAL_STRING("unknown error", zb_result_str(ZB_RESULT_COUNT));
  TEST_ASSERT_EQUAL_STRING("unknown error", zb_result_str((zb_result)-1));
}

static void test_assert_handler(void) {
  zb_set_assert_handler(capture);
  g_fired = 0;
  ZB_ASSERT(1 + 1 == 2);
  TEST_ASSERT_EQUAL_INT(0, g_fired);
  ZB_ASSERT(1 + 1 == 3);
#ifdef NDEBUG
  TEST_ASSERT_EQUAL_INT(0, g_fired); /* assertions compile out in release builds */
  zb_assert_fail("1 + 1 == 3", __FILE__, __LINE__);
#endif
  TEST_ASSERT_EQUAL_INT(1, g_fired);
  TEST_ASSERT_EQUAL_STRING("1 + 1 == 3", g_expr);
  zb_set_assert_handler(NULL);
}

static void test_min_max(void) {
  TEST_ASSERT_EQUAL_INT(2, ZB_MIN(2, 5));
  TEST_ASSERT_EQUAL_INT(5, ZB_MAX(2, 5));
  int a[7];
  TEST_ASSERT_EQUAL_size_t(7, ZB_ARRAY_LEN(a));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_result_names);
  RUN_TEST(test_assert_handler);
  RUN_TEST(test_min_max);
  return UNITY_END();
}

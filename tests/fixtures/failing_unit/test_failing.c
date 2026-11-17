/* SPDX-License-Identifier: MIT */
/* A deliberately failing Unity test. It is built but not registered with
 * CTest; tests/engine runs it to check how failures are reported. */
#include "support/test_support.h"
#include "unity.h"

ZB_TEST_LEAK_CHECKED

static void test_deliberate_failure(void) { TEST_ASSERT_EQUAL_INT_MESSAGE(1, 2, "deliberate failure"); }

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_deliberate_failure);
  return UNITY_END();
}

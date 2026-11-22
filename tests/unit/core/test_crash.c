/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/crash.h"
#include "core/log.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static char *read_all(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  static char buf[200000];
  size_t n = fread(buf, 1, sizeof buf - 1, f);
  fclose(f);
  buf[n] = 0;
  return buf;
}

/* spec: platform-targets: A fatal error */
static void test_crash_log(void) {
  TEST_ASSERT_FALSE(zb_crash_write("before install"));
  TEST_ASSERT_EQUAL_STRING("", zb_crash_path());
  TEST_ASSERT_FALSE(zb_crash_install(NULL, false));
  TEST_ASSERT_TRUE(zb_crash_install("", false));
  TEST_ASSERT_TRUE(zb_crash_install("", true)); /* (again: the same) */
  TEST_ASSERT_EQUAL_STRING("crash.log", zb_crash_path());
  zb_log_set_stderr(false);
  for (int i = 0; i < 250; ++i) {
    zb_info("line %d", i);
  }
  TEST_ASSERT_TRUE(zb_crash_write("a test"));
  char *text = read_all("crash.log");
  TEST_ASSERT_NOT_NULL(text);
  TEST_ASSERT_NOT_NULL(strstr(text, "Zauberer stopped: a test"));
  TEST_ASSERT_NOT_NULL(strstr(text, "line 249"));
  TEST_ASSERT_NOT_NULL(strstr(text, "line 50"));
  TEST_ASSERT_NULL(strstr(text, "line 49\n")); /* (only the last 200 lines) */
  zb_crash_uninstall();
  zb_crash_uninstall();
  TEST_ASSERT_FALSE(zb_crash_write("after"));
  TEST_ASSERT_TRUE(zb_crash_install("/nonexistent-dir-zb/", false));
  TEST_ASSERT_FALSE(zb_crash_write("cannot write")); /* (no such folder) */
  zb_crash_uninstall();
  remove("crash.log");
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_crash_log);
  return UNITY_END();
}

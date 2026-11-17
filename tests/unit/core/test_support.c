/* SPDX-License-Identifier: MIT */
/* Tests of the test support library itself. */
#include "support/test_support.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static void test_failing_allocator(void) {
  zb_failing_allocator f;
  const zb_allocator *a = zb_failing_init(&f, zb_heap_allocator(), 1);
  void *p = zb_alloc(a, 4);
  TEST_ASSERT_NOT_NULL(p);
  TEST_ASSERT_NULL(zb_realloc(a, p, 8));
  zb_free(a, p);
  f.budget = -1;
  p = zb_realloc(a, NULL, 8);
  TEST_ASSERT_NOT_NULL(p);
  zb_free(a, p);
}

static void test_files(void) {
  char path[1200];
  snprintf(path, sizeof path, "%s/sub/dir/file.txt", zb_test_tmp_dir());
  TEST_ASSERT_TRUE(zb_test_write_file(path, "hello", 5));
  size_t len = 0;
  char *data = zb_test_read_file(path, &len);
  TEST_ASSERT_EQUAL_size_t(5, len);
  TEST_ASSERT_EQUAL_STRING("hello", data);
  free(data);
  TEST_ASSERT_NULL(zb_test_read_file("/definitely/not/here.txt", NULL));
  char sub[1200];
  snprintf(sub, sizeof sub, "%s/sub", zb_test_tmp_dir());
  zb_test_remove_tree(sub);
  TEST_ASSERT_NULL(zb_test_read_file(path, NULL));
  TEST_ASSERT_NOT_NULL(zb_test_source_dir());
}

static void test_alloc_restarts(void) {
  const zb_allocator *a = zb_test_alloc();
  void *p = zb_alloc(a, 1);
  zb_free(a, p);
  zb_test_begin(); /* restart while active */
  zb_test_end();
  zb_test_end();       /* idempotent */
  a = zb_test_alloc(); /* lazily begins again */
  TEST_ASSERT_NOT_NULL(a);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_failing_allocator);
  RUN_TEST(test_files);
  RUN_TEST(test_alloc_restarts);
  return UNITY_END();
}

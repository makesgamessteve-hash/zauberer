/* SPDX-License-Identifier: MIT */
#include "core/array.h"
#include "support/test_support.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static void test_array_push_at_remove(void) {
  zb_array arr;
  zb_array_init(&arr, zb_test_alloc(), sizeof(int));
  for (int i = 0; i < 20; ++i) {
    TEST_ASSERT_NOT_NULL(zb_array_push(&arr, &i));
  }
  TEST_ASSERT_EQUAL_size_t(20, arr.len);
  TEST_ASSERT_EQUAL_INT(7, *(int *)zb_array_at(&arr, 7));
  zb_array_remove(&arr, 0); /* keeps order */
  TEST_ASSERT_EQUAL_INT(1, *(int *)zb_array_at(&arr, 0));
  zb_array_remove_swap(&arr, 0); /* last moves in */
  TEST_ASSERT_EQUAL_INT(19, *(int *)zb_array_at(&arr, 0));
  zb_array_remove_swap(&arr, arr.len - 1);
  TEST_ASSERT_EQUAL_size_t(17, arr.len);
  zb_array_pop(&arr);
  TEST_ASSERT_EQUAL_size_t(16, arr.len);
  int *zero = zb_array_push(&arr, NULL);
  TEST_ASSERT_EQUAL_INT(0, *zero);
  zb_array_clear(&arr);
  TEST_ASSERT_EQUAL_size_t(0, arr.len);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_array_reserve(&arr, 4)); /* already larger */
  zb_array_free(&arr);
  TEST_ASSERT_NULL(arr.data);
}

static void test_array_oom(void) {
  zb_failing_allocator f;
  zb_array arr;
  zb_array_init(&arr, zb_failing_init(&f, zb_test_alloc(), 0), sizeof(int));
  int v = 3;
  TEST_ASSERT_NULL(zb_array_push(&arr, &v));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_array_reserve(&arr, 100));
  zb_array_free(&arr);
}

static void test_map_put_get_remove_iterate(void) {
  zb_map m;
  zb_map_init(&m, zb_test_alloc());
  uint64_t v;
  TEST_ASSERT_FALSE(zb_map_get(&m, 1, &v));
  TEST_ASSERT_FALSE(zb_map_remove(&m, 1));
  for (uint64_t k = 0; k < 1000; ++k) {
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_map_put(&m, k * 7919u, k));
  }
  TEST_ASSERT_EQUAL_size_t(1000, m.count);
  TEST_ASSERT_TRUE(zb_map_get(&m, 7919u * 500u, &v));
  TEST_ASSERT_EQUAL_UINT64(500, v);
  TEST_ASSERT_TRUE(zb_map_get(&m, 0, NULL));
  zb_map_put(&m, 0, 42); /* overwrite */
  zb_map_get(&m, 0, &v);
  TEST_ASSERT_EQUAL_UINT64(42, v);
  for (uint64_t k = 0; k < 1000; k += 2) {
    TEST_ASSERT_TRUE(zb_map_remove(&m, k * 7919u));
  }
  TEST_ASSERT_FALSE(zb_map_remove(&m, 0));
  TEST_ASSERT_FALSE(zb_map_get(&m, 2u * 7919u, &v));
  size_t cursor = 0, seen = 0;
  uint64_t key;
  while (zb_map_next(&m, &cursor, &key, &v)) {
    TEST_ASSERT_EQUAL_UINT64(key / 7919u, v);
    seen++;
  }
  TEST_ASSERT_EQUAL_size_t(500, seen);
  /* Many removals then inserts reuse deleted slots via rehash. */
  for (uint64_t k = 5000; k < 6000; ++k) {
    zb_map_put(&m, k, k);
  }
  TEST_ASSERT_EQUAL_size_t(1500, m.count);
  zb_map_clear(&m);
  TEST_ASSERT_FALSE(zb_map_get(&m, 5000, &v));
  zb_map_free(&m);
  zb_map_clear(&m);
}

static void test_map_oom(void) {
  zb_failing_allocator f;
  zb_map m;
  zb_map_init(&m, zb_failing_init(&f, zb_test_alloc(), 0));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_map_put(&m, 1, 1));
  zb_map_free(&m);
}

static void test_strtab(void) {
  zb_strtab t;
  zb_strtab_init(&t, zb_test_alloc());
  uint32_t a = zb_strtab_intern(&t, "jump", 4);
  uint32_t b = zb_strtab_intern(&t, "cast_left", 9);
  TEST_ASSERT_NOT_EQUAL(a, b);
  TEST_ASSERT_EQUAL_UINT32(a, zb_strtab_intern(&t, "jump", 4));
  TEST_ASSERT_EQUAL_UINT32(b, zb_strtab_find(&t, "cast_left", 9));
  TEST_ASSERT_EQUAL_UINT32(ZB_STR_NONE, zb_strtab_find(&t, "nope", 4));
  TEST_ASSERT_EQUAL_UINT32(ZB_STR_NONE, zb_strtab_find(&t, "jum", 3));
  TEST_ASSERT_EQUAL_STRING("jump", zb_strtab_get(&t, a));
  TEST_ASSERT_NULL(zb_strtab_get(&t, 99));
  char name[32];
  for (int i = 0; i < 300; ++i) {
    snprintf(name, sizeof name, "action_%d", i);
    zb_strtab_intern(&t, name, strlen(name));
  }
  TEST_ASSERT_EQUAL_size_t(302, zb_strtab_count(&t));
  TEST_ASSERT_EQUAL_STRING("action_299", zb_strtab_get(&t, zb_strtab_find(&t, "action_299", 10)));
  zb_strtab_free(&t);
}

static void test_strtab_oom(void) {
  zb_failing_allocator f;
  zb_strtab t;
  zb_strtab_init(&t, zb_failing_init(&f, zb_test_alloc(), 0));
  TEST_ASSERT_EQUAL_UINT32(ZB_STR_NONE, zb_strtab_intern(&t, "x", 1));
  zb_strtab_free(&t);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_array_push_at_remove);
  RUN_TEST(test_array_oom);
  RUN_TEST(test_map_put_get_remove_iterate);
  RUN_TEST(test_map_oom);
  RUN_TEST(test_strtab);
  RUN_TEST(test_strtab_oom);
  return UNITY_END();
}

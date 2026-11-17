/* SPDX-License-Identifier: MIT */
#include "core/alloc.h"
#include "support/test_support.h"
#include "unity.h"

#include <string.h>

ZB_TEST_LEAK_CHECKED

static void test_heap_alloc_realloc_free(void) {
  const zb_allocator *a = zb_heap_allocator();
  char *p = zb_alloc(a, 16);
  TEST_ASSERT_NOT_NULL(p);
  p = zb_realloc(a, p, 64);
  TEST_ASSERT_NOT_NULL(p);
  zb_free(a, p);
  void *z = zb_alloc(a, 0);
  TEST_ASSERT_NOT_NULL(z);
  z = zb_realloc(a, z, 0);
  zb_free(a, z);
}

static void test_calloc_and_strdup(void) {
  const zb_allocator *a = zb_test_alloc();
  unsigned char *z = zb_calloc(a, 32);
  for (int i = 0; i < 32; ++i) {
    TEST_ASSERT_EQUAL_UINT8(0, z[i]);
  }
  char *s = zb_strdup(a, "zauberer");
  TEST_ASSERT_EQUAL_STRING("zauberer", s);
  char *n = zb_strndup(a, "carpet ride", 6);
  TEST_ASSERT_EQUAL_STRING("carpet", n);
  zb_free(a, z);
  zb_free(a, s);
  zb_free(a, n);
}

static void test_calloc_strdup_fail(void) {
  zb_failing_allocator f;
  const zb_allocator *a = zb_failing_init(&f, zb_test_alloc(), 0);
  TEST_ASSERT_NULL(zb_calloc(a, 8));
  TEST_ASSERT_NULL(zb_strdup(a, "x"));
}

static void test_arena_basic(void) {
  zb_arena arena;
  zb_arena_init(&arena, zb_test_alloc(), 64);
  int *a = zb_arena_alloc(&arena, sizeof(int) * 4, sizeof(int));
  TEST_ASSERT_NOT_NULL(a);
  TEST_ASSERT_EQUAL_UINT(0, (uintptr_t)a % sizeof(int));
  double *d = zb_arena_alloc(&arena, sizeof(double), 32);
  TEST_ASSERT_EQUAL_UINT(0, (uintptr_t)d % 32);
  /* Larger than a block: gets its own block. */
  char *big = zb_arena_alloc(&arena, 1000, 1);
  TEST_ASSERT_NOT_NULL(big);
  memset(big, 'x', 1000);
  char *s = zb_arena_strndup(&arena, "hello world", 5);
  TEST_ASSERT_EQUAL_STRING("hello", s);
  TEST_ASSERT_TRUE(arena.total >= 1016);
  zb_arena_reset(&arena);
  TEST_ASSERT_EQUAL_size_t(0, arena.total);
  void *again = zb_arena_alloc(&arena, 8, 8);
  TEST_ASSERT_NOT_NULL(again);
  zb_arena_destroy(&arena);
  zb_arena_reset(&arena); /* reset of an empty arena is harmless */
}

static void test_arena_default_block_and_failure(void) {
  zb_failing_allocator f;
  zb_arena arena;
  zb_arena_init(&arena, zb_failing_init(&f, zb_test_alloc(), 1), 0);
  TEST_ASSERT_EQUAL_size_t(4096, arena.block_size);
  TEST_ASSERT_NOT_NULL(zb_arena_alloc(&arena, 16, 8));
  TEST_ASSERT_NULL(zb_arena_alloc(&arena, 8192, 8));
  TEST_ASSERT_NULL(zb_arena_strndup(&arena, "too big for any block", 5000));
  zb_arena_destroy(&arena);
}

typedef struct counter {
  size_t n;
  size_t bytes;
} counter;

static void count_entry(const zb_tracking_entry *e, void *user) {
  counter *c = user;
  c->n++;
  c->bytes += e->size;
}

static void test_tracking_counts_and_report(void) {
  zb_tracking_allocator t;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_tracking_init(&t, zb_test_alloc()));
  const zb_allocator *a = zb_tracking_iface(&t);
  void *ptrs[600];
  for (int i = 0; i < 600; ++i) { /* forces the table to grow */
    ptrs[i] = zb_alloc(a, 10);
  }
  TEST_ASSERT_EQUAL_size_t(600, zb_tracking_live_count(&t));
  TEST_ASSERT_EQUAL_size_t(6000, zb_tracking_live_bytes(&t));
  for (int i = 0; i < 600; i += 2) {
    zb_free(a, ptrs[i]);
  }
  counter c = {0, 0};
  TEST_ASSERT_EQUAL_size_t(300, zb_tracking_report(&t, count_entry, &c));
  TEST_ASSERT_EQUAL_size_t(300, c.n);
  TEST_ASSERT_EQUAL_size_t(3000, c.bytes);
  TEST_ASSERT_EQUAL_size_t(300, zb_tracking_report(&t, NULL, NULL));
  for (int i = 1; i < 600; i += 2) {
    ptrs[i] = zb_realloc(a, ptrs[i], 20);
  }
  TEST_ASSERT_EQUAL_size_t(6000, zb_tracking_live_bytes(&t));
  for (int i = 1; i < 600; i += 2) {
    zb_free(a, ptrs[i]);
  }
  zb_free(a, NULL);
  TEST_ASSERT_EQUAL_size_t(0, zb_tracking_live_count(&t));
  TEST_ASSERT_TRUE(t.peak_bytes >= 6000);
  void *r = zb_realloc(a, NULL, 5); /* realloc(NULL) behaves like alloc */
  TEST_ASSERT_EQUAL_size_t(1, zb_tracking_live_count(&t));
  zb_free(a, r);
  zb_tracking_destroy(&t);
}

static int g_locks;
static void fake_lock(void *m) { (void)m; g_locks++; }
static void fake_unlock(void *m) { (void)m; g_locks--; }

static void test_tracking_lock_and_failures(void) {
  zb_failing_allocator f;
  const zb_allocator *parent = zb_failing_init(&f, zb_test_alloc(), 1);
  zb_tracking_allocator t;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_tracking_init(&t, parent)); /* uses the only success */
  zb_tracking_lock lock = {fake_lock, fake_unlock, NULL};
  zb_tracking_set_lock(&t, lock);
  const zb_allocator *a = zb_tracking_iface(&t);
  TEST_ASSERT_NULL(zb_alloc(a, 8));
  TEST_ASSERT_EQUAL_INT(0, g_locks);
  f.budget = -1;
  void *p = zb_alloc(a, 8);
  f.budget = f.calls; /* next call fails */
  TEST_ASSERT_NULL(zb_realloc(a, p, 64));
  TEST_ASSERT_EQUAL_size_t(1, zb_tracking_live_count(&t));
  zb_free(a, p);
  zb_tracking_destroy(&t);

  zb_failing_allocator none;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_tracking_init(&t, zb_failing_init(&none, zb_test_alloc(), 0)));
}

static void test_tracking_bookkeeping_growth_failure(void) {
  zb_failing_allocator f;
  const zb_allocator *parent = zb_failing_init(&f, zb_test_alloc(), -1);
  zb_tracking_allocator t;
  zb_tracking_init(&t, parent);
  const zb_allocator *a = zb_tracking_iface(&t);
  void *ptrs[128];
  for (int i = 0; i < 127; ++i) {
    ptrs[i] = zb_alloc(a, 4);
  }
  /* Table of 256 is now at the growth threshold; make growth fail. */
  f.budget = f.calls;
  TEST_ASSERT_NULL(zb_alloc(a, 4));
  TEST_ASSERT_NULL(zb_realloc(a, ptrs[0], 8));
  f.budget = -1;
  for (int i = 0; i < 127; ++i) {
    zb_free(a, ptrs[i]);
  }
  zb_tracking_destroy(&t);
}

/* spec: test-automation: Leaking test */
static void test_leak_is_reported_with_site(void) {
  zb_tracking_allocator t;
  zb_tracking_init(&t, zb_heap_allocator());
  void *leak = zb_alloc(zb_tracking_iface(&t), 24);
  char buf[512];
  TEST_ASSERT_EQUAL_size_t(1, zb_test_describe_leaks(&t, buf, sizeof buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "test_alloc.c:"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "(24 bytes)"));
  zb_free(zb_tracking_iface(&t), leak);
  TEST_ASSERT_EQUAL_size_t(0, zb_test_describe_leaks(&t, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("", buf);
  zb_tracking_destroy(&t);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_heap_alloc_realloc_free);
  RUN_TEST(test_calloc_and_strdup);
  RUN_TEST(test_calloc_strdup_fail);
  RUN_TEST(test_arena_basic);
  RUN_TEST(test_arena_default_block_and_failure);
  RUN_TEST(test_tracking_counts_and_report);
  RUN_TEST(test_tracking_lock_and_failures);
  RUN_TEST(test_tracking_bookkeeping_growth_failure);
  RUN_TEST(test_leak_is_reported_with_site);
  return UNITY_END();
}

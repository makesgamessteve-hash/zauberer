/* SPDX-License-Identifier: MIT */
#include "core/hash.h"
#include "support/test_support.h"
#include "unity.h"

#include <string.h>

ZB_TEST_LEAK_CHECKED

/* Fixed vectors: these values must be identical on every platform
 * (Linux, Windows, macOS, WebAssembly). Changing them breaks seeds. */
static void test_fixed_vectors(void) {
  TEST_ASSERT_EQUAL_HEX64(0x5692161d100b05e5ULL, zb_mix64(1));
  TEST_ASSERT_EQUAL_HEX64(zb_hash64("", 0, 0), zb_hash64("", 0, 0));
  char hex[17];
  zb_hash_hex(zb_hash64("zauberer", 8, 0), hex);
  TEST_ASSERT_EQUAL_STRING("0c83bbfeb70d6bd6", hex);
  zb_hash_hex(zb_rng_at(42, 7, 0), hex);
  TEST_ASSERT_EQUAL_STRING("cca0b579a1ec656e", hex);
}

static void test_hash_properties(void) {
  uint64_t a = zb_hash64("abcdefghij", 10, 0);
  TEST_ASSERT_NOT_EQUAL(a, zb_hash64("abcdefghij", 10, 1));
  TEST_ASSERT_NOT_EQUAL(a, zb_hash64("abcdefghik", 10, 0));
  TEST_ASSERT_NOT_EQUAL(zb_hash64("a", 1, 0), zb_hash64("a\0", 2, 0));
  TEST_ASSERT_EQUAL_HEX64(zb_hash64("key", 3, 0), zb_hash_str("key"));
  TEST_ASSERT_NOT_EQUAL(zb_hash_combine(1, 2), zb_hash_combine(2, 1));
}

static void test_rng_stream(void) {
  zb_rng r = zb_rng_make(42, 7);
  TEST_ASSERT_EQUAL_HEX64(zb_rng_at(42, 7, 0), zb_rng_next(&r));
  TEST_ASSERT_EQUAL_HEX64(zb_rng_at(42, 7, 1), zb_rng_next(&r));
  TEST_ASSERT_NOT_EQUAL(zb_rng_at(42, 7, 0), zb_rng_at(42, 8, 0));
  for (int i = 0; i < 1000; ++i) {
    double d = zb_rng_next_double(&r);
    TEST_ASSERT_TRUE(d >= 0.0 && d < 1.0);
    int64_t v = zb_rng_range(&r, -3, 3);
    TEST_ASSERT_TRUE(v >= -3 && v <= 3);
  }
  TEST_ASSERT_EQUAL_INT64(5, zb_rng_range(&r, 5, 5));
  (void)zb_rng_range(&r, INT64_MIN, INT64_MAX);
}

static void test_rng_range_covers_all_values(void) {
  zb_rng r = zb_rng_make(1, 1);
  int seen[6] = {0};
  for (int i = 0; i < 600; ++i) {
    seen[zb_rng_range(&r, 0, 5)]++;
  }
  for (int i = 0; i < 6; ++i) {
    TEST_ASSERT_TRUE(seen[i] > 50);
  }
}

static void test_hasher(void) {
  zb_hasher h1, h2;
  zb_hasher_init(&h1, 9);
  zb_hasher_init(&h2, 9);
  zb_hasher_str(&h1, "tick");
  zb_hasher_str(&h2, "tick");
  zb_hasher_u64(&h1, 600);
  zb_hasher_u64(&h2, 600);
  zb_hasher_f64(&h1, 0.5);
  zb_hasher_f64(&h2, 0.5);
  TEST_ASSERT_EQUAL_HEX64(zb_hasher_final(&h1), zb_hasher_final(&h2));
  zb_hasher_f64(&h1, 0.0);
  zb_hasher_f64(&h2, -0.0);
  TEST_ASSERT_NOT_EQUAL(zb_hasher_final(&h1), zb_hasher_final(&h2));
  zb_hasher a, b;
  zb_hasher_init(&a, 0);
  zb_hasher_init(&b, 0);
  double nan1 = 0.0 / 0.0;
  double nan2 = -nan1;
  zb_hasher_f64(&a, nan1);
  zb_hasher_f64(&b, nan2);
  TEST_ASSERT_EQUAL_HEX64(zb_hasher_final(&a), zb_hasher_final(&b));
  zb_hasher_bytes(&a, "x", 1);
  TEST_ASSERT_NOT_EQUAL(zb_hasher_final(&a), zb_hasher_final(&b));
}

static void test_hex(void) {
  char buf[17];
  zb_hash_hex(0x0123456789abcdefULL, buf);
  TEST_ASSERT_EQUAL_STRING("0123456789abcdef", buf);
  zb_hash_hex(0, buf);
  TEST_ASSERT_EQUAL_STRING("0000000000000000", buf);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_fixed_vectors);
  RUN_TEST(test_hash_properties);
  RUN_TEST(test_rng_stream);
  RUN_TEST(test_rng_range_covers_all_values);
  RUN_TEST(test_hasher);
  RUN_TEST(test_hex);
  return UNITY_END();
}

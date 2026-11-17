/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "core/hash.h"
#include "support/test_support.h"
#include "unity.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

#define SAMPLES 100000

typedef double (*fn1)(double);

/* splitmix64: platform-independent argument generator. */
static uint64_t next_u64(uint64_t *s) {
  uint64_t z = (*s += 0x9e3779b97f4a7c15ull);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

/* Uniform in [lo, hi) (exactly representable steps, no libm). */
static double uniform(uint64_t *s, double lo, double hi) {
  return lo + (hi - lo) * ((double)(next_u64(s) >> 11) * (1.0 / 9007199254740992.0));
}

/* Log-uniform magnitude in [2^lo_exp, 2^hi_exp), random sign when signed. */
static double log_uniform(uint64_t *s, int lo_exp, int hi_exp, bool sign) {
  double m = 1.0 + uniform(s, 0.0, 1.0);
  int e = lo_exp + (int)(next_u64(s) % (uint64_t)(hi_exp - lo_exp));
  double v = zb_dscalbn(m, e);
  return sign && (next_u64(s) & 1) ? -v : v;
}

static int64_t ordered(double x) {
  int64_t i;
  memcpy(&i, &x, sizeof i);
  return i < 0 ? INT64_MIN - i : i;
}

static uint64_t ulp_diff(double a, double b) {
  if (isnan(a) || isnan(b)) {
    return isnan(a) && isnan(b) ? 0 : UINT64_MAX;
  }
  int64_t ia = ordered(a), ib = ordered(b);
  return ia > ib ? (uint64_t)ia - (uint64_t)ib : (uint64_t)ib - (uint64_t)ia;
}

typedef struct case1 {
  const char *name;
  fn1 f, ref;
  int kind; /* 0 uniform, 1 log-uniform signed, 2 log-uniform positive */
  double lo, hi;
} case1;

static const case1 CASES[] = {
    {"sin", zb_dsin, sin, 0, -10000.0, 10000.0}, {"sin small", zb_dsin, sin, 1, -40, 3},
    {"cos", zb_dcos, cos, 0, -10000.0, 10000.0}, {"cos small", zb_dcos, cos, 1, -40, 3},
    {"tan", zb_dtan, tan, 0, -10000.0, 10000.0}, {"tan small", zb_dtan, tan, 1, -40, 3},
    {"atan", zb_datan, atan, 1, -40, 80},        {"asin", zb_dasin, asin, 0, -1.0, 1.0},
    {"acos", zb_dacos, acos, 0, -1.0, 1.0},      {"exp", zb_dexp, exp, 0, -745.0, 709.0},
    {"exp small", zb_dexp, exp, 1, -40, 0},      {"log", zb_dlog, log, 2, -1074, 1023},
    {"log near 1", zb_dlog, log, 0, 0.5, 2.0},
};

static double gen(uint64_t *s, const case1 *c) {
  switch (c->kind) {
    case 0: return uniform(s, c->lo, c->hi);
    case 1: return log_uniform(s, (int)c->lo, (int)c->hi, true);
    default: return log_uniform(s, (int)c->lo, (int)c->hi, false);
  }
}

/* spec: deterministic-math: Compared with a reference */
static void test_accuracy(void) {
  for (size_t k = 0; k < sizeof CASES / sizeof CASES[0]; ++k) {
    const case1 *c = &CASES[k];
    uint64_t seed = 1000 + k, worst = 0;
    double worst_x = 0.0;
    for (int i = 0; i < SAMPLES; ++i) {
      double x = gen(&seed, c);
      uint64_t d = ulp_diff(c->f(x), c->ref(x));
      if (d > worst) {
        worst = d;
        worst_x = x;
      }
    }
    char msg[160];
    snprintf(msg, sizeof msg, "%s: %llu ulp at %.17g", c->name, (unsigned long long)worst, worst_x);
    TEST_ASSERT_TRUE_MESSAGE(worst <= 2, msg);
  }
  /* sin and cos near multiples of pi/2, where reduction matters most. */
  uint64_t seed = 77;
  for (int i = 0; i < SAMPLES; ++i) {
    double x = (double)(int)(next_u64(&seed) % 20000) * (ZB_DPI / 2.0) + uniform(&seed, -1e-6, 1e-6);
    TEST_ASSERT_TRUE(ulp_diff(zb_dsin(x), sin(x)) <= 2);
    TEST_ASSERT_TRUE(ulp_diff(zb_dcos(x), cos(x)) <= 2);
  }
  /* atan2 over all quadrants. */
  for (int i = 0; i < SAMPLES; ++i) {
    double y = uniform(&seed, -1e3, 1e3), x = uniform(&seed, -1e3, 1e3);
    TEST_ASSERT_TRUE(ulp_diff(zb_datan2(y, x), atan2(y, x)) <= 2);
  }
  /* pow: relative error, as exp(y * log(x)). */
  double worst = 0.0;
  for (int i = 0; i < SAMPLES; ++i) {
    double x = uniform(&seed, 1e-3, 1e3), y = uniform(&seed, -8.0, 8.0);
    double r = pow(x, y);
    double e = fabs(zb_dpow(x, y) - r) / r;
    worst = e > worst ? e : worst;
  }
  TEST_ASSERT_TRUE(worst <= 1e-13);
  /* Huge arguments are reduced deterministically (fmod by a rounded 2 pi). */
  double big = 1e7;
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, sin(big), zb_dsin(big));
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, cos(-big), zb_dcos(-big));
}

/* spec: deterministic-math: Out of domain */
static void test_special_values(void) {
  TEST_ASSERT_TRUE(isnan(zb_dlog(-1.0)));
  TEST_ASSERT_TRUE(isnan(zb_dasin(2.0)));
  TEST_ASSERT_TRUE(isnan(zb_dacos(-2.0)));
  TEST_ASSERT_TRUE(isnan(zb_dsin((double)INFINITY)));
  TEST_ASSERT_TRUE(isnan(zb_dcos((double)-INFINITY)));
  TEST_ASSERT_TRUE(isnan(zb_dtan((double)NAN)));
  TEST_ASSERT_TRUE(isnan(zb_datan((double)NAN)));
  TEST_ASSERT_TRUE(isnan(zb_dexp((double)NAN)));
  TEST_ASSERT_TRUE(isnan(zb_dlog((double)NAN)));
  TEST_ASSERT_TRUE(isnan(zb_dasin((double)NAN)));
  TEST_ASSERT_TRUE(isnan(zb_dacos((double)NAN)));
  TEST_ASSERT_TRUE(isnan(zb_datan2((double)NAN, 1.0)));
  TEST_ASSERT_TRUE(isnan(zb_dpow(-2.0, 0.5)));
  TEST_ASSERT_TRUE(isnan(zb_dpow((double)NAN, 2.0)));
  TEST_ASSERT_TRUE(isinf(zb_dlog(0.0)) && zb_dlog(-0.0) < 0.0);
  TEST_ASSERT_TRUE(isinf(zb_dlog((double)INFINITY)));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dlog(1.0));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dsin(0.0));
  TEST_ASSERT_TRUE(signbit(zb_dsin(-0.0)));
  TEST_ASSERT_EQUAL_DOUBLE(1.0, zb_dcos(0.0));
  TEST_ASSERT_EQUAL_DOUBLE(1e-30, zb_dtan(1e-30));
  TEST_ASSERT_EQUAL_DOUBLE(1e-30, zb_datan(1e-30));
  TEST_ASSERT_EQUAL_DOUBLE(1e-30, zb_dasin(1e-30));
  TEST_ASSERT_DOUBLE_WITHIN(1e-16, ZB_DPI / 2, zb_dacos(1e-30));
  TEST_ASSERT_EQUAL_DOUBLE(ZB_DPI / 2, zb_datan(1e300));
  TEST_ASSERT_EQUAL_DOUBLE(-ZB_DPI / 2, zb_datan((double)-INFINITY));
  TEST_ASSERT_EQUAL_DOUBLE(ZB_DPI / 2, zb_dasin(1.0));
  TEST_ASSERT_EQUAL_DOUBLE(-ZB_DPI / 2, zb_dasin(-1.0));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dacos(1.0));
  TEST_ASSERT_EQUAL_DOUBLE(ZB_DPI, zb_dacos(-1.0));
  TEST_ASSERT_DOUBLE_WITHIN(1e-15, asin(0.99), zb_dasin(0.99));
  TEST_ASSERT_DOUBLE_WITHIN(1e-15, acos(-0.7), zb_dacos(-0.7));
  /* exp */
  TEST_ASSERT_TRUE(isinf(zb_dexp(710.0)));
  TEST_ASSERT_TRUE(isinf(zb_dexp((double)INFINITY)));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dexp(-746.0));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dexp((double)-INFINITY));
  TEST_ASSERT_EQUAL_DOUBLE(1.0, zb_dexp(0.0));
  TEST_ASSERT_TRUE(zb_dexp(-740.0) > 0.0); /* subnormal result */
  TEST_ASSERT_TRUE(ulp_diff(zb_dexp(-740.0), exp(-740.0)) <= 1);
  /* atan2 quadrants and special cases (C11 Annex F). */
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_datan2(0.0, 1.0));
  TEST_ASSERT_TRUE(signbit(zb_datan2(-0.0, 2.0)));
  TEST_ASSERT_EQUAL_DOUBLE(ZB_DPI, zb_datan2(0.0, -1.0));
  TEST_ASSERT_EQUAL_DOUBLE(-ZB_DPI, zb_datan2(-0.0, -1.0));
  TEST_ASSERT_EQUAL_DOUBLE(ZB_DPI / 2, zb_datan2(1.0, 0.0));
  TEST_ASSERT_EQUAL_DOUBLE(-ZB_DPI / 2, zb_datan2(-1.0, 0.0));
  TEST_ASSERT_EQUAL_DOUBLE(ZB_DPI / 4, zb_datan2((double)INFINITY, (double)INFINITY));
  TEST_ASSERT_EQUAL_DOUBLE(-ZB_DPI / 4, zb_datan2((double)-INFINITY, (double)INFINITY));
  TEST_ASSERT_EQUAL_DOUBLE(3 * ZB_DPI / 4, zb_datan2((double)INFINITY, (double)-INFINITY));
  TEST_ASSERT_EQUAL_DOUBLE(-3 * ZB_DPI / 4, zb_datan2((double)-INFINITY, (double)-INFINITY));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_datan2(1.0, (double)INFINITY));
  TEST_ASSERT_TRUE(signbit(zb_datan2(-1.0, (double)INFINITY)));
  TEST_ASSERT_EQUAL_DOUBLE(ZB_DPI, zb_datan2(1.0, (double)-INFINITY));
  TEST_ASSERT_EQUAL_DOUBLE(-ZB_DPI, zb_datan2(-1.0, (double)-INFINITY));
  TEST_ASSERT_EQUAL_DOUBLE(ZB_DPI / 2, zb_datan2(1e300, 1e-300));
  TEST_ASSERT_EQUAL_DOUBLE(-ZB_DPI / 2, zb_datan2((double)-INFINITY, 1.0));
  TEST_ASSERT_EQUAL_DOUBLE(ZB_DPI, zb_datan2(1e-300, -1e300));
  TEST_ASSERT_EQUAL_DOUBLE(atan(0.5), zb_datan2(0.5, 1.0));
  /* pow (C11 F.10.4.4) */
  TEST_ASSERT_EQUAL_DOUBLE(1.0, zb_dpow((double)NAN, 0.0));
  TEST_ASSERT_EQUAL_DOUBLE(1.0, zb_dpow(1.0, (double)NAN));
  TEST_ASSERT_EQUAL_DOUBLE(-8.0, zb_dpow(-2.0, 3.0));
  TEST_ASSERT_EQUAL_DOUBLE(16.0, zb_dpow(-2.0, 4.0));
  TEST_ASSERT_TRUE(isinf(zb_dpow(-2.0, 1e17)) && zb_dpow(-2.0, 1e17) > 0.0);
  TEST_ASSERT_TRUE(zb_dpow(-2.0, 1e300) > 0.0);
  TEST_ASSERT_TRUE(isinf(zb_dpow(0.0, -1.0)) && zb_dpow(-0.0, -1.0) < 0.0);
  TEST_ASSERT_TRUE(zb_dpow(-0.0, -2.0) > 0.0);
  TEST_ASSERT_TRUE(signbit(zb_dpow(-0.0, 3.0)));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dpow(0.0, 2.5));
  TEST_ASSERT_EQUAL_DOUBLE(1.0, zb_dpow(-1.0, (double)INFINITY));
  TEST_ASSERT_TRUE(isinf(zb_dpow(2.0, (double)INFINITY)));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dpow(0.5, (double)INFINITY));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dpow(2.0, (double)-INFINITY));
  TEST_ASSERT_TRUE(isinf(zb_dpow(0.5, (double)-INFINITY)));
  TEST_ASSERT_TRUE(isinf(zb_dpow((double)INFINITY, 2.0)));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dpow((double)INFINITY, -2.0));
  TEST_ASSERT_TRUE(zb_dpow((double)-INFINITY, 3.0) < 0.0 && isinf(zb_dpow((double)-INFINITY, 3.0)));
  TEST_ASSERT_TRUE(signbit(zb_dpow((double)-INFINITY, -3.0)));
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1024.0, zb_dpow(2.0, 10.0));
  /* scalbn edge cases */
  TEST_ASSERT_TRUE(isinf(zb_dscalbn(1.0, 5000)));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dscalbn(1.0, -5000));
  TEST_ASSERT_EQUAL_DOUBLE(ldexp(1.0, -1060), zb_dscalbn(1.0, -1060));
  TEST_ASSERT_EQUAL_DOUBLE(ldexp(1.0, -1100), zb_dscalbn(ldexp(1.0, 1000), -2100));
  TEST_ASSERT_EQUAL_DOUBLE(ldexp(1.0, 1000), zb_dscalbn(ldexp(1.0, -1000), 2000));
  TEST_ASSERT_TRUE(isnan(zb_dlog(-1e-310)));
  TEST_ASSERT_TRUE(ulp_diff(zb_dlog(1e-310), log(1e-310)) <= 1);
}

/* Golden hash over every function: identical on every platform. Update only
 * when the functions change on purpose (and say so in the change). */
#define GOLDEN "cd1bff44cc392bb5"

/* spec: deterministic-math: Golden values */
static void test_golden(void) {
  static const fn1 fns[] = {zb_dsin, zb_dcos, zb_dtan, zb_datan, zb_dasin, zb_dacos, zb_dexp, zb_dlog};
  static const double ranges[][2] = {{-1e5, 1e5}, {-1e5, 1e5}, {-1e3, 1e3}, {-1e3, 1e3},
                                     {-1.0, 1.0}, {-1.0, 1.0}, {-700, 700}, {1e-300, 1e300}};
  zb_hasher h;
  zb_hasher_init(&h, 0x646d617468ull);
  for (size_t f = 0; f < sizeof fns / sizeof fns[0]; ++f) {
    uint64_t seed = 42 + f;
    for (int i = 0; i < SAMPLES; ++i) {
      double x = f == 7 ? log_uniform(&seed, -996, 996, false) : uniform(&seed, ranges[f][0], ranges[f][1]);
      zb_hasher_f64(&h, fns[f](x));
    }
  }
  uint64_t seed = 99;
  for (int i = 0; i < SAMPLES; ++i) {
    double y = uniform(&seed, -1e3, 1e3), x = uniform(&seed, -1e3, 1e3);
    zb_hasher_f64(&h, zb_datan2(y, x));
    /* Separate statements: argument evaluation order differs between compilers. */
    double base = uniform(&seed, 1e-3, 1e3);
    double power = uniform(&seed, -8.0, 8.0);
    zb_hasher_f64(&h, zb_dpow(base, power));
  }
  char hex[17];
  zb_hash_hex(zb_hasher_final(&h), hex);
  printf("dmath golden hash: %s\n", hex);
  TEST_ASSERT_EQUAL_STRING(GOLDEN, hex);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_accuracy);
  RUN_TEST(test_special_values);
  RUN_TEST(test_golden);
  return UNITY_END();
}

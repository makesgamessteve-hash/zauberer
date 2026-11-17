/* SPDX-License-Identifier: MIT */
/* Deterministic math. The polynomial kernels and reduction constants are the
 * fdlibm ones (Sun Microsystems, freely redistributable, as used by musl and
 * FreeBSD); the code is a fresh C11 rendition without type punning through
 * unions. */
#include "core/dmath.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static uint64_t bits(double x) {
  uint64_t u;
  memcpy(&u, &x, sizeof u);
  return u;
}

static double from_bits(uint64_t u) {
  double x;
  memcpy(&x, &u, sizeof x);
  return x;
}

static uint32_t high_word(double x) { return (uint32_t)(bits(x) >> 32); }

/* Clears the low 32 bits of the mantissa. */
static double trunc_low(double x) { return from_bits(bits(x) & 0xffffffff00000000ull); }

static double nan_value(void) { return from_bits(0x7ff8000000000000ull); }

double zb_dscalbn(double x, int n) {
  double y = x;
  if (n > 1023) {
    y *= from_bits(0x7fe0000000000000ull); /* 2^1023 */
    n -= 1023;
    if (n > 1023) {
      y *= from_bits(0x7fe0000000000000ull);
      n -= 1023;
      if (n > 1023) {
        n = 1023;
      }
    }
  } else if (n < -1022) {
    /* 2^-1022 * 2^53 keeps the final scale below -53 so the subnormal result
     * is rounded only once. */
    y *= from_bits(0x0360000000000000ull);
    n += 1022 - 53;
    if (n < -1022) {
      y *= from_bits(0x0360000000000000ull);
      n += 1022 - 53;
      if (n < -1022) {
        n = -1022;
      }
    }
  }
  return y * from_bits((uint64_t)(0x3ff + n) << 52);
}

/* ---- sin, cos, tan --------------------------------------------------------- */

static const double S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03, S3 = -1.98412698298579493134e-04,
                    S4 = 2.75573137070700676789e-06, S5 = -2.50507602534068634195e-08, S6 = 1.58969099521155010221e-10;

static const double C1 = 4.16666666666666019037e-02, C2 = -1.38888888888741095749e-03, C3 = 2.48015872894767294178e-05,
                    C4 = -2.75573143513906633035e-07, C5 = 2.08757232129817482790e-09, C6 = -1.13596475577881948265e-11;

/* sin(x + y) for |x| <= pi/4, y the tail of a reduced argument. */
static double k_sin(double x, double y, int iy) {
  double z = x * x;
  double w = z * z;
  double r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
  double v = z * x;
  if (iy == 0) {
    return x + v * (S1 + z * r);
  }
  return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

static double k_cos(double x, double y) {
  double z = x * x;
  double w = z * z;
  double r = z * (C1 + z * (C2 + z * C3)) + w * w * (C4 + z * (C5 + z * C6));
  double hz = 0.5 * z;
  w = 1.0 - hz;
  return w + (((1.0 - w) - hz) + (z * r - x * y));
}

static const double T[] = {
    3.33333333333334091986e-01, 1.33333333333201242699e-01, 5.39682539762260521377e-02, 2.18694882948595424599e-02,
    8.86323982359930005737e-03, 3.59207910759131235356e-03, 1.45620945432529025516e-03, 5.88041240820264096874e-04,
    2.46463134818469906812e-04, 7.81794442939557092300e-05, 7.14072491382608190305e-05, -1.85586374855275456654e-05,
    2.59073051863633712884e-05,
};
static const double PIO4 = 7.85398163397448278999e-01, PIO4_LO = 3.06161699786838301793e-17;

/* tan(x + y) for |x| <= pi/4, or -1/tan(x + y) when odd. */
static double k_tan(double x, double y, int odd) {
  uint32_t hx = high_word(x);
  int big = (hx & 0x7fffffff) >= 0x3FE59428; /* |x| >= 0.6744 */
  int sign = 0;
  if (big) {
    sign = (int)(hx >> 31);
    if (sign) {
      x = -x;
      y = -y;
    }
    x = (PIO4 - x) + (PIO4_LO - y);
    y = 0.0;
  }
  double z = x * x;
  double w = z * z;
  double r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))));
  double v = z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w * T[12])))));
  double s = z * x;
  r = y + z * (s * (r + v) + y) + s * T[0];
  w = x + r;
  if (big) {
    s = 1 - 2 * odd;
    v = s - 2.0 * (x + (r - w * w / (w + s)));
    return sign ? -v : v;
  }
  if (!odd) {
    return w;
  }
  /* -1/(x + r) computed accurately. */
  double w0 = trunc_low(w);
  v = r - (w0 - x);
  double a = -1.0 / w;
  double a0 = trunc_low(a);
  return a0 + a * (1.0 + a0 * w0 + a0 * v);
}

static const double TOINT = 6755399441055744.0; /* 1.5 * 2^52 */
static const double INVPIO2 = 6.36619772367581382433e-01;
static const double PIO2_1 = 1.57079632673412561417e+00, PIO2_1T = 6.07710050650619224932e-11;
static const double PIO2_2 = 6.07710050630396597660e-11, PIO2_2T = 2.02226624879595063154e-21;
static const double PIO2_3 = 2.02226624871116645580e-21, PIO2_3T = 8.47842766036889956997e-32;
static const double TWO_PI = 6.28318530717958647692;

/* Reduces x to y[0] + y[1] in [-pi/4, pi/4]; returns the quadrant. */
static int rem_pio2(double x, double *y) {
  if ((high_word(x) & 0x7fffffff) >= 0x413921fb) { /* |x| >= 2^20 * pi/2 */
    x = fmod(x, TWO_PI);
  }
  uint32_t ix = high_word(x) & 0x7fffffff;
  double fn = x * INVPIO2 + TOINT - TOINT;
  int n = (int)fn;
  double r = x - fn * PIO2_1;
  double w = fn * PIO2_1T;
  if (r - w < -PIO4) {
    n--;
    fn--;
    r = x - fn * PIO2_1;
    w = fn * PIO2_1T;
  } else if (r - w > PIO4) {
    n++;
    fn++;
    r = x - fn * PIO2_1;
    w = fn * PIO2_1T;
  }
  y[0] = r - w;
  int ex = (int)(ix >> 20);
  int ey = (int)((high_word(y[0]) >> 20) & 0x7ff);
  if (ex - ey > 16) { /* cancellation: second round, good to 118 bits */
    double t = r;
    w = fn * PIO2_2;
    r = t - w;
    w = fn * PIO2_2T - ((t - r) - w);
    y[0] = r - w;
    ey = (int)((high_word(y[0]) >> 20) & 0x7ff);
    if (ex - ey > 49) { /* third round, good to 151 bits */
      t = r;
      w = fn * PIO2_3;
      r = t - w;
      w = fn * PIO2_3T - ((t - r) - w);
      y[0] = r - w;
    }
  }
  y[1] = (r - y[0]) - w;
  return n;
}

double zb_dsin(double x) {
  uint32_t ix = high_word(x) & 0x7fffffff;
  if (ix <= 0x3fe921fb) {  /* |x| ~<= pi/4 */
    if (ix < 0x3e500000) { /* |x| < 2^-26 */
      return x;
    }
    return k_sin(x, 0.0, 0);
  }
  if (ix >= 0x7ff00000) {
    return x - x; /* NaN for inf and NaN */
  }
  double y[2];
  int n = rem_pio2(x, y);
  switch (n & 3) {
    case 0: return k_sin(y[0], y[1], 1);
    case 1: return k_cos(y[0], y[1]);
    case 2: return -k_sin(y[0], y[1], 1);
    default: return -k_cos(y[0], y[1]);
  }
}

double zb_dcos(double x) {
  uint32_t ix = high_word(x) & 0x7fffffff;
  if (ix <= 0x3fe921fb) {
    if (ix < 0x3e46a09e) { /* |x| < 2^-27 * sqrt(2) */
      return 1.0;
    }
    return k_cos(x, 0.0);
  }
  if (ix >= 0x7ff00000) {
    return x - x;
  }
  double y[2];
  int n = rem_pio2(x, y);
  switch (n & 3) {
    case 0: return k_cos(y[0], y[1]);
    case 1: return -k_sin(y[0], y[1], 1);
    case 2: return -k_cos(y[0], y[1]);
    default: return k_sin(y[0], y[1], 1);
  }
}

double zb_dtan(double x) {
  uint32_t ix = high_word(x) & 0x7fffffff;
  if (ix <= 0x3fe921fb) {
    if (ix < 0x3e400000) { /* |x| < 2^-27 */
      return x;
    }
    return k_tan(x, 0.0, 0);
  }
  if (ix >= 0x7ff00000) {
    return x - x;
  }
  double y[2];
  int n = rem_pio2(x, y);
  return k_tan(y[0], y[1], n & 1);
}

/* ---- atan, atan2, asin, acos ---------------------------------------------- */

static const double ATANHI[] = {4.63647609000806093515e-01, 7.85398163397448278999e-01, 9.82793723247329054082e-01,
                                1.57079632679489655800e+00};
static const double ATANLO[] = {2.26987774529616870924e-17, 3.06161699786838301793e-17, 1.39033110312309984516e-17,
                                6.12323399573676603587e-17};
static const double AT[] = {
    3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01, -1.11111104054623557880e-01,
    9.09088713343650656196e-02, -7.69187620504482999495e-02, 6.66107313738753120669e-02, -5.83357013379057348645e-02,
    4.97687799461593236017e-02, -3.65315727442169155270e-02, 1.62858201153657823623e-02,
};

double zb_datan(double x) {
  uint32_t hx = high_word(x);
  uint32_t sign = hx >> 31;
  uint32_t ix = hx & 0x7fffffff;
  int id;
  if (ix >= 0x44100000) { /* |x| >= 2^66 */
    if (isnan(x)) {
      return x;
    }
    return sign ? -ATANHI[3] : ATANHI[3];
  }
  if (ix < 0x3fdc0000) {   /* |x| < 0.4375 */
    if (ix < 0x3e400000) { /* |x| < 2^-27 */
      return x;
    }
    id = -1;
  } else {
    x = fabs(x);
    if (ix < 0x3ff30000) {   /* |x| < 1.1875 */
      if (ix < 0x3fe60000) { /* 7/16 <= |x| < 11/16 */
        id = 0;
        x = (2.0 * x - 1.0) / (2.0 + x);
      } else { /* 11/16 <= |x| < 19/16 */
        id = 1;
        x = (x - 1.0) / (x + 1.0);
      }
    } else if (ix < 0x40038000) { /* |x| < 2.4375 */
      id = 2;
      x = (x - 1.5) / (1.0 + 1.5 * x);
    } else { /* 2.4375 <= |x| < 2^66 */
      id = 3;
      x = -1.0 / x;
    }
  }
  double z = x * x;
  double w = z * z;
  double s1 = z * (AT[0] + w * (AT[2] + w * (AT[4] + w * (AT[6] + w * (AT[8] + w * AT[10])))));
  double s2 = w * (AT[1] + w * (AT[3] + w * (AT[5] + w * (AT[7] + w * AT[9]))));
  if (id < 0) {
    return x - x * (s1 + s2);
  }
  z = ATANHI[id] - (x * (s1 + s2) - ATANLO[id] - x);
  return sign ? -z : z;
}

static const double PI = 3.1415926535897931160e+00, PI_LO = 1.2246467991473531772e-16;
static const double PIO2_HI = 1.57079632679489655800e+00, PIO2_LO = 6.12323399573676603587e-17;

double zb_datan2(double y, double x) {
  if (isnan(x) || isnan(y)) {
    return x + y;
  }
  if (x == 1.0) {
    return zb_datan(y);
  }
  uint32_t hx = high_word(x), hy = high_word(y);
  uint32_t ix = hx & 0x7fffffff, iy = hy & 0x7fffffff;
  int m = (int)(((hy >> 31) & 1) | ((hx >> 30) & 2)); /* 2 * sign(x) + sign(y) */
  if (y == 0.0) {
    switch (m) {
      case 0:
      case 1: return y; /* atan(+-0, +anything) = +-0 */
      case 2: return PI;
      default: return -PI;
    }
  }
  if (x == 0.0) {
    return (m & 1) ? -PIO2_HI : PIO2_HI;
  }
  if (ix == 0x7ff00000) { /* x is +-inf */
    if (iy == 0x7ff00000) {
      switch (m) {
        case 0: return PIO4;
        case 1: return -PIO4;
        case 2: return 3.0 * PIO4;
        default: return -3.0 * PIO4;
      }
    }
    switch (m) {
      case 0: return 0.0;
      case 1: return -0.0;
      case 2: return PI;
      default: return -PI;
    }
  }
  if (ix + (64u << 20) < iy || iy == 0x7ff00000) { /* |y/x| > 2^64 */
    return (m & 1) ? -PIO2_HI : PIO2_HI;
  }
  double z;
  if ((m & 2) && iy + (64u << 20) < ix) { /* |y/x| < 2^-64 and x < 0 */
    z = 0.0;
  } else {
    z = zb_datan(fabs(y / x));
  }
  switch (m) {
    case 0: return z;
    case 1: return -z;
    case 2: return PI - (z - PI_LO);
    default: return (z - PI_LO) - PI;
  }
}

static const double PS0 = 1.66666666666666657415e-01, PS1 = -3.25565818622400915405e-01,
                    PS2 = 2.01212532134862925881e-01, PS3 = -4.00555345006794114027e-02,
                    PS4 = 7.91534994289814532176e-04, PS5 = 3.47933107596021167570e-05,
                    QS1 = -2.40339491173441421878e+00, QS2 = 2.02094576023350569471e+00,
                    QS3 = -6.88283971605453293030e-01, QS4 = 7.70381505559019352791e-02;

static double asin_r(double z) {
  double p = z * (PS0 + z * (PS1 + z * (PS2 + z * (PS3 + z * (PS4 + z * PS5)))));
  double q = 1.0 + z * (QS1 + z * (QS2 + z * (QS3 + z * QS4)));
  return p / q;
}

double zb_dasin(double x) {
  uint32_t hx = high_word(x);
  uint32_t ix = hx & 0x7fffffff;
  if (ix >= 0x3ff00000) { /* |x| >= 1 or NaN */
    if (fabs(x) == 1.0) {
      return x * PIO2_HI + x * PIO2_LO;
    }
    return isnan(x) ? x : nan_value();
  }
  if (ix < 0x3fe00000) {   /* |x| < 0.5 */
    if (ix < 0x3e500000) { /* |x| < 2^-26 */
      return x;
    }
    return x + x * asin_r(x * x);
  }
  double z = (1.0 - fabs(x)) * 0.5;
  double s = sqrt(z);
  double r = asin_r(z);
  double v;
  if (ix >= 0x3fef3333) { /* |x| > 0.975 */
    v = PIO2_HI - (2.0 * (s + s * r) - PIO2_LO);
  } else {
    double f = trunc_low(s);
    double c = (z - f * f) / (s + f);
    v = 0.5 * PIO2_HI - (2.0 * s * r - (PIO2_LO - 2.0 * c) - (0.5 * PIO2_HI - 2.0 * f));
  }
  return (hx >> 31) ? -v : v;
}

double zb_dacos(double x) {
  uint32_t hx = high_word(x);
  uint32_t ix = hx & 0x7fffffff;
  if (ix >= 0x3ff00000) {
    if (x == 1.0) {
      return 0.0;
    }
    if (x == -1.0) {
      return 2.0 * PIO2_HI + 2.0 * PIO2_LO;
    }
    return isnan(x) ? x : nan_value();
  }
  if (ix < 0x3fe00000) {    /* |x| < 0.5 */
    if (ix <= 0x3c600000) { /* |x| < 2^-57 */
      return PIO2_HI + PIO2_LO;
    }
    return PIO2_HI - (x - (PIO2_LO - x * asin_r(x * x)));
  }
  if (hx >> 31) { /* x < -0.5 */
    double z = (1.0 + x) * 0.5;
    double s = sqrt(z);
    double w = asin_r(z) * s - PIO2_LO;
    return 2.0 * (PIO2_HI - (s + w));
  }
  double z = (1.0 - x) * 0.5;
  double s = sqrt(z);
  double df = trunc_low(s);
  double c = (z - df * df) / (s + df);
  double w = asin_r(z) * s + c;
  return 2.0 * (df + w);
}

/* ---- exp, log, pow -------------------------------------------------------- */

static const double LN2HI = 6.93147180369123816490e-01, LN2LO = 1.90821492927058770002e-10,
                    INVLN2 = 1.44269504088896338700e+00;
static const double P1 = 1.66666666666666019037e-01, P2 = -2.77777777770155933842e-03, P3 = 6.61375632143793436117e-05,
                    P4 = -1.65339022054652515390e-06, P5 = 4.13813679705723846039e-08;

double zb_dexp(double x) {
  uint32_t hx = high_word(x);
  int sign = (int)(hx >> 31);
  hx &= 0x7fffffff;
  if (hx >= 0x4086232b) { /* |x| >= 708.39 */
    if (isnan(x)) {
      return x;
    }
    if (x > 709.782712893383973096) {
      return x * from_bits(0x7fe0000000000000ull); /* overflow to +inf */
    }
    if (x < -745.13321910194110842) {
      return 0.0;
    }
  }
  double hi, lo;
  int k;
  if (hx > 0x3fd62e42) {    /* |x| > 0.5 ln2 */
    if (hx >= 0x3ff0a2b2) { /* |x| >= 1.5 ln2 */
      k = (int)(INVLN2 * x + (sign ? -0.5 : 0.5));
    } else {
      k = 1 - sign - sign;
    }
    hi = x - k * LN2HI; /* exact */
    lo = k * LN2LO;
    x = hi - lo;
  } else if (hx > 0x3e300000) { /* |x| > 2^-28 */
    k = 0;
    hi = x;
    lo = 0.0;
  } else {
    return 1.0 + x;
  }
  double xx = x * x;
  double c = x - xx * (P1 + xx * (P2 + xx * (P3 + xx * (P4 + xx * P5))));
  double y = 1.0 + (x * c / (2.0 - c) - lo + hi);
  return k == 0 ? y : zb_dscalbn(y, k);
}

static const double LG1 = 6.666666666666735130e-01, LG2 = 3.999999999940941908e-01, LG3 = 2.857142874366239149e-01,
                    LG4 = 2.222219843214978396e-01, LG5 = 1.818357216161805012e-01, LG6 = 1.531383769920937332e-01,
                    LG7 = 1.479819860511658591e-01;

double zb_dlog(double x) {
  uint64_t u = bits(x);
  uint32_t hx = (uint32_t)(u >> 32);
  int k = 0;
  if (hx < 0x00100000 || (hx >> 31)) {
    if ((u << 1) == 0) {
      return -1.0 / (x * x); /* log(+-0) = -inf */
    }
    if (hx >> 31) {
      return isnan(x) ? x : nan_value(); /* log(negative) = NaN */
    }
    k -= 54; /* subnormal: scale up by 2^54 */
    x *= 18014398509481984.0;
    u = bits(x);
    hx = (uint32_t)(u >> 32);
  } else if (hx >= 0x7ff00000) {
    return x;
  } else if (hx == 0x3ff00000 && (u << 32) == 0) {
    return 0.0;
  }
  /* Reduce x into [sqrt(2)/2, sqrt(2)]. */
  hx += 0x3ff00000 - 0x3fe6a09e;
  k += (int)(hx >> 20) - 0x3ff;
  hx = (hx & 0x000fffff) + 0x3fe6a09e;
  x = from_bits((uint64_t)hx << 32 | (u & 0xffffffffull));
  double f = x - 1.0;
  double hfsq = 0.5 * f * f;
  double s = f / (2.0 + f);
  double z = s * s;
  double w = z * z;
  double t1 = w * (LG2 + w * (LG4 + w * LG6));
  double t2 = z * (LG1 + w * (LG3 + w * (LG5 + w * LG7)));
  double r = t2 + t1;
  double dk = (double)k;
  return s * (hfsq + r) + dk * LN2LO - hfsq + f + dk * LN2HI;
}

/* 0 = not an integer, 1 = odd integer, 2 = even integer. */
static int integer_kind(double y) {
  if (floor(y) != y) {
    return 0;
  }
  if (fabs(y) >= 9007199254740992.0) { /* 2^53: every such double is even */
    return 2;
  }
  return fmod(y, 2.0) != 0.0 ? 1 : 2;
}

double zb_dpow(double x, double y) {
  if (y == 0.0 || x == 1.0) {
    return 1.0;
  }
  if (isnan(x) || isnan(y)) {
    return x + y;
  }
  int yk = integer_kind(y);
  if (isinf(y)) {
    double ax = fabs(x);
    if (ax == 1.0) {
      return 1.0;
    }
    return (ax > 1.0) == (y > 0.0) ? (double)INFINITY : 0.0;
  }
  if (x == 0.0) {
    int negative = signbit(x) && yk == 1;
    double r = y < 0.0 ? (double)INFINITY : 0.0;
    return negative ? -r : r;
  }
  if (isinf(x)) {
    int negative = x < 0.0 && yk == 1;
    double r = y < 0.0 ? 0.0 : (double)INFINITY;
    return negative ? -r : r;
  }
  double sign = 1.0;
  if (x < 0.0) {
    if (yk == 0) {
      return nan_value();
    }
    if (yk == 1) {
      sign = -1.0;
    }
    x = -x;
  }
  return sign * zb_dexp(y * zb_dlog(x));
}

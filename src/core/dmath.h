/* SPDX-License-Identifier: MIT */
/* Deterministic transcendental functions.
 *
 * Bit-identical on every platform and compiler: only IEEE-754 operations that
 * are exact or correctly rounded (+ - * /, sqrt, floor, fmod) are used, with a
 * fixed evaluation order (the build disables floating-point contraction and
 * fast-math). Kernels follow the fdlibm minimax polynomials. Accuracy: < 2 ulp
 * for sin, cos, tan, atan, asin, acos, exp and log; pow is exp(y * log(x)) with
 * a relative error below 1e-13 while |y * log(x)| <= 100. Arguments of sin, cos
 * and tan are reduced exactly up to |x| = 1.6e6; larger arguments are reduced
 * with fmod by a rounded 2*pi (deterministic, but less accurate). */
#ifndef ZB_CORE_DMATH_H
#define ZB_CORE_DMATH_H

#define ZB_DPI 3.14159265358979323846

double zb_dsin(double x);
double zb_dcos(double x);
double zb_dtan(double x);
double zb_datan(double x);
double zb_datan2(double y, double x);
double zb_dasin(double x);
double zb_dacos(double x);
double zb_dexp(double x);
double zb_dlog(double x);
double zb_dpow(double x, double y);
/* x * 2^n, exact unless the result is subnormal (then correctly rounded). */
double zb_dscalbn(double x, int n);

#endif

/* SPDX-License-Identifier: MIT */
/* Deterministic 3D gradient noise (improved Perlin noise with hashed lattice
 * gradients) and fractal sums. Only + - * and floor are used, so results are
 * bit-identical on every platform. Inputs are planet positions divided by a
 * wavelength, which keeps the noise seamless across cube faces. */
#ifndef ZB_WORLD_NOISE_H
#define ZB_WORLD_NOISE_H

#include "core/base.h"

/* Gradient noise in about [-1, 1]; 0 at every lattice point. */
double zb_noise3(uint64_t seed, double x, double y, double z);
/* Fractal sum: octaves with frequency x2 and amplitude x gain each; the
 * result is normalised to about [-1, 1]. */
double zb_fbm3(uint64_t seed, double x, double y, double z, int octaves, double gain);
/* Ridged multifractal in [0, 1]: sharp crests where the noise crosses zero,
 * with higher octaves weighted by the lower ones (rough peaks, smooth valleys). */
double zb_ridged3(uint64_t seed, double x, double y, double z, int octaves);

#endif

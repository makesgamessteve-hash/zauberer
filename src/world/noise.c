/* SPDX-License-Identifier: MIT */
#include "world/noise.h"

#include <math.h>

/* Lattice hash: the top four bits of a multiplicative hash of the combined
 * corner coordinates select one of 16 gradients. */
static unsigned corner(uint64_t key) { return (unsigned)((key * 0x9e3779b97f4a7c15ull) >> 60); }

/* Perlin's 12 cube-edge gradients, four of them twice (16 entries). */
static const double GX[16] = {1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, -1, 0, 0};
static const double GY[16] = {1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, 1, 1, -1};
static const double GZ[16] = {0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 0, 1, 1};

static double grad(unsigned h, double x, double y, double z) { return GX[h] * x + GY[h] * y + GZ[h] * z; }

static double fade(double t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
static double lerp(double a, double b, double t) { return a + t * (b - a); }

double zb_noise3(uint64_t seed, double x, double y, double z) {
  double fx = floor(x), fy = floor(y), fz = floor(z);
  int64_t ix = (int64_t)fx, iy = (int64_t)fy, iz = (int64_t)fz;
  x -= fx;
  y -= fy;
  z -= fz;
  double u = fade(x), v = fade(y), w = fade(z);
  double x1 = x - 1.0, y1 = y - 1.0, z1 = z - 1.0;
  uint64_t hx0 = seed ^ ((uint64_t)ix * 0x8da6b343ull);
  uint64_t hx1 = seed ^ ((uint64_t)(ix + 1) * 0x8da6b343ull);
  uint64_t hy0 = (uint64_t)iy * 0xd8163841ull, hy1 = (uint64_t)(iy + 1) * 0xd8163841ull;
  uint64_t hz0 = (uint64_t)iz * 0xcb1ab31full, hz1 = (uint64_t)(iz + 1) * 0xcb1ab31full;
  double n000 = grad(corner(hx0 ^ hy0 ^ hz0), x, y, z);
  double n100 = grad(corner(hx1 ^ hy0 ^ hz0), x1, y, z);
  double n010 = grad(corner(hx0 ^ hy1 ^ hz0), x, y1, z);
  double n110 = grad(corner(hx1 ^ hy1 ^ hz0), x1, y1, z);
  double n001 = grad(corner(hx0 ^ hy0 ^ hz1), x, y, z1);
  double n101 = grad(corner(hx1 ^ hy0 ^ hz1), x1, y, z1);
  double n011 = grad(corner(hx0 ^ hy1 ^ hz1), x, y1, z1);
  double n111 = grad(corner(hx1 ^ hy1 ^ hz1), x1, y1, z1);
  double a = lerp(lerp(n000, n100, u), lerp(n010, n110, u), v);
  double b = lerp(lerp(n001, n101, u), lerp(n011, n111, u), v);
  return lerp(a, b, w);
}

/* Octaves are offset so their lattices never line up. */
#define OCTAVE_SHIFT 17.31

double zb_fbm3(uint64_t seed, double x, double y, double z, int octaves, double gain) {
  double sum = 0.0, amp = 1.0, norm = 0.0;
  for (int i = 0; i < octaves; ++i) {
    sum += amp * zb_noise3(seed + (uint64_t)(uint32_t)i * 0x632be59bd9b4e019ull, x, y, z);
    norm += amp;
    amp *= gain;
    x = x * 2.0 + OCTAVE_SHIFT;
    y = y * 2.0 + OCTAVE_SHIFT;
    z = z * 2.0 + OCTAVE_SHIFT;
  }
  return norm > 0.0 ? sum / norm : 0.0;
}

double zb_ridged3(uint64_t seed, double x, double y, double z, int octaves) {
  double sum = 0.0, amp = 1.0, norm = 0.0, weight = 1.0;
  for (int i = 0; i < octaves; ++i) {
    double n = 1.0 - fabs(zb_noise3(seed + (uint64_t)(uint32_t)i * 0x632be59bd9b4e019ull, x, y, z));
    n *= n;
    n *= weight;
    weight = n * 2.0;
    weight = weight > 1.0 ? 1.0 : weight;
    sum += amp * n;
    norm += amp;
    amp *= 0.5;
    x = x * 2.0 + OCTAVE_SHIFT;
    y = y * 2.0 + OCTAVE_SHIFT;
    z = z * 2.0 + OCTAVE_SHIFT;
  }
  return norm > 0.0 ? sum / norm : 0.0;
}

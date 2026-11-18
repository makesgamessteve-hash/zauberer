/* SPDX-License-Identifier: MIT */
#include "world/ley.h"

#include "core/dmath.h"
#include "core/hash.h"
#include "world/dvec.h"

#include <math.h>
#include <string.h>

#define CANDIDATES 6 /* places tried per nexus; the highest wins */
#define LINKS 3      /* lines from each nexus to its nearest */

static double unit(uint64_t h) { return (double)(h >> 11) / 9007199254740992.0; }

/* A uniformly spread point on the sphere from a hash. */
static zb_dvec3 sphere_point(uint64_t h) {
  uint64_t h2 = zb_hash64(&h, sizeof h, 0x6c6579);
  double z = 2.0 * unit(h) - 1.0, r = sqrt(1.0 - z * z), phi = 2.0 * ZB_DPI * unit(h2);
  return zb_dv(r * zb_dcos(phi), r * zb_dsin(phi), z);
}

static bool has_line(const zb_ley *l, int a, int b) {
  for (int k = 0; k < l->lines; ++k) {
    if (l->line[k].a == a && l->line[k].b == b) {
      return true;
    }
  }
  return false;
}

void zb_ley_build(zb_ley *l, const zb_terrain *t, double radius, uint64_t seed) {
  memset(l, 0, sizeof *l);
  l->radius = radius;
  l->strength = 1.0;
  for (int k = 0; k < ZB_LEY_NEXUSES; ++k) {
    double best = -1e300;
    for (int c = 0; c < CANDIDATES; ++c) {
      uint64_t key[3] = {seed, (uint64_t)k, (uint64_t)c};
      zb_dvec3 d = sphere_point(zb_hash64(key, sizeof key, 0x6e657875));
      double h = zb_terrain_macro_height(t, d);
      if (h > best) {
        best = h;
        l->nexus[k].dir = d;
      }
    }
    uint64_t key[2] = {seed, (uint64_t)k};
    uint64_t h = zb_hash64(key, sizeof key, 0x636f6c);
    int c0 = (int)(h % 8), c1 = (c0 + 1 + (int)((h >> 8) % 7)) % 8;
    l->nexus[k].colors[0] = (uint8_t)c0;
    l->nexus[k].colors[1] = (uint8_t)c1;
    l->nexus[k].power = (float)(0.5 + 0.5 * unit(zb_hash64(&h, sizeof h, 0x706f77)));
  }
  /* Each nexus to its nearest few: a connected-looking web, not a mesh. */
  for (int a = 0; a < ZB_LEY_NEXUSES; ++a) {
    int near[LINKS];
    double dot[LINKS];
    for (int k = 0; k < LINKS; ++k) {
      near[k] = -1;
      dot[k] = -2.0;
    }
    for (int b = 0; b < ZB_LEY_NEXUSES; ++b) {
      if (b == a) {
        continue;
      }
      double d = zb_dv_dot(l->nexus[a].dir, l->nexus[b].dir);
      for (int k = 0; k < LINKS; ++k) {
        if (d > dot[k]) {
          for (int m = LINKS - 1; m > k; --m) {
            near[m] = near[m - 1];
            dot[m] = dot[m - 1];
          }
          near[k] = b;
          dot[k] = d;
          break;
        }
      }
    }
    for (int k = 0; k < LINKS; ++k) {
      int lo = a < near[k] ? a : near[k], hi = a < near[k] ? near[k] : a;
      if (!has_line(l, lo, hi) && l->lines < ZB_LEY_MAX_LINES) {
        l->line[l->lines++] = (zb_ley_line){(uint8_t)lo, (uint8_t)hi};
      }
    }
  }
}

/* The angle from p to the great-circle arc a..b (all unit vectors). */
static double arc_angle(zb_dvec3 p, zb_dvec3 a, zb_dvec3 b) {
  zb_dvec3 n = zb_dv_norm(zb_dv_cross(a, b));
  double s = zb_dv_dot(p, n);
  zb_dvec3 q = zb_dv_sub(p, zb_dv_scale(n, s));
  if (zb_dv_dot(q, q) > 1e-24) {
    q = zb_dv_norm(q);
    if (zb_dv_dot(zb_dv_cross(a, q), n) >= 0.0 && zb_dv_dot(zb_dv_cross(q, b), n) >= 0.0) {
      return zb_dasin(fmin(fabs(s), 1.0));
    }
  }
  double da = zb_dv_angle(p, a), db = zb_dv_angle(p, b);
  return da < db ? da : db;
}

void zb_ley_sample_at(const zb_ley *l, zb_dvec3 dir, zb_ley_sample *out) {
  out->line_distance = out->nexus_distance = INFINITY;
  out->line = out->nexus = -1;
  double best = -2.0;
  for (int k = 0; k < ZB_LEY_NEXUSES; ++k) {
    double d = zb_dv_dot(dir, l->nexus[k].dir);
    if (d > best) {
      best = d;
      out->nexus = k;
    }
  }
  out->nexus_distance = zb_dv_angle(dir, l->nexus[out->nexus].dir) * l->radius;
  for (int k = 0; k < l->lines; ++k) {
    const zb_ley_line *ln = &l->line[k];
    double d = arc_angle(dir, l->nexus[ln->a].dir, l->nexus[ln->b].dir) * l->radius;
    if (d < out->line_distance) {
      out->line_distance = d;
      out->line = k;
    }
  }
}

void zb_ley_line_colors(const zb_ley *l, int line, int *a, int *b) {
  const zb_ley_line *ln = &l->line[line];
  *a = l->nexus[ln->a].colors[0];
  *b = l->nexus[ln->b].colors[0];
}

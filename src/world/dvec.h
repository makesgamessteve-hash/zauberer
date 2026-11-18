/* SPDX-License-Identifier: MIT */
/* Double-precision vector helpers for world code (header-only). */
#ifndef ZB_WORLD_DVEC_H
#define ZB_WORLD_DVEC_H

#include <math.h>

#include "core/dmath.h"
#include "world/planet.h"

static inline zb_dvec3 zb_dv(double x, double y, double z) {
  zb_dvec3 r = {x, y, z};
  return r;
}
static inline zb_dvec3 zb_dv_add(zb_dvec3 a, zb_dvec3 b) { return zb_dv(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline zb_dvec3 zb_dv_sub(zb_dvec3 a, zb_dvec3 b) { return zb_dv(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline zb_dvec3 zb_dv_scale(zb_dvec3 a, double s) { return zb_dv(a.x * s, a.y * s, a.z * s); }
static inline double zb_dv_dot(zb_dvec3 a, zb_dvec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline zb_dvec3 zb_dv_cross(zb_dvec3 a, zb_dvec3 b) {
  return zb_dv(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline double zb_dv_len(zb_dvec3 a) { return sqrt(zb_dv_dot(a, a)); }
static inline zb_dvec3 zb_dv_norm(zb_dvec3 a) {
  double l = zb_dv_len(a);
  return l > 0.0 ? zb_dv_scale(a, 1.0 / l) : zb_dv(0.0, 0.0, 1.0);
}
/* Angle between two unit vectors (radians), accurate for small angles. */
static inline double zb_dv_angle(zb_dvec3 a, zb_dvec3 b) {
  return zb_datan2(zb_dv_len(zb_dv_cross(a, b)), zb_dv_dot(a, b));
}
/* Rotates v about the unit axis by angle (Rodrigues). */
static inline zb_dvec3 zb_dv_rotate(zb_dvec3 v, zb_dvec3 axis, double angle) {
  double c = zb_dcos(angle), s = zb_dsin(angle);
  zb_dvec3 k = zb_dv_cross(axis, v);
  double d = zb_dv_dot(axis, v) * (1.0 - c);
  return zb_dv(v.x * c + k.x * s + axis.x * d, v.y * c + k.y * s + axis.y * d, v.z * c + k.z * s + axis.z * d);
}

/* A rotation as a 3x3 matrix (rows), for rotating many vectors by one angle. */
typedef struct zb_drot {
  double m[9];
} zb_drot;

static inline zb_drot zb_drot_make(zb_dvec3 axis, double angle) {
  double c = zb_dcos(angle), s = zb_dsin(angle), t = 1.0 - c;
  double x = axis.x, y = axis.y, z = axis.z;
  zb_drot r = {{t * x * x + c, t * x * y - s * z, t * x * z + s * y, t * x * y + s * z, t * y * y + c,
                t * y * z - s * x, t * x * z - s * y, t * y * z + s * x, t * z * z + c}};
  return r;
}
static inline zb_dvec3 zb_drot_apply(const zb_drot *r, zb_dvec3 v) {
  return zb_dv(r->m[0] * v.x + r->m[1] * v.y + r->m[2] * v.z, r->m[3] * v.x + r->m[4] * v.y + r->m[5] * v.z,
               r->m[6] * v.x + r->m[7] * v.y + r->m[8] * v.z);
}

#endif

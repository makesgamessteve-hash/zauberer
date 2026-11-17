/* SPDX-License-Identifier: MIT */
#include "core/math.h"

#include <math.h>

zb_vec3 zb_v3(float x, float y, float z) {
  zb_vec3 v = {x, y, z};
  return v;
}

zb_vec3 zb_v3_add(zb_vec3 a, zb_vec3 b) { return zb_v3(a.x + b.x, a.y + b.y, a.z + b.z); }
zb_vec3 zb_v3_sub(zb_vec3 a, zb_vec3 b) { return zb_v3(a.x - b.x, a.y - b.y, a.z - b.z); }
zb_vec3 zb_v3_scale(zb_vec3 a, float s) { return zb_v3(a.x * s, a.y * s, a.z * s); }
float zb_v3_dot(zb_vec3 a, zb_vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

zb_vec3 zb_v3_cross(zb_vec3 a, zb_vec3 b) {
  return zb_v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

float zb_v3_length(zb_vec3 a) { return sqrtf(zb_v3_dot(a, a)); }

zb_vec3 zb_v3_normalize(zb_vec3 a) {
  float len = zb_v3_length(a);
  if (len <= 0.0f) {
    return zb_v3(0.0f, 0.0f, 0.0f);
  }
  return zb_v3_scale(a, 1.0f / len);
}

zb_vec3 zb_v3_lerp(zb_vec3 a, zb_vec3 b, float t) {
  return zb_v3(zb_lerpf(a.x, b.x, t), zb_lerpf(a.y, b.y, t), zb_lerpf(a.z, b.z, t));
}

float zb_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float zb_lerpf(float a, float b, float t) { return a + (b - a) * t; }

zb_mat4 zb_m4_identity(void) {
  zb_mat4 r = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  return r;
}

zb_mat4 zb_m4_mul(zb_mat4 a, zb_mat4 b) {
  zb_mat4 r;
  for (int c = 0; c < 4; ++c) {
    for (int row = 0; row < 4; ++row) {
      float s = 0.0f;
      for (int k = 0; k < 4; ++k) {
        s += a.m[k * 4 + row] * b.m[c * 4 + k];
      }
      r.m[c * 4 + row] = s;
    }
  }
  return r;
}

zb_vec4 zb_m4_mul_v4(zb_mat4 m, zb_vec4 v) {
  zb_vec4 r;
  r.x = m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w;
  r.y = m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w;
  r.z = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w;
  r.w = m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w;
  return r;
}

zb_vec3 zb_m4_transform_point(zb_mat4 m, zb_vec3 p) {
  zb_vec4 v = {p.x, p.y, p.z, 1.0f};
  zb_vec4 r = zb_m4_mul_v4(m, v);
  return zb_v3(r.x, r.y, r.z);
}

zb_mat4 zb_m4_translate(zb_vec3 t) {
  zb_mat4 r = zb_m4_identity();
  r.m[12] = t.x;
  r.m[13] = t.y;
  r.m[14] = t.z;
  return r;
}

zb_mat4 zb_m4_scale(zb_vec3 s) {
  zb_mat4 r = zb_m4_identity();
  r.m[0] = s.x;
  r.m[5] = s.y;
  r.m[10] = s.z;
  return r;
}

zb_mat4 zb_m4_from_quat(zb_quat q) {
  float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
  float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
  float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
  zb_mat4 r = zb_m4_identity();
  r.m[0] = 1.0f - 2.0f * (yy + zz);
  r.m[1] = 2.0f * (xy + wz);
  r.m[2] = 2.0f * (xz - wy);
  r.m[4] = 2.0f * (xy - wz);
  r.m[5] = 1.0f - 2.0f * (xx + zz);
  r.m[6] = 2.0f * (yz + wx);
  r.m[8] = 2.0f * (xz + wy);
  r.m[9] = 2.0f * (yz - wx);
  r.m[10] = 1.0f - 2.0f * (xx + yy);
  return r;
}

zb_mat4 zb_m4_trs(zb_vec3 t, zb_quat r, zb_vec3 s) {
  return zb_m4_mul(zb_m4_translate(t), zb_m4_mul(zb_m4_from_quat(r), zb_m4_scale(s)));
}

zb_mat4 zb_m4_perspective(float fovy, float aspect, float znear, float zfar) {
  float f = 1.0f / tanf(fovy * 0.5f);
  zb_mat4 r = {{0}};
  r.m[0] = f / aspect;
  r.m[5] = f;
  r.m[10] = (zfar + znear) / (znear - zfar);
  r.m[11] = -1.0f;
  r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
  return r;
}

zb_mat4 zb_m4_ortho(float left, float right, float bottom, float top, float znear, float zfar) {
  zb_mat4 r = zb_m4_identity();
  r.m[0] = 2.0f / (right - left);
  r.m[5] = 2.0f / (top - bottom);
  r.m[10] = -2.0f / (zfar - znear);
  r.m[12] = -(right + left) / (right - left);
  r.m[13] = -(top + bottom) / (top - bottom);
  r.m[14] = -(zfar + znear) / (zfar - znear);
  return r;
}

zb_mat4 zb_m4_look_at(zb_vec3 eye, zb_vec3 target, zb_vec3 up) {
  zb_vec3 f = zb_v3_normalize(zb_v3_sub(target, eye));
  zb_vec3 s = zb_v3_normalize(zb_v3_cross(f, up));
  zb_vec3 u = zb_v3_cross(s, f);
  zb_mat4 r = zb_m4_identity();
  r.m[0] = s.x;
  r.m[4] = s.y;
  r.m[8] = s.z;
  r.m[1] = u.x;
  r.m[5] = u.y;
  r.m[9] = u.z;
  r.m[2] = -f.x;
  r.m[6] = -f.y;
  r.m[10] = -f.z;
  r.m[12] = -zb_v3_dot(s, eye);
  r.m[13] = -zb_v3_dot(u, eye);
  r.m[14] = zb_v3_dot(f, eye);
  return r;
}

zb_mat4 zb_m4_normal_matrix(zb_mat4 m) {
  float a = m.m[0], b = m.m[4], c = m.m[8];
  float d = m.m[1], e = m.m[5], f = m.m[9];
  float g = m.m[2], h = m.m[6], i = m.m[10];
  float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  zb_mat4 r = zb_m4_identity();
  if (det == 0.0f) {
    return r;
  }
  float inv = 1.0f / det;
  /* inverse-transpose == cofactor matrix / det */
  r.m[0] = (e * i - f * h) * inv;
  r.m[1] = -(b * i - c * h) * inv;
  r.m[2] = (b * f - c * e) * inv;
  r.m[4] = -(d * i - f * g) * inv;
  r.m[5] = (a * i - c * g) * inv;
  r.m[6] = -(a * f - c * d) * inv;
  r.m[8] = (d * h - e * g) * inv;
  r.m[9] = -(a * h - b * g) * inv;
  r.m[10] = (a * e - b * d) * inv;
  return r;
}

zb_quat zb_quat_identity(void) {
  zb_quat q = {0.0f, 0.0f, 0.0f, 1.0f};
  return q;
}

zb_quat zb_quat_axis_angle(zb_vec3 axis, float radians) {
  zb_vec3 n = zb_v3_normalize(axis);
  float s = sinf(radians * 0.5f);
  zb_quat q = {n.x * s, n.y * s, n.z * s, cosf(radians * 0.5f)};
  return q;
}

zb_quat zb_quat_mul(zb_quat a, zb_quat b) {
  zb_quat q;
  q.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
  q.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
  q.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
  q.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
  return q;
}

zb_quat zb_quat_normalize(zb_quat q) {
  float len = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (len <= 0.0f) {
    return zb_quat_identity();
  }
  zb_quat r = {q.x / len, q.y / len, q.z / len, q.w / len};
  return r;
}

zb_vec3 zb_quat_rotate(zb_quat q, zb_vec3 v) {
  zb_vec3 u = zb_v3(q.x, q.y, q.z);
  zb_vec3 t = zb_v3_scale(zb_v3_cross(u, v), 2.0f);
  return zb_v3_add(zb_v3_add(v, zb_v3_scale(t, q.w)), zb_v3_cross(u, t));
}

static zb_plane make_plane(float a, float b, float c, float d) {
  float len = sqrtf(a * a + b * b + c * c);
  zb_plane p = {{a / len, b / len, c / len}, d / len};
  return p;
}

zb_frustum zb_frustum_from_matrix(zb_mat4 vp) {
  const float *m = vp.m;
  zb_frustum f;
  /* rows of the matrix */
  float r0[4] = {m[0], m[4], m[8], m[12]};
  float r1[4] = {m[1], m[5], m[9], m[13]};
  float r2[4] = {m[2], m[6], m[10], m[14]};
  float r3[4] = {m[3], m[7], m[11], m[15]};
  f.planes[0] = make_plane(r3[0] + r0[0], r3[1] + r0[1], r3[2] + r0[2], r3[3] + r0[3]); /* left */
  f.planes[1] = make_plane(r3[0] - r0[0], r3[1] - r0[1], r3[2] - r0[2], r3[3] - r0[3]); /* right */
  f.planes[2] = make_plane(r3[0] + r1[0], r3[1] + r1[1], r3[2] + r1[2], r3[3] + r1[3]); /* bottom */
  f.planes[3] = make_plane(r3[0] - r1[0], r3[1] - r1[1], r3[2] - r1[2], r3[3] - r1[3]); /* top */
  f.planes[4] = make_plane(r3[0] + r2[0], r3[1] + r2[1], r3[2] + r2[2], r3[3] + r2[3]); /* near */
  f.planes[5] = make_plane(r3[0] - r2[0], r3[1] - r2[1], r3[2] - r2[2], r3[3] - r2[3]); /* far */
  return f;
}

bool zb_frustum_sphere_visible(const zb_frustum *f, zb_vec3 center, float radius) {
  for (int i = 0; i < 6; ++i) {
    if (zb_v3_dot(f->planes[i].n, center) + f->planes[i].d < -radius) {
      return false;
    }
  }
  return true;
}

/* SPDX-License-Identifier: MIT */
/* Small vector, matrix and quaternion library (column-major matrices, right-handed). */
#ifndef ZB_CORE_MATH_H
#define ZB_CORE_MATH_H

#include "core/base.h"

typedef struct zb_vec2 {
  float x, y;
} zb_vec2;
typedef struct zb_vec3 {
  float x, y, z;
} zb_vec3;
typedef struct zb_vec4 {
  float x, y, z, w;
} zb_vec4;
typedef struct zb_quat {
  float x, y, z, w;
} zb_quat;
/* m[column * 4 + row] */
typedef struct zb_mat4 {
  float m[16];
} zb_mat4;
/* Plane: dot(normal, p) + d >= 0 means inside. */
typedef struct zb_plane {
  zb_vec3 n;
  float d;
} zb_plane;
typedef struct zb_frustum {
  zb_plane planes[6];
} zb_frustum;

#define ZB_PI 3.14159265358979323846f

zb_vec3 zb_v3(float x, float y, float z);
zb_vec3 zb_v3_add(zb_vec3 a, zb_vec3 b);
zb_vec3 zb_v3_sub(zb_vec3 a, zb_vec3 b);
zb_vec3 zb_v3_scale(zb_vec3 a, float s);
float zb_v3_dot(zb_vec3 a, zb_vec3 b);
zb_vec3 zb_v3_cross(zb_vec3 a, zb_vec3 b);
float zb_v3_length(zb_vec3 a);
/* Returns the zero vector for zero-length input. */
zb_vec3 zb_v3_normalize(zb_vec3 a);
zb_vec3 zb_v3_lerp(zb_vec3 a, zb_vec3 b, float t);

float zb_clampf(float v, float lo, float hi);
float zb_lerpf(float a, float b, float t);

zb_mat4 zb_m4_identity(void);
zb_mat4 zb_m4_mul(zb_mat4 a, zb_mat4 b);
zb_vec4 zb_m4_mul_v4(zb_mat4 m, zb_vec4 v);
zb_vec3 zb_m4_transform_point(zb_mat4 m, zb_vec3 p);
zb_mat4 zb_m4_translate(zb_vec3 t);
zb_mat4 zb_m4_scale(zb_vec3 s);
zb_mat4 zb_m4_from_quat(zb_quat q);
/* Translation * rotation * scale. */
zb_mat4 zb_m4_trs(zb_vec3 t, zb_quat r, zb_vec3 s);
/* OpenGL clip space (-1..1 depth). fovy in radians. */
zb_mat4 zb_m4_perspective(float fovy, float aspect, float znear, float zfar);
zb_mat4 zb_m4_ortho(float left, float right, float bottom, float top, float znear, float zfar);
zb_mat4 zb_m4_look_at(zb_vec3 eye, zb_vec3 target, zb_vec3 up);
/* Upper-left 3x3 inverse-transpose packed into a mat4 (for normals). */
zb_mat4 zb_m4_normal_matrix(zb_mat4 m);

zb_quat zb_quat_identity(void);
zb_quat zb_quat_axis_angle(zb_vec3 axis, float radians);
zb_quat zb_quat_mul(zb_quat a, zb_quat b);
zb_quat zb_quat_normalize(zb_quat q);
zb_vec3 zb_quat_rotate(zb_quat q, zb_vec3 v);

/* Extracts normalized frustum planes from a view-projection matrix. */
zb_frustum zb_frustum_from_matrix(zb_mat4 view_proj);
bool zb_frustum_sphere_visible(const zb_frustum *f, zb_vec3 center, float radius);

#endif

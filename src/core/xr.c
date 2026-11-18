/* SPDX-License-Identifier: MIT */
#include "core/xr.h"

#include <math.h>
#include <string.h>

#define MAX_FRAMEBUFFER 65536.0f

static bool all_finite(const float *d, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (!isfinite(d[i])) {
      return false;
    }
  }
  return true;
}

/* A whole number in [lo, hi]. */
static bool whole(float v, float lo, float hi, int *out) {
  if (v < lo || v > hi || v != floorf(v)) {
    return false;
  }
  *out = (int)v;
  return true;
}

static void copy_matrix(zb_mat4 *m, const float *src) { memcpy(m->m, src, sizeof m->m); }

bool zb_xr_frame_decode(const float *data, size_t n, uint32_t framebuffer, zb_xr_frame *out) {
  if (!data || n < ZB_XR_HEADER_FLOATS || (data[0] != 1.0f && data[0] != 2.0f)) {
    return false;
  }
  int count = (int)data[0];
  if (n != (size_t)(ZB_XR_HEADER_FLOATS + count * ZB_XR_VIEW_FLOATS) || !all_finite(data, n)) {
    return false;
  }
  zb_xr_frame f;
  memset(&f, 0, sizeof f);
  f.framebuffer = framebuffer;
  f.view_count = count;
  if (!whole(data[1], 1.0f, MAX_FRAMEBUFFER, &f.width) || !whole(data[2], 1.0f, MAX_FRAMEBUFFER, &f.height)) {
    return false;
  }
  f.head.position = zb_v3(data[3], data[4], data[5]);
  zb_quat q = {data[6], data[7], data[8], data[9]};
  if (q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w < 1e-12f) {
    return false;
  }
  f.head.orientation = zb_quat_normalize(q);
  for (int i = 0; i < count; ++i) {
    const float *v = data + ZB_XR_HEADER_FLOATS + i * ZB_XR_VIEW_FLOATS;
    zb_xr_view *view = &f.views[i];
    if (!whole(v[0], 0.0f, (float)f.width - 1.0f, &view->x) || !whole(v[1], 0.0f, (float)f.height - 1.0f, &view->y) ||
        !whole(v[2], 1.0f, (float)(f.width - view->x), &view->width) ||
        !whole(v[3], 1.0f, (float)(f.height - view->y), &view->height)) {
      return false;
    }
    copy_matrix(&view->view, v + 4);
    copy_matrix(&view->proj, v + 20);
  }
  *out = f;
  return true;
}

/* Off-axis perspective projection from the tangents of the frustum's sides
 * (left and bottom negative). */
static zb_mat4 frustum(float left, float right, float bottom, float top, float znear, float zfar) {
  zb_mat4 m;
  memset(&m, 0, sizeof m);
  m.m[0] = 2.0f / (right - left);
  m.m[5] = 2.0f / (top - bottom);
  m.m[8] = (right + left) / (right - left);
  m.m[9] = (top + bottom) / (top - bottom);
  m.m[10] = -(zfar + znear) / (zfar - znear);
  m.m[11] = -1.0f;
  m.m[14] = -2.0f * zfar * znear / (zfar - znear);
  return m;
}

/* The inverse of the rigid transform translate(eye) * rotate(q). */
static zb_mat4 rigid_inverse(zb_vec3 eye, zb_quat q) {
  zb_mat4 rot = zb_m4_from_quat(q);
  zb_mat4 inv = zb_m4_identity();
  for (int c = 0; c < 3; ++c) {
    zb_vec3 axis = zb_v3(rot.m[c * 4], rot.m[c * 4 + 1], rot.m[c * 4 + 2]);
    inv.m[0 * 4 + c] = axis.x;
    inv.m[1 * 4 + c] = axis.y;
    inv.m[2 * 4 + c] = axis.z;
    inv.m[12 + c] = -zb_v3_dot(axis, eye);
  }
  return inv;
}

void zb_xr_emulate(zb_xr_pose head, int width, int height, float ipd, zb_xr_frame *out) {
  memset(out, 0, sizeof *out);
  out->width = width < 2 ? 2 : width;
  out->height = height < 1 ? 1 : height;
  out->view_count = 2;
  zb_quat q = zb_quat_normalize(head.orientation); /* identity for a zero quaternion */
  out->head.position = head.position;
  out->head.orientation = q;
  zb_vec3 right = zb_quat_rotate(q, zb_v3(1.0f, 0.0f, 0.0f));
  const float outward = tanf(50.0f * ZB_PI / 180.0f), inward = tanf(40.0f * ZB_PI / 180.0f);
  int half = out->width / 2;
  for (int i = 0; i < 2; ++i) {
    zb_xr_view *v = &out->views[i];
    v->x = i * half;
    v->y = 0;
    v->width = i == 0 ? half : out->width - half;
    v->height = out->height;
    float side = i == 0 ? -1.0f : 1.0f;
    zb_vec3 eye = zb_v3_add(head.position, zb_v3_scale(right, side * 0.5f * ipd));
    v->view = rigid_inverse(eye, q);
    /* Horizontal tangents like real lenses; the vertical ones follow the viewport's aspect. */
    float l = i == 0 ? -outward : -inward, r = i == 0 ? inward : outward;
    float t = 0.5f * (r - l) * (float)v->height / (float)v->width;
    v->proj = frustum(l, r, -t, t, 0.1f, 1000.0f);
  }
}

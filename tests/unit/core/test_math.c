/* SPDX-License-Identifier: MIT */
#include "core/math.h"
#include "support/test_support.h"
#include "unity.h"

ZB_TEST_LEAK_CHECKED

#define EPS 1e-5f

static void assert_v3(zb_vec3 e, zb_vec3 a) {
  TEST_ASSERT_FLOAT_WITHIN(EPS, e.x, a.x);
  TEST_ASSERT_FLOAT_WITHIN(EPS, e.y, a.y);
  TEST_ASSERT_FLOAT_WITHIN(EPS, e.z, a.z);
}

static void test_vec3(void) {
  zb_vec3 a = zb_v3(1, 2, 3), b = zb_v3(4, 5, 6);
  assert_v3(zb_v3(5, 7, 9), zb_v3_add(a, b));
  assert_v3(zb_v3(-3, -3, -3), zb_v3_sub(a, b));
  assert_v3(zb_v3(2, 4, 6), zb_v3_scale(a, 2));
  TEST_ASSERT_FLOAT_WITHIN(EPS, 32.0f, zb_v3_dot(a, b));
  assert_v3(zb_v3(0, 0, 1), zb_v3_cross(zb_v3(1, 0, 0), zb_v3(0, 1, 0)));
  TEST_ASSERT_FLOAT_WITHIN(EPS, 5.0f, zb_v3_length(zb_v3(3, 4, 0)));
  assert_v3(zb_v3(0.6f, 0.8f, 0), zb_v3_normalize(zb_v3(3, 4, 0)));
  assert_v3(zb_v3(0, 0, 0), zb_v3_normalize(zb_v3(0, 0, 0)));
  assert_v3(zb_v3(2.5f, 3.5f, 4.5f), zb_v3_lerp(a, b, 0.5f));
  TEST_ASSERT_FLOAT_WITHIN(EPS, 1.0f, zb_clampf(3.0f, 0.0f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(EPS, 0.0f, zb_clampf(-3.0f, 0.0f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(EPS, 0.5f, zb_clampf(0.5f, 0.0f, 1.0f));
}

static void test_mat4_basics(void) {
  zb_mat4 t = zb_m4_translate(zb_v3(1, 2, 3));
  zb_mat4 s = zb_m4_scale(zb_v3(2, 2, 2));
  zb_mat4 ts = zb_m4_mul(t, s);
  assert_v3(zb_v3(3, 4, 5), zb_m4_transform_point(ts, zb_v3(1, 1, 1)));
  zb_mat4 i = zb_m4_mul(zb_m4_identity(), t);
  for (int k = 0; k < 16; ++k) {
    TEST_ASSERT_FLOAT_WITHIN(EPS, t.m[k], i.m[k]);
  }
  zb_vec4 v = {1, 0, 0, 0};
  zb_vec4 r = zb_m4_mul_v4(t, v); /* direction: translation ignored */
  TEST_ASSERT_FLOAT_WITHIN(EPS, 1.0f, r.x);
  TEST_ASSERT_FLOAT_WITHIN(EPS, 0.0f, r.w);
}

static void test_quat_and_trs(void) {
  zb_quat q = zb_quat_axis_angle(zb_v3(0, 1, 0), ZB_PI * 0.5f);
  assert_v3(zb_v3(0, 0, -1), zb_quat_rotate(q, zb_v3(1, 0, 0)));
  zb_mat4 m = zb_m4_from_quat(q);
  assert_v3(zb_v3(0, 0, -1), zb_m4_transform_point(m, zb_v3(1, 0, 0)));
  zb_quat qq = zb_quat_mul(q, q);
  assert_v3(zb_v3(-1, 0, 0), zb_quat_rotate(qq, zb_v3(1, 0, 0)));
  zb_quat n = zb_quat_normalize((zb_quat){0, 0, 0, 2});
  TEST_ASSERT_FLOAT_WITHIN(EPS, 1.0f, n.w);
  zb_quat zero = zb_quat_normalize((zb_quat){0, 0, 0, 0});
  TEST_ASSERT_FLOAT_WITHIN(EPS, 1.0f, zero.w);
  zb_mat4 trs = zb_m4_trs(zb_v3(10, 0, 0), zb_quat_identity(), zb_v3(2, 2, 2));
  assert_v3(zb_v3(12, 2, 2), zb_m4_transform_point(trs, zb_v3(1, 1, 1)));
}

static void test_projection_and_view(void) {
  zb_mat4 p = zb_m4_perspective(ZB_PI * 0.5f, 1.0f, 1.0f, 100.0f);
  zb_vec4 near_pt = zb_m4_mul_v4(p, (zb_vec4){0, 0, -1, 1});
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, -1.0f, near_pt.z / near_pt.w);
  zb_vec4 far_pt = zb_m4_mul_v4(p, (zb_vec4){0, 0, -100, 1});
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, far_pt.z / far_pt.w);
  zb_mat4 o = zb_m4_ortho(0, 10, 0, 10, -1, 1);
  assert_v3(zb_v3(1, 1, 0), zb_m4_transform_point(o, zb_v3(10, 10, 0)));
  zb_mat4 v = zb_m4_look_at(zb_v3(0, 0, 5), zb_v3(0, 0, 0), zb_v3(0, 1, 0));
  assert_v3(zb_v3(0, 0, -5), zb_m4_transform_point(v, zb_v3(0, 0, 0)));
}

static void test_normal_matrix(void) {
  zb_mat4 s = zb_m4_scale(zb_v3(2, 1, 1));
  zb_mat4 n = zb_m4_normal_matrix(s);
  TEST_ASSERT_FLOAT_WITHIN(EPS, 0.5f, n.m[0]);
  TEST_ASSERT_FLOAT_WITHIN(EPS, 1.0f, n.m[5]);
  zb_mat4 rot = zb_m4_from_quat(zb_quat_axis_angle(zb_v3(0, 0, 1), 0.7f));
  zb_mat4 nr = zb_m4_normal_matrix(rot);
  for (int k = 0; k < 11; ++k) {
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, rot.m[k], nr.m[k]);
  }
  zb_mat4 singular = zb_m4_scale(zb_v3(0, 1, 1));
  zb_mat4 ns = zb_m4_normal_matrix(singular);
  TEST_ASSERT_FLOAT_WITHIN(EPS, 1.0f, ns.m[0]); /* identity fallback */
}

static void test_frustum(void) {
  zb_mat4 p = zb_m4_perspective(ZB_PI * 0.5f, 1.0f, 0.1f, 100.0f);
  zb_mat4 v = zb_m4_look_at(zb_v3(0, 0, 0), zb_v3(0, 0, -1), zb_v3(0, 1, 0));
  zb_frustum f = zb_frustum_from_matrix(zb_m4_mul(p, v));
  TEST_ASSERT_TRUE(zb_frustum_sphere_visible(&f, zb_v3(0, 0, -10), 1.0f));
  TEST_ASSERT_FALSE(zb_frustum_sphere_visible(&f, zb_v3(0, 0, 10), 1.0f));     /* behind */
  TEST_ASSERT_FALSE(zb_frustum_sphere_visible(&f, zb_v3(0, 0, -200), 1.0f));   /* too far */
  TEST_ASSERT_FALSE(zb_frustum_sphere_visible(&f, zb_v3(50, 0, -10), 1.0f));   /* right */
  TEST_ASSERT_TRUE(zb_frustum_sphere_visible(&f, zb_v3(10.5f, 0, -10), 1.0f)); /* touching edge */
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_vec3);
  RUN_TEST(test_mat4_basics);
  RUN_TEST(test_quat_and_trs);
  RUN_TEST(test_projection_and_view);
  RUN_TEST(test_normal_matrix);
  RUN_TEST(test_frustum);
  return UNITY_END();
}

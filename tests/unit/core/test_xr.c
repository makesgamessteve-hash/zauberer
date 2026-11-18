/* SPDX-License-Identifier: MIT */
#include "core/xr.h"
#include "support/test_support.h"
#include "unity.h"

#include <math.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

#define EPS 1e-5f

static void assert_v3(zb_vec3 e, zb_vec3 a) {
  TEST_ASSERT_FLOAT_WITHIN(EPS, e.x, a.x);
  TEST_ASSERT_FLOAT_WITHIN(EPS, e.y, a.y);
  TEST_ASSERT_FLOAT_WITHIN(EPS, e.z, a.z);
}

/* A valid two-view frame as the bridge writes it: 800 x 400, two 380 x 380
 * viewports with a 10 px border, distinct matrices. */
static size_t make_frame(float *d) {
  memset(d, 0, ZB_XR_MAX_FLOATS * sizeof(float));
  const float header[ZB_XR_HEADER_FLOATS] = {2, 800, 400, 0.1f, 0.2f, -0.3f, 0, 0, 0, 2};
  memcpy(d, header, sizeof header);
  for (int i = 0; i < 2; ++i) {
    float *v = d + ZB_XR_HEADER_FLOATS + i * ZB_XR_VIEW_FLOATS;
    v[0] = (float)(10 + i * 400);
    v[1] = 10;
    v[2] = 380;
    v[3] = 380;
    for (int k = 0; k < 16; ++k) {
      v[4 + k] = (float)(i * 100 + k);
      v[20 + k] = (float)(i * 100 + 50 + k);
    }
  }
  return ZB_XR_HEADER_FLOATS + 2 * ZB_XR_VIEW_FLOATS;
}

static void test_decode(void) {
  float d[ZB_XR_MAX_FLOATS];
  size_t n = make_frame(d);
  zb_xr_frame f;
  TEST_ASSERT_TRUE(zb_xr_frame_decode(d, n, 7, &f));
  TEST_ASSERT_EQUAL_UINT32(7, f.framebuffer);
  TEST_ASSERT_EQUAL_INT(800, f.width);
  TEST_ASSERT_EQUAL_INT(400, f.height);
  TEST_ASSERT_EQUAL_INT(2, f.view_count);
  assert_v3(zb_v3(0.1f, 0.2f, -0.3f), f.head.position);
  TEST_ASSERT_FLOAT_WITHIN(EPS, 1.0f, f.head.orientation.w); /* normalised */
  TEST_ASSERT_EQUAL_INT(410, f.views[1].x);
  TEST_ASSERT_EQUAL_INT(10, f.views[1].y);
  TEST_ASSERT_EQUAL_INT(380, f.views[1].width);
  TEST_ASSERT_EQUAL_INT(380, f.views[1].height);
  TEST_ASSERT_EQUAL_FLOAT(115.0f, f.views[1].view.m[15]);
  TEST_ASSERT_EQUAL_FLOAT(150.0f, f.views[1].proj.m[0]);
  /* One view (e.g. a phone-like inline device) is fine too. */
  d[0] = 1;
  TEST_ASSERT_TRUE(zb_xr_frame_decode(d, ZB_XR_HEADER_FLOATS + ZB_XR_VIEW_FLOATS, 0, &f));
  TEST_ASSERT_EQUAL_INT(1, f.view_count);
}

/* spec: webxr-session: Bad frame data */
static void test_decode_rejects_bad_data(void) {
  float d[ZB_XR_MAX_FLOATS];
  size_t n = make_frame(d);
  zb_xr_frame f, before;
  memset(&f, 0x5a, sizeof f);
  before = f;
  struct {
    int index;
    float value;
  } cases[] = {
      {0, 3.0f},                                      /* three views */
      {0, 0.0f},                                      /* no view */
      {0, 1.5f},                                      /* not a count */
      {1, 0.0f},                                      /* no framebuffer width */
      {2, -400.0f},                                   /* negative height */
      {1, 800.5f},                                    /* fractional size */
      {ZB_XR_HEADER_FLOATS + ZB_XR_VIEW_FLOATS, 500}, /* right viewport outside the framebuffer */
      {ZB_XR_HEADER_FLOATS + 2, 0.0f},                /* empty viewport */
      {ZB_XR_HEADER_FLOATS + 1, -1.0f},               /* viewport below the framebuffer */
      {ZB_XR_HEADER_FLOATS + 3, 391.0f},              /* viewport taller than the framebuffer */
      {ZB_XR_HEADER_FLOATS + 4 + 5, NAN},             /* NaN in a view matrix */
      {ZB_XR_HEADER_FLOATS + 20, INFINITY},           /* infinite projection entry */
      {3, NAN},                                       /* NaN in the head position */
  };
  for (size_t i = 0; i < ZB_ARRAY_LEN(cases); ++i) {
    make_frame(d);
    d[cases[i].index] = cases[i].value;
    TEST_ASSERT_FALSE_MESSAGE(zb_xr_frame_decode(d, n, 0, &f), "malformed frame accepted");
    TEST_ASSERT_EQUAL_MEMORY(&before, &f, sizeof f);
  }
  make_frame(d);
  d[9] = 0.0f; /* zero orientation quaternion */
  TEST_ASSERT_FALSE(zb_xr_frame_decode(d, n, 0, &f));
  make_frame(d);
  TEST_ASSERT_FALSE(zb_xr_frame_decode(d, n - 1, 0, &f)); /* length does not match the count */
  TEST_ASSERT_FALSE(zb_xr_frame_decode(d, 3, 0, &f));     /* shorter than the header */
  TEST_ASSERT_FALSE(zb_xr_frame_decode(NULL, n, 0, &f));
  TEST_ASSERT_TRUE(zb_xr_frame_decode(d, n, 0, &f));
}

static void test_emulated_headset(void) {
  zb_xr_pose head = {zb_v3(1, 2, 3), zb_quat_axis_angle(zb_v3(0, 1, 0), ZB_PI * 0.5f)};
  zb_xr_frame f;
  zb_xr_emulate(head, 640, 360, 0.064f, &f);
  TEST_ASSERT_EQUAL_INT(2, f.view_count);
  TEST_ASSERT_EQUAL_UINT32(0, f.framebuffer);
  TEST_ASSERT_EQUAL_INT(640, f.width);
  assert_v3(head.position, f.head.position);
  /* Side by side. */
  TEST_ASSERT_EQUAL_INT(0, f.views[0].x);
  TEST_ASSERT_EQUAL_INT(320, f.views[0].width);
  TEST_ASSERT_EQUAL_INT(320, f.views[1].x);
  TEST_ASSERT_EQUAL_INT(320, f.views[1].width);
  TEST_ASSERT_EQUAL_INT(360, f.views[1].height);
  /* Turned 90 degrees left, the head's right is world -Z: the eyes sit 32 mm
   * either side of the head along it, and each view maps its eye to the origin. */
  zb_vec3 eyes[2] = {zb_v3(1, 2, 3.032f), zb_v3(1, 2, 2.968f)};
  for (int i = 0; i < 2; ++i) {
    assert_v3(zb_v3(0, 0, 0), zb_m4_transform_point(f.views[i].view, eyes[i]));
    /* Looking along the head's forward (world -X). */
    zb_vec3 ahead = zb_m4_transform_point(f.views[i].view, zb_v3_add(eyes[i], zb_v3(-1, 0, 0)));
    assert_v3(zb_v3(0, 0, -1), ahead);
  }
  /* Real-lens frusta: wider outwards, so the centres shift towards the nose. */
  TEST_ASSERT_TRUE(f.views[0].proj.m[8] < -0.05f);
  TEST_ASSERT_TRUE(f.views[1].proj.m[8] > 0.05f);
  TEST_ASSERT_FLOAT_WITHIN(EPS, -f.views[0].proj.m[8], f.views[1].proj.m[8]);
  TEST_ASSERT_FLOAT_WITHIN(EPS, 0.0f, f.views[0].proj.m[9]);
  /* Square pixels: horizontal and vertical scale follow the viewport's aspect. */
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, f.views[0].proj.m[0] * 320.0f, f.views[0].proj.m[5] * 360.0f);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, f.views[0].proj.m[11]);
  /* Degenerate sizes and a zero quaternion still give a usable rig. */
  zb_xr_pose bad = {zb_v3(0, 0, 0), {0, 0, 0, 0}};
  zb_xr_emulate(bad, 0, 0, 0.064f, &f);
  TEST_ASSERT_EQUAL_INT(2, f.width);
  TEST_ASSERT_EQUAL_INT(1, f.height);
  TEST_ASSERT_EQUAL_INT(1, f.views[1].width);
  TEST_ASSERT_FLOAT_WITHIN(EPS, 1.0f, f.head.orientation.w);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_decode);
  RUN_TEST(test_decode_rejects_bad_data);
  RUN_TEST(test_emulated_headset);
  return UNITY_END();
}

/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "input/gesture.h"
#include "unity.h"

#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define PTS 48

static float g_xy[2 * 1024];

/* A form's template drawn on screen: scaled, turned, shifted, maybe mirrored,
 * reversed, jittered (deterministically), with n points. */
typedef struct drawing {
  double scale, turn, dx, dy, jitter;
  bool mirror, reverse;
  int n;
} drawing;

static int draw(zb_form f, const drawing *d) {
  static float t[2 * 1024];
  zb_gesture_template(f, t, d->n);
  uint32_t seed = 12345u + (uint32_t)f;
  for (int k = 0; k < d->n; ++k) {
    int src = d->reverse ? d->n - 1 - k : k;
    double x = t[2 * src], y = d->mirror ? -t[2 * src + 1] : t[2 * src + 1];
    seed = seed * 1664525u + 1013904223u;
    double jx = ((seed >> 8) / 16777216.0 - 0.5) * d->jitter;
    seed = seed * 1664525u + 1013904223u;
    double jy = ((seed >> 8) / 16777216.0 - 0.5) * d->jitter;
    double c = zb_dcos(d->turn), s = zb_dsin(d->turn);
    g_xy[2 * k] = (float)(d->dx + d->scale * (c * x - s * y + jx));
    g_xy[2 * k + 1] = (float)(d->dy + d->scale * (s * x + c * y + jy));
  }
  return d->n;
}

/* spec: gesture-recognition: The eight forms */
static void test_templates(void) {
  for (int f = ZB_FORM_LINE; f < ZB_FORMS; ++f) {
    drawing d = {100.0, 0.0, 300.0, 200.0, 0.0, false, false, PTS};
    zb_gesture g;
    zb_gesture_recognize(g_xy, draw((zb_form)f, &d), NULL, &g);
    TEST_ASSERT_EQUAL_STRING(zb_form_name((zb_form)f), zb_form_name(g.form));
    TEST_ASSERT_TRUE_MESSAGE(g.accuracy > 0.95, zb_form_name((zb_form)f));
    TEST_ASSERT_TRUE(g.second != g.form && g.second_accuracy < g.accuracy);
  }
}

/* spec: gesture-recognition: Any orientation, either way round */
static void test_variations(void) {
  const drawing ways[] = {
      {100.0, 0.65, 0.0, 0.0, 0.0, false, false, PTS},  /* turned */
      {100.0, 0.0, 0.0, 0.0, 0.0, false, true, PTS},    /* drawn backwards */
      {100.0, 2.0, 50.0, -20.0, 0.0, true, false, PTS}, /* mirrored and turned */
      {340.0, 0.0, 0.0, 0.0, 0.0, false, false, 200},   /* bigger, more points */
      {30.0, -1.2, 0.0, 0.0, 0.0, false, false, 24},    /* small, few points */
      {100.0, 0.3, 0.0, 0.0, 0.12, false, false, 80},   /* a shaky hand */
  };
  for (int f = ZB_FORM_LINE; f < ZB_FORMS; ++f) {
    for (size_t w = 0; w < sizeof ways / sizeof ways[0]; ++w) {
      zb_gesture g;
      zb_gesture_recognize(g_xy, draw((zb_form)f, &ways[w]), NULL, &g);
      char msg[64];
      snprintf(msg, sizeof msg, "%s, way %d: %s %.2f", zb_form_name((zb_form)f), (int)w, zb_form_name(g.form),
               g.accuracy);
      TEST_ASSERT_EQUAL_INT_MESSAGE(f, g.form, msg);
      TEST_ASSERT_TRUE_MESSAGE(g.accuracy > (ways[w].jitter > 0.0 ? 0.7 : 0.9), msg);
    }
  }
  /* A circle and a square started somewhere else on their outline. */
  float ring[2 * 64];
  for (int k = 0; k < 64; ++k) {
    double a = 0.9 + 2.0 * ZB_DPI * k / 63.0;
    ring[2 * k] = (float)(50.0 * zb_dcos(a));
    ring[2 * k + 1] = (float)(50.0 * zb_dsin(a));
  }
  zb_gesture g;
  zb_gesture_recognize(ring, 64, NULL, &g);
  TEST_ASSERT_EQUAL_INT(ZB_FORM_CIRCLE, g.form);
  const float box[] = {0, -50, 50, -50, 50, 50, -50, 50, -50, -50, 0, -50};
  zb_gesture_recognize(box, 6, NULL, &g);
  TEST_ASSERT_EQUAL_INT(ZB_FORM_SQUARE, g.form);
}

/* spec: gesture-recognition: Taps, scribbles and assist */
static void test_taps_and_scribbles(void) {
  const float dot[] = {10, 10, 12, 11, 11, 13};
  zb_gesture g;
  zb_gesture_recognize(dot, 3, NULL, &g);
  TEST_ASSERT_EQUAL_INT(ZB_FORM_TAP, g.form);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, g.accuracy);
  zb_gesture_options big = {40.0, false};
  const float flick[] = {0, 0, 30, 0};
  zb_gesture_recognize(flick, 2, &big, &g);
  TEST_ASSERT_EQUAL_INT(ZB_FORM_TAP, g.form);
  zb_gesture_recognize(flick, 2, NULL, &g);
  TEST_ASSERT_EQUAL_INT(ZB_FORM_LINE, g.form);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, g.angle);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 15.0, g.size);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 15.0, g.cx);
  const float still[] = {5, 5, 5, 5, 5, 5};
  zb_gesture_recognize(still, 3, NULL, &g);
  TEST_ASSERT_EQUAL_INT(ZB_FORM_TAP, g.form);
  /* A scribble is no form; assist is kinder to sloppy shapes. */
  float mess[2 * 120];
  uint32_t seed = 99u;
  for (int k = 0; k < 120; ++k) {
    seed = seed * 1664525u + 1013904223u;
    mess[2 * k] = (float)((seed >> 8) % 200);
    seed = seed * 1664525u + 1013904223u;
    mess[2 * k + 1] = (float)((seed >> 8) % 200);
  }
  zb_gesture_recognize(mess, 120, NULL, &g);
  TEST_ASSERT_EQUAL_INT(ZB_FORM_NONE, g.form);
  TEST_ASSERT_TRUE(g.second >= ZB_FORM_LINE && g.second_accuracy < ZB_GESTURE_MIN_ACCURACY);
  drawing sloppy = {100.0, 0.0, 0.0, 0.0, 0.35, false, false, 60};
  zb_gesture plain, kind;
  zb_gesture_recognize(g_xy, draw(ZB_FORM_TRIANGLE, &sloppy), NULL, &plain);
  zb_gesture_options assist = {0.0, true};
  zb_gesture_recognize(g_xy, draw(ZB_FORM_TRIANGLE, &sloppy), &assist, &kind);
  TEST_ASSERT_TRUE(kind.accuracy > plain.accuracy);
  /* Nothing to look at. */
  zb_gesture_recognize(NULL, 3, NULL, &g);
  TEST_ASSERT_EQUAL_INT(ZB_FORM_NONE, g.form);
  zb_gesture_recognize(dot, 0, NULL, &g);
  TEST_ASSERT_EQUAL_INT(ZB_FORM_NONE, g.form);
  /* Names. */
  for (int f = 0; f < ZB_FORMS; ++f) {
    TEST_ASSERT_EQUAL_INT(f, zb_form_by_name(zb_form_name((zb_form)f)));
  }
  TEST_ASSERT_EQUAL_INT(ZB_FORM_NONE, zb_form_by_name("hexagon"));
  TEST_ASSERT_EQUAL_INT(ZB_FORM_NONE, zb_form_by_name(NULL));
  TEST_ASSERT_EQUAL_STRING("none", zb_form_name(ZB_FORM_NONE));
  float one[2];
  zb_gesture_template(ZB_FORM_TAP, one, 1);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, one[0]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_templates);
  RUN_TEST(test_variations);
  RUN_TEST(test_taps_and_scribbles);
  return UNITY_END();
}

/* SPDX-License-Identifier: MIT */
#include "core/keys.h"
#include "support/test_support.h"
#include "unity.h"

ZB_TEST_LEAK_CHECKED

static void test_key_names(void) {
  TEST_ASSERT_EQUAL_STRING("space", zb_key_name(ZB_KEY_SPACE));
  TEST_ASSERT_EQUAL_STRING("x", zb_key_name(ZB_KEY_X));
  TEST_ASSERT_EQUAL_STRING("unknown", zb_key_name(ZB_KEY_COUNT));
  TEST_ASSERT_EQUAL_INT(ZB_KEY_LSHIFT, zb_key_from_name("left_shift"));
  TEST_ASSERT_EQUAL_INT(ZB_KEY_UNKNOWN, zb_key_from_name("hyper"));
  for (int k = 1; k < ZB_KEY_COUNT; ++k) {
    TEST_ASSERT_EQUAL_INT(k, zb_key_from_name(zb_key_name((zb_key)k)));
  }
}

static void test_mouse_and_pad_names(void) {
  TEST_ASSERT_EQUAL_STRING("right", zb_mouse_button_name(ZB_MOUSE_RIGHT));
  TEST_ASSERT_EQUAL_STRING("none", zb_mouse_button_name((zb_mouse_button)9));
  TEST_ASSERT_EQUAL_INT(ZB_MOUSE_X2, zb_mouse_button_from_name("x2"));
  TEST_ASSERT_EQUAL_INT(0, zb_mouse_button_from_name("wheel"));
  TEST_ASSERT_EQUAL_STRING("left_shoulder", zb_pad_button_name(ZB_PAD_LEFT_SHOULDER));
  TEST_ASSERT_EQUAL_STRING("?", zb_pad_button_name(ZB_PAD_BUTTON_COUNT));
  TEST_ASSERT_EQUAL_INT(ZB_PAD_X, zb_pad_button_from_name("x"));
  TEST_ASSERT_EQUAL_INT(-1, zb_pad_button_from_name("z"));
  TEST_ASSERT_EQUAL_STRING("right_trigger", zb_pad_axis_name(ZB_AXIS_RIGHT_TRIGGER));
  TEST_ASSERT_EQUAL_STRING("?", zb_pad_axis_name(ZB_PAD_AXIS_COUNT));
  TEST_ASSERT_EQUAL_INT(ZB_AXIS_LEFT_Y, zb_pad_axis_from_name("left_y"));
  TEST_ASSERT_EQUAL_INT(-1, zb_pad_axis_from_name("throttle"));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_key_names);
  RUN_TEST(test_mouse_and_pad_names);
  return UNITY_END();
}

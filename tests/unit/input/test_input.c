/* SPDX-License-Identifier: MIT */
#include "core/keys.h"
#include "core/log.h"
#include "core/xr.h"
#include "input/input.h"
#include "support/test_support.h"
#include "unity.h"

#include <string.h>

static zb_input *in;

void setUp(void) {
  zb_test_begin();
  zb_log_reset();
  zb_log_set_stderr(false);
  in = zb_input_create(zb_test_alloc());
}

void tearDown(void) {
  zb_input_destroy(in);
  zb_log_reset();
  zb_test_end();
}

static zb_event key(zb_key k, bool down, uint64_t t) {
  zb_event e;
  memset(&e, 0, sizeof e);
  e.type = ZB_EV_KEY;
  e.u.key.key = (uint16_t)k;
  e.u.key.down = down;
  e.time_ns = t;
  return e;
}

static zb_event mouse_button(int b, bool down, bool synthetic, uint64_t t) {
  zb_event e;
  memset(&e, 0, sizeof e);
  e.type = ZB_EV_MOUSE_BUTTON;
  e.u.mouse_button.button = (uint8_t)b;
  e.u.mouse_button.down = down;
  e.u.mouse_button.synthetic = synthetic;
  e.time_ns = t;
  return e;
}

static zb_event touch(zb_event_type type, uint64_t finger, float x, float y, uint64_t t) {
  zb_event e;
  memset(&e, 0, sizeof e);
  e.type = type;
  e.u.touch.finger = finger;
  e.u.touch.x = x;
  e.u.touch.y = y;
  e.time_ns = t;
  return e;
}

static zb_value parse(zb_luon_doc *doc, const char *text) {
  zb_luon_error err;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_luon_parse(doc, zb_test_alloc(), text, strlen(text), &err));
  return doc->root;
}

/* spec: input-actions: Button held across ticks */
static void test_button_held_across_ticks(void) {
  int cast = zb_input_define(in, "cast_left", ZB_ACTION_BUTTON);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_bind(in, cast, "mouse:left"));
  zb_event down = mouse_button(ZB_MOUSE_LEFT, true, false, 0);
  zb_input_handle_event(in, &down);
  for (int t = 1; t <= 10; ++t) {
    zb_input_sample(in, 0);
    const zb_action_state *s = zb_input_state(in, cast);
    TEST_ASSERT_TRUE(s->held);
    TEST_ASSERT_EQUAL(t == 1, s->pressed);
    TEST_ASSERT_FALSE(s->released);
  }
  zb_event up = mouse_button(ZB_MOUSE_LEFT, false, false, 0);
  zb_input_handle_event(in, &up);
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, cast)->released);
  TEST_ASSERT_FALSE(zb_input_state(in, cast)->held);
  zb_input_sample(in, 0);
  TEST_ASSERT_FALSE(zb_input_state(in, cast)->released);
}

static void test_tap_between_ticks_is_seen(void) {
  int jump = zb_input_define(in, "jump", ZB_ACTION_BUTTON);
  zb_input_bind(in, jump, "key:space");
  zb_event d = key(ZB_KEY_SPACE, true, 0), u = key(ZB_KEY_SPACE, false, 0);
  zb_input_handle_event(in, &d);
  zb_input_handle_event(in, &u);
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, jump)->pressed);
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, jump)->released);
}

static void test_define_and_bind_rules(void) {
  int a = zb_input_define(in, "move", ZB_ACTION_AXIS2);
  TEST_ASSERT_EQUAL_INT(a, zb_input_define(in, "move", ZB_ACTION_AXIS2));
  TEST_ASSERT_EQUAL_INT(-1, zb_input_define(in, "move", ZB_ACTION_BUTTON));
  TEST_ASSERT_EQUAL_INT(-1, zb_input_define(in, "", ZB_ACTION_BUTTON));
  TEST_ASSERT_EQUAL_STRING("move", zb_input_action_name(in, a));
  TEST_ASSERT_NULL(zb_input_action_name(in, 99));
  TEST_ASSERT_EQUAL_INT(ZB_ACTION_AXIS2, zb_input_action_type(in, a));
  TEST_ASSERT_EQUAL_INT(-1, zb_input_find(in, "nothing"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, 99, "key:a"));
  const char *ok2[] = {"keys:w,s,a,d", "gamepad:left_stick", "gamepad:right_stick", "mouse:motion", "touch:stick:move"};
  for (size_t i = 0; i < ZB_ARRAY_LEN(ok2); ++i) {
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_bind(in, a, ok2[i]));
  }
  const char *bad2[] = {"key:a",        "keys:w,s,a",     "keys:w,s,a,d,e", "keys:w,s,a,nope", "mouse:left",
                        "gamepad:a",    "touch:button:x", "nonsense",       "joystick:1",      "gamepad:wheel",
                        "touch:stick:", "touch:other",    "keys:a,d"};
  for (size_t i = 0; i < ZB_ARRAY_LEN(bad2); ++i) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZB_ERR_INVALID, zb_input_bind(in, a, bad2[i]), bad2[i]);
  }
  TEST_ASSERT_EQUAL_size_t(5, zb_input_binding_count(in, a));
  TEST_ASSERT_EQUAL_STRING("mouse:motion", zb_input_binding(in, a, 3));
  TEST_ASSERT_NULL(zb_input_binding(in, a, 9));
  int b = zb_input_define(in, "fire", ZB_ACTION_BUTTON);
  const char *okb[] = {
      "key:f", "mouse:right", "gamepad:a", "gamepad:right_trigger", "touch:button:fire", "gamepad:left_stick"};
  for (size_t i = 0; i < ZB_ARRAY_LEN(okb); ++i) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZB_OK, zb_input_bind(in, b, okb[i]), okb[i]);
  }
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, b, "key:nope"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, b, "mouse:middle_finger"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, b, "touch:stick:s"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, b, "mouse:motion"));
  int c = zb_input_define(in, "zoom", ZB_ACTION_AXIS1);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_bind(in, c, "keys:q,e"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, c, "keys:q"));
  zb_input_bind(in, b, "key:g");
  zb_input_bind(in, b, "key:i");
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_input_bind(in, b, "key:h")); /* 8 bindings max */
  char longb[100];
  memset(longb, 'k', sizeof longb - 1);
  longb[sizeof longb - 1] = '\0';
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, c, longb));
  zb_input_clear_bindings(in, b);
  zb_input_clear_bindings(in, 99);
  TEST_ASSERT_EQUAL_size_t(0, zb_input_binding_count(in, b));
  TEST_ASSERT_NULL(zb_input_state(in, 99));
  char name[16];
  for (int i = (int)zb_input_action_count(in); i < ZB_INPUT_MAX_ACTIONS; ++i) {
    snprintf(name, sizeof name, "a%d", i);
    TEST_ASSERT_TRUE(zb_input_define(in, name, ZB_ACTION_BUTTON) >= 0);
  }
  TEST_ASSERT_EQUAL_INT(-1, zb_input_define(in, "overflow", ZB_ACTION_BUTTON));
}

static void test_axes_from_keys_mouse_pad(void) {
  int move = zb_input_define(in, "move", ZB_ACTION_AXIS2);
  int zoom = zb_input_define(in, "zoom", ZB_ACTION_AXIS1);
  int look = zb_input_define(in, "look", ZB_ACTION_AXIS2);
  int trig = zb_input_define(in, "trigger", ZB_ACTION_BUTTON);
  int fire = zb_input_define(in, "fire", ZB_ACTION_BUTTON);
  zb_input_bind(in, move, "keys:w,s,a,d");
  zb_input_bind(in, move, "gamepad:left_stick");
  zb_input_bind(in, zoom, "keys:q,e");
  zb_input_bind(in, zoom, "gamepad:left_x");
  zb_input_bind(in, look, "mouse:motion");
  zb_input_bind(in, trig, "gamepad:right_trigger");
  zb_input_bind(in, fire, "gamepad:a");
  zb_event w = key(ZB_KEY_W, true, 0), dd = key(ZB_KEY_D, true, 0), e = key(ZB_KEY_E, true, 0);
  zb_input_handle_event(in, &w);
  zb_input_handle_event(in, &dd);
  zb_input_handle_event(in, &e);
  zb_event repeat = key(ZB_KEY_W, true, 0);
  repeat.u.key.repeat = true;
  zb_input_handle_event(in, &repeat);
  zb_event motion;
  memset(&motion, 0, sizeof motion);
  motion.type = ZB_EV_MOUSE_MOTION;
  motion.u.mouse_motion.dx = 3;
  motion.u.mouse_motion.dy = 4;
  zb_input_handle_event(in, &motion);
  zb_event synth_motion = motion;
  synth_motion.u.mouse_motion.synthetic = true;
  zb_input_handle_event(in, &synth_motion);
  zb_input_sample(in, 0);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, zb_input_state(in, move)->x);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, zb_input_state(in, move)->y);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, zb_input_state(in, zoom)->x);
  TEST_ASSERT_EQUAL_FLOAT(3.0f, zb_input_state(in, look)->x);
  TEST_ASSERT_EQUAL_FLOAT(-4.0f, zb_input_state(in, look)->y);
  zb_input_sample(in, 0);
  TEST_ASSERT_FALSE(zb_input_state(in, look)->held); /* motion is per tick */
  /* gamepad */
  zb_event added;
  memset(&added, 0, sizeof added);
  added.type = ZB_EV_GAMEPAD_ADDED;
  added.u.gamepad.id = 7;
  zb_input_handle_event(in, &added);
  zb_event ax;
  memset(&ax, 0, sizeof ax);
  ax.type = ZB_EV_GAMEPAD_AXIS;
  ax.u.gamepad_axis.id = 7;
  ax.u.gamepad_axis.axis = ZB_AXIS_LEFT_X;
  ax.u.gamepad_axis.value = -2.0f;
  zb_input_handle_event(in, &ax);
  ax.u.gamepad_axis.axis = ZB_AXIS_RIGHT_TRIGGER;
  ax.u.gamepad_axis.value = 0.9f;
  zb_input_handle_event(in, &ax);
  ax.u.gamepad_axis.axis = 99;
  zb_input_handle_event(in, &ax);
  zb_event k2 = key(ZB_KEY_W, false, 0), k3 = key(ZB_KEY_D, false, 0), k4 = key(ZB_KEY_E, false, 0);
  zb_input_handle_event(in, &k2);
  zb_input_handle_event(in, &k3);
  zb_input_handle_event(in, &k4);
  zb_input_sample(in, 0);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, zb_input_state(in, zoom)->x); /* clamped */
  TEST_ASSERT_TRUE(zb_input_state(in, trig)->held);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, -2.0f, zb_input_state(in, move)->x); /* stick reports raw values */
  /* small stick values fall into the dead zone */
  ax.u.gamepad_axis.axis = ZB_AXIS_LEFT_X;
  ax.u.gamepad_axis.value = 0.05f;
  zb_input_handle_event(in, &ax);
  zb_input_sample(in, 0);
  TEST_ASSERT_FALSE(zb_input_state(in, move)->held);
  zb_event btn;
  memset(&btn, 0, sizeof btn);
  btn.type = ZB_EV_GAMEPAD_BUTTON;
  btn.u.gamepad_button.id = 7;
  btn.u.gamepad_button.button = ZB_PAD_A;
  btn.u.gamepad_button.down = true;
  zb_input_handle_event(in, &btn);
  btn.u.gamepad_button.button = 200;
  zb_input_handle_event(in, &btn);
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, fire)->pressed);
}

/* spec: input-actions: Unplug while holding */
static void test_unplug_while_holding(void) {
  int fire = zb_input_define(in, "fire", ZB_ACTION_BUTTON);
  zb_input_bind(in, fire, "gamepad:a");
  zb_event btn;
  memset(&btn, 0, sizeof btn);
  btn.type = ZB_EV_GAMEPAD_BUTTON;
  btn.u.gamepad_button.id = 3; /* button before ADDED creates the pad */
  btn.u.gamepad_button.button = ZB_PAD_A;
  btn.u.gamepad_button.down = true;
  zb_input_handle_event(in, &btn);
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, fire)->held);
  zb_event removed;
  memset(&removed, 0, sizeof removed);
  removed.type = ZB_EV_GAMEPAD_REMOVED;
  removed.u.gamepad.id = 3;
  zb_input_handle_event(in, &removed);
  zb_input_handle_event(in, &removed); /* unknown pad: ignored */
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, fire)->released);
  TEST_ASSERT_FALSE(zb_input_state(in, fire)->held);
  /* more than 8 pads */
  for (uint32_t id = 10; id < 20; ++id) {
    btn.u.gamepad_button.id = id;
    zb_input_handle_event(in, &btn);
  }
}

/* spec: input-actions: Mod changes a default binding */
static void test_defaults_overlay(void) {
  zb_luon_doc base, mod;
  zb_value b = parse(&base,
                     "{ jump = { type = 'button', bind = { 'key:space', 'gamepad:a' } },"
                     "  move = { type = 'axis2', bind = { 'keys:w,s,a,d' } } }");
  zb_value m = parse(&mod, "{ jump = { type = 'button', bind = { 'key:j' } } }");
  char err[200];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_apply_defaults(in, &b, err, sizeof err));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_apply_defaults(in, &m, err, sizeof err));
  int jump = zb_input_find(in, "jump");
  TEST_ASSERT_EQUAL_size_t(1, zb_input_binding_count(in, jump));
  TEST_ASSERT_EQUAL_STRING("key:j", zb_input_binding(in, jump, 0));
  zb_luon_free(&base);
  zb_luon_free(&mod);
  const char *bad[] = {"{ 'positional' }", "{ x = { type = 'lever' } }", "{ jump = { type = 'axis1' } }",
                       "{ y = { type = 'button', bind = { 'key:nope' } } }"};
  for (size_t i = 0; i < ZB_ARRAY_LEN(bad); ++i) {
    zb_luon_doc d;
    zb_value v = parse(&d, bad[i]);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZB_ERR_INVALID, zb_input_apply_defaults(in, &v, err, sizeof err), bad[i]);
    zb_luon_free(&d);
  }
}

/* spec: input-actions: Rebinding survives restart */
/* spec: input-actions: Stale configuration entry */
static void test_user_overrides(void) {
  int jump = zb_input_define(in, "jump", ZB_ACTION_BUTTON);
  zb_input_bind(in, jump, "key:space");
  const char *mine[] = {"key:j", "gamepad:b"};
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_set_user_binding(in, jump, mine, 2));
  const char *bad[] = {"key:nope"};
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_set_user_binding(in, jump, bad, 1));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_set_user_binding(in, 99, mine, 1));
  char buf[512];
  size_t n = zb_input_write_user(in, buf, sizeof buf);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "[\"jump\"] = { \"key:j\", \"gamepad:b\" }"));
  /* "restart": a fresh input system reads the saved table */
  zb_input *again = zb_input_create(zb_test_alloc());
  int j2 = zb_input_define(again, "jump", ZB_ACTION_BUTTON);
  zb_input_bind(again, j2, "key:space");
  zb_luon_doc doc;
  zb_value saved = parse(&doc, buf);
  TEST_ASSERT_EQUAL_size_t(0, zb_input_apply_user(again, &saved));
  TEST_ASSERT_EQUAL_STRING("key:j", zb_input_binding(again, j2, 0));
  /* package defaults applied later do not undo user overrides */
  zb_luon_doc d2;
  zb_value defs = parse(&d2, "{ jump = { type = 'button', bind = { 'key:k' } } }");
  char err[100];
  zb_input_apply_defaults(again, &defs, err, sizeof err);
  TEST_ASSERT_EQUAL_STRING("key:j", zb_input_binding(again, j2, 0));
  zb_luon_free(&d2);
  zb_luon_free(&doc);
  zb_luon_doc stale;
  zb_value st = parse(&stale, "{ ghost = { 'key:g' }, jump = { 'key:nope' }, [1] = { 'key:x' }, jump2 = 'x' }");
  zb_input_define(again, "jump2", ZB_ACTION_BUTTON);
  TEST_ASSERT_EQUAL_size_t(4, zb_input_apply_user(again, &st));
  TEST_ASSERT_EQUAL_STRING("key:j", zb_input_binding(again, j2, 0));
  zb_luon_free(&stale);
  zb_input_destroy(again);
  zb_input_write_user(in, buf, 4); /* truncation is safe */
}

/* spec: input-actions: Keyboard on a tablet */
/* spec: input-actions: Synthesised mouse events */
static void test_touch_mode_switching(void) {
  TEST_ASSERT_EQUAL_INT(ZB_INPUT_DESKTOP, zb_input_get_mode(in));
  zb_input_set_mode(in, ZB_INPUT_TOUCH);
  TEST_ASSERT_TRUE(zb_input_take_mode_change(in));
  TEST_ASSERT_FALSE(zb_input_take_mode_change(in));
  zb_event td = touch(ZB_EV_TOUCH_DOWN, 1, 0.5f, 0.5f, 1000000000ull);
  zb_input_handle_event(in, &td);
  zb_event synth = mouse_button(ZB_MOUSE_LEFT, true, true, 1000000000ull);
  zb_input_handle_event(in, &synth);
  TEST_ASSERT_EQUAL_INT(ZB_INPUT_TOUCH, zb_input_get_mode(in));
  zb_event early_key = key(ZB_KEY_A, true, 1100000000ull); /* within the grace window */
  zb_input_handle_event(in, &early_key);
  TEST_ASSERT_EQUAL_INT(ZB_INPUT_TOUCH, zb_input_get_mode(in));
  zb_event tu = touch(ZB_EV_TOUCH_UP, 1, 0.5f, 0.5f, 1200000000ull);
  zb_input_handle_event(in, &tu);
  zb_event late_key = key(ZB_KEY_B, true, 2000000000ull);
  zb_input_handle_event(in, &late_key);
  TEST_ASSERT_EQUAL_INT(ZB_INPUT_DESKTOP, zb_input_get_mode(in));
  TEST_ASSERT_TRUE(zb_input_take_mode_change(in));
  zb_event td2 = touch(ZB_EV_TOUCH_DOWN, 2, 0.1f, 0.1f, 3000000000ull);
  zb_input_handle_event(in, &td2);
  TEST_ASSERT_EQUAL_INT(ZB_INPUT_TOUCH, zb_input_get_mode(in));
  zb_event mouse = mouse_button(ZB_MOUSE_LEFT, true, false, 4000000000ull);
  zb_input_handle_event(in, &mouse);
  TEST_ASSERT_EQUAL_INT(ZB_INPUT_DESKTOP, zb_input_get_mode(in));
  zb_event still;
  memset(&still, 0, sizeof still);
  still.type = ZB_EV_MOUSE_MOTION;
  zb_input_handle_event(in, &still); /* zero motion does not switch */
  zb_event unknown_touch = touch(ZB_EV_TOUCH_MOTION, 99, 0, 0, 0);
  zb_input_handle_event(in, &unknown_touch);
  zb_event other;
  memset(&other, 0, sizeof other);
  other.type = ZB_EV_QUIT;
  zb_input_handle_event(in, &other);
}

/* spec: input-actions: Virtual stick drives an axis */
static void test_virtual_controls(void) {
  int move = zb_input_define(in, "move", ZB_ACTION_AXIS2);
  int fire = zb_input_define(in, "fire", ZB_ACTION_BUTTON);
  zb_input_bind(in, move, "touch:stick:move");
  zb_input_bind(in, fire, "touch:button:fire");
  zb_input_bind(in, fire, "touch:button:ghost");
  TEST_ASSERT_EQUAL_INT(0, zb_input_add_stick(in, "move", 0.2f, 0.7f, 0.1f));
  TEST_ASSERT_EQUAL_INT(1, zb_input_add_button(in, "fire", 0.7f, 0.6f, 0.2f, 0.2f));
  TEST_ASSERT_EQUAL_INT(-1, zb_input_add_button(in, "fire", 0, 0, 1, 1));
  TEST_ASSERT_EQUAL_INT(-1, zb_input_add_stick(in, "bad", 0, 0, 0));
  zb_event d = touch(ZB_EV_TOUCH_DOWN, 1, 0.2f, 0.7f, 0);
  zb_input_handle_event(in, &d);
  zb_event m = touch(ZB_EV_TOUCH_MOTION, 1, 0.5f, 0.7f, 0); /* dragged far right */
  zb_input_handle_event(in, &m);
  zb_event f = touch(ZB_EV_TOUCH_DOWN, 2, 0.8f, 0.7f, 0);
  zb_input_handle_event(in, &f);
  zb_event outside = touch(ZB_EV_TOUCH_DOWN, 3, 0.5f, 0.1f, 0);
  zb_input_handle_event(in, &outside);
  zb_input_sample(in, 0);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, zb_input_state(in, move)->x);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, zb_input_state(in, move)->y);
  TEST_ASSERT_TRUE(zb_input_state(in, fire)->pressed);
  /* the touch stays owned by the stick even when it leaves the circle */
  zb_event m2 = touch(ZB_EV_TOUCH_MOTION, 1, 0.2f, 0.65f, 0);
  zb_input_handle_event(in, &m2);
  zb_input_sample(in, 0);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, zb_input_state(in, move)->y);
  zb_event u = touch(ZB_EV_TOUCH_UP, 1, 0.2f, 0.7f, 0);
  zb_input_handle_event(in, &u);
  zb_event u2 = touch(ZB_EV_TOUCH_UP, 2, 0.8f, 0.7f, 0);
  zb_input_handle_event(in, &u2);
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, move)->released);
  TEST_ASSERT_TRUE(zb_input_state(in, fire)->released);
  for (uint64_t i = 10; i < 25; ++i) { /* more fingers than slots */
    zb_event e = touch(ZB_EV_TOUCH_DOWN, i, 0.0f, 0.0f, 0);
    zb_input_handle_event(in, &e);
  }
  char name[16];
  for (int i = 2; i < ZB_INPUT_MAX_CONTROLS; ++i) {
    snprintf(name, sizeof name, "c%d", i);
    zb_input_add_button(in, name, 0, 0, 0.01f, 0.01f);
  }
  TEST_ASSERT_EQUAL_INT(-1, zb_input_add_button(in, "full", 0, 0, 1, 1));
}

static void test_inject(void) {
  int fire = zb_input_define(in, "fire", ZB_ACTION_BUTTON);
  zb_input_inject(in, fire, 1.0f, 0.0f, 2);
  zb_input_inject(in, 99, 1.0f, 0.0f, 2);
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, fire)->pressed);
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, fire)->held);
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, fire)->released);
  TEST_ASSERT_EQUAL_UINT8(0, zb_input_player(in));
}

/* spec: input-actions: Replay reproduces actions */
static void test_record_and_replay(void) {
  int fire = zb_input_define(in, "fire", ZB_ACTION_BUTTON);
  int move = zb_input_define(in, "move", ZB_ACTION_AXIS2);
  zb_input_recorder *rec = zb_input_recorder_create(zb_test_alloc(), in, 42, 99);
  zb_action_state seen[30][2];
  for (int t = 0; t < 30; ++t) {
    if (t == 5)
      zb_input_inject(in, fire, 1, 0, 3);
    if (t == 10)
      zb_input_inject(in, move, 0.5f, -0.25f, 10);
    zb_input_sample(in, 0);
    zb_input_recorder_add(rec, in);
    seen[t][0] = *zb_input_state(in, fire);
    seen[t][1] = *zb_input_state(in, move);
  }
  size_t len = 0;
  const unsigned char *bytes = zb_input_recorder_bytes(rec, &len);
  TEST_ASSERT_TRUE(len > 0);
  zb_input *other = zb_input_create(zb_test_alloc());
  int m2 = zb_input_define(other, "move", ZB_ACTION_AXIS2); /* different order */
  int f2 = zb_input_define(other, "fire", ZB_ACTION_BUTTON);
  char err[100];
  zb_input_replay *rp = zb_input_replay_open(zb_test_alloc(), other, bytes, len, err, sizeof err);
  TEST_ASSERT_NOT_NULL(rp);
  TEST_ASSERT_EQUAL_UINT64(42, zb_input_replay_seed(rp));
  TEST_ASSERT_EQUAL_UINT64(99, zb_input_replay_package_hash(rp));
  TEST_ASSERT_EQUAL_UINT64(30, zb_input_replay_tick_count(rp));
  for (int t = 0; t < 30; ++t) {
    TEST_ASSERT_TRUE(zb_input_replay_next(rp, other));
    TEST_ASSERT_EQUAL_MEMORY(&seen[t][0], zb_input_state(other, f2), sizeof(zb_action_state));
    TEST_ASSERT_EQUAL_MEMORY(&seen[t][1], zb_input_state(other, m2), sizeof(zb_action_state));
  }
  TEST_ASSERT_FALSE(zb_input_replay_next(rp, other));
  zb_input_replay_close(rp);
  /* corrupt inputs */
  TEST_ASSERT_NULL(zb_input_replay_open(zb_test_alloc(), other, (const unsigned char *)"garbage!", 8, err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "not an input recording"));
  TEST_ASSERT_NULL(zb_input_replay_open(zb_test_alloc(), other, bytes, 12, err, sizeof err));
  TEST_ASSERT_NULL(zb_input_replay_open(zb_test_alloc(), other, bytes, 32, err, sizeof err));
  unsigned char *cut = zb_alloc(zb_test_alloc(), len);
  memcpy(cut, bytes, len);
  zb_input_replay *partial = zb_input_replay_open(zb_test_alloc(), other, cut, len - 3, err, sizeof err);
  TEST_ASSERT_NOT_NULL(partial);
  while (zb_input_replay_next(partial, other)) {
  }
  zb_input_replay_close(partial);
  zb_free(zb_test_alloc(), cut);
  zb_input_replay_close(NULL);
  zb_input_destroy(other);
  zb_input_recorder_destroy(rec);
  zb_input_recorder_destroy(NULL);
  zb_failing_allocator fa;
  TEST_ASSERT_NULL(zb_input_recorder_create(zb_failing_init(&fa, zb_test_alloc(), 1), in, 0, 0));
  TEST_ASSERT_NULL(zb_input_recorder_create(zb_failing_init(&fa, zb_test_alloc(), 0), in, 0, 0));
  TEST_ASSERT_NULL(zb_input_replay_open(zb_failing_init(&fa, zb_test_alloc(), 0), in, bytes, len, err, sizeof err));
  zb_input_recorder *r2 = zb_input_recorder_create(zb_failing_init(&fa, zb_test_alloc(), 3), in, 0, 0);
  if (r2) {
    size_t l2;
    zb_input_recorder_bytes(r2, &l2);
    zb_input_recorder_destroy(r2);
  }
}

static zb_event xr_button(zb_xr_hand hand, zb_xr_button button, bool down) {
  zb_event e;
  memset(&e, 0, sizeof e);
  e.type = ZB_EV_XR_BUTTON;
  e.u.xr_button.hand = (uint8_t)hand;
  e.u.xr_button.button = (uint8_t)button;
  e.u.xr_button.down = down;
  return e;
}

/* spec: webxr-session: Select presses an action */
static void test_xr_buttons(void) {
  int any = zb_input_define(in, "any", ZB_ACTION_BUTTON);
  int right = zb_input_define(in, "right", ZB_ACTION_BUTTON);
  int left = zb_input_define(in, "left", ZB_ACTION_BUTTON);
  int grab = zb_input_define(in, "grab", ZB_ACTION_AXIS1);
  int left_grab = zb_input_define(in, "left_grab", ZB_ACTION_BUTTON);
  int aim = zb_input_define(in, "aim", ZB_ACTION_AXIS2);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_bind(in, any, "xr:select"));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_bind(in, right, "xr:right_select"));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_bind(in, left, "xr:left_select"));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_bind(in, grab, "xr:squeeze"));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_bind(in, left_grab, "xr:left_squeeze"));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_input_bind(in, left_grab, "xr:right_squeeze"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, any, "xr:grab"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, any, "xr:left_"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, any, "xr:"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_input_bind(in, aim, "xr:select"));
  TEST_ASSERT_EQUAL_STRING("xr:right_select", zb_input_binding(in, right, 0));

  /* The right controller's select starts and ends between two ticks. */
  zb_event ev = xr_button(ZB_XR_HAND_RIGHT, ZB_XR_SELECT, true);
  zb_input_handle_event(in, &ev);
  ev = xr_button(ZB_XR_HAND_RIGHT, ZB_XR_SELECT, false);
  zb_input_handle_event(in, &ev);
  zb_input_sample(in, 0);
  TEST_ASSERT_TRUE(zb_input_state(in, any)->pressed);
  TEST_ASSERT_TRUE(zb_input_state(in, right)->pressed);
  TEST_ASSERT_FALSE(zb_input_state(in, left)->pressed);
  TEST_ASSERT_FALSE(zb_input_state(in, grab)->held);
  zb_input_sample(in, 1);
  TEST_ASSERT_TRUE(zb_input_state(in, any)->released);
  TEST_ASSERT_FALSE(zb_input_state(in, right)->held);

  /* A squeeze held by a source without a hand: "xr:squeeze" only. */
  ev = xr_button(ZB_XR_HAND_NONE, ZB_XR_SQUEEZE, true);
  zb_input_handle_event(in, &ev);
  zb_input_sample(in, 2);
  zb_input_sample(in, 3);
  TEST_ASSERT_TRUE(zb_input_state(in, grab)->held);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, zb_input_state(in, grab)->x);
  TEST_ASSERT_FALSE(zb_input_state(in, left_grab)->held);
  ev = xr_button(ZB_XR_HAND_RIGHT, ZB_XR_SQUEEZE, true);
  zb_input_handle_event(in, &ev);
  zb_input_sample(in, 4);
  TEST_ASSERT_TRUE(zb_input_state(in, left_grab)->pressed);
  /* Out-of-range hands and buttons are ignored. */
  ev = xr_button(ZB_XR_HAND_COUNT, ZB_XR_SELECT, true);
  zb_input_handle_event(in, &ev);
  ev = xr_button(ZB_XR_HAND_LEFT, ZB_XR_BUTTON_COUNT, true);
  zb_input_handle_event(in, &ev);
  zb_input_sample(in, 5);
  TEST_ASSERT_FALSE(zb_input_state(in, any)->held);
  TEST_ASSERT_FALSE(zb_input_state(in, left)->held);
}

static void test_oom_create(void) {
  zb_failing_allocator fa;
  TEST_ASSERT_NULL(zb_input_create(zb_failing_init(&fa, zb_test_alloc(), 0)));
  zb_input_destroy(NULL);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_button_held_across_ticks);
  RUN_TEST(test_tap_between_ticks_is_seen);
  RUN_TEST(test_define_and_bind_rules);
  RUN_TEST(test_axes_from_keys_mouse_pad);
  RUN_TEST(test_unplug_while_holding);
  RUN_TEST(test_defaults_overlay);
  RUN_TEST(test_user_overrides);
  RUN_TEST(test_touch_mode_switching);
  RUN_TEST(test_virtual_controls);
  RUN_TEST(test_inject);
  RUN_TEST(test_record_and_replay);
  RUN_TEST(test_xr_buttons);
  RUN_TEST(test_oom_create);
  return UNITY_END();
}

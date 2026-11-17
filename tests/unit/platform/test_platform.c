/* SPDX-License-Identifier: MIT */
#include "core/keys.h"
#include "platform/platform.h"
#include "support/test_support.h"
#include "unity.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Runs with SDL_VIDEO_DRIVER=offscreen, SDL_AUDIO_DRIVER=dummy and
 * ZB_NO_DIALOGS=1 (set by CTest). */

static zb_platform *g_p;

void setUp(void) {
  zb_test_begin();
  g_p = zb_platform_create(zb_test_alloc());
  TEST_ASSERT_NOT_NULL(g_p);
}

void tearDown(void) {
  zb_platform_destroy(g_p);
  zb_test_end();
}

static void test_name_and_clock(void) {
#if defined(_WIN32)
  TEST_ASSERT_EQUAL_STRING("windows", zb_platform_name());
#elif defined(__APPLE__)
  TEST_ASSERT_EQUAL_STRING("macos", zb_platform_name());
#else
  TEST_ASSERT_EQUAL_STRING("linux", zb_platform_name());
#endif
  TEST_ASSERT_TRUE(zb_platform_cpu_count(g_p) >= 1);
  uint64_t a = zb_platform_now_ns(g_p);
  SDL_Delay(2);
  TEST_ASSERT_TRUE(zb_platform_now_ns(g_p) > a);
  const char *base = zb_platform_base_path(g_p);
  TEST_ASSERT_TRUE(strlen(base) > 0);
  char last = base[strlen(base) - 1];
  TEST_ASSERT_TRUE_MESSAGE(last == '/' || last == '\\', base);
  zb_platform_destroy(NULL);
  zb_failing_allocator fa;
  TEST_ASSERT_NULL(zb_platform_create(zb_failing_init(&fa, zb_test_alloc(), 0)));
}

static bool collect(const char *name, bool is_dir, void *user) {
  char *out = user;
  strcat(out, name);
  strcat(out, is_dir ? "/," : ",");
  return strcmp(name, "stop") != 0;
}

static void test_file_system(void) {
  const zb_fs_api *fs = zb_platform_fs(g_p);
  char dir[512], path[600];
  snprintf(dir, sizeof dir, "%s/fs", zb_test_tmp_dir());
  zb_test_remove_tree(dir);
  snprintf(path, sizeof path, "%s/nested/deeper/file.txt", dir);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->write(fs->ctx, path, "content", 7));
  char *data;
  size_t len;
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->read(fs->ctx, path, zb_test_alloc(), &data, &len));
  TEST_ASSERT_EQUAL_size_t(7, len);
  TEST_ASSERT_EQUAL_STRING("content", data);
  zb_free(zb_test_alloc(), data);
  zb_failing_allocator fa;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, fs->read(fs->ctx, path, zb_failing_init(&fa, zb_test_alloc(), 0), &data, &len));
  zb_fs_stat st;
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->stat(fs->ctx, path, &st));
  TEST_ASSERT_FALSE(st.is_dir);
  TEST_ASSERT_EQUAL_UINT64(7, st.size);
  TEST_ASSERT_TRUE(st.mtime_ns > 0);
  char sub[600];
  snprintf(sub, sizeof sub, "%s/nested", dir);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->stat(fs->ctx, sub, &st));
  TEST_ASSERT_TRUE(st.is_dir);
  char missing[600];
  snprintf(missing, sizeof missing, "%s/missing.txt", dir);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->read(fs->ctx, missing, zb_test_alloc(), &data, &len));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_IO, fs->read(fs->ctx, sub, zb_test_alloc(), &data, &len)); /* a directory */
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->stat(fs->ctx, missing, &st));
  snprintf(path, sizeof path, "%s/nested/stop", dir);
  fs->write(fs->ctx, path, "", 0);
  snprintf(path, sizeof path, "%s/nested/zzz", dir);
  fs->write(fs->ctx, path, "", 0);
  char names[256] = "";
  char listdir[600];
  snprintf(listdir, sizeof listdir, "%s/nested/", dir);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->list(fs->ctx, listdir, collect, names));
  TEST_ASSERT_NOT_NULL(strstr(names, "stop,"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->list(fs->ctx, missing, collect, names));
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->remove(fs->ctx, path));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->remove(fs->ctx, path));
  snprintf(sub, sizeof sub, "%s/made/here", dir);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->mkdirs(fs->ctx, sub));
  /* writing below a file fails */
  snprintf(path, sizeof path, "%s/nested/stop/inner.txt", dir);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_IO, fs->write(fs->ctx, path, "x", 1));
  /* renaming onto a directory fails and the temporary file is removed */
  snprintf(path, sizeof path, "%s/made", dir);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_IO, fs->write(fs->ctx, path, "x", 1));
  zb_test_remove_tree(dir);
}

typedef struct shared {
  const zb_thread_api *t;
  void *mutex;
  void *cond;
  int value;
} shared;

static int worker(void *arg) {
  shared *s = arg;
  s->t->mutex_lock(s->mutex);
  s->value = 42;
  s->t->cond_broadcast(s->cond);
  s->t->mutex_unlock(s->mutex);
  return 0;
}

static void test_threads(void) {
  const zb_thread_api *t = zb_platform_threads(g_p);
  TEST_ASSERT_NOT_NULL(t);
  shared s = {t, t->mutex_create(), t->cond_create(), 0};
  t->mutex_lock(s.mutex);
  void *th = t->thread_create(worker, &s, "worker");
  while (s.value == 0) {
    t->cond_wait(s.cond, s.mutex);
  }
  t->mutex_unlock(s.mutex);
  t->thread_join(th);
  TEST_ASSERT_EQUAL_INT(42, s.value);
  t->cond_destroy(s.cond);
  t->mutex_destroy(s.mutex);
}

static void test_user_dir(void) {
  char home[600];
  snprintf(home, sizeof home, "%s/xdg", zb_test_tmp_dir());
  /* SDL keeps its own copy of the environment. */
  SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "XDG_DATA_HOME", home, true);
  char out[1024];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_platform_user_dir(g_p, out, sizeof out));
#if defined(_WIN32)
  TEST_ASSERT_NOT_NULL_MESSAGE(strstr(out, "\\Zauberer\\"), out); /* %APPDATA%\Zauberer\ */
#elif defined(__APPLE__)
  TEST_ASSERT_NOT_NULL_MESSAGE(strstr(out, "Application Support/Zauberer/"), out);
#else
  TEST_ASSERT_NOT_NULL_MESSAGE(strstr(out, "xdg/zauberer/"), out);
#endif
  zb_fs_stat st;
  const zb_fs_api *fs = zb_platform_fs(g_p);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->stat(fs->ctx, out, &st));
  TEST_ASSERT_TRUE(st.is_dir);
  zb_platform_user_data_sync(g_p);
  SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "XDG_DATA_HOME");
}

static void test_video_failure(void) {
  SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "no-such-driver", SDL_HINT_OVERRIDE);
  zb_window_desc desc = {"test", 320, 200, false};
  char err[256];
  TEST_ASSERT_EQUAL_INT(ZB_ERR_UNSUPPORTED, zb_platform_open_window(g_p, &desc, err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "cannot initialise video"));
  TEST_ASSERT_FALSE(zb_platform_has_window(g_p));
  int w, h;
  zb_platform_drawable_size(g_p, &w, &h);
  TEST_ASSERT_EQUAL_INT(0, w);
  zb_platform_swap(g_p);
  zb_platform_set_fullscreen(g_p, true);
  zb_platform_set_window_size(g_p, 100, 100);
  TEST_ASSERT_FALSE(zb_platform_is_fullscreen(g_p));
  SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
}

/* spec: platform-targets: Resize at runtime */
static void test_window(void) {
  zb_window_desc desc = {"Zauberer test", 640, 360, false};
  char err[256] = "";
  zb_result r = zb_platform_open_window(g_p, &desc, err, sizeof err);
  if (r != ZB_OK && !getenv("ZB_REQUIRE_GL")) {
    TEST_IGNORE_MESSAGE(err); /* CI machines without OpenGL 3.3 (Linux CI sets ZB_REQUIRE_GL) */
  }
  TEST_ASSERT_EQUAL_INT_MESSAGE(ZB_OK, r, err);
  TEST_ASSERT_TRUE(zb_platform_has_window(g_p));
  TEST_ASSERT_FALSE(zb_platform_is_gles(g_p));
  int w, h;
  zb_platform_drawable_size(g_p, &w, &h);
  TEST_ASSERT_EQUAL_INT(640, w);
  TEST_ASSERT_EQUAL_INT(360, h);
  TEST_ASSERT_NOT_NULL(zb_platform_gl_proc(g_p, "glGetString"));
  zb_platform_swap(g_p);
  zb_platform_set_fullscreen(g_p, true);
  zb_platform_set_fullscreen(g_p, false);
  zb_platform_set_window_size(g_p, 800, 600);
  bool resized = false;
  SDL_Event rev;
  while (SDL_PollEvent(&rev)) {
    zb_event out;
    if (zb_platform_translate_event(g_p, &rev, &out) && out.type == ZB_EV_RESIZE && out.u.resize.width == 800) {
      resized = true;
    }
  }
  TEST_ASSERT_TRUE(resized);
  zb_platform_drawable_size(g_p, &w, &h);
  TEST_ASSERT_EQUAL_INT(600, h);
  TEST_ASSERT_FALSE(zb_platform_is_fullscreen(g_p));
  zb_platform_request_quit(g_p);
  SDL_Event ev;
  bool quit = false;
  while (SDL_PollEvent(&ev)) {
    zb_event out;
    if (zb_platform_translate_event(g_p, &ev, &out) && out.type == ZB_EV_QUIT) {
      quit = true;
    }
  }
  TEST_ASSERT_TRUE(quit);
}

static zb_event translate(const SDL_Event *e, bool expect) {
  zb_event out;
  TEST_ASSERT_EQUAL(expect, zb_platform_translate_event(g_p, e, &out));
  return out;
}

static void test_event_translation(void) {
  SDL_Event e;
  SDL_zero(e);
  e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
  TEST_ASSERT_EQUAL_INT(ZB_EV_QUIT, translate(&e, true).type);
  e.type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
  e.window.data1 = 1920;
  e.window.data2 = 1080;
  zb_event z = translate(&e, true);
  TEST_ASSERT_EQUAL_INT(ZB_EV_RESIZE, z.type);
  TEST_ASSERT_EQUAL_INT(1080, z.u.resize.height);
  SDL_zero(e);
  e.type = SDL_EVENT_KEY_DOWN;
  e.key.scancode = SDL_SCANCODE_SPACE;
  e.key.repeat = true;
  z = translate(&e, true);
  TEST_ASSERT_EQUAL_INT(ZB_KEY_SPACE, z.u.key.key);
  TEST_ASSERT_TRUE(z.u.key.down);
  TEST_ASSERT_TRUE(z.u.key.repeat);
  e.type = SDL_EVENT_KEY_UP;
  e.key.scancode = SDL_SCANCODE_F11;
  z = translate(&e, true);
  TEST_ASSERT_EQUAL_INT(ZB_KEY_F11, z.u.key.key);
  TEST_ASSERT_FALSE(z.u.key.down);
  e.key.scancode = SDL_SCANCODE_KP_MEMCLEAR; /* not mapped */
  translate(&e, false);
  SDL_zero(e);
  e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  e.button.button = SDL_BUTTON_RIGHT;
  e.button.x = 10;
  e.button.y = 20;
  z = translate(&e, true);
  TEST_ASSERT_EQUAL_INT(ZB_MOUSE_RIGHT, z.u.mouse_button.button);
  TEST_ASSERT_FALSE(z.u.mouse_button.synthetic);
  e.type = SDL_EVENT_MOUSE_BUTTON_UP;
  e.button.which = SDL_TOUCH_MOUSEID;
  z = translate(&e, true);
  TEST_ASSERT_TRUE(z.u.mouse_button.synthetic);
  TEST_ASSERT_FALSE(z.u.mouse_button.down);
  SDL_zero(e);
  e.type = SDL_EVENT_MOUSE_MOTION;
  e.motion.xrel = 3;
  e.motion.yrel = -4;
  e.motion.which = SDL_PEN_MOUSEID;
  z = translate(&e, true);
  TEST_ASSERT_EQUAL_FLOAT(-4.0f, z.u.mouse_motion.dy);
  TEST_ASSERT_TRUE(z.u.mouse_motion.synthetic);
  SDL_zero(e);
  e.type = SDL_EVENT_MOUSE_WHEEL;
  e.wheel.y = 1;
  TEST_ASSERT_EQUAL_FLOAT(1.0f, translate(&e, true).u.wheel.dy);
  SDL_zero(e);
  e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
  e.gbutton.which = 5;
  e.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
  z = translate(&e, true);
  TEST_ASSERT_EQUAL_INT(ZB_PAD_A, z.u.gamepad_button.button);
  TEST_ASSERT_TRUE(z.u.gamepad_button.down);
  e.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
  e.gbutton.button = 200;
  translate(&e, false);
  SDL_zero(e);
  e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
  e.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
  e.gaxis.value = -32768;
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, translate(&e, true).u.gamepad_axis.value);
  e.gaxis.axis = 100;
  translate(&e, false);
  SDL_zero(e);
  e.type = SDL_EVENT_FINGER_DOWN;
  e.tfinger.fingerID = 9;
  e.tfinger.x = 0.25f;
  z = translate(&e, true);
  TEST_ASSERT_EQUAL_INT(ZB_EV_TOUCH_DOWN, z.type);
  TEST_ASSERT_EQUAL_UINT64(9, z.u.touch.finger);
  e.type = SDL_EVENT_FINGER_MOTION;
  TEST_ASSERT_EQUAL_INT(ZB_EV_TOUCH_MOTION, translate(&e, true).type);
  e.type = SDL_EVENT_FINGER_UP;
  TEST_ASSERT_EQUAL_INT(ZB_EV_TOUCH_UP, translate(&e, true).type);
  SDL_zero(e);
  e.type = SDL_EVENT_CLIPBOARD_UPDATE;
  translate(&e, false);
}

static void test_virtual_gamepad(void) {
  TEST_ASSERT_TRUE(SDL_InitSubSystem(SDL_INIT_GAMEPAD));
  SDL_VirtualJoystickDesc desc;
  SDL_INIT_INTERFACE(&desc);
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
  desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
  desc.name = "Zauberer test pad";
  SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
  TEST_ASSERT_NOT_EQUAL(0, id);
  SDL_Event e;
  SDL_zero(e);
  e.type = SDL_EVENT_GAMEPAD_ADDED;
  e.gdevice.which = id;
  zb_event z = translate(&e, true);
  TEST_ASSERT_EQUAL_INT(ZB_EV_GAMEPAD_ADDED, z.type);
  TEST_ASSERT_EQUAL_UINT32(id, z.u.gamepad.id);
  e.type = SDL_EVENT_GAMEPAD_REMOVED;
  TEST_ASSERT_EQUAL_INT(ZB_EV_GAMEPAD_REMOVED, translate(&e, true).type);
  /* added again and left open: destroy closes it */
  e.type = SDL_EVENT_GAMEPAD_ADDED;
  translate(&e, true);
  /* more pads than slots are ignored */
  for (int i = 0; i < 9; ++i) {
    translate(&e, true);
  }
  zb_platform_destroy(g_p);
  g_p = zb_platform_create(zb_test_alloc());
  SDL_DetachVirtualJoystick(id);
  SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

static void no_fetch(void *user, zb_result r) {
  (void)user;
  (void)r;
  TEST_FAIL_MESSAGE("never called on the desktop");
}

static void no_state(void *user, char *out, size_t len) {
  (void)user;
  (void)out;
  (void)len;
}

static void test_web_bridge_on_desktop(void) {
  TEST_ASSERT_FALSE(zb_platform_has_fetch(g_p));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_UNSUPPORTED, zb_platform_fetch(g_p, "assets/base/x.png", no_fetch, NULL));
  zb_platform_set_state_provider(g_p, no_state, NULL);
  TEST_ASSERT_TRUE(SDL_InitSubSystem(SDL_INIT_EVENTS));
  zb_platform_push_audio_unlock();
  SDL_Event e;
  bool unlocked = false;
  while (SDL_PollEvent(&e)) {
    zb_event out;
    if (zb_platform_translate_event(g_p, &e, &out) && out.type == ZB_EV_AUDIO_UNLOCKED) {
      unlocked = true;
    }
  }
  TEST_ASSERT_TRUE(unlocked);
  SDL_zero(e);
  e.type = SDL_EVENT_USER;
  e.user.code = 1;
  zb_event out;
  TEST_ASSERT_FALSE(zb_platform_translate_event(g_p, &e, &out));
  SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

static void test_messages_and_touch(void) {
  zb_platform_show_error(g_p, "Test", "this is shown on stderr only (ZB_NO_DIALOGS)");
  zb_test_setenv("ZB_TOUCH_PRIMARY", NULL);
  TEST_ASSERT_FALSE(zb_platform_touch_primary(g_p));
  zb_test_setenv("ZB_TOUCH_PRIMARY", "1");
  TEST_ASSERT_TRUE(zb_platform_touch_primary(g_p));
  zb_test_setenv("ZB_TOUCH_PRIMARY", NULL);
}

static SDL_AtomicInt g_fills;

static void fill(void *user, float *out, int frames) {
  (void)user;
  memset(out, 0, (size_t)frames * 2 * sizeof(float));
  SDL_AddAtomicInt(&g_fills, 1);
}

static void test_audio_device(void) {
  int rate = 0;
  SDL_SetAtomicInt(&g_fills, 0);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_platform_audio_open(g_p, fill, NULL, &rate));
  TEST_ASSERT_EQUAL_INT(48000, rate);
  TEST_ASSERT_TRUE(zb_platform_audio_is_open(g_p));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, zb_platform_audio_open(g_p, fill, NULL, &rate));
  for (int i = 0; i < 200 && SDL_GetAtomicInt(&g_fills) == 0; ++i) {
    SDL_Delay(5);
  }
  TEST_ASSERT_TRUE(SDL_GetAtomicInt(&g_fills) > 0);
  zb_platform_audio_close(g_p);
  TEST_ASSERT_FALSE(zb_platform_audio_is_open(g_p));
  zb_platform_audio_close(g_p);
  SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "no-such-driver", SDL_HINT_OVERRIDE);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_UNSUPPORTED, zb_platform_audio_open(g_p, fill, NULL, &rate));
  SDL_ResetHint(SDL_HINT_AUDIO_DRIVER);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_name_and_clock);
  RUN_TEST(test_file_system);
  RUN_TEST(test_threads);
  RUN_TEST(test_user_dir);
  RUN_TEST(test_video_failure);
  RUN_TEST(test_window);
  RUN_TEST(test_event_translation);
  RUN_TEST(test_virtual_gamepad);
  RUN_TEST(test_web_bridge_on_desktop);
  RUN_TEST(test_messages_and_touch);
  RUN_TEST(test_audio_device);
  int failures = UNITY_END();
  SDL_Quit();
  return failures;
}

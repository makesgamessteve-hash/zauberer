/* SPDX-License-Identifier: MIT */
#include "platform/platform.h"
#include "support/test_support.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool collect(const char *name, bool is_dir, void *user) {
  char *out = user;
  strcat(out, name);
  strcat(out, is_dir ? "/," : ",");
  return strcmp(name, "stop") != 0;
}

static void test_basics(void) {
#ifdef __EMSCRIPTEN__
  TEST_ASSERT_EQUAL_STRING("web-node", zb_platform_name());
#else
  TEST_ASSERT_EQUAL_STRING("headless", zb_platform_name());
#endif
  TEST_ASSERT_NULL(zb_platform_threads(g_p));
  TEST_ASSERT_EQUAL_UINT(1, zb_platform_cpu_count(g_p));
  uint64_t t = zb_platform_now_ns(g_p);
  TEST_ASSERT_TRUE(zb_platform_now_ns(g_p) >= t);
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  const char *base = zb_platform_base_path(g_p);
  TEST_ASSERT_NOT_NULL(strstr(base, "/"));
  TEST_ASSERT_EQUAL_CHAR('/', base[strlen(base) - 1]);
#else
  TEST_ASSERT_EQUAL_STRING("./", zb_platform_base_path(g_p));
#endif
  zb_window_desc desc = {"x", 10, 10, false};
  char err[128];
  TEST_ASSERT_EQUAL_INT(ZB_ERR_UNSUPPORTED, zb_platform_open_window(g_p, &desc, err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "headless-only"));
  TEST_ASSERT_FALSE(zb_platform_has_window(g_p));
  int w = 5, h = 5;
  zb_platform_drawable_size(g_p, &w, &h);
  TEST_ASSERT_EQUAL_INT(0, w + h);
  zb_platform_swap(g_p);
  zb_platform_set_fullscreen(g_p, true);
  zb_platform_set_window_size(g_p, 640, 480);
  TEST_ASSERT_FALSE(zb_platform_is_fullscreen(g_p));
  TEST_ASSERT_NULL(zb_platform_gl_proc(g_p, "glGetString"));
  TEST_ASSERT_FALSE(zb_platform_is_gles(g_p));
  zb_event ev;
  TEST_ASSERT_FALSE(zb_platform_translate_event(g_p, NULL, &ev));
  TEST_ASSERT_EQUAL_INT(0, zb_platform_rumble(g_p, 0.5f, 100));
  zb_platform_set_vsync(g_p, false);
  zb_platform_push_focus(true);
  zb_platform_request_quit(g_p);
  zb_platform_show_error(g_p, "Test", "headless error output");
  zb_test_setenv("ZB_TOUCH_PRIMARY", NULL);
  TEST_ASSERT_FALSE(zb_platform_touch_primary(g_p));
  zb_test_setenv("ZB_TOUCH_PRIMARY", "1");
  TEST_ASSERT_TRUE(zb_platform_touch_primary(g_p));
  zb_test_setenv("ZB_TOUCH_PRIMARY", NULL);
  int rate;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_UNSUPPORTED, zb_platform_audio_open(g_p, NULL, NULL, &rate));
  zb_platform_audio_close(g_p);
  TEST_ASSERT_FALSE(zb_platform_audio_is_open(g_p));
  zb_platform_user_data_sync(g_p);
  TEST_ASSERT_FALSE(zb_platform_has_fetch(g_p));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_UNSUPPORTED, zb_platform_fetch(g_p, "x", NULL, NULL));
  zb_platform_set_state_provider(g_p, NULL, NULL);
  zb_platform_push_audio_unlock();
  zb_xr_handler xr = {NULL, NULL, NULL};
  zb_platform_set_xr_handler(g_p, &xr);
  zb_platform_set_xr_handler(g_p, NULL);
  zb_platform_destroy(NULL);
  zb_failing_allocator fa;
  TEST_ASSERT_NULL(zb_platform_create(zb_failing_init(&fa, zb_test_alloc(), 0)));
}

static void test_file_system(void) {
  const zb_fs_api *fs = zb_platform_fs(g_p);
  char dir[512], path[600], sub[600], missing[600];
  snprintf(dir, sizeof dir, "%s/hfs", zb_test_tmp_dir());
  zb_test_remove_tree(dir);
  snprintf(path, sizeof path, "%s/a/b/file.txt", dir);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->write(fs->ctx, path, "data", 4));
  char *data;
  size_t len;
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->read(fs->ctx, path, zb_test_alloc(), &data, &len));
  TEST_ASSERT_EQUAL_STRING("data", data);
  zb_free(zb_test_alloc(), data);
  zb_failing_allocator fa;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, fs->read(fs->ctx, path, zb_failing_init(&fa, zb_test_alloc(), 0), &data, &len));
  snprintf(missing, sizeof missing, "%s/none", dir);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->read(fs->ctx, missing, zb_test_alloc(), &data, &len));
  snprintf(sub, sizeof sub, "%s/a", dir);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_IO, fs->read(fs->ctx, sub, zb_test_alloc(), &data, &len)); /* a directory */
  zb_fs_stat st;
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->stat(fs->ctx, path, &st));
  TEST_ASSERT_EQUAL_UINT64(4, st.size);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->stat(fs->ctx, sub, &st));
  TEST_ASSERT_TRUE(st.is_dir);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->stat(fs->ctx, missing, &st));
  snprintf(path, sizeof path, "%s/a/stop", dir);
  fs->write(fs->ctx, path, "", 0);
  char names[256] = "";
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->list(fs->ctx, sub, collect, names));
  TEST_ASSERT_NOT_NULL(strstr(names, "b/,"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->list(fs->ctx, missing, collect, names));
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->remove(fs->ctx, path));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->remove(fs->ctx, path));
  snprintf(sub, sizeof sub, "%s/x/y/z", dir);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->mkdirs(fs->ctx, sub));
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->mkdirs(fs->ctx, sub));
  snprintf(path, sizeof path, "%s/a/b/file.txt/below", dir);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_IO, fs->mkdirs(fs->ctx, path));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_IO, fs->write(fs->ctx, path, "x", 1));
  snprintf(path, sizeof path, "%s/x", dir);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_IO, fs->write(fs->ctx, path, "x", 1)); /* rename onto a directory */
  zb_test_remove_tree(dir);
}

static void test_user_dir(void) {
  char out[1024], home[600], saved_home[1024] = "";
  const char *h = getenv("HOME");
  if (h) {
    snprintf(saved_home, sizeof saved_home, "%s", h);
  }
  snprintf(home, sizeof home, "%s/xdg", zb_test_tmp_dir());
  zb_test_setenv("XDG_DATA_HOME", home);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_platform_user_dir(g_p, out, sizeof out));
  TEST_ASSERT_NOT_NULL(strstr(out, "xdg/zauberer/"));
  zb_test_setenv("XDG_DATA_HOME", NULL);
  zb_test_setenv("HOME", home);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_platform_user_dir(g_p, out, sizeof out));
  TEST_ASSERT_NOT_NULL(strstr(out, "xdg/.local/share/zauberer/"));
  zb_test_setenv("HOME", NULL);
  char cwd_user[600];
  snprintf(cwd_user, sizeof cwd_user, "./user/");
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_platform_user_dir(g_p, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING(cwd_user, out);
  zb_test_remove_tree("./user");
  zb_test_setenv("HOME", h ? saved_home : NULL);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_basics);
  RUN_TEST(test_file_system);
  RUN_TEST(test_user_dir);
  return UNITY_END();
}

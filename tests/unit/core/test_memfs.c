/* SPDX-License-Identifier: MIT */
#include "core/memfs.h"
#include "support/test_support.h"
#include "unity.h"

#include <string.h>

ZB_TEST_LEAK_CHECKED

typedef struct seen {
  char names[8][32];
  bool dirs[8];
  int n;
  int stop;
} seen;

static bool collect(const char *name, bool is_dir, void *user) {
  seen *s = user;
  snprintf(s->names[s->n], 32, "%s", name);
  s->dirs[s->n++] = is_dir;
  return s->stop == 0 || s->n < s->stop;
}

static void test_put_read_stat_list(void) {
  zb_memfs *m = zb_memfs_create(zb_test_alloc());
  const zb_fs_api *fs = zb_memfs_api(m);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_memfs_put(m, "a/b/c.txt", "hello", 5));
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->write(fs->ctx, "a/b/c.txt", "world!", 6)); /* replace */
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->write(fs->ctx, "a/d.txt", "", 0));
  char *data;
  size_t len;
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->read(fs->ctx, "a/b/c.txt", zb_test_alloc(), &data, &len));
  TEST_ASSERT_EQUAL_STRING("world!", data);
  zb_free(zb_test_alloc(), data);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->read(fs->ctx, "a/d.txt", zb_test_alloc(), &data, &len));
  TEST_ASSERT_EQUAL_size_t(0, len);
  zb_free(zb_test_alloc(), data);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->read(fs->ctx, "nope", zb_test_alloc(), &data, &len));
  zb_fs_stat st;
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->stat(fs->ctx, "a/b/c.txt", &st));
  TEST_ASSERT_FALSE(st.is_dir);
  TEST_ASSERT_EQUAL_UINT64(6, st.size);
  zb_memfs_touch(m, "a/b/c.txt", 777);
  zb_memfs_touch(m, "missing", 1);
  fs->stat(fs->ctx, "a/b/c.txt", &st);
  TEST_ASSERT_EQUAL_UINT64(777, st.mtime_ns);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->stat(fs->ctx, "a/b", &st));
  TEST_ASSERT_TRUE(st.is_dir);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->stat(fs->ctx, "zzz", &st));
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->mkdirs(fs->ctx, "empty/dir"));
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->mkdirs(fs->ctx, "empty/dir"));
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->stat(fs->ctx, "empty", &st));
  seen s;
  memset(&s, 0, sizeof s);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->list(fs->ctx, "a", collect, &s));
  TEST_ASSERT_EQUAL_INT(2, s.n);
  TEST_ASSERT_EQUAL_STRING("b", s.names[0]);
  TEST_ASSERT_TRUE(s.dirs[0]);
  TEST_ASSERT_EQUAL_STRING("d.txt", s.names[1]);
  memset(&s, 0, sizeof s);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->list(fs->ctx, "", collect, &s));
  TEST_ASSERT_EQUAL_INT(2, s.n); /* a, empty */
  memset(&s, 0, sizeof s);
  s.stop = 1;
  fs->list(fs->ctx, "", collect, &s);
  TEST_ASSERT_EQUAL_INT(1, s.n);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->list(fs->ctx, "zzz", collect, &s));
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->remove(fs->ctx, "a/d.txt"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, fs->remove(fs->ctx, "a/d.txt"));
  zb_memfs_destroy(m);
  zb_memfs_destroy(NULL);
}

static void test_path_join(void) {
  char out[32];
  TEST_ASSERT_TRUE(zb_path_join(out, sizeof out, "assets/", "/base"));
  TEST_ASSERT_EQUAL_STRING("assets/base", out);
  TEST_ASSERT_TRUE(zb_path_join(out, sizeof out, "", "x"));
  TEST_ASSERT_EQUAL_STRING("x", out);
  TEST_ASSERT_FALSE(zb_path_join(out, 4, "long", "path"));
}

static void test_oom(void) {
  for (long budget = 0; budget < 12; ++budget) {
    zb_failing_allocator f;
    const zb_allocator *a = zb_failing_init(&f, zb_test_alloc(), budget);
    zb_memfs *m = zb_memfs_create(a);
    if (!m) {
      continue;
    }
    const zb_fs_api *fs = zb_memfs_api(m);
    zb_memfs_put(m, "x/y", "1", 1);
    fs->mkdirs(fs->ctx, "dir");
    char *data;
    size_t len;
    if (fs->read(fs->ctx, "x/y", a, &data, &len) == ZB_OK) {
      zb_free(a, data);
    }
    seen s;
    memset(&s, 0, sizeof s);
    fs->list(fs->ctx, "", collect, &s);
    zb_memfs_destroy(m);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_put_read_stat_list);
  RUN_TEST(test_path_join);
  RUN_TEST(test_oom);
  return UNITY_END();
}

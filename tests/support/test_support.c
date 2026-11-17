/* SPDX-License-Identifier: MIT */
#include "support/test_support.h"

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKDIR(p) _mkdir(p)
#else
#include <dirent.h>
#include <unistd.h>
#define MKDIR(p) mkdir((p), 0755)
#endif

static zb_tracking_allocator g_tracking;
static bool g_active;

const zb_allocator *zb_test_alloc(void) {
  if (!g_active) {
    zb_test_begin();
  }
  return zb_tracking_iface(&g_tracking);
}

void zb_test_begin(void) {
  if (g_active) {
    zb_tracking_destroy(&g_tracking);
  }
  zb_tracking_init(&g_tracking, zb_heap_allocator());
  g_active = true;
}

typedef struct leak_buf {
  char *buf;
  size_t len;
  size_t used;
} leak_buf;

static void describe_one(const zb_tracking_entry *e, void *user) {
  leak_buf *b = user;
  if (b->used < b->len) {
    int n = snprintf(b->buf + b->used, b->len - b->used, "  leak: %s:%d (%zu bytes)\n",
                     e->file, e->line, e->size);
    if (n > 0) {
      b->used += (size_t)n;
      if (b->used > b->len) {
        b->used = b->len;
      }
    }
  }
}

size_t zb_test_describe_leaks(const zb_tracking_allocator *t, char *buf, size_t len) {
  leak_buf b = {buf, len, 0};
  if (len) {
    buf[0] = '\0';
  }
  return zb_tracking_report(t, describe_one, &b);
}

void zb_test_end(void) {
  if (!g_active) {
    return;
  }
  char msg[4096];
  size_t leaks = zb_test_describe_leaks(&g_tracking, msg, sizeof msg);
  zb_tracking_destroy(&g_tracking);
  g_active = false;
  if (leaks) {
    fprintf(stderr, "%zu leaked allocation(s):\n%s", leaks, msg);
    TEST_FAIL_MESSAGE("memory leaked during test (see allocation sites above)");
  }
}

/* ---- Failing allocator ----------------------------------------------------- */

static bool failing_should_fail(zb_failing_allocator *f) {
  f->calls++;
  return f->budget >= 0 && f->calls > f->budget;
}

static void *failing_alloc(void *ctx, size_t size, const char *file, int line) {
  zb_failing_allocator *f = ctx;
  return failing_should_fail(f) ? NULL : f->parent->alloc(f->parent->ctx, size, file, line);
}

static void *failing_realloc(void *ctx, void *ptr, size_t size, const char *file, int line) {
  zb_failing_allocator *f = ctx;
  return failing_should_fail(f) ? NULL : f->parent->realloc(f->parent->ctx, ptr, size, file, line);
}

static void failing_free(void *ctx, void *ptr, const char *file, int line) {
  zb_failing_allocator *f = ctx;
  f->parent->free(f->parent->ctx, ptr, file, line);
}

const zb_allocator *zb_failing_init(zb_failing_allocator *f, const zb_allocator *parent, long budget) {
  f->iface.alloc = failing_alloc;
  f->iface.realloc = failing_realloc;
  f->iface.free = failing_free;
  f->iface.ctx = f;
  f->parent = parent;
  f->budget = budget;
  f->calls = 0;
  return &f->iface;
}

/* ---- Files ----------------------------------------------------------------- */

const char *zb_test_source_dir(void) {
  const char *s = getenv("ZB_SOURCE_DIR");
  return s ? s : ".";
}

static void mkdirs_for(const char *path) {
  char tmp[1024];
  snprintf(tmp, sizeof tmp, "%s", path);
  for (char *p = tmp + 1; *p; ++p) {
    if (*p == '/' || *p == '\\') {
      char c = *p;
      *p = '\0';
      MKDIR(tmp);
      *p = c;
    }
  }
}

const char *zb_test_tmp_dir(void) {
  static char dir[1024];
  if (!dir[0]) {
    const char *base = getenv("ZB_BINARY_DIR");
#ifdef _WIN32
    snprintf(dir, sizeof dir, "%s/test-tmp/%lu", base ? base : ".", (unsigned long)GetCurrentProcessId());
#else
    snprintf(dir, sizeof dir, "%s/test-tmp/%ld", base ? base : ".", (long)getpid());
#endif
    char marker[1100];
    snprintf(marker, sizeof marker, "%s/.keep", dir);
    mkdirs_for(marker);
  }
  return dir;
}

bool zb_test_write_file(const char *path, const char *data, size_t len) {
  mkdirs_for(path);
  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  bool ok = fwrite(data, 1, len, f) == len;
  fclose(f);
  return ok;
}

char *zb_test_read_file(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc((size_t)n + 1);
  size_t got = fread(buf, 1, (size_t)n, f);
  fclose(f);
  buf[got] = '\0';
  if (len) {
    *len = got;
  }
  return buf;
}

void zb_test_remove_tree(const char *path) {
#ifdef _WIN32
  char cmd[1200];
  snprintf(cmd, sizeof cmd, "rmdir /s /q \"%s\" 2>nul", path);
  system(cmd);
#else
  DIR *d = opendir(path);
  if (!d) {
    remove(path);
    return;
  }
  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
      continue;
    }
    char child[1024];
    snprintf(child, sizeof child, "%s/%s", path, e->d_name);
    zb_test_remove_tree(child);
  }
  closedir(d);
  rmdir(path);
#endif
}

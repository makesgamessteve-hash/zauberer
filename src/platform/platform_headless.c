/* SPDX-License-Identifier: MIT */
/* Platform layer without SDL, for headless-only builds (the WebAssembly test
 * build running under Node.js). POSIX file access, no window, no audio, no
 * threads (the job system runs inline). */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "platform/platform.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

struct zb_platform {
  const zb_allocator *a;
  zb_fs_api fs;
};

const char *zb_platform_name(void) {
#ifdef __EMSCRIPTEN__
  return "web-node";
#else
  return "headless";
#endif
}

static zb_result px_read(void *ctx, const char *path, const zb_allocator *a, char **data, size_t *len) {
  (void)ctx;
  struct stat st;
  if (stat(path, &st) != 0) {
    return errno == ENOENT ? ZB_ERR_NOT_FOUND : ZB_ERR_IO;
  }
  FILE *f = S_ISDIR(st.st_mode) ? NULL : fopen(path, "rb");
  if (!f) {
    return ZB_ERR_IO;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) {
    fclose(f);
    return ZB_ERR_IO;
  }
  char *buf = zb_alloc(a, (size_t)n + 1);
  if (!buf) {
    fclose(f);
    return ZB_ERR_NOMEM;
  }
  size_t got = fread(buf, 1, (size_t)n, f);
  fclose(f);
  buf[got] = '\0';
  *data = buf;
  *len = got;
  return ZB_OK;
}

static zb_result px_mkdirs(void *ctx, const char *path) {
  (void)ctx;
  char tmp[1024];
  snprintf(tmp, sizeof tmp, "%s", path);
  for (char *p = tmp + 1; *p; ++p) {
    if (*p == '/') {
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
    return ZB_ERR_IO;
  }
  return ZB_OK;
}

static zb_result px_write(void *ctx, const char *path, const void *data, size_t len) {
  char dir[1024];
  snprintf(dir, sizeof dir, "%s", path);
  char *slash = strrchr(dir, '/');
  if (slash && slash != dir) {
    *slash = '\0';
    px_mkdirs(ctx, dir);
  }
  char tmp[1100];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *f = fopen(tmp, "wb");
  if (!f) {
    return ZB_ERR_IO;
  }
  bool ok = fwrite(data, 1, len, f) == len;
  ok = fclose(f) == 0 && ok;
  if (!ok || rename(tmp, path) != 0) {
    remove(tmp);
    return ZB_ERR_IO;
  }
  return ZB_OK;
}

static zb_result px_list(void *ctx, const char *dir, zb_fs_list_fn fn, void *user) {
  (void)ctx;
  DIR *d = opendir(dir);
  if (!d) {
    return ZB_ERR_NOT_FOUND;
  }
  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
      continue;
    }
    char full[1024];
    snprintf(full, sizeof full, "%s/%s", dir, e->d_name);
    struct stat st;
    bool is_dir = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
    if (!fn(e->d_name, is_dir, user)) {
      break;
    }
  }
  closedir(d);
  return ZB_OK;
}

static zb_result px_stat(void *ctx, const char *path, zb_fs_stat *out) {
  (void)ctx;
  struct stat st;
  if (stat(path, &st) != 0) {
    return ZB_ERR_NOT_FOUND;
  }
  out->is_dir = S_ISDIR(st.st_mode);
  out->size = (uint64_t)st.st_size;
  out->mtime_ns = (uint64_t)st.st_mtime * 1000000000ull;
  return ZB_OK;
}

static zb_result px_remove(void *ctx, const char *path) {
  (void)ctx;
  return remove(path) == 0 ? ZB_OK : ZB_ERR_NOT_FOUND;
}

zb_platform *zb_platform_create(const zb_allocator *a) {
  zb_platform *p = zb_calloc(a, sizeof(*p));
  if (!p) {
    return NULL;
  }
  p->a = a;
  p->fs.ctx = p;
  p->fs.read = px_read;
  p->fs.write = px_write;
  p->fs.list = px_list;
  p->fs.stat = px_stat;
  p->fs.mkdirs = px_mkdirs;
  p->fs.remove = px_remove;
  return p;
}

void zb_platform_destroy(zb_platform *p) {
  if (p) {
    zb_free(p->a, p);
  }
}

const zb_fs_api *zb_platform_fs(zb_platform *p) { return &p->fs; }
const zb_thread_api *zb_platform_threads(zb_platform *p) { (void)p; return NULL; }
unsigned zb_platform_cpu_count(zb_platform *p) { (void)p; return 1; }

uint64_t zb_platform_now_ns(zb_platform *p) {
  (void)p;
#ifdef __EMSCRIPTEN__
  return (uint64_t)(emscripten_get_now() * 1000000.0);
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

/* The executable's directory (Linux), else the working directory. The
 * web-node build looks for content relative to the working directory. */
const char *zb_platform_base_path(zb_platform *p) {
  (void)p;
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  static char base[1024];
  ssize_t n = readlink("/proc/self/exe", base, sizeof base - 1);
  if (n > 0) {
    base[n] = '\0';
    char *slash = strrchr(base, '/');
    if (slash) {
      slash[1] = '\0';
      return base;
    }
  }
#endif
  return "./";
}

zb_result zb_platform_user_dir(zb_platform *p, char *out, size_t len) {
  const char *xdg = getenv("XDG_DATA_HOME");
  const char *home = getenv("HOME");
  if (xdg && *xdg) {
    snprintf(out, len, "%s/zauberer/", xdg);
  } else if (home && *home) {
    snprintf(out, len, "%s/.local/share/zauberer/", home);
  } else {
    snprintf(out, len, "./user/");
  }
  return px_mkdirs(p, out);
}

zb_result zb_platform_open_window(zb_platform *p, const zb_window_desc *desc, char *err, size_t err_len) {
  (void)p;
  (void)desc;
  snprintf(err, err_len, "this build is headless-only and cannot open a window");
  return ZB_ERR_UNSUPPORTED;
}

bool zb_platform_has_window(zb_platform *p) { (void)p; return false; }
void zb_platform_drawable_size(zb_platform *p, int *w, int *h) { (void)p; *w = 0; *h = 0; }
void zb_platform_swap(zb_platform *p) { (void)p; }
void zb_platform_set_fullscreen(zb_platform *p, bool on) { (void)p; (void)on; }
bool zb_platform_is_fullscreen(zb_platform *p) { (void)p; return false; }
void zb_platform_set_window_size(zb_platform *p, int w, int h) { (void)p; (void)w; (void)h; }
zb_gl_proc zb_platform_gl_proc(zb_platform *p, const char *name) { (void)p; (void)name; return NULL; }
bool zb_platform_is_gles(zb_platform *p) { (void)p; return false; }

bool zb_platform_translate_event(zb_platform *p, const void *native, zb_event *out) {
  (void)p;
  (void)native;
  (void)out;
  return false;
}

void zb_platform_request_quit(zb_platform *p) { (void)p; }

void zb_platform_show_error(zb_platform *p, const char *title, const char *message) {
  (void)p;
  fprintf(stderr, "%s: %s\n", title, message);
}

bool zb_platform_touch_primary(zb_platform *p) {
  (void)p;
  return getenv("ZB_TOUCH_PRIMARY") != NULL;
}

zb_result zb_platform_audio_open(zb_platform *p, zb_audio_fill_fn fill, void *user, int *sample_rate) {
  (void)p;
  (void)fill;
  (void)user;
  (void)sample_rate;
  return ZB_ERR_UNSUPPORTED;
}

void zb_platform_audio_close(zb_platform *p) { (void)p; }
bool zb_platform_audio_is_open(zb_platform *p) { (void)p; return false; }
void zb_platform_user_data_sync(zb_platform *p) { (void)p; }

bool zb_platform_has_fetch(zb_platform *p) { (void)p; return false; }

zb_result zb_platform_fetch(zb_platform *p, const char *path, zb_fetch_done_fn done, void *user) {
  (void)p;
  (void)path;
  (void)done;
  (void)user;
  return ZB_ERR_UNSUPPORTED;
}

void zb_platform_set_state_provider(zb_platform *p, zb_state_fn fn, void *user) {
  (void)p;
  (void)fn;
  (void)user;
}

void zb_platform_push_audio_unlock(void) {}

/* SPDX-License-Identifier: MIT */
#include "core/memfs.h"

#include "core/array.h"

#include <stdio.h>
#include <string.h>

typedef struct mem_file {
  char *path;
  char *data;
  size_t len;
  uint64_t mtime;
} mem_file;

struct zb_memfs {
  const zb_allocator *a;
  zb_array files; /* mem_file */
  zb_array dirs;  /* char * (explicit directories from mkdirs) */
  uint64_t clock;
  zb_fs_api api;
};

bool zb_path_join(char *out, size_t out_len, const char *a, const char *b) {
  size_t la = strlen(a);
  while (la > 0 && a[la - 1] == '/') {
    la--;
  }
  while (*b == '/') {
    b++;
  }
  int n = la ? snprintf(out, out_len, "%.*s/%s", (int)la, a, b) : snprintf(out, out_len, "%s", b);
  return n >= 0 && (size_t)n < out_len;
}

static mem_file *find_file(zb_memfs *m, const char *path) {
  for (size_t i = 0; i < m->files.len; ++i) {
    mem_file *f = zb_array_at(&m->files, i);
    if (strcmp(f->path, path) == 0) {
      return f;
    }
  }
  return NULL;
}

/* True if some file or directory lives below dir (dir == "" means root). */
static bool is_dir(zb_memfs *m, const char *dir) {
  size_t n = strlen(dir);
  if (n == 0) {
    return true;
  }
  for (size_t i = 0; i < m->files.len; ++i) {
    mem_file *f = zb_array_at(&m->files, i);
    if (strncmp(f->path, dir, n) == 0 && f->path[n] == '/') {
      return true;
    }
  }
  for (size_t i = 0; i < m->dirs.len; ++i) {
    const char *d = *(char **)zb_array_at(&m->dirs, i);
    if (strcmp(d, dir) == 0 || (strncmp(d, dir, n) == 0 && d[n] == '/')) {
      return true;
    }
  }
  return false;
}

static zb_result mem_read(void *ctx, const char *path, const zb_allocator *a, char **data, size_t *len) {
  zb_memfs *m = ctx;
  mem_file *f = find_file(m, path);
  if (!f) {
    return ZB_ERR_NOT_FOUND;
  }
  char *copy = zb_alloc(a, f->len + 1);
  if (!copy) {
    return ZB_ERR_NOMEM;
  }
  if (f->len) {
    memcpy(copy, f->data, f->len);
  }
  copy[f->len] = '\0';
  *data = copy;
  *len = f->len;
  return ZB_OK;
}

static zb_result mem_write(void *ctx, const char *path, const void *data, size_t len) {
  return zb_memfs_put(ctx, path, data, len);
}

typedef struct seen_names {
  zb_array names; /* char * borrowed from paths; compared by prefix length */
} seen_names;

static zb_result mem_list(void *ctx, const char *dir, zb_fs_list_fn fn, void *user) {
  zb_memfs *m = ctx;
  if (!is_dir(m, dir)) {
    return ZB_ERR_NOT_FOUND;
  }
  size_t n = strlen(dir);
  zb_array seen;
  zb_array_init(&seen, m->a, sizeof(char *));
  zb_result r = ZB_OK;
  size_t total = m->files.len + m->dirs.len;
  for (size_t i = 0; i < total; ++i) {
    const char *p = i < m->files.len ? ((mem_file *)zb_array_at(&m->files, i))->path
                                     : *(char **)zb_array_at(&m->dirs, i - m->files.len);
    const char *rest;
    if (n == 0) {
      rest = p;
    } else if (strncmp(p, dir, n) == 0 && p[n] == '/') {
      rest = p + n + 1;
    } else {
      continue;
    }
    const char *slash = strchr(rest, '/');
    size_t len = slash ? (size_t)(slash - rest) : strlen(rest);
    bool child_is_dir = slash != NULL || i >= m->files.len;
    bool dup = false;
    for (size_t k = 0; k < seen.len; ++k) {
      const char *s = *(char **)zb_array_at(&seen, k);
      if (strncmp(s, rest, len) == 0 && s[len] == '\0') {
        dup = true;
        break;
      }
    }
    if (dup) {
      continue;
    }
    char *name = zb_strndup(m->a, rest, len);
    if (!name || !zb_array_push(&seen, &name)) {
      zb_free(m->a, name);
      r = ZB_ERR_NOMEM;
      break;
    }
    if (!fn(name, child_is_dir, user)) {
      break;
    }
  }
  for (size_t k = 0; k < seen.len; ++k) {
    zb_free(m->a, *(char **)zb_array_at(&seen, k));
  }
  zb_array_free(&seen);
  return r;
}

static zb_result mem_stat(void *ctx, const char *path, zb_fs_stat *st) {
  zb_memfs *m = ctx;
  mem_file *f = find_file(m, path);
  if (f) {
    st->is_dir = false;
    st->size = f->len;
    st->mtime_ns = f->mtime;
    return ZB_OK;
  }
  if (is_dir(m, path)) {
    st->is_dir = true;
    st->size = 0;
    st->mtime_ns = 0;
    return ZB_OK;
  }
  return ZB_ERR_NOT_FOUND;
}

static zb_result mem_mkdirs(void *ctx, const char *path) {
  zb_memfs *m = ctx;
  if (is_dir(m, path)) {
    return ZB_OK;
  }
  char *copy = zb_strdup(m->a, path);
  if (!copy || !zb_array_push(&m->dirs, &copy)) {
    zb_free(m->a, copy);
    return ZB_ERR_NOMEM;
  }
  return ZB_OK;
}

static zb_result mem_remove(void *ctx, const char *path) {
  zb_memfs *m = ctx;
  for (size_t i = 0; i < m->files.len; ++i) {
    mem_file *f = zb_array_at(&m->files, i);
    if (strcmp(f->path, path) == 0) {
      zb_free(m->a, f->path);
      zb_free(m->a, f->data);
      zb_array_remove(&m->files, i);
      return ZB_OK;
    }
  }
  return ZB_ERR_NOT_FOUND;
}

zb_memfs *zb_memfs_create(const zb_allocator *a) {
  zb_memfs *m = zb_calloc(a, sizeof(*m));
  if (!m) {
    return NULL;
  }
  m->a = a;
  zb_array_init(&m->files, a, sizeof(mem_file));
  zb_array_init(&m->dirs, a, sizeof(char *));
  m->api.ctx = m;
  m->api.read = mem_read;
  m->api.write = mem_write;
  m->api.list = mem_list;
  m->api.stat = mem_stat;
  m->api.mkdirs = mem_mkdirs;
  m->api.remove = mem_remove;
  return m;
}

void zb_memfs_destroy(zb_memfs *m) {
  if (!m) {
    return;
  }
  for (size_t i = 0; i < m->files.len; ++i) {
    mem_file *f = zb_array_at(&m->files, i);
    zb_free(m->a, f->path);
    zb_free(m->a, f->data);
  }
  for (size_t i = 0; i < m->dirs.len; ++i) {
    zb_free(m->a, *(char **)zb_array_at(&m->dirs, i));
  }
  zb_array_free(&m->files);
  zb_array_free(&m->dirs);
  zb_free(m->a, m);
}

const zb_fs_api *zb_memfs_api(zb_memfs *m) { return &m->api; }

zb_result zb_memfs_put(zb_memfs *m, const char *path, const void *data, size_t len) {
  char *copy = zb_alloc(m->a, len ? len : 1);
  if (!copy) {
    return ZB_ERR_NOMEM;
  }
  if (len) {
    memcpy(copy, data, len);
  }
  mem_file *f = find_file(m, path);
  if (f) {
    zb_free(m->a, f->data);
  } else {
    mem_file nf = {zb_strdup(m->a, path), NULL, 0, 0};
    if (!nf.path || !(f = zb_array_push(&m->files, &nf))) {
      zb_free(m->a, nf.path);
      zb_free(m->a, copy);
      return ZB_ERR_NOMEM;
    }
  }
  f->data = copy;
  f->len = len;
  f->mtime = ++m->clock;
  return ZB_OK;
}

void zb_memfs_touch(zb_memfs *m, const char *path, uint64_t mtime_ns) {
  mem_file *f = find_file(m, path);
  if (f) {
    f->mtime = mtime_ns;
  }
}

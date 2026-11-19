/* SPDX-License-Identifier: MIT */
#include "vfs/vfs.h"

#include "core/array.h"
#include "core/hash.h"
#include "core/log.h"
#include "core/luon.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct zb_vfs_req {
  zb_vfs *v;
  char path[ZB_VFS_MAX_PATH];
  char real[ZB_VFS_MAX_PATH * 2];
  zb_vfs_req_state state;
  zb_result error;
  char *data;
  size_t len;
  zb_vfs_req_fn fn;
  void *user;
  zb_job *job;
  bool done; /* result available, callback not yet delivered */
  bool released;
  /* written by the worker */
  zb_result job_result;
  char *job_data;
  size_t job_len;
};

struct zb_vfs {
  const zb_allocator *a;
  const zb_fs_api *fs;
  zb_jobs *jobs;
  zb_array roots;    /* char * */
  zb_array packages; /* zb_package */
  zb_array order;    /* size_t index into packages */
  zb_strtab paths;
  zb_array owners;   /* uint32_t package index per path id */
  zb_array requests; /* zb_vfs_req * */
  zb_vfs_fetcher fetcher;
  bool has_fetcher;
  char off[ZB_VFS_MAX_OFF][ZB_VFS_ID_LEN]; /* packages the player turned off */
  size_t off_count;
};

/* ---- Versions --------------------------------------------------------------- */

static bool parse_int(const char **s, int *out) {
  if (!isdigit((unsigned char)**s)) {
    return false;
  }
  long v = 0;
  while (isdigit((unsigned char)**s)) {
    v = v * 10 + (**s - '0');
    if (v > 1000000) {
      return false;
    }
    (*s)++;
  }
  *out = (int)v;
  return true;
}

bool zb_version_parse(const char *s, zb_version *out) {
  zb_version v = {0, 0, 0};
  if (!s || !parse_int(&s, &v.major)) {
    return false;
  }
  if (*s == '.') {
    s++;
    if (!parse_int(&s, &v.minor)) {
      return false;
    }
    if (*s == '.') {
      s++;
      if (!parse_int(&s, &v.patch)) {
        return false;
      }
    }
  }
  /* Pre-release and build suffixes are accepted but ignored. */
  if (*s != '\0' && *s != '-' && *s != '+') {
    return false;
  }
  *out = v;
  return true;
}

int zb_version_compare(zb_version a, zb_version b) {
  if (a.major != b.major)
    return a.major < b.major ? -1 : 1;
  if (a.minor != b.minor)
    return a.minor < b.minor ? -1 : 1;
  if (a.patch != b.patch)
    return a.patch < b.patch ? -1 : 1;
  return 0;
}

static bool satisfies_one(zb_version v, const char *c, bool *valid) {
  if (strcmp(c, "*") == 0) {
    return true;
  }
  char op[3] = {0};
  if (strncmp(c, ">=", 2) == 0 || strncmp(c, "<=", 2) == 0) {
    memcpy(op, c, 2);
    c += 2;
  } else if (*c == '>' || *c == '<' || *c == '=' || *c == '^') {
    op[0] = *c;
    c++;
  }
  zb_version want;
  if (!zb_version_parse(c, &want)) {
    *valid = false;
    return false;
  }
  int cmp = zb_version_compare(v, want);
  if (strcmp(op, ">=") == 0)
    return cmp >= 0;
  if (strcmp(op, "<=") == 0)
    return cmp <= 0;
  if (op[0] == '>')
    return cmp > 0;
  if (op[0] == '<')
    return cmp < 0;
  if (op[0] == '^')
    return v.major == want.major && cmp >= 0;
  return cmp == 0;
}

bool zb_version_satisfies(zb_version v, const char *constraint, bool *valid) {
  *valid = true;
  char buf[128];
  snprintf(buf, sizeof buf, "%s", constraint);
  bool any = false;
  bool ok = true;
  for (char *tok = strtok(buf, " ,"); tok; tok = strtok(NULL, " ,")) {
    any = true;
    if (!satisfies_one(v, tok, valid)) {
      ok = false;
    }
  }
  return any ? ok : true;
}

/* ---- Paths ---------------------------------------------------------------------- */

zb_result zb_vfs_normalize(const char *path, char *out, size_t out_len) {
  if (!path || path[0] == '/' || path[0] == '\\' || (isalpha((unsigned char)path[0]) && path[1] == ':')) {
    return ZB_ERR_INVALID;
  }
  size_t seg_starts[64];
  size_t depth = 0;
  size_t o = 0;
  const char *p = path;
  while (*p) {
    const char *start = p;
    while (*p && *p != '/' && *p != '\\') {
      p++;
    }
    size_t len = (size_t)(p - start);
    if (*p) {
      p++;
    }
    if (len == 0 || (len == 1 && start[0] == '.')) {
      continue;
    }
    if (len == 2 && start[0] == '.' && start[1] == '.') {
      if (depth == 0) {
        return ZB_ERR_INVALID;
      }
      o = seg_starts[--depth];
      continue;
    }
    if (depth == ZB_ARRAY_LEN(seg_starts)) {
      return ZB_ERR_INVALID;
    }
    size_t need = (o > 0 ? 1 : 0) + len;
    if (o + need + 1 > out_len) {
      return ZB_ERR_INVALID;
    }
    seg_starts[depth++] = o;
    if (o > 0) {
      out[o++] = '/';
    }
    memcpy(out + o, start, len);
    o += len;
  }
  if (out_len == 0) {
    return ZB_ERR_INVALID;
  }
  out[o] = '\0';
  return ZB_OK;
}

/* ---- Packages ----------------------------------------------------------- */

static void package_free(const zb_allocator *a, zb_package *p) {
  zb_free(a, p->id);
  zb_free(a, p->name);
  zb_free(a, p->version);
  zb_free(a, p->api);
  zb_free(a, p->license_code);
  zb_free(a, p->license_assets);
  zb_free(a, p->root);
  for (size_t i = 0; i < p->requires_count; ++i) {
    zb_free(a, p->requires[i].id);
    zb_free(a, p->requires[i].constraint);
  }
  for (size_t i = 0; i < p->optional_count; ++i) {
    zb_free(a, p->optional[i].id);
    zb_free(a, p->optional[i].constraint);
  }
  zb_free(a, p->requires);
  zb_free(a, p->optional);
}

zb_vfs *zb_vfs_create(const zb_allocator *a, const zb_fs_api *fs, zb_jobs *jobs) {
  zb_vfs *v = zb_calloc(a, sizeof(*v));
  if (!v) {
    return NULL;
  }
  v->a = a;
  v->fs = fs;
  v->jobs = jobs;
  zb_array_init(&v->roots, a, sizeof(char *));
  zb_array_init(&v->packages, a, sizeof(zb_package));
  zb_array_init(&v->order, a, sizeof(size_t));
  zb_strtab_init(&v->paths, a);
  zb_array_init(&v->owners, a, sizeof(uint32_t));
  zb_array_init(&v->requests, a, sizeof(zb_vfs_req *));
  return v;
}

static void req_free(zb_vfs *v, zb_vfs_req *r) {
  zb_free(v->a, r->data);
  zb_free(v->a, r->job_data);
  zb_free(v->a, r);
}

void zb_vfs_destroy(zb_vfs *v) {
  if (!v) {
    return;
  }
  for (size_t i = 0; i < v->requests.len; ++i) {
    zb_vfs_req *r = *(zb_vfs_req **)zb_array_at(&v->requests, i);
    if (r->job) {
      zb_job_cancel(v->jobs, r->job);
      zb_job_wait(v->jobs, r->job);
      zb_job_release(v->jobs, r->job);
    }
    req_free(v, r);
  }
  for (size_t i = 0; i < v->roots.len; ++i) {
    zb_free(v->a, *(char **)zb_array_at(&v->roots, i));
  }
  for (size_t i = 0; i < v->packages.len; ++i) {
    package_free(v->a, zb_array_at(&v->packages, i));
  }
  zb_array_free(&v->roots);
  zb_array_free(&v->packages);
  zb_array_free(&v->order);
  zb_strtab_free(&v->paths);
  zb_array_free(&v->owners);
  zb_array_free(&v->requests);
  zb_free(v->a, v);
}

zb_result zb_vfs_add_root(zb_vfs *v, const char *dir) {
  char *copy = zb_strdup(v->a, dir);
  if (!copy || !zb_array_push(&v->roots, &copy)) {
    zb_free(v->a, copy);
    return ZB_ERR_NOMEM;
  }
  return ZB_OK;
}

static zb_package *pkg_at(const zb_vfs *v, size_t i) { return zb_array_at(&v->packages, i); }

static void disable(zb_package *p, const char *fmt, ...) ZB_PRINTF(2, 3);

static void disable(zb_package *p, const char *fmt, ...) {
  if (!p->enabled) {
    return;
  }
  p->enabled = false;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(p->error, sizeof p->error, fmt, ap);
  va_end(ap);
  zb_warn("package '%s' disabled: %s", p->id ? p->id : p->root, p->error);
}

static bool valid_id(const char *s) {
  if (!*s) {
    return false;
  }
  for (; *s; ++s) {
    if (!(islower((unsigned char)*s) || isdigit((unsigned char)*s) || *s == '_' || *s == '-')) {
      return false;
    }
  }
  return true;
}

static bool read_deps(zb_vfs *v, const zb_value *t, zb_pkg_dep **out, size_t *count) {
  size_t n = zb_table_count(t);
  *count = 0;
  *out = NULL;
  if (n == 0) {
    return true;
  }
  *out = zb_calloc(v->a, n * sizeof(zb_pkg_dep));
  if (!*out) {
    return false;
  }
  for (size_t i = 0; i < n; ++i) {
    const zb_table_entry *e = zb_table_entry_at(t, i);
    if (e->key.type != ZB_VAL_STRING || e->value.type != ZB_VAL_STRING) {
      return false;
    }
    (*out)[i].id = zb_strdup(v->a, e->key.as.str.ptr);
    (*out)[i].constraint = zb_strdup(v->a, e->value.as.str.ptr);
    *count = i + 1;
    if (!(*out)[i].id || !(*out)[i].constraint) {
      return false;
    }
  }
  return true;
}

/* Parses the manifest into p; on problems the package is disabled. */
static void load_manifest(zb_vfs *v, zb_package *p) {
  char path[ZB_VFS_MAX_PATH * 2];
  zb_path_join(path, sizeof path, p->root, "package.lua");
  char *text = NULL;
  size_t len = 0;
  if (v->fs->read(v->fs->ctx, path, v->a, &text, &len) != ZB_OK) {
    disable(p, "cannot read package.lua");
    return;
  }
  zb_luon_doc doc;
  zb_luon_error err;
  zb_result r = zb_luon_parse(&doc, v->a, text, len, &err);
  zb_free(v->a, text);
  if (r != ZB_OK) {
    disable(p, "package.lua:%d:%d: %s", err.line, err.column, err.message);
    return;
  }
  const zb_value *root = &doc.root;
  const char *id = zb_value_str(zb_table_get(root, "id"), NULL);
  const char *version = zb_value_str(zb_table_get(root, "version"), NULL);
  const char *api = zb_value_str(zb_table_get(root, "api"), NULL);
  const zb_value *license = zb_table_get(root, "license");
  zb_version tmp;
  if (!zb_value_is(root, ZB_VAL_TABLE)) {
    disable(p, "package.lua must return a table");
  } else if (!id || !valid_id(id)) {
    disable(p, "package.lua: 'id' missing or invalid (lower-case letters, digits, '_' and '-')");
  } else if (!version || !zb_version_parse(version, &tmp)) {
    disable(p, "package.lua: 'version' missing or not a version number");
  } else if (!api || !zb_version_parse(api, &tmp)) {
    disable(p, "package.lua: 'api' missing or not a version number");
  } else if (!license || (license->type != ZB_VAL_STRING && license->type != ZB_VAL_TABLE)) {
    disable(p, "package.lua: 'license' missing");
  }
  if (id && valid_id(id)) {
    p->id = zb_strdup(v->a, id);
  }
  if (p->enabled) {
    const char *name = zb_value_str(zb_table_get(root, "name"), id);
    p->name = zb_strdup(v->a, name);
    p->version = zb_strdup(v->a, version);
    p->api = zb_strdup(v->a, api);
    if (license->type == ZB_VAL_STRING) {
      p->license_code = zb_strdup(v->a, license->as.str.ptr);
      p->license_assets = zb_strdup(v->a, license->as.str.ptr);
    } else {
      p->license_code = zb_strdup(v->a, zb_value_str(zb_table_get(license, "code"), "unknown"));
      p->license_assets = zb_strdup(v->a, zb_value_str(zb_table_get(license, "assets"), "unknown"));
    }
    if (!p->id || !p->name || !p->version || !p->api || !p->license_code || !p->license_assets) {
      disable(p, "out of memory while reading package.lua");
    }
    p->affects_worldgen = zb_value_bool(zb_table_get(root, "affects_worldgen"), false);
    if (!read_deps(v, zb_table_get(root, "requires"), &p->requires, &p->requires_count) ||
        !read_deps(v, zb_table_get(root, "optional"), &p->optional, &p->optional_count)) {
      disable(p, "package.lua: 'requires'/'optional' must map package ids to version constraints");
    }
  }
  zb_luon_free(&doc);
}

typedef struct discover_ctx {
  zb_vfs *v;
  const char *root;
  bool oom;
} discover_ctx;

static bool discover_cb(const char *name, bool is_dir, void *user) {
  discover_ctx *dc = user;
  if (!is_dir || name[0] == '.') {
    return true;
  }
  char dir[ZB_VFS_MAX_PATH * 2];
  char manifest[ZB_VFS_MAX_PATH * 2];
  zb_path_join(dir, sizeof dir, dc->root, name);
  zb_path_join(manifest, sizeof manifest, dir, "package.lua");
  zb_fs_stat st;
  if (dc->v->fs->stat(dc->v->fs->ctx, manifest, &st) != ZB_OK || st.is_dir) {
    return true;
  }
  zb_package p;
  memset(&p, 0, sizeof p);
  p.enabled = true;
  p.root = zb_strdup(dc->v->a, dir);
  if (!p.root || !zb_array_push(&dc->v->packages, &p)) {
    zb_free(dc->v->a, p.root);
    dc->oom = true;
    return false;
  }
  return true;
}

static int cmp_pkg_root(const void *a, const void *b) {
  return strcmp(((const zb_package *)a)->root, ((const zb_package *)b)->root);
}

static size_t find_enabled(const zb_vfs *v, const char *id) {
  for (size_t i = 0; i < v->packages.len; ++i) {
    zb_package *p = pkg_at(v, i);
    if (p->enabled && p->id && strcmp(p->id, id) == 0) {
      return i;
    }
  }
  return SIZE_MAX;
}

/* Disables packages whose required dependencies are missing or mismatched,
 * repeating until nothing changes (failures cascade). */
static void check_dependencies(zb_vfs *v) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 0; i < v->packages.len; ++i) {
      zb_package *p = pkg_at(v, i);
      for (size_t d = 0; p->enabled && d < p->requires_count; ++d) {
        zb_pkg_dep *dep = &p->
                             requires[
                                 d];
        size_t k = find_enabled(v, dep->id);
        if (k == SIZE_MAX) {
          disable(p, "missing dependency '%s'", dep->id);
          changed = true;
          break;
        }
        zb_version have;
        zb_version_parse(pkg_at(v, k)->version, &have);
        bool valid;
        if (!zb_version_satisfies(have, dep->constraint, &valid)) {
          if (valid) {
            disable(p, "requires '%s' %s but %s is installed", dep->id, dep->constraint, pkg_at(v, k)->version);
          } else {
            disable(p, "invalid version constraint '%s' for '%s'", dep->constraint, dep->id);
          }
          changed = true;
          break;
        }
      }
    }
  }
}

static bool depends_on(const zb_vfs *v, const zb_package *p, const char *id) {
  for (size_t d = 0; d < p->requires_count; ++d) {
    if (strcmp(p->requires[d].id, id) == 0) {
      return true;
    }
  }
  for (size_t d = 0; d < p->optional_count; ++d) {
    if (strcmp(p->optional[d].id, id) == 0 && find_enabled(v, id) != SIZE_MAX) {
      return true;
    }
  }
  return false;
}

/* Stable topological order (Kahn), ties broken by id; leftovers form cycles. */
static zb_result order_packages(zb_vfs *v) {
  size_t n = v->packages.len;
  bool *placed = zb_calloc(v->a, n ? n : 1);
  if (!placed) {
    return ZB_ERR_NOMEM;
  }
  zb_array_clear(&v->order);
  for (;;) {
    size_t best = SIZE_MAX;
    for (size_t i = 0; i < n; ++i) {
      zb_package *p = pkg_at(v, i);
      if (placed[i] || !p->enabled) {
        continue;
      }
      bool ready = true;
      for (size_t k = 0; k < n && ready; ++k) {
        zb_package *q = pkg_at(v, k);
        if (k != i && !placed[k] && q->enabled && depends_on(v, p, q->id)) {
          ready = false;
        }
      }
      if (ready && (best == SIZE_MAX || strcmp(p->id, pkg_at(v, best)->id) < 0)) {
        best = i;
      }
    }
    if (best == SIZE_MAX) {
      break;
    }
    placed[best] = true;
    if (!zb_array_push(&v->order, &best)) {
      zb_free(v->a, placed);
      return ZB_ERR_NOMEM;
    }
  }
  /* Anything enabled but unplaced is part of (or depends on) a cycle. */
  char names[200] = {0};
  for (size_t i = 0; i < n; ++i) {
    zb_package *p = pkg_at(v, i);
    if (!placed[i] && p->enabled) {
      size_t used = strlen(names);
      snprintf(names + used, sizeof names - used, "%s%s", used ? ", " : "", p->id);
    }
  }
  for (size_t i = 0; i < n; ++i) {
    zb_package *p = pkg_at(v, i);
    if (!placed[i] && p->enabled) {
      disable(p, "dependency cycle among: %s", names);
    }
  }
  zb_free(v->a, placed);
  return ZB_OK;
}

typedef struct walk_ctx {
  zb_vfs *v;
  uint32_t pkg;
  char rel[ZB_VFS_MAX_PATH];
  const char *root;
  int depth;
  bool oom;
} walk_ctx;

static bool walk_cb(const char *name, bool is_dir, void *user);

static void walk_dir(walk_ctx *w) {
  char dir[ZB_VFS_MAX_PATH * 2];
  if (w->rel[0]) {
    zb_path_join(dir, sizeof dir, w->root, w->rel);
  } else {
    snprintf(dir, sizeof dir, "%s", w->root);
  }
  w->v->fs->list(w->v->fs->ctx, dir, walk_cb, w);
}

static bool walk_cb(const char *name, bool is_dir, void *user) {
  walk_ctx *w = user;
  if (name[0] == '.') {
    return true;
  }
  size_t saved = strlen(w->rel);
  char path[ZB_VFS_MAX_PATH];
  if (!(saved ? zb_path_join(path, sizeof path, w->rel, name) : zb_path_join(path, sizeof path, "", name))) {
    return true;
  }
  if (is_dir) {
    if (w->depth >= 16) {
      return true;
    }
    snprintf(w->rel, sizeof w->rel, "%s", path);
    w->depth++;
    walk_dir(w);
    w->depth--;
    w->rel[saved] = '\0';
    return !w->oom;
  }
  uint32_t id = zb_strtab_intern(&w->v->paths, path, strlen(path));
  if (id == ZB_STR_NONE) {
    w->oom = true;
    return false;
  }
  while (w->v->owners.len <= id) {
    uint32_t none = UINT32_MAX;
    if (!zb_array_push(&w->v->owners, &none)) {
      w->oom = true;
      return false;
    }
  }
  *(uint32_t *)zb_array_at(&w->v->owners, id) = w->pkg;
  return true;
}

void zb_vfs_turn_off(zb_vfs *v, const char *id) {
  if (!id || !*id || strlen(id) >= ZB_VFS_ID_LEN || zb_vfs_is_turned_off(v, id) || v->off_count >= ZB_VFS_MAX_OFF) {
    return;
  }
  snprintf(v->off[v->off_count++], ZB_VFS_ID_LEN, "%s", id);
}

void zb_vfs_set_turned_off(zb_vfs *v, const char *id, bool off) {
  if (off) {
    zb_vfs_turn_off(v, id);
    return;
  }
  for (size_t i = 0; i < v->off_count; ++i) {
    if (strcmp(v->off[i], id) == 0) {
      memmove(v->off[i], v->off[i + 1], (v->off_count - i - 1) * sizeof v->off[0]);
      v->off_count--;
      return;
    }
  }
}

bool zb_vfs_is_turned_off(const zb_vfs *v, const char *id) {
  for (size_t i = 0; id && i < v->off_count; ++i) {
    if (strcmp(v->off[i], id) == 0) {
      return true;
    }
  }
  return false;
}

zb_result zb_vfs_mount(zb_vfs *v, int api_major, int api_minor) {
  bool any_root = false;
  for (size_t r = 0; r < v->roots.len; ++r) {
    const char *root = *(char **)zb_array_at(&v->roots, r);
    zb_fs_stat st;
    if (v->fs->stat(v->fs->ctx, root, &st) != ZB_OK || !st.is_dir) {
      continue;
    }
    any_root = true;
    /* Collect the directory names first so discovery order is sorted. */
    size_t first = v->packages.len;
    discover_ctx dc = {v, root, false};
    v->fs->list(v->fs->ctx, root, discover_cb, &dc);
    if (dc.oom) {
      return ZB_ERR_NOMEM;
    }
    size_t count = v->packages.len - first;
    if (count > 1) {
      qsort(pkg_at(v, first), count, sizeof(zb_package), cmp_pkg_root);
    }
  }
  if (!any_root) {
    return ZB_ERR_NOT_FOUND;
  }
  for (size_t i = 0; i < v->packages.len; ++i) {
    zb_package *p = pkg_at(v, i);
    load_manifest(v, p);
  }
  /* Duplicate ids: the first discovered wins. */
  for (size_t i = 0; i < v->packages.len; ++i) {
    zb_package *p = pkg_at(v, i);
    for (size_t k = 0; k < i && p->enabled; ++k) {
      zb_package *q = pkg_at(v, k);
      if (q->enabled && strcmp(q->id, p->id) == 0) {
        disable(p, "duplicate package id '%s' (already provided by %s)", p->id, q->root);
      }
    }
  }
  /* Packages the player turned off. */
  for (size_t i = 0; i < v->packages.len; ++i) {
    zb_package *p = pkg_at(v, i);
    if (p->enabled && zb_vfs_is_turned_off(v, p->id)) {
      disable(p, "turned off");
    }
  }
  /* Scripting API compatibility. */
  for (size_t i = 0; i < v->packages.len; ++i) {
    zb_package *p = pkg_at(v, i);
    zb_version want;
    if (p->enabled && zb_version_parse(p->api, &want) && (want.major != api_major || want.minor > api_minor)) {
      disable(p, "requires scripting API %s but the engine provides %d.%d", p->api, api_major, api_minor);
    }
  }
  check_dependencies(v);
  zb_result r = order_packages(v);
  if (r != ZB_OK) {
    return r;
  }
  for (size_t i = 0; i < v->order.len; ++i) {
    size_t idx = *(size_t *)zb_array_at(&v->order, i);
    walk_ctx w;
    memset(&w, 0, sizeof w);
    w.v = v;
    w.pkg = (uint32_t)idx;
    w.root = pkg_at(v, idx)->root;
    walk_dir(&w);
    if (w.oom) {
      return ZB_ERR_NOMEM;
    }
    zb_info("package %s %s loaded from %s", pkg_at(v, idx)->id, pkg_at(v, idx)->version, pkg_at(v, idx)->root);
  }
  return ZB_OK;
}

size_t zb_vfs_package_count(const zb_vfs *v) { return v->packages.len; }

const zb_package *zb_vfs_package(const zb_vfs *v, size_t i) { return i < v->packages.len ? pkg_at(v, i) : NULL; }

size_t zb_vfs_load_count(const zb_vfs *v) { return v->order.len; }

const zb_package *zb_vfs_load_order(const zb_vfs *v, size_t i) {
  return i < v->order.len ? pkg_at(v, *(size_t *)zb_array_at(&v->order, i)) : NULL;
}

const zb_package *zb_vfs_find_package(const zb_vfs *v, const char *id) {
  for (size_t i = 0; i < v->packages.len; ++i) {
    zb_package *p = pkg_at(v, i);
    if (p->id && strcmp(p->id, id) == 0) {
      return p;
    }
  }
  return NULL;
}

uint64_t zb_vfs_package_set_hash(const zb_vfs *v) {
  zb_hasher h;
  zb_hasher_init(&h, 0x7061636b);
  for (size_t i = 0; i < v->order.len; ++i) {
    const zb_package *p = zb_vfs_load_order(v, i);
    zb_hasher_str(&h, p->id);
    zb_hasher_str(&h, p->version);
  }
  return zb_hasher_final(&h);
}

/* ---- Lookup ---------------------------------------------------------------- */

static uint32_t owner_index(const zb_vfs *v, const char *path) {
  char norm[ZB_VFS_MAX_PATH];
  if (zb_vfs_normalize(path, norm, sizeof norm) != ZB_OK) {
    return UINT32_MAX;
  }
  uint32_t id = zb_strtab_find(&v->paths, norm, strlen(norm));
  if (id == ZB_STR_NONE || id >= v->owners.len) {
    return UINT32_MAX;
  }
  return *(uint32_t *)zb_array_at(&v->owners, id);
}

bool zb_vfs_exists(const zb_vfs *v, const char *path) { return owner_index(v, path) != UINT32_MAX; }

const zb_package *zb_vfs_owner(const zb_vfs *v, const char *path) {
  uint32_t i = owner_index(v, path);
  return i == UINT32_MAX ? NULL : pkg_at(v, i);
}

zb_result zb_vfs_real_path(const zb_vfs *v, const char *path, char *out, size_t out_len) {
  char norm[ZB_VFS_MAX_PATH];
  zb_result r = zb_vfs_normalize(path, norm, sizeof norm);
  if (r != ZB_OK) {
    return r;
  }
  uint32_t i = owner_index(v, norm);
  if (i == UINT32_MAX) {
    return ZB_ERR_NOT_FOUND;
  }
  return zb_path_join(out, out_len, pkg_at(v, i)->root, norm) ? ZB_OK : ZB_ERR_INVALID;
}

zb_result zb_vfs_read(zb_vfs *v, const char *path, const zb_allocator *a, char **data, size_t *len) {
  char real[ZB_VFS_MAX_PATH * 2];
  zb_result r = zb_vfs_real_path(v, path, real, sizeof real);
  if (r != ZB_OK) {
    return r;
  }
  return v->fs->read(v->fs->ctx, real, a, data, len);
}

zb_result zb_vfs_read_package(zb_vfs *v, const zb_package *pkg, const char *path, const zb_allocator *a, char **data,
                              size_t *len) {
  char norm[ZB_VFS_MAX_PATH];
  char real[ZB_VFS_MAX_PATH * 2];
  zb_result r = zb_vfs_normalize(path, norm, sizeof norm);
  if (r != ZB_OK) {
    return r;
  }
  zb_path_join(real, sizeof real, pkg->root, norm);
  return v->fs->read(v->fs->ctx, real, a, data, len);
}

typedef struct child {
  const char *name;
  size_t len;
  bool is_dir;
} child;

static int cmp_child(const void *a, const void *b) {
  const child *x = a, *y = b;
  size_t n = ZB_MIN(x->len, y->len);
  int c = memcmp(x->name, y->name, n);
  return c ? c : (x->len < y->len ? -1 : (x->len > y->len ? 1 : 0));
}

zb_result zb_vfs_list(zb_vfs *v, const char *dir, zb_fs_list_fn fn, void *user) {
  char norm[ZB_VFS_MAX_PATH];
  zb_result r = zb_vfs_normalize(dir, norm, sizeof norm);
  if (r != ZB_OK) {
    return r;
  }
  size_t n = strlen(norm);
  zb_array kids;
  zb_array_init(&kids, v->a, sizeof(child));
  size_t count = zb_strtab_count(&v->paths);
  for (uint32_t id = 0; id < count; ++id) {
    if (id >= v->owners.len || *(uint32_t *)zb_array_at(&v->owners, id) == UINT32_MAX) {
      continue;
    }
    const char *p = zb_strtab_get(&v->paths, id);
    const char *rest;
    if (n == 0) {
      rest = p;
    } else if (strncmp(p, norm, n) == 0 && p[n] == '/') {
      rest = p + n + 1;
    } else {
      continue;
    }
    const char *slash = strchr(rest, '/');
    child c = {rest, slash ? (size_t)(slash - rest) : strlen(rest), slash != NULL};
    bool dup = false;
    for (size_t k = 0; k < kids.len; ++k) {
      child *e = zb_array_at(&kids, k);
      if (e->len == c.len && memcmp(e->name, c.name, c.len) == 0) {
        dup = true;
        break;
      }
    }
    if (!dup && !zb_array_push(&kids, &c)) {
      zb_array_free(&kids);
      return ZB_ERR_NOMEM;
    }
  }
  if (kids.len > 1) {
    qsort(kids.data, kids.len, sizeof(child), cmp_child);
  }
  for (size_t k = 0; k < kids.len; ++k) {
    child *c = zb_array_at(&kids, k);
    char name[ZB_VFS_MAX_PATH];
    snprintf(name, sizeof name, "%.*s", (int)c->len, c->name);
    if (!fn(name, c->is_dir, user)) {
      break;
    }
  }
  bool found = kids.len > 0 || n == 0;
  zb_array_free(&kids);
  return found ? ZB_OK : ZB_ERR_NOT_FOUND;
}

size_t zb_vfs_file_count(const zb_vfs *v) {
  size_t n = 0;
  for (size_t i = 0; i < v->owners.len; ++i) {
    if (*(uint32_t *)zb_array_at(&v->owners, i) != UINT32_MAX) {
      n++;
    }
  }
  return n;
}

/* ---- Asynchronous requests ------------------------------------------------ */

static void read_job(void *arg) {
  zb_vfs_req *r = arg;
  r->job_result = r->v->fs->read(r->v->fs->ctx, r->real, r->v->a, &r->job_data, &r->job_len);
}

zb_vfs_req *zb_vfs_request(zb_vfs *v, const char *path, zb_vfs_req_fn fn, void *user) {
  zb_vfs_req *r = zb_calloc(v->a, sizeof(*r));
  if (!r) {
    return NULL;
  }
  r->v = v;
  r->fn = fn;
  r->user = user;
  r->state = ZB_REQ_PENDING;
  snprintf(r->path, sizeof r->path, "%s", path);
  if (!zb_array_push(&v->requests, &r)) {
    zb_free(v->a, r);
    return NULL;
  }
  zb_result res = zb_vfs_real_path(v, path, r->real, sizeof r->real);
  if (res != ZB_OK) {
    r->error = res;
    r->done = true;
    return r;
  }
  if (v->has_fetcher) {
    v->fetcher.start(v->fetcher.ctx, r, r->real);
  } else if (v->jobs) {
    r->job = zb_job_submit(v->jobs, read_job, r, ZB_JOB_NORMAL);
    if (!r->job) {
      r->error = ZB_ERR_NOMEM;
      r->done = true;
    }
  } else {
    read_job(r);
    zb_vfs_req_complete(r, r->job_result, r->job_data, r->job_len);
    r->job_data = NULL;
  }
  return r;
}

void zb_vfs_req_complete(zb_vfs_req *r, zb_result result, char *data, size_t len) {
  r->error = result;
  if (result == ZB_OK) {
    r->data = data;
    r->len = len;
  } else {
    zb_free(r->v->a, data);
  }
  r->done = true;
}

zb_vfs_req_state zb_vfs_req_state_of(const zb_vfs_req *r) { return r->state; }
zb_result zb_vfs_req_error(const zb_vfs_req *r) { return r->error; }
const char *zb_vfs_req_path(const zb_vfs_req *r) { return r->path; }

const char *zb_vfs_req_data(const zb_vfs_req *r, size_t *len) {
  if (len) {
    *len = r->state == ZB_REQ_READY ? r->len : 0;
  }
  return r->state == ZB_REQ_READY ? r->data : NULL;
}

static void remove_request(zb_vfs *v, zb_vfs_req *r) {
  for (size_t i = 0; i < v->requests.len; ++i) {
    if (*(zb_vfs_req **)zb_array_at(&v->requests, i) == r) {
      zb_array_remove(&v->requests, i);
      return;
    }
  }
}

void zb_vfs_req_release(zb_vfs *v, zb_vfs_req *r) {
  if (!r) {
    return;
  }
  if (r->job) {
    zb_job_cancel(v->jobs, r->job);
    zb_job_wait(v->jobs, r->job);
    zb_job_release(v->jobs, r->job);
    r->job = NULL;
  }
  if (r->state == ZB_REQ_PENDING && !r->done && v->has_fetcher) {
    /* The fetcher still holds r: keep it until it completes, then free. */
    r->released = true;
    return;
  }
  remove_request(v, r);
  req_free(v, r);
}

size_t zb_vfs_poll(zb_vfs *v) {
  size_t completed = 0;
  for (size_t i = 0; i < v->requests.len;) {
    zb_vfs_req *r = *(zb_vfs_req **)zb_array_at(&v->requests, i);
    if (r->job && zb_job_get_state(v->jobs, r->job) == ZB_JOB_DONE) {
      zb_job_release(v->jobs, r->job);
      r->job = NULL;
      zb_vfs_req_complete(r, r->job_result, r->job_data, r->job_len);
      r->job_data = NULL;
    }
    if (r->done && r->state == ZB_REQ_PENDING) {
      r->state = r->error == ZB_OK ? ZB_REQ_READY : ZB_REQ_FAILED;
      completed++;
      if (r->released) {
        zb_array_remove(&v->requests, i);
        req_free(v, r);
        continue;
      }
      if (r->fn) {
        r->fn(r, r->user); /* may release r (and start new requests) */
      }
      if (i < v->requests.len && *(zb_vfs_req **)zb_array_at(&v->requests, i) == r) {
        ++i;
      }
      continue;
    }
    ++i;
  }
  return completed;
}

size_t zb_vfs_pending_count(const zb_vfs *v) {
  size_t n = 0;
  for (size_t i = 0; i < v->requests.len; ++i) {
    zb_vfs_req *r = *(zb_vfs_req **)zb_array_at(&v->requests, i);
    if (r->state == ZB_REQ_PENDING) {
      n++;
    }
  }
  return n;
}

void zb_vfs_set_fetcher(zb_vfs *v, const zb_vfs_fetcher *fetcher) {
  if (fetcher) {
    v->fetcher = *fetcher;
    v->has_fetcher = true;
  } else {
    v->has_fetcher = false;
  }
}

const zb_allocator *zb_vfs_allocator(const zb_vfs *v) { return v->a; }

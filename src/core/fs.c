/* SPDX-License-Identifier: MIT */
/* File system helpers over a zb_fs_api. */
#include "core/fs.h"

#include <string.h>

#include "core/array.h"

#define REMOVE_DEPTH 16 /* folders nested deeper are left alone */

typedef struct child_list {
  const zb_allocator *a;
  zb_array names; /* char * */
  bool nomem;
} child_list;

static bool collect_child(const char *name, bool is_dir, void *user) {
  (void)is_dir;
  child_list *c = user;
  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    return true;
  }
  char *copy = zb_strdup(c->a, name);
  if (!copy || !zb_array_push(&c->names, &copy)) {
    zb_free(c->a, copy);
    c->nomem = true;
    return false;
  }
  return true;
}

static zb_result remove_tree(const zb_fs_api *fs, const zb_allocator *a, const char *path, int depth) {
  zb_fs_stat st;
  if (fs->stat(fs->ctx, path, &st) != ZB_OK) {
    return ZB_OK; /* nothing there */
  }
  zb_result r = ZB_OK;
  if (st.is_dir) {
    if (depth >= REMOVE_DEPTH) {
      return ZB_ERR_INVALID;
    }
    child_list c = {a, {0}, false};
    zb_array_init(&c.names, a, sizeof(char *));
    fs->list(fs->ctx, path, collect_child, &c);
    if (c.nomem) {
      r = ZB_ERR_NOMEM;
    }
    for (size_t i = 0; i < c.names.len; ++i) {
      char *name = *(char **)zb_array_at(&c.names, i);
      char child[1024];
      if (r == ZB_OK) {
        r = zb_path_join(child, sizeof child, path, name) ? remove_tree(fs, a, child, depth + 1) : ZB_ERR_INVALID;
      }
      zb_free(a, name);
    }
    zb_array_free(&c.names);
  }
  if (r == ZB_OK) {
    fs->remove(fs->ctx, path);
    /* (some file systems keep no empty folders: gone is gone) */
    r = fs->stat(fs->ctx, path, &st) == ZB_OK ? ZB_ERR_IO : ZB_OK;
  }
  return r;
}

zb_result zb_fs_remove_tree(const zb_fs_api *fs, const zb_allocator *a, const char *path) {
  return remove_tree(fs, a, path, 0);
}

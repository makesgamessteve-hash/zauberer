/* SPDX-License-Identifier: MIT */
/* File system interface. The platform layer provides the real
 * implementation; core/memfs.c provides an in-memory one (tests, web boot
 * set). Paths use '/' as separator. */
#ifndef ZB_CORE_FS_H
#define ZB_CORE_FS_H

#include "core/alloc.h"

/* Return false to stop the enumeration. */
typedef bool (*zb_fs_list_fn)(const char *name, bool is_dir, void *user);

typedef struct zb_fs_stat {
  bool is_dir;
  uint64_t size;
  uint64_t mtime_ns;
} zb_fs_stat;

typedef struct zb_fs_api {
  void *ctx;
  /* Reads a whole file; *data is allocated with a (NUL-terminated, the
   * terminator is not counted in *len). */
  zb_result (*read)(void *ctx, const char *path, const zb_allocator *a, char **data, size_t *len);
  /* Writes atomically (temporary file, then rename); creates parent dirs. */
  zb_result (*write)(void *ctx, const char *path, const void *data, size_t len);
  zb_result (*list)(void *ctx, const char *dir, zb_fs_list_fn fn, void *user);
  zb_result (*stat)(void *ctx, const char *path, zb_fs_stat *st);
  zb_result (*mkdirs)(void *ctx, const char *path);
  zb_result (*remove)(void *ctx, const char *path);
} zb_fs_api;

/* Removes a file or a folder with everything in it (up to 16 levels deep).
 * ZB_OK also when nothing was there. */
zb_result zb_fs_remove_tree(const zb_fs_api *fs, const zb_allocator *a, const char *path);

/* Joins two path parts with a single '/' into out; returns false if it does not fit. */
bool zb_path_join(char *out, size_t out_len, const char *a, const char *b);

#endif

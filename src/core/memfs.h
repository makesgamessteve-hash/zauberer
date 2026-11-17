/* SPDX-License-Identifier: MIT */
/* An in-memory file system implementing zb_fs_api. */
#ifndef ZB_CORE_MEMFS_H
#define ZB_CORE_MEMFS_H

#include "core/fs.h"

typedef struct zb_memfs zb_memfs;

zb_memfs *zb_memfs_create(const zb_allocator *a);
void zb_memfs_destroy(zb_memfs *m);
const zb_fs_api *zb_memfs_api(zb_memfs *m);
/* Adds or replaces a file; creates parent directories implicitly. */
zb_result zb_memfs_put(zb_memfs *m, const char *path, const void *data, size_t len);
/* Sets the modification time reported by stat (default: a counter). */
void zb_memfs_touch(zb_memfs *m, const char *path, uint64_t mtime_ns);

#endif

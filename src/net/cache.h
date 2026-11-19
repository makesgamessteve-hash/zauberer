/* SPDX-License-Identifier: MIT */
/* A content-addressed cache for blobs received in co-op sessions (the world
 * bundle), so that joining the same world again skips the transfer. Files
 * live under <user dir>net-cache/<hash>.blob, where hash is the blob's SHA-1
 * (zb_net_blob_hash); loading checks the hash again. */
#ifndef ZB_NET_CACHE_H
#define ZB_NET_CACHE_H

#include "core/fs.h"
#include "net/session.h"

/* The cache file of a hash; ZB_ERR_INVALID unless hash is 40 lowercase hex digits. */
zb_result zb_net_cache_path(const char *user_dir, const char *hash, char *out, size_t out_len);
/* True when the cache holds a file for this hash (not verified; zb_net_cache_load does that). */
bool zb_net_cache_has(const zb_fs_api *fs, const char *user_dir, const char *hash);
/* Reads a cached blob (allocated with a) and verifies it; a file whose
 * content no longer matches is removed and ZB_ERR_NOT_FOUND returned. */
zb_result zb_net_cache_load(const zb_fs_api *fs, const char *user_dir, const char *hash, const zb_allocator *a,
                            char **data, size_t *len);
/* Stores a blob under its hash (written to hash_out). */
zb_result zb_net_cache_store(const zb_fs_api *fs, const char *user_dir, const void *data, size_t len,
                             char hash_out[ZB_NET_HASH_HEX + 1]);

#endif

/* SPDX-License-Identifier: MIT */
#include "net/cache.h"

#include <stdio.h>
#include <string.h>

zb_result zb_net_cache_path(const char *user_dir, const char *hash, char *out, size_t out_len) {
  if (strlen(hash) != ZB_NET_HASH_HEX || strspn(hash, "0123456789abcdef") != ZB_NET_HASH_HEX) {
    return ZB_ERR_INVALID;
  }
  int n = snprintf(out, out_len, "%snet-cache/%s.blob", user_dir ? user_dir : "", hash);
  return n > 0 && (size_t)n < out_len ? ZB_OK : ZB_ERR_INVALID;
}

bool zb_net_cache_has(const zb_fs_api *fs, const char *user_dir, const char *hash) {
  char path[1024];
  zb_fs_stat st;
  return zb_net_cache_path(user_dir, hash, path, sizeof path) == ZB_OK && fs->stat(fs->ctx, path, &st) == ZB_OK &&
         !st.is_dir;
}

zb_result zb_net_cache_load(const zb_fs_api *fs, const char *user_dir, const char *hash, const zb_allocator *a,
                            char **data, size_t *len) {
  char path[1024];
  zb_result r = zb_net_cache_path(user_dir, hash, path, sizeof path);
  if (r != ZB_OK) {
    return r;
  }
  r = fs->read(fs->ctx, path, a, data, len);
  if (r != ZB_OK) {
    return r == ZB_ERR_NOMEM ? r : ZB_ERR_NOT_FOUND;
  }
  char actual[ZB_NET_HASH_HEX + 1];
  zb_net_blob_hash(*data, *len, actual);
  if (strcmp(actual, hash) != 0) {
    zb_free(a, *data);
    *data = NULL;
    *len = 0;
    fs->remove(fs->ctx, path);
    return ZB_ERR_NOT_FOUND;
  }
  return ZB_OK;
}

zb_result zb_net_cache_store(const zb_fs_api *fs, const char *user_dir, const void *data, size_t len,
                             char hash_out[ZB_NET_HASH_HEX + 1]) {
  zb_net_blob_hash(data, len, hash_out);
  char path[1024];
  zb_result r = zb_net_cache_path(user_dir, hash_out, path, sizeof path);
  return r == ZB_OK ? fs->write(fs->ctx, path, data, len) : r;
}

/* SPDX-License-Identifier: MIT */
/* Packages and the virtual file system.
 *
 * A package is a folder with a package.lua manifest. Packages are discovered
 * under search roots (assets/, mods/, the user's mods/), ordered by their
 * dependencies, and overlaid: a path resolves to the last-loaded package that
 * contains it. Reads are asynchronous (handles completed by zb_vfs_poll); a
 * synchronous read exists for the boot set (manifests, scripts, data). */
#ifndef ZB_VFS_VFS_H
#define ZB_VFS_VFS_H

#include "core/alloc.h"
#include "core/fs.h"
#include "core/jobs.h"

#define ZB_VFS_MAX_PATH 512

typedef struct zb_pkg_dep {
  char *id;
  char *constraint; /* e.g. ">=0.1 <2", "^1.2", "*" */
} zb_pkg_dep;

typedef struct zb_package {
  char *id;
  char *name;
  char *version;
  char *api;
  char *license_code;
  char *license_assets;
  char *root;       /* directory of the package in the file system */
  zb_pkg_dep *requires;
  size_t requires_count;
  zb_pkg_dep *optional;
  size_t optional_count;
  bool affects_worldgen;
  bool enabled;
  char error[256];  /* why the package is disabled */
} zb_package;

typedef struct zb_vfs zb_vfs;

/* jobs may be NULL: async reads then complete synchronously at the next poll. */
zb_vfs *zb_vfs_create(const zb_allocator *a, const zb_fs_api *fs, zb_jobs *jobs);
void zb_vfs_destroy(zb_vfs *v);
zb_result zb_vfs_add_root(zb_vfs *v, const char *dir);
/* Discovers, validates and orders packages and builds the overlay. Returns
 * ZB_ERR_NOT_FOUND when none of the roots exists. Problems with individual
 * packages disable them (see zb_package.error) without failing the mount. */
zb_result zb_vfs_mount(zb_vfs *v, int api_major, int api_minor);

/* All discovered packages (enabled or not). */
size_t zb_vfs_package_count(const zb_vfs *v);
const zb_package *zb_vfs_package(const zb_vfs *v, size_t i);
/* Enabled packages in load order. */
size_t zb_vfs_load_count(const zb_vfs *v);
const zb_package *zb_vfs_load_order(const zb_vfs *v, size_t i);
const zb_package *zb_vfs_find_package(const zb_vfs *v, const char *id);
/* Hash of the enabled package ids and versions in load order. */
uint64_t zb_vfs_package_set_hash(const zb_vfs *v);

/* Normalises a package-relative path: '\\' -> '/', removes "." and resolves
 * ".."; rejects absolute paths, drive letters and escapes from the root. */
zb_result zb_vfs_normalize(const char *path, char *out, size_t out_len);

bool zb_vfs_exists(const zb_vfs *v, const char *path);
/* The package that provides path after overlaying. */
const zb_package *zb_vfs_owner(const zb_vfs *v, const char *path);
/* File-system path of the overlay winner for path. */
zb_result zb_vfs_real_path(const zb_vfs *v, const char *path, char *out, size_t out_len);
/* Synchronous overlay read (boot set only). */
zb_result zb_vfs_read(zb_vfs *v, const char *path, const zb_allocator *a, char **data, size_t *len);
/* Synchronous read of one specific package's file (no overlay). */
zb_result zb_vfs_read_package(zb_vfs *v, const zb_package *pkg, const char *path, const zb_allocator *a,
                              char **data, size_t *len);
/* Merged, sorted, de-duplicated listing of the direct children of dir. */
zb_result zb_vfs_list(zb_vfs *v, const char *dir, zb_fs_list_fn fn, void *user);
/* Number of overlaid files. */
size_t zb_vfs_file_count(const zb_vfs *v);

/* ---- Asynchronous requests ------------------------------------------------- */

typedef enum zb_vfs_req_state { ZB_REQ_PENDING, ZB_REQ_READY, ZB_REQ_FAILED } zb_vfs_req_state;

typedef struct zb_vfs_req zb_vfs_req;
typedef void (*zb_vfs_req_fn)(zb_vfs_req *req, void *user);

/* Starts loading path; the handle is pending until a later zb_vfs_poll, which
 * invokes fn (if not NULL) exactly once. Returns NULL only when out of memory. */
zb_vfs_req *zb_vfs_request(zb_vfs *v, const char *path, zb_vfs_req_fn fn, void *user);
zb_vfs_req_state zb_vfs_req_state_of(const zb_vfs_req *r);
zb_result zb_vfs_req_error(const zb_vfs_req *r);
const char *zb_vfs_req_path(const zb_vfs_req *r);
/* Data of a ready request (NUL-terminated; terminator not counted). */
const char *zb_vfs_req_data(const zb_vfs_req *r, size_t *len);
/* Releases the handle; a pending load is cancelled or its result discarded. */
void zb_vfs_req_release(zb_vfs *v, zb_vfs_req *r);
/* Completes finished loads and invokes callbacks; returns how many completed. */
size_t zb_vfs_poll(zb_vfs *v);
size_t zb_vfs_pending_count(const zb_vfs *v);

/* External fetcher (the web build fetches over HTTP). start must eventually
 * call zb_vfs_req_complete on the main thread. */
typedef struct zb_vfs_fetcher {
  void *ctx;
  void (*start)(void *ctx, zb_vfs_req *req, const char *real_path);
} zb_vfs_fetcher;

void zb_vfs_set_fetcher(zb_vfs *v, const zb_vfs_fetcher *fetcher);
/* data must be allocated with the vfs allocator (or NULL on failure); the vfs
 * takes ownership. */
void zb_vfs_req_complete(zb_vfs_req *r, zb_result result, char *data, size_t len);
const zb_allocator *zb_vfs_allocator(const zb_vfs *v);

/* ---- Versions ------------------------------------------------------------- */

typedef struct zb_version {
  int major, minor, patch;
} zb_version;

bool zb_version_parse(const char *s, zb_version *out);
int zb_version_compare(zb_version a, zb_version b);
/* Space- or comma-separated constraints: "*", "1.2", "=1.2", ">=1.2", ">1",
 * "<=2", "<2", "^1.2" (same major, at least 1.2). Returns false on a match
 * failure; *valid is set to false for a malformed constraint. */
bool zb_version_satisfies(zb_version v, const char *constraint, bool *valid);

#endif

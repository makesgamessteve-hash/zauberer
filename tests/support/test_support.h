/* SPDX-License-Identifier: MIT */
/* Shared helpers for Unity unit tests: per-test leak checking through the
 * tracking allocator, an allocator that fails on demand, and temp files. */
#ifndef ZB_TEST_SUPPORT_H
#define ZB_TEST_SUPPORT_H

#include "core/alloc.h"

/* The tracking allocator for the current test (valid between setUp/tearDown). */
const zb_allocator *zb_test_alloc(void);

/* Starts tracking; called from setUp. */
void zb_test_begin(void);
/* Stops tracking; fails the current test if allocations are still live,
 * naming every allocation site. Called from tearDown. */
void zb_test_end(void);

/* Formats the live allocations of t into buf ("file:line (n bytes)" lines);
 * returns the number of leaks. Used by zb_test_end and its own test. */
size_t zb_test_describe_leaks(const zb_tracking_allocator *t, char *buf, size_t len);

/* Defines Unity's setUp/tearDown with leak checking. */
#define ZB_TEST_LEAK_CHECKED \
  void setUp(void) { zb_test_begin(); } \
  void tearDown(void) { zb_test_end(); }

/* An allocator that fails every allocation after `budget` successful ones. */
typedef struct zb_failing_allocator {
  zb_allocator iface;
  const zb_allocator *parent;
  long budget; /* < 0: never fail */
  long calls;
} zb_failing_allocator;

const zb_allocator *zb_failing_init(zb_failing_allocator *f, const zb_allocator *parent, long budget);

/* Directory for temporary files of this test binary (created on demand). */
const char *zb_test_tmp_dir(void);
/* Writes a file (creating parent directories); returns false on error. */
bool zb_test_write_file(const char *path, const char *data, size_t len);
/* Reads a whole file into a malloc'ed, NUL-terminated buffer (free with free()). */
char *zb_test_read_file(const char *path, size_t *len);
/* Removes a directory tree (best effort). */
void zb_test_remove_tree(const char *path);
/* Repository root (from ZB_SOURCE_DIR, falling back to the current directory). */
const char *zb_test_source_dir(void);

#endif

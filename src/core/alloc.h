/* SPDX-License-Identifier: MIT */
/* Allocator interface, heap allocator, arenas and the leak-tracking allocator. */
#ifndef ZB_CORE_ALLOC_H
#define ZB_CORE_ALLOC_H

#include "core/base.h"

typedef struct zb_allocator {
  void *(*alloc)(void *ctx, size_t size, const char *file, int line);
  void *(*realloc)(void *ctx, void *ptr, size_t size, const char *file, int line);
  void (*free)(void *ctx, void *ptr, const char *file, int line);
  void *ctx;
} zb_allocator;

#define zb_alloc(a, size) ((a)->alloc((a)->ctx, (size), __FILE__, __LINE__))
#define zb_realloc(a, ptr, size) ((a)->realloc((a)->ctx, (ptr), (size), __FILE__, __LINE__))
#define zb_free(a, ptr) ((a)->free((a)->ctx, (ptr), __FILE__, __LINE__))

/* Allocator backed by malloc/realloc/free. */
const zb_allocator *zb_heap_allocator(void);

/* Allocate zeroed memory. */
void *zb_calloc_at(const zb_allocator *a, size_t size, const char *file, int line);
#define zb_calloc(a, size) zb_calloc_at((a), (size), __FILE__, __LINE__)

/* Duplicate a string (or the first len bytes of it) with the allocator. */
char *zb_strdup_at(const zb_allocator *a, const char *s, const char *file, int line);
char *zb_strndup_at(const zb_allocator *a, const char *s, size_t len, const char *file, int line);
#define zb_strdup(a, s) zb_strdup_at((a), (s), __FILE__, __LINE__)
#define zb_strndup(a, s, n) zb_strndup_at((a), (s), (n), __FILE__, __LINE__)

/* ---- Arena ------------------------------------------------------------------ */

typedef struct zb_arena_block zb_arena_block;

typedef struct zb_arena {
  const zb_allocator *parent;
  zb_arena_block *head;
  size_t block_size;
  size_t total;
} zb_arena;

void zb_arena_init(zb_arena *arena, const zb_allocator *parent, size_t block_size);
/* Returns NULL when the parent allocator fails. align must be a power of two. */
void *zb_arena_alloc(zb_arena *arena, size_t size, size_t align);
char *zb_arena_strndup(zb_arena *arena, const char *s, size_t len);
/* Release all blocks but the first and make the arena empty again. */
void zb_arena_reset(zb_arena *arena);
void zb_arena_destroy(zb_arena *arena);

/* ---- Tracking allocator ---------------------------------------------------- */

typedef struct zb_tracking_entry {
  void *ptr;
  size_t size;
  const char *file;
  int line;
} zb_tracking_entry;

typedef struct zb_tracking_lock {
  void (*lock)(void *mutex);
  void (*unlock)(void *mutex);
  void *mutex;
} zb_tracking_lock;

typedef struct zb_tracking_allocator {
  zb_allocator iface;
  const zb_allocator *parent;
  zb_tracking_entry *entries;
  size_t capacity;
  size_t count;
  size_t bytes;
  size_t peak_bytes;
  size_t total_allocations;
  zb_tracking_lock lock;
} zb_tracking_allocator;

/* Wrap parent; every live allocation is recorded with its call site. */
zb_result zb_tracking_init(zb_tracking_allocator *t, const zb_allocator *parent);
/* Optional lock for allocations from several threads. */
void zb_tracking_set_lock(zb_tracking_allocator *t, zb_tracking_lock lock);
const zb_allocator *zb_tracking_iface(zb_tracking_allocator *t);
size_t zb_tracking_live_count(const zb_tracking_allocator *t);
size_t zb_tracking_live_bytes(const zb_tracking_allocator *t);
/* Calls fn for every live allocation; returns the number of live allocations. */
size_t zb_tracking_report(const zb_tracking_allocator *t,
                          void (*fn)(const zb_tracking_entry *e, void *user), void *user);
void zb_tracking_destroy(zb_tracking_allocator *t);

#endif

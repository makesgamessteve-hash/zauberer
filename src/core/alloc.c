/* SPDX-License-Identifier: MIT */
#include "core/alloc.h"

#include <stdlib.h>
#include <string.h>

/* ---- Heap allocator --------------------------------------------------------- */

static void *heap_alloc(void *ctx, size_t size, const char *file, int line) {
  (void)ctx;
  (void)file;
  (void)line;
  return malloc(size ? size : 1);
}

static void *heap_realloc(void *ctx, void *ptr, size_t size, const char *file, int line) {
  (void)ctx;
  (void)file;
  (void)line;
  return realloc(ptr, size ? size : 1);
}

static void heap_free(void *ctx, void *ptr, const char *file, int line) {
  (void)ctx;
  (void)file;
  (void)line;
  free(ptr);
}

static const zb_allocator k_heap = {heap_alloc, heap_realloc, heap_free, NULL};

const zb_allocator *zb_heap_allocator(void) { return &k_heap; }

void *zb_calloc_at(const zb_allocator *a, size_t size, const char *file, int line) {
  void *p = a->alloc(a->ctx, size, file, line);
  if (p) {
    memset(p, 0, size);
  }
  return p;
}

char *zb_strndup_at(const zb_allocator *a, const char *s, size_t len, const char *file, int line) {
  char *p = a->alloc(a->ctx, len + 1, file, line);
  if (p) {
    memcpy(p, s, len);
    p[len] = '\0';
  }
  return p;
}

char *zb_strdup_at(const zb_allocator *a, const char *s, const char *file, int line) {
  return zb_strndup_at(a, s, strlen(s), file, line);
}

/* ---- Arena ------------------------------------------------------------------ */

struct zb_arena_block {
  zb_arena_block *next;
  size_t size;
  size_t used;
  /* data follows, aligned to max_align_t */
};

#define ZB_ARENA_HEADER (((sizeof(zb_arena_block) + 15u) / 16u) * 16u)

void zb_arena_init(zb_arena *arena, const zb_allocator *parent, size_t block_size) {
  arena->parent = parent;
  arena->head = NULL;
  arena->block_size = block_size ? block_size : 4096;
  arena->total = 0;
}

static zb_arena_block *arena_new_block(zb_arena *arena, size_t min_size) {
  size_t size = ZB_MAX(arena->block_size, min_size);
  zb_arena_block *b = zb_alloc(arena->parent, ZB_ARENA_HEADER + size);
  if (!b) {
    return NULL;
  }
  b->next = arena->head;
  b->size = size;
  b->used = 0;
  arena->head = b;
  return b;
}

static void *arena_try(zb_arena_block *b, size_t size, size_t align) {
  uintptr_t base = (uintptr_t)b + ZB_ARENA_HEADER;
  uintptr_t aligned = (base + b->used + (align - 1)) & ~(uintptr_t)(align - 1);
  size_t offset = (size_t)(aligned - base);
  if (offset + size > b->size) {
    return NULL;
  }
  b->used = offset + size;
  return (void *)aligned;
}

void *zb_arena_alloc(zb_arena *arena, size_t size, size_t align) {
  ZB_ASSERT(align != 0 && (align & (align - 1)) == 0);
  void *p = arena->head ? arena_try(arena->head, size, align) : NULL;
  if (!p) {
    zb_arena_block *b = arena_new_block(arena, size + align);
    if (!b) {
      return NULL;
    }
    p = arena_try(b, size, align);
  }
  arena->total += size;
  return p;
}

char *zb_arena_strndup(zb_arena *arena, const char *s, size_t len) {
  char *p = zb_arena_alloc(arena, len + 1, 1);
  if (p) {
    memcpy(p, s, len);
    p[len] = '\0';
  }
  return p;
}

void zb_arena_reset(zb_arena *arena) {
  zb_arena_block *b = arena->head;
  while (b && b->next) {
    zb_arena_block *next = b->next;
    zb_free(arena->parent, b);
    b = next;
  }
  arena->head = b;
  if (b) {
    b->used = 0;
  }
  arena->total = 0;
}

void zb_arena_destroy(zb_arena *arena) {
  zb_arena_block *b = arena->head;
  while (b) {
    zb_arena_block *next = b->next;
    zb_free(arena->parent, b);
    b = next;
  }
  arena->head = NULL;
  arena->total = 0;
}

/* ---- Tracking allocator ---------------------------------------------------- */

static void *const k_tombstone = (void *)&k_heap;

static size_t ptr_slot(const void *p, size_t capacity) {
  uint64_t x = (uint64_t)(uintptr_t)p;
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  return (size_t)(x & (uint64_t)(capacity - 1));
}

static bool tracking_insert_raw(zb_tracking_entry *entries, size_t capacity, zb_tracking_entry e) {
  size_t i = ptr_slot(e.ptr, capacity);
  for (size_t n = 0; n < capacity; ++n) {
    zb_tracking_entry *slot = &entries[(i + n) & (capacity - 1)];
    if (slot->ptr == NULL || slot->ptr == k_tombstone) {
      *slot = e;
      return true;
    }
  }
  return false;
}

static bool tracking_grow(zb_tracking_allocator *t) {
  size_t cap = t->capacity ? t->capacity * 2 : 256;
  zb_tracking_entry *entries = zb_calloc(t->parent, cap * sizeof(*entries));
  if (!entries) {
    return false;
  }
  for (size_t i = 0; i < t->capacity; ++i) {
    if (t->entries[i].ptr && t->entries[i].ptr != k_tombstone) {
      tracking_insert_raw(entries, cap, t->entries[i]);
    }
  }
  zb_free(t->parent, t->entries);
  t->entries = entries;
  t->capacity = cap;
  return true;
}

static zb_tracking_entry *tracking_find(zb_tracking_allocator *t, const void *p) {
  if (!t->capacity) {
    return NULL;
  }
  size_t i = ptr_slot(p, t->capacity);
  for (size_t n = 0; n < t->capacity; ++n) {
    zb_tracking_entry *slot = &t->entries[(i + n) & (t->capacity - 1)];
    if (slot->ptr == p) {
      return slot;
    }
    if (slot->ptr == NULL) {
      return NULL;
    }
  }
  return NULL;
}

static void tracking_lock(zb_tracking_allocator *t) {
  if (t->lock.lock) {
    t->lock.lock(t->lock.mutex);
  }
}

static void tracking_unlock(zb_tracking_allocator *t) {
  if (t->lock.unlock) {
    t->lock.unlock(t->lock.mutex);
  }
}

/* Makes room for one more entry; must be called with the lock held. */
static bool tracking_reserve(zb_tracking_allocator *t) { return (t->count + 1) * 2 <= t->capacity || tracking_grow(t); }

static void tracking_record(zb_tracking_allocator *t, void *p, size_t size, const char *file, int line) {
  zb_tracking_entry e = {p, size, file, line};
  tracking_insert_raw(t->entries, t->capacity, e);
  t->count++;
  t->bytes += size;
  t->total_allocations++;
  if (t->bytes > t->peak_bytes) {
    t->peak_bytes = t->bytes;
  }
}

static void tracking_forget(zb_tracking_allocator *t, void *ptr) {
  zb_tracking_entry *e = tracking_find(t, ptr);
  ZB_ASSERT(e != NULL && "free of untracked pointer");
  if (e) {
    t->count--;
    t->bytes -= e->size;
    e->ptr = k_tombstone;
  }
}

static void *tracking_alloc(void *ctx, size_t size, const char *file, int line) {
  zb_tracking_allocator *t = ctx;
  void *p = NULL;
  tracking_lock(t);
  if (tracking_reserve(t)) {
    p = t->parent->alloc(t->parent->ctx, size, file, line);
    if (p) {
      tracking_record(t, p, size, file, line);
    }
  }
  tracking_unlock(t);
  return p;
}

static void tracking_free(void *ctx, void *ptr, const char *file, int line) {
  zb_tracking_allocator *t = ctx;
  if (!ptr) {
    return;
  }
  tracking_lock(t);
  tracking_forget(t, ptr);
  tracking_unlock(t);
  t->parent->free(t->parent->ctx, ptr, file, line);
}

static void *tracking_realloc(void *ctx, void *ptr, size_t size, const char *file, int line) {
  zb_tracking_allocator *t = ctx;
  if (!ptr) {
    return tracking_alloc(ctx, size, file, line);
  }
  void *p = NULL;
  tracking_lock(t);
  if (tracking_reserve(t)) {
    p = t->parent->realloc(t->parent->ctx, ptr, size, file, line);
    if (p) {
      tracking_forget(t, ptr);
      tracking_record(t, p, size, file, line);
    }
  }
  tracking_unlock(t);
  return p;
}

zb_result zb_tracking_init(zb_tracking_allocator *t, const zb_allocator *parent) {
  memset(t, 0, sizeof(*t));
  t->parent = parent;
  t->iface.alloc = tracking_alloc;
  t->iface.realloc = tracking_realloc;
  t->iface.free = tracking_free;
  t->iface.ctx = t;
  return tracking_grow(t) ? ZB_OK : ZB_ERR_NOMEM;
}

void zb_tracking_set_lock(zb_tracking_allocator *t, zb_tracking_lock lock) { t->lock = lock; }

const zb_allocator *zb_tracking_iface(zb_tracking_allocator *t) { return &t->iface; }

size_t zb_tracking_live_count(const zb_tracking_allocator *t) { return t->count; }

size_t zb_tracking_live_bytes(const zb_tracking_allocator *t) { return t->bytes; }

size_t zb_tracking_report(const zb_tracking_allocator *t, void (*fn)(const zb_tracking_entry *e, void *user),
                          void *user) {
  size_t n = 0;
  for (size_t i = 0; i < t->capacity; ++i) {
    const zb_tracking_entry *e = &t->entries[i];
    if (e->ptr && e->ptr != k_tombstone) {
      if (fn) {
        fn(e, user);
      }
      n++;
    }
  }
  return n;
}

void zb_tracking_destroy(zb_tracking_allocator *t) {
  zb_free(t->parent, t->entries);
  t->entries = NULL;
  t->capacity = 0;
  t->count = 0;
  t->bytes = 0;
}

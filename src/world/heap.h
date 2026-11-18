/* SPDX-License-Identifier: MIT */
/* A binary min-heap of (key, id, aux) for world algorithms (Dijkstra,
 * priority-flood). Equal keys pop in id order, so results are deterministic. */
#ifndef ZB_WORLD_HEAP_H
#define ZB_WORLD_HEAP_H

#include "core/alloc.h"

typedef struct zb_heap_item {
  double key;
  int32_t id, aux;
} zb_heap_item;

typedef struct zb_heap {
  const zb_allocator *a;
  zb_heap_item *items;
  size_t len, cap;
  bool failed; /* a push ran out of memory (the item was dropped) */
} zb_heap;

static inline void zb_heap_init(zb_heap *h, const zb_allocator *a) {
  h->a = a;
  h->items = NULL;
  h->len = h->cap = 0;
  h->failed = false;
}

static inline void zb_heap_free(zb_heap *h) {
  zb_free(h->a, h->items);
  h->items = NULL;
  h->len = h->cap = 0;
}

static inline bool zb_heap_less(const zb_heap_item *x, const zb_heap_item *y) {
  return x->key < y->key || (x->key == y->key && x->id < y->id);
}

/* Room for `cap` items without growing (false when out of memory). */
static inline bool zb_heap_reserve(zb_heap *h, size_t cap) {
  if (cap <= h->cap) {
    return true;
  }
  zb_heap_item *n = zb_realloc(h->a, h->items, sizeof(zb_heap_item) * cap);
  if (!n) {
    return false;
  }
  h->items = n;
  h->cap = cap;
  return true;
}

static inline void zb_heap_push(zb_heap *h, double key, int32_t id, int32_t aux) {
  if (h->len == h->cap && !zb_heap_reserve(h, h->cap ? h->cap * 2 : 64)) {
    h->failed = true;
    return;
  }
  size_t i = h->len++;
  zb_heap_item it = {key, id, aux};
  while (i > 0) {
    size_t parent = (i - 1) / 2;
    if (!zb_heap_less(&it, &h->items[parent])) {
      break;
    }
    h->items[i] = h->items[parent];
    i = parent;
  }
  h->items[i] = it;
}

static inline bool zb_heap_pop(zb_heap *h, zb_heap_item *out) {
  if (h->len == 0) {
    return false;
  }
  *out = h->items[0];
  zb_heap_item last = h->items[--h->len];
  size_t i = 0;
  for (;;) {
    size_t c = 2 * i + 1;
    if (c >= h->len) {
      break;
    }
    if (c + 1 < h->len && zb_heap_less(&h->items[c + 1], &h->items[c])) {
      c++;
    }
    if (!zb_heap_less(&h->items[c], &last)) {
      break;
    }
    h->items[i] = h->items[c];
    i = c;
  }
  if (h->len > 0) {
    h->items[i] = last;
  }
  return true;
}

#endif

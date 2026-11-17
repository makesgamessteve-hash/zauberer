/* SPDX-License-Identifier: MIT */
/* Growable arrays, a uint64 hash map and string interning. */
#ifndef ZB_CORE_ARRAY_H
#define ZB_CORE_ARRAY_H

#include "core/alloc.h"

/* ---- Dynamic array ------------------------------------------------------- */

typedef struct zb_array {
  const zb_allocator *a;
  unsigned char *data;
  size_t len;
  size_t cap;
  size_t elem;
} zb_array;

void zb_array_init(zb_array *arr, const zb_allocator *a, size_t elem_size);
zb_result zb_array_reserve(zb_array *arr, size_t cap);
/* Appends a copy of elem (or zeroes when elem is NULL); returns the new slot. */
void *zb_array_push(zb_array *arr, const void *elem);
void *zb_array_at(const zb_array *arr, size_t index);
void zb_array_pop(zb_array *arr);
/* Removes index by moving the last element into it (order not kept). */
void zb_array_remove_swap(zb_array *arr, size_t index);
/* Removes index keeping order. */
void zb_array_remove(zb_array *arr, size_t index);
void zb_array_clear(zb_array *arr);
void zb_array_free(zb_array *arr);

/* ---- Hash map: uint64 key -> uint64 value ------------------------------- */

typedef struct zb_map_slot {
  uint64_t key;
  uint64_t value;
  uint8_t state; /* 0 empty, 1 used, 2 deleted */
} zb_map_slot;

typedef struct zb_map {
  const zb_allocator *a;
  zb_map_slot *slots;
  size_t cap;
  size_t count;
  size_t used; /* used + deleted */
} zb_map;

void zb_map_init(zb_map *m, const zb_allocator *a);
zb_result zb_map_put(zb_map *m, uint64_t key, uint64_t value);
bool zb_map_get(const zb_map *m, uint64_t key, uint64_t *value);
bool zb_map_remove(zb_map *m, uint64_t key);
/* Iterate: start with *cursor = 0; returns false when done. */
bool zb_map_next(const zb_map *m, size_t *cursor, uint64_t *key, uint64_t *value);
void zb_map_clear(zb_map *m);
void zb_map_free(zb_map *m);

/* ---- String interning -------------------------------------------------- */

typedef struct zb_strtab {
  zb_arena arena;
  zb_map index;     /* hash -> first id with that hash (chained through next) */
  zb_array strings; /* const char * */
  zb_array lengths; /* size_t */
  zb_array next;    /* uint32_t: next id with the same hash, or UINT32_MAX */
} zb_strtab;

#define ZB_STR_NONE UINT32_MAX

void zb_strtab_init(zb_strtab *t, const zb_allocator *a);
/* Returns the id for s, adding it if needed; ZB_STR_NONE when out of memory. */
uint32_t zb_strtab_intern(zb_strtab *t, const char *s, size_t len);
uint32_t zb_strtab_find(const zb_strtab *t, const char *s, size_t len);
const char *zb_strtab_get(const zb_strtab *t, uint32_t id);
size_t zb_strtab_count(const zb_strtab *t);
void zb_strtab_free(zb_strtab *t);

#endif

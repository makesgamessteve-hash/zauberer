/* SPDX-License-Identifier: MIT */
#include "core/array.h"

#include "core/hash.h"

#include <string.h>

/* ---- Dynamic array ------------------------------------------------------- */

void zb_array_init(zb_array *arr, const zb_allocator *a, size_t elem_size) {
  arr->a = a;
  arr->data = NULL;
  arr->len = 0;
  arr->cap = 0;
  arr->elem = elem_size;
}

zb_result zb_array_reserve(zb_array *arr, size_t cap) {
  if (cap <= arr->cap) {
    return ZB_OK;
  }
  unsigned char *data = zb_realloc(arr->a, arr->data, cap * arr->elem);
  if (!data) {
    return ZB_ERR_NOMEM;
  }
  arr->data = data;
  arr->cap = cap;
  return ZB_OK;
}

void *zb_array_push(zb_array *arr, const void *elem) {
  if (arr->len == arr->cap && zb_array_reserve(arr, arr->cap ? arr->cap * 2 : 8) != ZB_OK) {
    return NULL;
  }
  unsigned char *slot = arr->data + arr->len * arr->elem;
  if (elem) {
    memcpy(slot, elem, arr->elem);
  } else {
    memset(slot, 0, arr->elem);
  }
  arr->len++;
  return slot;
}

void *zb_array_at(const zb_array *arr, size_t index) {
  ZB_ASSERT(index < arr->len);
  return arr->data + index * arr->elem;
}

void zb_array_pop(zb_array *arr) {
  ZB_ASSERT(arr->len > 0);
  arr->len--;
}

void zb_array_remove_swap(zb_array *arr, size_t index) {
  ZB_ASSERT(index < arr->len);
  if (index != arr->len - 1) {
    memcpy(arr->data + index * arr->elem, arr->data + (arr->len - 1) * arr->elem, arr->elem);
  }
  arr->len--;
}

void zb_array_remove(zb_array *arr, size_t index) {
  ZB_ASSERT(index < arr->len);
  memmove(arr->data + index * arr->elem, arr->data + (index + 1) * arr->elem, (arr->len - index - 1) * arr->elem);
  arr->len--;
}

void zb_array_clear(zb_array *arr) { arr->len = 0; }

void zb_array_free(zb_array *arr) {
  zb_free(arr->a, arr->data);
  arr->data = NULL;
  arr->len = 0;
  arr->cap = 0;
}

/* ---- Hash map ----------------------------------------------------------- */

void zb_map_init(zb_map *m, const zb_allocator *a) {
  m->a = a;
  m->slots = NULL;
  m->cap = 0;
  m->count = 0;
  m->used = 0;
}

static zb_map_slot *map_lookup(const zb_map *m, uint64_t key) {
  size_t i = (size_t)(zb_mix64(key) & (m->cap - 1));
  for (;;) {
    zb_map_slot *s = &m->slots[i];
    if (s->state == 0 || (s->state == 1 && s->key == key)) {
      return s;
    }
    i = (i + 1) & (m->cap - 1);
  }
}

static zb_result map_rehash(zb_map *m, size_t cap) {
  zb_map_slot *old = m->slots;
  size_t old_cap = m->cap;
  zb_map_slot *slots = zb_calloc(m->a, cap * sizeof(*slots));
  if (!slots) {
    return ZB_ERR_NOMEM;
  }
  m->slots = slots;
  m->cap = cap;
  m->count = 0;
  m->used = 0;
  for (size_t i = 0; i < old_cap; ++i) {
    if (old[i].state == 1) {
      zb_map_slot *s = map_lookup(m, old[i].key);
      *s = old[i];
      m->count++;
      m->used++;
    }
  }
  zb_free(m->a, old);
  return ZB_OK;
}

zb_result zb_map_put(zb_map *m, uint64_t key, uint64_t value) {
  if ((m->used + 1) * 4 > m->cap * 3) {
    size_t cap = m->cap ? m->cap : 16;
    while ((m->count + 1) * 2 > cap) {
      cap *= 2;
    }
    zb_result r = map_rehash(m, cap);
    if (r != ZB_OK) {
      return r;
    }
  }
  zb_map_slot *s = map_lookup(m, key);
  if (s->state == 0) {
    s->state = 1;
    s->key = key;
    m->count++;
    m->used++;
  }
  s->value = value;
  return ZB_OK;
}

bool zb_map_get(const zb_map *m, uint64_t key, uint64_t *value) {
  if (!m->cap) {
    return false;
  }
  zb_map_slot *s = map_lookup(m, key);
  if (s->state != 1) {
    return false;
  }
  if (value) {
    *value = s->value;
  }
  return true;
}

bool zb_map_remove(zb_map *m, uint64_t key) {
  if (!m->cap) {
    return false;
  }
  zb_map_slot *s = map_lookup(m, key);
  if (s->state != 1) {
    return false;
  }
  s->state = 2;
  m->count--;
  return true;
}

bool zb_map_next(const zb_map *m, size_t *cursor, uint64_t *key, uint64_t *value) {
  while (*cursor < m->cap) {
    const zb_map_slot *s = &m->slots[(*cursor)++];
    if (s->state == 1) {
      *key = s->key;
      *value = s->value;
      return true;
    }
  }
  return false;
}

void zb_map_clear(zb_map *m) {
  if (m->slots) {
    memset(m->slots, 0, m->cap * sizeof(*m->slots));
  }
  m->count = 0;
  m->used = 0;
}

void zb_map_free(zb_map *m) {
  zb_free(m->a, m->slots);
  m->slots = NULL;
  m->cap = 0;
  m->count = 0;
  m->used = 0;
}

/* ---- String interning -------------------------------------------------- */

void zb_strtab_init(zb_strtab *t, const zb_allocator *a) {
  zb_arena_init(&t->arena, a, 4096);
  zb_map_init(&t->index, a);
  zb_array_init(&t->strings, a, sizeof(const char *));
  zb_array_init(&t->lengths, a, sizeof(size_t));
  zb_array_init(&t->next, a, sizeof(uint32_t));
}

uint32_t zb_strtab_find(const zb_strtab *t, const char *s, size_t len) {
  uint64_t first;
  if (!zb_map_get(&t->index, zb_hash64(s, len, 0), &first)) {
    return ZB_STR_NONE;
  }
  for (uint32_t id = (uint32_t)first; id != ZB_STR_NONE; id = *(uint32_t *)zb_array_at(&t->next, id)) {
    size_t l = *(size_t *)zb_array_at(&t->lengths, id);
    const char *str = *(const char **)zb_array_at(&t->strings, id);
    if (l == len && memcmp(str, s, len) == 0) {
      return id;
    }
  }
  return ZB_STR_NONE;
}

uint32_t zb_strtab_intern(zb_strtab *t, const char *s, size_t len) {
  uint32_t found = zb_strtab_find(t, s, len);
  if (found != ZB_STR_NONE) {
    return found;
  }
  uint64_t h = zb_hash64(s, len, 0);
  uint64_t first = ZB_STR_NONE;
  zb_map_get(&t->index, h, &first);
  uint32_t id = (uint32_t)t->strings.len;
  char *copy = zb_arena_strndup(&t->arena, s, len);
  uint32_t next = (uint32_t)first;
  if (!copy || zb_array_reserve(&t->strings, t->strings.len + 1) != ZB_OK ||
      zb_array_reserve(&t->lengths, t->lengths.len + 1) != ZB_OK ||
      zb_array_reserve(&t->next, t->next.len + 1) != ZB_OK || zb_map_put(&t->index, h, id) != ZB_OK) {
    return ZB_STR_NONE;
  }
  zb_array_push(&t->strings, &copy);
  zb_array_push(&t->lengths, &len);
  zb_array_push(&t->next, &next);
  return id;
}

const char *zb_strtab_get(const zb_strtab *t, uint32_t id) {
  if (id >= t->strings.len) {
    return NULL;
  }
  return *(const char **)zb_array_at(&t->strings, id);
}

size_t zb_strtab_count(const zb_strtab *t) { return t->strings.len; }

void zb_strtab_free(zb_strtab *t) {
  zb_arena_destroy(&t->arena);
  zb_map_free(&t->index);
  zb_array_free(&t->strings);
  zb_array_free(&t->lengths);
  zb_array_free(&t->next);
}

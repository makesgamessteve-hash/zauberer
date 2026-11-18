/* SPDX-License-Identifier: MIT */
#include "world/delta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/deflate.h"
#include "core/hash.h"

#define MAGIC "ZBRD"
#define HEADER 33 /* magic 4, version 4, face 1, rx 4, ry 4, raw size 4, checksum 8, compressed size 4 */
#define EDIT_BYTES 10
#define MAX_RAW (64u << 20)

typedef struct chunk_edits {
  zb_chunk chunk;
  zb_array edits; /* zb_edit */
} chunk_edits;

struct zb_deltas {
  const zb_allocator *a;
  zb_map index;   /* chunk key -> position in lists */
  zb_array lists; /* chunk_edits */
  zb_map dirty;   /* region key -> 1 */
};

static uint64_t region_key(int face, int32_t rx, int32_t ry) {
  return (uint64_t)(uint32_t)face << 56 | (uint64_t)((uint32_t)rx & 0xffffffu) << 28 | ((uint32_t)ry & 0xffffffu);
}

/* Appends n bytes to a byte array. */
static bool append(zb_array *arr, const void *p, size_t n) {
  if (zb_array_reserve(arr, arr->len + n) != ZB_OK) {
    return false;
  }
  memcpy((uint8_t *)arr->data + arr->len, p, n);
  arr->len += n;
  return true;
}

static int32_t region_of(int32_t v) { return v >= 0 ? v / ZB_REGION_CHUNKS : -1 - (-1 - v) / ZB_REGION_CHUNKS; }

zb_deltas *zb_deltas_create(const zb_allocator *a) {
  zb_deltas *d = zb_calloc(a, sizeof *d);
  if (!d) {
    return NULL;
  }
  d->a = a;
  zb_map_init(&d->index, a);
  zb_array_init(&d->lists, a, sizeof(chunk_edits));
  zb_map_init(&d->dirty, a);
  return d;
}

void zb_deltas_destroy(zb_deltas *d) {
  if (!d) {
    return;
  }
  for (size_t i = 0; i < d->lists.len; ++i) {
    zb_array_free(&((chunk_edits *)zb_array_at(&d->lists, i))->edits);
  }
  zb_array_free(&d->lists);
  zb_map_free(&d->index);
  zb_map_free(&d->dirty);
  zb_free(d->a, d);
}

static bool same_target(const zb_edit *a, const zb_edit *b) {
  if (a->kind != b->kind) {
    return false;
  }
  return a->kind == ZB_EDIT_PLANT_REMOVED ? a->index == b->index : (a->x == b->x && a->y == b->y);
}

static zb_result add(zb_deltas *d, zb_chunk c, const zb_edit *e, bool dirty) {
  uint64_t at;
  chunk_edits *list;
  if (zb_map_get(&d->index, zb_chunk_key(c), &at)) {
    list = zb_array_at(&d->lists, (size_t)at);
  } else {
    chunk_edits fresh;
    fresh.chunk = c;
    zb_array_init(&fresh.edits, d->a, sizeof(zb_edit));
    list = zb_array_push(&d->lists, &fresh);
    if (!list) {
      return ZB_ERR_NOMEM;
    }
    if (zb_map_put(&d->index, zb_chunk_key(c), d->lists.len - 1) != ZB_OK) {
      zb_array_pop(&d->lists);
      return ZB_ERR_NOMEM;
    }
  }
  bool replaced = false;
  for (size_t i = 0; i < list->edits.len && !replaced; ++i) {
    zb_edit *old = zb_array_at(&list->edits, i);
    if (same_target(old, e)) {
      *old = *e;
      replaced = true;
    }
  }
  if (!replaced && !zb_array_push(&list->edits, e)) {
    return ZB_ERR_NOMEM;
  }
  if (dirty) {
    return zb_map_put(&d->dirty, region_key(c.face, region_of(c.x), region_of(c.y)), 1);
  }
  return ZB_OK;
}

zb_result zb_deltas_add(zb_deltas *d, zb_chunk c, const zb_edit *e) {
  if ((e->kind != ZB_EDIT_PLANT_REMOVED && e->kind != ZB_EDIT_MATERIAL) ||
      (e->kind == ZB_EDIT_MATERIAL &&
       (e->x >= ZB_SURFACE_RES || e->y >= ZB_SURFACE_RES || e->value == ZB_MAT_NONE || e->value >= ZB_MAT_COUNT))) {
    return ZB_ERR_INVALID;
  }
  return add(d, c, e, true);
}

const zb_edit *zb_deltas_get(const zb_deltas *d, zb_chunk c, size_t *count) {
  uint64_t at;
  if (!zb_map_get(&d->index, zb_chunk_key(c), &at)) {
    *count = 0;
    return NULL;
  }
  const chunk_edits *list = zb_array_at(&d->lists, (size_t)at);
  *count = list->edits.len;
  return (const zb_edit *)list->edits.data;
}

size_t zb_deltas_chunks(const zb_deltas *d) { return d->lists.len; }

bool zb_deltas_apply(const zb_deltas *d, zb_chunk_terrain *ct) {
  size_t n;
  const zb_edit *e = zb_deltas_get(d, ct->chunk, &n);
  bool changed = false;
  for (size_t i = 0; i < n; ++i) {
    if (e[i].kind == ZB_EDIT_PLANT_REMOVED && ct->plants && e[i].index < ct->plant_count &&
        ct->plants[e[i].index].layer != ZB_PLANT_REMOVED) {
      ct->plants[e[i].index].layer = ZB_PLANT_REMOVED;
      changed = true;
    } else if (e[i].kind == ZB_EDIT_MATERIAL && ct->material) {
      uint8_t *m = &ct->material[e[i].y * ZB_SURFACE_RES + e[i].x];
      changed |= *m != e[i].value;
      *m = e[i].value;
    }
  }
  return changed;
}

static void call_region(uint64_t key, zb_region_fn fn, void *ctx) {
  int32_t rx = (int32_t)((key >> 28) & 0xffffffu), ry = (int32_t)(key & 0xffffffu);
  rx = rx & 0x800000 ? rx - 0x1000000 : rx;
  ry = ry & 0x800000 ? ry - 0x1000000 : ry;
  fn(ctx, (int)(key >> 56), rx, ry);
}

void zb_deltas_each_dirty(const zb_deltas *d, zb_region_fn fn, void *ctx) {
  size_t cursor = 0;
  uint64_t key, value;
  while (zb_map_next(&d->dirty, &cursor, &key, &value)) {
    call_region(key, fn, ctx);
  }
}

zb_result zb_deltas_each_region(const zb_deltas *d, zb_region_fn fn, void *ctx) {
  zb_map seen;
  zb_map_init(&seen, d->a);
  for (size_t i = 0; i < d->lists.len; ++i) {
    const chunk_edits *l = zb_array_at(&d->lists, i);
    uint64_t key = region_key(l->chunk.face, region_of(l->chunk.x), region_of(l->chunk.y)), v;
    if (l->edits.len && !zb_map_get(&seen, key, &v)) {
      if (zb_map_put(&seen, key, 1) != ZB_OK) {
        zb_map_free(&seen);
        return ZB_ERR_NOMEM;
      }
      call_region(key, fn, ctx);
    }
  }
  zb_map_free(&seen);
  return ZB_OK;
}

void zb_deltas_clear_dirty(zb_deltas *d) { zb_map_clear(&d->dirty); }

void zb_region_file_name(int face, int32_t rx, int32_t ry, char *out, size_t len) {
  snprintf(out, len, "%d.%ld.%ld.bin", face, (long)rx, (long)ry);
}

/* ---- Region files --------------------------------------------------------------- */

static void put_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static uint32_t get_u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int cmp_lists(const void *a, const void *b) {
  const chunk_edits *x = *(const chunk_edits *const *)a, *y = *(const chunk_edits *const *)b;
  uint64_t kx = zb_chunk_key(x->chunk), ky = zb_chunk_key(y->chunk);
  return kx < ky ? -1 : (kx > ky);
}

zb_result zb_deltas_write_region(const zb_deltas *d, int face, int32_t rx, int32_t ry, zb_array *out) {
  /* The region's chunks, in a fixed order. */
  zb_array mine, raw;
  zb_array_init(&mine, d->a, sizeof(const chunk_edits *));
  zb_array_init(&raw, d->a, 1);
  zb_result r = ZB_OK;
  for (size_t i = 0; i < d->lists.len && r == ZB_OK; ++i) {
    const chunk_edits *l = zb_array_at(&d->lists, i);
    if (l->chunk.face == face && region_of(l->chunk.x) == rx && region_of(l->chunk.y) == ry && l->edits.len &&
        !zb_array_push(&mine, &l)) {
      r = ZB_ERR_NOMEM;
    }
  }
  if (mine.len > 1) {
    qsort(mine.data, mine.len, sizeof(const chunk_edits *), cmp_lists);
  }
  uint8_t b[12];
  put_u32(b, (uint32_t)mine.len);
  r = r == ZB_OK && !append(&raw, b, 4) ? ZB_ERR_NOMEM : r;
  for (size_t i = 0; i < mine.len && r == ZB_OK; ++i) {
    const chunk_edits *l = *(const chunk_edits *const *)zb_array_at(&mine, i);
    put_u32(b, (uint32_t)l->chunk.x);
    put_u32(b + 4, (uint32_t)l->chunk.y);
    put_u32(b + 8, (uint32_t)l->edits.len);
    r = append(&raw, b, 12) ? ZB_OK : ZB_ERR_NOMEM;
    for (size_t k = 0; k < l->edits.len && r == ZB_OK; ++k) {
      const zb_edit *e = zb_array_at(&l->edits, k);
      uint8_t eb[EDIT_BYTES] = {
          e->kind, e->value, (uint8_t)e->x, (uint8_t)(e->x >> 8), (uint8_t)e->y, (uint8_t)(e->y >> 8)};
      put_u32(eb + 6, e->index);
      r = append(&raw, eb, EDIT_BYTES) ? ZB_OK : ZB_ERR_NOMEM;
    }
  }
  uint8_t head[HEADER];
  memcpy(head, MAGIC, 4);
  put_u32(head + 4, ZB_DELTA_VERSION);
  head[8] = (uint8_t)face;
  put_u32(head + 9, (uint32_t)rx);
  put_u32(head + 13, (uint32_t)ry);
  put_u32(head + 17, (uint32_t)raw.len);
  uint64_t sum = zb_hash64(raw.data, raw.len, 0x7a627264);
  put_u32(head + 21, (uint32_t)sum);
  put_u32(head + 25, (uint32_t)(sum >> 32));
  size_t start = out->len;
  if (r == ZB_OK && (!append(out, head, HEADER) || zb_deflate(raw.data, raw.len, out) != ZB_OK)) {
    r = ZB_ERR_NOMEM;
  }
  if (r == ZB_OK) {
    put_u32((uint8_t *)out->data + start + 29, (uint32_t)(out->len - start - HEADER));
  } else {
    out->len = start;
  }
  zb_array_free(&mine);
  zb_array_free(&raw);
  return r;
}

static zb_result refuse(char *err, size_t err_len, const char *why) {
  snprintf(err, err_len, "%s", why);
  return ZB_ERR_INVALID;
}

zb_result zb_deltas_read_region(zb_deltas *d, const void *data, size_t len, char *err, size_t err_len) {
  const uint8_t *p = data;
  if (len < HEADER || memcmp(p, MAGIC, 4) != 0) {
    return refuse(err, err_len, "not a region file");
  }
  if (get_u32(p + 4) > ZB_DELTA_VERSION) {
    return refuse(err, err_len, "the region file comes from a newer version of the game");
  }
  int face = p[8];
  uint32_t raw_len = get_u32(p + 17), packed = get_u32(p + 29);
  uint64_t sum = (uint64_t)get_u32(p + 21) | (uint64_t)get_u32(p + 25) << 32;
  if (face >= ZB_PLANET_FACES || raw_len < 4 || raw_len > MAX_RAW || packed != len - HEADER) {
    return refuse(err, err_len, "the region file is cut short or damaged");
  }
  uint8_t *raw = zb_alloc(d->a, raw_len);
  if (!raw) {
    return ZB_ERR_NOMEM;
  }
  if (zb_inflate(p + HEADER, packed, raw, raw_len) != ZB_OK || zb_hash64(raw, raw_len, 0x7a627264) != sum) {
    zb_free(d->a, raw);
    return refuse(err, err_len, "the region file is damaged (checksum)");
  }
  zb_result r = ZB_OK;
  size_t at = 4;
  uint32_t chunks = get_u32(raw);
  for (uint32_t i = 0; i < chunks && r == ZB_OK; ++i) {
    if (at + 12 > raw_len) {
      r = refuse(err, err_len, "the region file is cut short or damaged");
      break;
    }
    zb_chunk c = {face, (int32_t)get_u32(raw + at), (int32_t)get_u32(raw + at + 4)};
    uint32_t n = get_u32(raw + at + 8);
    at += 12;
    if ((size_t)n * EDIT_BYTES > raw_len - at) {
      r = refuse(err, err_len, "the region file is cut short or damaged");
      break;
    }
    for (uint32_t k = 0; k < n && r == ZB_OK; ++k, at += EDIT_BYTES) {
      const uint8_t *eb = raw + at;
      zb_edit e = {eb[0], eb[1], (uint16_t)(eb[2] | eb[3] << 8), (uint16_t)(eb[4] | eb[5] << 8), get_u32(eb + 6)};
      bool ok =
          e.kind == ZB_EDIT_PLANT_REMOVED || (e.kind == ZB_EDIT_MATERIAL && e.x < ZB_SURFACE_RES &&
                                              e.y < ZB_SURFACE_RES && e.value > ZB_MAT_NONE && e.value < ZB_MAT_COUNT);
      r = ok ? add(d, c, &e, false) : refuse(err, err_len, "the region file holds an unknown edit");
    }
  }
  zb_free(d->a, raw);
  return r;
}

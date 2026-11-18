/* SPDX-License-Identifier: MIT */
/* Null render backend: issues no graphics calls and remembers the last frame,
 * so scene logic can be tested without a GPU. */
#include "render/backend.h"

#include <string.h>

typedef struct null_ctx {
  zb_render_backend iface;
  const zb_allocator *a;
  uint32_t next;
  size_t counts[3]; /* per zb_draw_kind */
  size_t frames;
  size_t live_meshes;
} null_ctx;

static void null_destroy(void *ctx) {
  null_ctx *n = ctx;
  zb_free(n->a, n);
}

static uint32_t null_create_texture(void *ctx, const zb_image *img) {
  (void)img;
  null_ctx *n = ctx;
  return ++n->next;
}

static void null_destroy_handle(void *ctx, uint32_t h) {
  (void)ctx;
  (void)h;
}

static uint32_t null_create_mesh(void *ctx, const zb_mesh_data *mesh) {
  (void)mesh;
  null_ctx *n = ctx;
  n->live_meshes++;
  return ++n->next;
}

static void null_destroy_mesh(void *ctx, uint32_t h) {
  null_ctx *n = ctx;
  if (h && n->live_meshes) {
    n->live_meshes--;
  }
}

static void null_execute(void *ctx, const zb_frame *frame) {
  null_ctx *n = ctx;
  memset(n->counts, 0, sizeof n->counts);
  for (size_t i = 0; i < frame->item_count; ++i) {
    n->counts[frame->items[i].kind]++;
  }
  n->frames++;
}

static zb_result null_capture(void *ctx, bool internal, const zb_allocator *a, zb_image *out) {
  (void)ctx;
  (void)internal;
  (void)a;
  memset(out, 0, sizeof(*out));
  return ZB_ERR_UNSUPPORTED;
}

zb_render_backend *zb_render_null_backend(const zb_allocator *a) {
  null_ctx *n = zb_calloc(a, sizeof(*n));
  if (!n) {
    return NULL;
  }
  n->a = a;
  n->iface.ctx = n;
  n->iface.name = "null";
  n->iface.destroy = null_destroy;
  n->iface.create_texture = null_create_texture;
  n->iface.destroy_texture = null_destroy_handle;
  n->iface.create_mesh = null_create_mesh;
  n->iface.destroy_mesh = null_destroy_mesh;
  n->iface.execute = null_execute;
  n->iface.capture = null_capture;
  return &n->iface;
}

size_t zb_render_null_item_count(const zb_render_backend *b, zb_draw_kind kind) {
  const null_ctx *n = b->ctx;
  return (unsigned)kind < 3 ? n->counts[kind] : 0;
}

size_t zb_render_null_meshes(const zb_render_backend *b) {
  const null_ctx *n = b->ctx;
  return n->live_meshes;
}

size_t zb_render_null_frames(const zb_render_backend *b) {
  const null_ctx *n = b->ctx;
  return n->frames;
}

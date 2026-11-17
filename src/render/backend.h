/* SPDX-License-Identifier: MIT */
/* Interface between the render frontend (scene -> draw lists) and a backend
 * (OpenGL, or the null backend used by headless runs and tests). */
#ifndef ZB_RENDER_BACKEND_H
#define ZB_RENDER_BACKEND_H

#include "core/math.h"
#include "render/image.h"
#include "render/mesh.h"

#define ZB_RENDER_MAX_VIEWS 2

typedef enum zb_draw_kind { ZB_DRAW_MODEL, ZB_DRAW_SPRITE } zb_draw_kind;

typedef struct zb_draw_item {
  zb_draw_kind kind;
  uint8_t view;
  uint64_t sort_key;
  uint32_t mesh;    /* backend mesh handle (models) */
  uint32_t texture; /* backend texture handle (0 = white) */
  uint32_t first;   /* first index (models) */
  uint32_t count;   /* index count (models) */
  zb_mat4 model;    /* model matrix (models) */
  zb_vec3 position; /* sprite anchor (sprites) */
  float size[2];    /* sprite world size */
  float pivot[2];   /* sprite pivot (0..1) */
  float uv[4];      /* sprite uv rect: u0, v0, u1, v1 */
} zb_draw_item;

typedef struct zb_view {
  int x, y, width, height; /* viewport inside the internal framebuffer */
  zb_mat4 view;
  zb_mat4 proj;
  zb_mat4 view_proj;
  zb_vec3 eye;
  zb_vec3 right;
  zb_vec3 up;
} zb_view;

typedef struct zb_text_vertex {
  float x, y; /* internal-framebuffer pixels, origin top-left */
  float u, v;
  float rgba[4];
} zb_text_vertex;

typedef struct zb_text_batch {
  uint32_t texture;
  uint32_t first; /* first vertex */
  uint32_t count; /* vertex count (6 per glyph) */
} zb_text_batch;

typedef struct zb_layout {
  int drawable_w, drawable_h;
  int internal_w, internal_h;
  int out_x, out_y, out_w, out_h; /* destination rect in the window (origin bottom-left) */
  int scale;                      /* integer scale, or 0 when scaled fractionally */
} zb_layout;

typedef struct zb_frame {
  zb_layout layout;
  float clear[4];
  zb_view views[ZB_RENDER_MAX_VIEWS];
  int view_count;
  zb_vec3 light_dir; /* direction the light travels */
  zb_vec3 light_color;
  zb_vec3 ambient;
  const zb_draw_item *items;
  size_t item_count;
  const zb_text_vertex *text;
  size_t text_count;
  const zb_text_batch *text_batches;
  size_t text_batch_count;
} zb_frame;

typedef struct zb_render_backend {
  void *ctx;
  const char *name;
  void (*destroy)(void *ctx);
  uint32_t (*create_texture)(void *ctx, const zb_image *img);
  void (*destroy_texture)(void *ctx, uint32_t texture);
  uint32_t (*create_mesh)(void *ctx, const zb_mesh_data *mesh);
  void (*destroy_mesh)(void *ctx, uint32_t mesh);
  void (*execute)(void *ctx, const zb_frame *frame);
  /* Reads back the internal framebuffer (internal == true) or the window. */
  zb_result (*capture)(void *ctx, bool internal, const zb_allocator *a, zb_image *out);
} zb_render_backend;

/* The null backend records the last frame for inspection. */
zb_render_backend *zb_render_null_backend(const zb_allocator *a);
size_t zb_render_null_item_count(const zb_render_backend *b, zb_draw_kind kind);
size_t zb_render_null_frames(const zb_render_backend *b);

/* The OpenGL backend. load_proc resolves GL entry points; shader sources are
 * read from packages (shaders/<name>.vert|frag). Fails (with a message naming
 * the required and detected versions) below GL 3.3 core / ES 3.0. */
typedef void (*zb_gl_fn)(void);
typedef zb_gl_fn (*zb_gl_loader)(void *ctx, const char *name);
struct zb_vfs;
zb_render_backend *zb_render_gl_backend(const zb_allocator *a, zb_gl_loader load_proc, void *loader_ctx, bool gles,
                                        struct zb_vfs *vfs, char *err, size_t err_len);
/* Parses a GL_VERSION string ("4.6 (Core Profile) Mesa", "OpenGL ES 3.0 ...",
 * "WebGL 2.0 ..."). */
bool zb_gl_parse_version(const char *s, bool *es, int *major, int *minor);
/* True when the version meets 3.3 (desktop) or 3.0 (ES/WebGL 2). */
bool zb_gl_version_ok(bool es, int major, int minor);

#endif

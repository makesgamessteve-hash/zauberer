/* SPDX-License-Identifier: MIT */
#include "render/render.h"

#include "core/array.h"
#include "core/log.h"
#include "core/luon.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum asset_kind { ASSET_TEXTURE, ASSET_MODEL, ASSET_SPRITE, ASSET_FONT } asset_kind;

typedef struct model_part {
  uint32_t first, count;
  uint32_t texture; /* backend texture (0 = white) */
} model_part;

typedef struct asset {
  asset_kind kind;
  char path[ZB_VFS_MAX_PATH];
  zb_asset_state state;
  zb_vfs_req *req;
  void *load_box; /* the request's callback context, freed on completion or release */
  /* texture */
  uint32_t tex;
  int width, height;
  /* model */
  uint32_t mesh;
  model_part *parts;
  size_t part_count;
  uint32_t *owned_textures;
  size_t owned_count;
  zb_vec3 center;
  float radius;
  /* sprite and font: texture asset handle */
  uint32_t texture_asset;
  int frame_w, frame_h, frames, fps;
  float pivot[2];
  float world_size[2];
  /* font */
  int glyph_w, glyph_h, columns;
  uint32_t *codepoints; /* glyph index -> code point */
  size_t glyph_count;
  uint32_t replacement;
  float advance;
} asset;

struct zb_render {
  const zb_allocator *a;
  zb_vfs *vfs;
  zb_render_backend *backend;
  zb_array assets; /* asset */
  int internal_h;
  int view_count;
  float clear[4];
  zb_vec3 eye, target, up;
  float fovy, znear, zfar;
  float eye_sep;
  zb_xr_view xr_views[ZB_RENDER_MAX_VIEWS];
  int xr_view_count;  /* explicit views; 0 = derived from the camera */
  float xr_hud;       /* head-locked text distance in explicit views (0 = per view) */
  float xr_hud_pixel; /* metres per canvas pixel on its plane (0 = from the views) */
  float vignette;
  uint32_t target_fb;
  int target_w, target_h; /* 0: present to the window */
  zb_vec3 light_dir, light_color, ambient;
  zb_atmosphere atmosphere;
  double time; /* seconds, for animated water and swaying plants */
  bool time_pinned;
  bool daylight;
  float foliage_tints[ZB_FOLIAGE_TINTS][3];
  uint32_t lut_asset, bayer_asset, atlas_asset;
  float dither;
  int atlas_layers;
  uint32_t white_tex, checker_tex;
  uint32_t cube_mesh;
  model_part cube_part;
  zb_vec3 cube_center;
  float cube_radius;
  /* frame state */
  zb_frame frame;
  zb_frustum frustums[ZB_RENDER_MAX_VIEWS];
  zb_array items; /* zb_draw_item */
  zb_array text;  /* zb_text_vertex */
  zb_array text_batches;
  zb_render_stats stats;
  bool in_frame;
};

static asset *asset_at(const zb_render *r, uint32_t handle) {
  if (handle == 0 || handle > r->assets.len) {
    return NULL;
  }
  return zb_array_at(&r->assets, handle - 1);
}

/* ---- Setup ----------------------------------------------------------------- */

zb_render *zb_render_create(const zb_allocator *a, zb_vfs *vfs, zb_render_backend *backend) {
  zb_render *r = zb_calloc(a, sizeof(*r));
  if (!r) {
    backend->destroy(backend->ctx);
    return NULL;
  }
  r->a = a;
  r->vfs = vfs;
  r->backend = backend;
  zb_array_init(&r->assets, a, sizeof(asset));
  zb_array_init(&r->items, a, sizeof(zb_draw_item));
  zb_array_init(&r->text, a, sizeof(zb_text_vertex));
  zb_array_init(&r->text_batches, a, sizeof(zb_text_batch));
  r->internal_h = ZB_RENDER_DEFAULT_INTERNAL_HEIGHT;
  r->view_count = 1;
  r->clear[0] = 0.05f;
  r->clear[1] = 0.05f;
  r->clear[2] = 0.1f;
  r->clear[3] = 1.0f;
  zb_render_set_camera(r, zb_v3(0, 1.5f, 4), zb_v3(0, 0, 0), zb_v3(0, 1, 0), 60.0f, 0.1f, 500.0f);
  zb_render_set_light(r, zb_v3(-0.4f, -1.0f, -0.3f), zb_v3(1, 0.95f, 0.85f), zb_v3(0.3f, 0.3f, 0.35f));
  r->eye_sep = 0.064f;
  zb_render_set_foliage_tints(r, NULL, 0);
  zb_image white = {1, 1, (unsigned char[]){255, 255, 255, 255}};
  r->white_tex = backend->create_texture(backend->ctx, &white);
  zb_image checker;
  if (zb_image_checkerboard(a, 8, &checker) == ZB_OK) {
    r->checker_tex = backend->create_texture(backend->ctx, &checker);
    zb_image_free(a, &checker);
  }
  zb_mesh_data cube;
  if (zb_mesh_cube(a, &cube) == ZB_OK) {
    r->cube_mesh = backend->create_mesh(backend->ctx, &cube);
    r->cube_part.first = 0;
    r->cube_part.count = (uint32_t)cube.index_count;
    r->cube_part.texture = r->white_tex;
    r->cube_center = cube.center;
    r->cube_radius = cube.radius;
    zb_mesh_free(a, &cube);
  }
  return r;
}

static void asset_release(zb_render *r, asset *as) {
  if (as->req) {
    zb_vfs_req_release(r->vfs, as->req);
    as->req = NULL;
  }
  zb_free(r->a, as->load_box);
  as->load_box = NULL;
  if (as->tex && as->tex != r->checker_tex) {
    r->backend->destroy_texture(r->backend->ctx, as->tex);
  }
  if (as->mesh && as->mesh != r->cube_mesh) {
    r->backend->destroy_mesh(r->backend->ctx, as->mesh);
  }
  for (size_t i = 0; i < as->owned_count; ++i) {
    r->backend->destroy_texture(r->backend->ctx, as->owned_textures[i]);
  }
  zb_free(r->a, as->owned_textures);
  zb_free(r->a, as->parts);
  zb_free(r->a, as->codepoints);
}

void zb_render_destroy(zb_render *r) {
  if (!r) {
    return;
  }
  for (size_t i = 0; i < r->assets.len; ++i) {
    asset_release(r, zb_array_at(&r->assets, i));
  }
  if (r->cube_mesh) {
    r->backend->destroy_mesh(r->backend->ctx, r->cube_mesh);
  }
  if (r->white_tex) {
    r->backend->destroy_texture(r->backend->ctx, r->white_tex);
  }
  if (r->checker_tex) {
    r->backend->destroy_texture(r->backend->ctx, r->checker_tex);
  }
  r->backend->destroy(r->backend->ctx);
  zb_array_free(&r->assets);
  zb_array_free(&r->items);
  zb_array_free(&r->text);
  zb_array_free(&r->text_batches);
  zb_free(r->a, r);
}

zb_render_backend *zb_render_backend_of(zb_render *r) { return r->backend; }

void zb_render_set_internal_height(zb_render *r, int height) {
  r->internal_h = height < 16 ? 16 : (height > 4320 ? 4320 : height);
}

int zb_render_internal_height(const zb_render *r) { return r->internal_h; }

void zb_render_set_view_count(zb_render *r, int views) {
  r->view_count = views < 1 ? 1 : (views > ZB_RENDER_MAX_VIEWS ? ZB_RENDER_MAX_VIEWS : views);
}

void zb_render_set_clear_color(zb_render *r, float red, float green, float blue) {
  r->clear[0] = red;
  r->clear[1] = green;
  r->clear[2] = blue;
}

void zb_render_set_camera(zb_render *r, zb_vec3 eye, zb_vec3 target, zb_vec3 up, float fovy_deg, float znear,
                          float zfar) {
  r->eye = eye;
  r->target = target;
  r->up = up;
  r->fovy = fovy_deg;
  r->znear = znear;
  r->zfar = zfar;
}

void zb_render_set_light(zb_render *r, zb_vec3 direction, zb_vec3 color, zb_vec3 ambient) {
  r->light_dir = zb_v3_normalize(direction);
  r->light_color = color;
  r->ambient = ambient;
}

void zb_render_set_eye_separation(zb_render *r, float metres) { r->eye_sep = metres; }

void zb_render_set_views(zb_render *r, const zb_xr_view *views, int count) {
  count = views == NULL || count < 0 ? 0 : (count > ZB_RENDER_MAX_VIEWS ? ZB_RENDER_MAX_VIEWS : count);
  for (int i = 0; i < count; ++i) {
    r->xr_views[i] = views[i];
  }
  r->xr_view_count = count;
}

void zb_render_set_xr_hud(zb_render *r, float distance, float pixel) {
  r->xr_hud = isfinite(distance) && distance > 0.0f ? (distance > 1000.0f ? 1000.0f : distance) : 0.0f;
  r->xr_hud_pixel = isfinite(pixel) && pixel > 0.0f ? (pixel > 1.0f ? 1.0f : pixel) : 0.0f;
}

float zb_render_xr_hud(const zb_render *r, float *pixel) {
  if (pixel) {
    *pixel = r->xr_hud_pixel;
  }
  return r->xr_hud;
}

void zb_render_set_vignette(zb_render *r, float strength) {
  r->vignette = isfinite(strength) && strength > 0.0f ? (strength > 1.0f ? 1.0f : strength) : 0.0f;
}

float zb_render_vignette(const zb_render *r) { return r->vignette; }

void zb_render_set_target(zb_render *r, uint32_t framebuffer, int width, int height) {
  bool on = width > 0 && height > 0;
  r->target_fb = on ? framebuffer : 0;
  r->target_w = on ? width : 0;
  r->target_h = on ? height : 0;
}

/* ---- Assets ---------------------------------------------------------------- */

static uint32_t find_asset(const zb_render *r, asset_kind kind, const char *path) {
  for (size_t i = 0; i < r->assets.len; ++i) {
    const asset *as = zb_array_at(&r->assets, i);
    if (as->kind == kind && strcmp(as->path, path) == 0) {
      return (uint32_t)i + 1;
    }
  }
  return 0;
}

static void warn_missing(const char *path, zb_result err) {
  char key[ZB_VFS_MAX_PATH + 16];
  snprintf(key, sizeof key, "missing:%s", path);
  zb_log_once(ZB_LOG_WARN, key, "asset '%s' could not be loaded (%s); using a placeholder", path, zb_result_str(err));
}

typedef struct load_ctx {
  zb_render *r;
  uint32_t handle;
} load_ctx;

static void on_loaded(zb_vfs_req *req, void *user);

static uint32_t new_asset(zb_render *r, asset_kind kind, const char *path) {
  uint32_t existing = find_asset(r, kind, path);
  if (existing) {
    return existing;
  }
  asset as;
  memset(&as, 0, sizeof as);
  as.kind = kind;
  as.state = ZB_ASSET_PENDING;
  snprintf(as.path, sizeof as.path, "%s", path);
  if (!zb_array_push(&r->assets, &as)) {
    return 0;
  }
  uint32_t handle = (uint32_t)r->assets.len;
  /* The callback receives the handle through a small heap block. */
  load_ctx *lc = zb_alloc(r->a, sizeof(*lc));
  if (lc) {
    lc->r = r;
    lc->handle = handle;
    asset *stored = asset_at(r, handle);
    stored->load_box = lc;
    stored->req = zb_vfs_request(r->vfs, path, on_loaded, lc);
    if (!stored->req) {
      zb_free(r->a, lc);
      stored->load_box = NULL;
      stored->state = ZB_ASSET_FAILED;
    }
  }
  return handle;
}

static void fail_asset(zb_render *r, asset *as, zb_result err) {
  as->state = ZB_ASSET_FAILED;
  if (as->kind == ASSET_TEXTURE) {
    as->tex = r->checker_tex;
    as->width = 8;
    as->height = 8;
  }
  warn_missing(as->path, err);
}

typedef struct mesh_read {
  zb_render *r;
  char dir[ZB_VFS_MAX_PATH];
} mesh_read;

static zb_result model_read(void *user, const char *relative, const zb_allocator *a, char **data, size_t *len) {
  mesh_read *mr = user;
  char path[ZB_VFS_MAX_PATH * 2];
  zb_path_join(path, sizeof path, mr->dir, relative);
  return zb_vfs_read(mr->r->vfs, path, a, data, len);
}

static void finish_model(zb_render *r, asset *as, const char *data, size_t len) {
  mesh_read mr;
  mr.r = r;
  snprintf(mr.dir, sizeof mr.dir, "%s", as->path);
  char *slash = strrchr(mr.dir, '/');
  if (slash) {
    *slash = '\0';
  } else {
    mr.dir[0] = '\0';
  }
  zb_mesh_data mesh;
  char err[256];
  zb_result res = zb_mesh_load_gltf(r->a, data, len, model_read, &mr, &mesh, err, sizeof err);
  if (res != ZB_OK) {
    zb_error("model '%s' could not be loaded: %s; drawing a placeholder cube", as->path, err);
    as->state = ZB_ASSET_FAILED;
    return;
  }
  as->owned_textures = mesh.image_count ? zb_calloc(r->a, mesh.image_count * sizeof(uint32_t)) : NULL;
  as->parts = zb_calloc(r->a, mesh.submesh_count * sizeof(model_part));
  if ((mesh.image_count && !as->owned_textures) || !as->parts) {
    zb_mesh_free(r->a, &mesh);
    as->state = ZB_ASSET_FAILED;
    return;
  }
  for (size_t i = 0; i < mesh.image_count; ++i) {
    as->owned_textures[i] = r->backend->create_texture(r->backend->ctx, &mesh.images[i]);
  }
  as->owned_count = mesh.image_count;
  as->mesh = r->backend->create_mesh(r->backend->ctx, &mesh);
  for (size_t i = 0; i < mesh.submesh_count; ++i) {
    as->parts[i].first = mesh.submeshes[i].first;
    as->parts[i].count = mesh.submeshes[i].count;
    int img = mesh.submeshes[i].image;
    as->parts[i].texture = img >= 0 ? as->owned_textures[img] : r->white_tex;
  }
  as->part_count = mesh.submesh_count;
  as->center = mesh.center;
  as->radius = mesh.radius;
  as->state = ZB_ASSET_READY;
  zb_mesh_free(r->a, &mesh);
}

static void finish_texture(zb_render *r, asset *as, const char *data, size_t len) {
  zb_image img;
  zb_result res = zb_image_decode_png(r->a, data, len, &img);
  if (res != ZB_OK) {
    fail_asset(r, as, res);
    return;
  }
  as->tex = r->backend->create_texture(r->backend->ctx, &img);
  as->width = img.width;
  as->height = img.height;
  as->state = ZB_ASSET_READY;
  zb_image_free(r->a, &img);
}

static bool parse_descriptor(zb_render *r, uint32_t handle, const char *data, size_t len, zb_luon_doc *doc) {
  zb_luon_error err;
  if (zb_luon_parse(doc, r->a, data, len, &err) != ZB_OK) {
    zb_error("descriptor '%s':%d: %s", asset_at(r, handle)->path, err.line, err.message);
    return false;
  }
  const char *tex = zb_value_str(zb_table_get(&doc->root, "texture"), NULL);
  if (!tex) {
    zb_error("descriptor '%s' has no 'texture'", asset_at(r, handle)->path);
    zb_luon_free(doc);
    return false;
  }
  /* Loading the texture may grow the asset array: look the asset up afterwards. */
  uint32_t texture = zb_render_load_texture(r, tex);
  asset_at(r, handle)->texture_asset = texture;
  return true;
}

static int num_field(const zb_value *t, const char *key, int def, int lo) {
  int v = (int)zb_value_num(zb_table_get(t, key), def);
  return v < lo ? lo : v;
}

static void finish_sprite(zb_render *r, uint32_t handle, const char *data, size_t len) {
  zb_luon_doc doc;
  if (!parse_descriptor(r, handle, data, len, &doc)) {
    asset_at(r, handle)->state = ZB_ASSET_FAILED;
    return;
  }
  asset *as = asset_at(r, handle); /* re-fetch: loading the texture may grow the array */
  const zb_value *root = &doc.root;
  as->frame_w = num_field(root, "frame_width", 16, 1);
  as->frame_h = num_field(root, "frame_height", 16, 1);
  as->frames = num_field(root, "frames", 1, 1);
  as->fps = num_field(root, "fps", 0, 0);
  const zb_value *pivot = zb_table_get(root, "pivot");
  as->pivot[0] = (float)zb_value_num(zb_table_index(pivot, 1), 0.5);
  as->pivot[1] = (float)zb_value_num(zb_table_index(pivot, 2), 0.0);
  const zb_value *size = zb_table_get(root, "size");
  as->world_size[0] = (float)zb_value_num(zb_table_index(size, 1), 1.0);
  as->world_size[1] = (float)zb_value_num(zb_table_index(size, 2), 1.0);
  as->state = ZB_ASSET_READY;
  zb_luon_free(&doc);
}

static void finish_font(zb_render *r, uint32_t handle, const char *data, size_t len) {
  zb_luon_doc doc;
  if (!parse_descriptor(r, handle, data, len, &doc)) {
    asset_at(r, handle)->state = ZB_ASSET_FAILED;
    return;
  }
  asset *as = asset_at(r, handle);
  const zb_value *root = &doc.root;
  as->glyph_w = num_field(root, "glyph_width", 8, 1);
  as->glyph_h = num_field(root, "glyph_height", 8, 1);
  as->columns = num_field(root, "columns", 16, 1);
  as->advance = (float)zb_value_num(zb_table_get(root, "advance"), as->glyph_w);
  const char *chars = zb_value_str(zb_table_get(root, "chars"), "");
  size_t n = 0;
  uint32_t cp;
  for (const char *p = chars; zb_utf8_decode(p, &cp); p += zb_utf8_decode(p, &cp)) {
    n++;
  }
  as->codepoints = zb_alloc(r->a, (n ? n : 1) * sizeof(uint32_t));
  if (!as->codepoints) {
    as->state = ZB_ASSET_FAILED;
    zb_luon_free(&doc);
    return;
  }
  size_t i = 0;
  for (const char *p = chars; zb_utf8_decode(p, &cp); p += zb_utf8_decode(p, &cp)) {
    as->codepoints[i++] = cp;
  }
  as->glyph_count = n;
  uint32_t repl = 0;
  zb_utf8_decode(zb_value_str(zb_table_get(root, "replacement"), "?"), &repl);
  as->replacement = 0;
  for (size_t k = 0; k < n; ++k) {
    if (as->codepoints[k] == repl) {
      as->replacement = (uint32_t)k;
    }
  }
  as->state = ZB_ASSET_READY;
  zb_luon_free(&doc);
}

static void on_loaded(zb_vfs_req *req, void *user) {
  load_ctx *lc = user;
  zb_render *r = lc->r;
  uint32_t handle = lc->handle;
  zb_free(r->a, lc);
  asset *as = asset_at(r, handle);
  as->req = NULL;
  as->load_box = NULL;
  size_t len = 0;
  const char *data = zb_vfs_req_data(req, &len);
  zb_result err = zb_vfs_req_error(req);
  if (!data) {
    if (as->kind == ASSET_MODEL) {
      warn_missing(as->path, err);
      as->state = ZB_ASSET_FAILED;
    } else if (as->kind == ASSET_TEXTURE) {
      fail_asset(r, as, err);
    } else {
      warn_missing(as->path, err);
      as->state = ZB_ASSET_FAILED;
    }
  } else if (as->kind == ASSET_TEXTURE) {
    finish_texture(r, as, data, len);
  } else if (as->kind == ASSET_MODEL) {
    finish_model(r, as, data, len);
  } else if (as->kind == ASSET_SPRITE) {
    finish_sprite(r, handle, data, len);
  } else {
    finish_font(r, handle, data, len);
  }
  zb_vfs_req_release(r->vfs, req);
}

uint32_t zb_render_load_texture(zb_render *r, const char *path) { return new_asset(r, ASSET_TEXTURE, path); }
uint32_t zb_render_load_model(zb_render *r, const char *path) { return new_asset(r, ASSET_MODEL, path); }
uint32_t zb_render_load_sprite(zb_render *r, const char *path) { return new_asset(r, ASSET_SPRITE, path); }
uint32_t zb_render_load_font(zb_render *r, const char *path) { return new_asset(r, ASSET_FONT, path); }

zb_asset_state zb_render_asset_state(const zb_render *r, uint32_t handle) {
  const asset *as = asset_at(r, handle);
  return as ? as->state : ZB_ASSET_FAILED;
}

size_t zb_render_asset_count(const zb_render *r) { return r->assets.len; }

/* ---- Text ------------------------------------------------------------------- */

size_t zb_utf8_decode(const char *s, uint32_t *cp) {
  const unsigned char *u = (const unsigned char *)s;
  if (u[0] == 0) {
    *cp = 0;
    return 0;
  }
  if (u[0] < 0x80) {
    *cp = u[0];
    return 1;
  }
  size_t n = (u[0] & 0xE0) == 0xC0 ? 2 : (u[0] & 0xF0) == 0xE0 ? 3 : (u[0] & 0xF8) == 0xF0 ? 4 : 0;
  uint32_t v = n == 2 ? (u[0] & 0x1Fu) : n == 3 ? (u[0] & 0x0Fu) : (u[0] & 0x07u);
  for (size_t i = 1; i < n; ++i) {
    if ((u[i] & 0xC0) != 0x80) {
      n = 0;
      break;
    }
    v = (v << 6) | (u[i] & 0x3Fu);
  }
  if (n == 0 || (n == 2 && v < 0x80) || (n == 3 && v < 0x800) || (n == 4 && (v < 0x10000 || v > 0x10FFFF))) {
    *cp = 0xFFFD;
    return 1;
  }
  *cp = v;
  return n;
}

size_t zb_render_text_glyphs(const zb_render *r, uint32_t font, const char *utf8, uint32_t *out, size_t max) {
  const asset *f = asset_at(r, font);
  if (!f || f->kind != ASSET_FONT || f->state != ZB_ASSET_READY) {
    return 0;
  }
  size_t n = 0;
  uint32_t cp;
  size_t step;
  for (const char *p = utf8; (step = zb_utf8_decode(p, &cp)) != 0 && n < max; p += step) {
    uint32_t glyph = f->replacement;
    for (size_t k = 0; k < f->glyph_count; ++k) {
      if (f->codepoints[k] == cp) {
        glyph = (uint32_t)k;
        break;
      }
    }
    out[n++] = glyph;
  }
  return n;
}

/* ---- Layout ----------------------------------------------------------------- */

zb_layout zb_render_compute_layout(int drawable_w, int drawable_h, int internal_h) {
  zb_layout l;
  memset(&l, 0, sizeof l);
  l.drawable_w = drawable_w;
  l.drawable_h = drawable_h;
  if (drawable_w <= 0 || drawable_h <= 0) {
    l.internal_h = internal_h;
    l.internal_w = internal_h * 16 / 9;
    return l;
  }
  l.internal_h = internal_h;
  l.internal_w = (int)((double)internal_h * (double)drawable_w / (double)drawable_h + 0.5);
  if (l.internal_w < 1) {
    l.internal_w = 1;
  }
  if (drawable_w % l.internal_w == 0 && drawable_h % l.internal_h == 0 &&
      drawable_w / l.internal_w == drawable_h / l.internal_h) {
    l.scale = drawable_h / l.internal_h;
    l.out_w = drawable_w;
    l.out_h = drawable_h;
  } else {
    double s = fmin((double)drawable_w / l.internal_w, (double)drawable_h / l.internal_h);
    l.scale = 0;
    l.out_w = (int)(l.internal_w * s + 0.5);
    l.out_h = (int)(l.internal_h * s + 0.5);
    if (l.out_w > drawable_w)
      l.out_w = drawable_w;
    if (l.out_h > drawable_h)
      l.out_h = drawable_h;
  }
  l.out_x = (drawable_w - l.out_w) / 2;
  l.out_y = (drawable_h - l.out_h) / 2;
  return l;
}

/* ---- Retro look and terrain ------------------------------------------------- */

void zb_render_set_atmosphere(zb_render *r, const zb_atmosphere *a) { r->atmosphere = *a; }
const zb_atmosphere *zb_render_atmosphere(const zb_render *r) { return &r->atmosphere; }

void zb_render_set_palette(zb_render *r, uint32_t lut_texture, uint32_t bayer_texture, float dither) {
  r->lut_asset = lut_texture;
  r->bayer_asset = bayer_texture;
  r->dither = dither;
}

bool zb_render_palette_enabled(const zb_render *r) { return r->lut_asset != 0 && r->bayer_asset != 0; }

void zb_render_set_terrain_atlas(zb_render *r, uint32_t texture, int layers) {
  r->atlas_asset = texture;
  r->atlas_layers = layers;
}

uint32_t zb_render_upload_mesh(zb_render *r, const zb_mesh_data *mesh) {
  return r->backend->create_mesh(r->backend->ctx, mesh);
}

void zb_render_release_mesh(zb_render *r, uint32_t mesh) {
  if (mesh) {
    r->backend->destroy_mesh(r->backend->ctx, mesh);
  }
}

zb_vec3 zb_render_camera_eye(const zb_render *r) { return r->eye; }

/* Backend texture of a texture asset that loaded successfully, else 0. */
static uint32_t ready_texture(const zb_render *r, uint32_t handle) {
  const asset *as = asset_at(r, handle);
  return as && as->kind == ASSET_TEXTURE && as->state == ZB_ASSET_READY ? as->tex : 0;
}

static void draw_mesh_item(zb_render *r, zb_draw_kind kind, uint32_t mesh, uint32_t index_count, zb_mat4 transform,
                           zb_vec3 center, float radius, float snow) {
  if (!r->in_frame || mesh == 0) {
    return;
  }
  zb_vec3 c = zb_m4_transform_point(transform, center);
  for (int v = 0; v < r->frame.view_count; ++v) {
    if (!zb_frustum_sphere_visible(&r->frustums[v], c, radius)) {
      r->stats.culled++;
      continue;
    }
    zb_draw_item it;
    memset(&it, 0, sizeof it);
    it.kind = kind;
    it.view = (uint8_t)v;
    it.mesh = mesh;
    it.count = index_count;
    it.model = transform;
    it.snow = snow;
    uint64_t pass = kind == ZB_DRAW_WATER ? 3ull : 2ull;
    it.sort_key = ((uint64_t)v << 60) | (pass << 56) | ((uint64_t)mesh << 16) | (r->items.len & 0xFFFF);
    if (zb_array_push(&r->items, &it)) {
      if (kind == ZB_DRAW_WATER) {
        r->stats.water_items++;
      } else {
        r->stats.terrain_items++;
        r->stats.terrain_triangles += index_count / 3;
      }
    }
  }
}

void zb_render_draw_terrain(zb_render *r, uint32_t mesh, uint32_t index_count, zb_mat4 transform, zb_vec3 center,
                            float radius, float snow) {
  draw_mesh_item(r, ZB_DRAW_TERRAIN, mesh, index_count, transform, center, radius, snow);
}

void zb_render_draw_water(zb_render *r, uint32_t mesh, uint32_t index_count, zb_mat4 transform, zb_vec3 center,
                          float radius) {
  draw_mesh_item(r, ZB_DRAW_WATER, mesh, index_count, transform, center, radius, 0.0f);
}

static uint32_t ready_texture(const zb_render *r, uint32_t handle);

void zb_render_set_time(zb_render *r, double seconds) {
  if (!r->time_pinned) {
    r->time = seconds;
  }
}

void zb_render_set_daylight(zb_render *r, bool on) { r->daylight = on; }
bool zb_render_daylight(const zb_render *r) { return r->daylight; }

double zb_render_pinned_time(const zb_render *r) { return r->time_pinned ? r->time : -1.0; }

void zb_render_pin_time(zb_render *r, double seconds) {
  r->time_pinned = seconds >= 0.0;
  r->time = seconds >= 0.0 ? seconds : r->time;
}

uint32_t zb_render_upload_foliage(zb_render *r, const zb_foliage_instance *instances, size_t count) {
  return count ? r->backend->create_foliage(r->backend->ctx, instances, count) : 0;
}

void zb_render_release_foliage(zb_render *r, uint32_t foliage) {
  if (foliage) {
    r->backend->destroy_foliage(r->backend->ctx, foliage);
  }
}

void zb_render_set_foliage_tints(zb_render *r, const float *rgb, int count) {
  for (int i = 0; i < ZB_FOLIAGE_TINTS; ++i) {
    for (int k = 0; k < 3; ++k) {
      r->foliage_tints[i][k] = i < count ? rgb[i * 3 + k] : 1.0f;
    }
  }
}

void zb_render_draw_foliage(zb_render *r, uint32_t foliage, uint32_t count, uint32_t texture, zb_mat4 transform,
                            zb_vec3 center, float radius) {
  uint32_t tex = ready_texture(r, texture);
  if (!r->in_frame || foliage == 0 || count == 0 || tex == 0) {
    return;
  }
  zb_vec3 c = zb_m4_transform_point(transform, center);
  for (int v = 0; v < r->frame.view_count; ++v) {
    if (!zb_frustum_sphere_visible(&r->frustums[v], c, radius)) {
      r->stats.culled++;
      continue;
    }
    zb_draw_item it;
    memset(&it, 0, sizeof it);
    it.kind = ZB_DRAW_FOLIAGE;
    it.view = (uint8_t)v;
    it.mesh = foliage;
    it.count = count;
    it.texture = tex;
    it.model = transform;
    it.sort_key = ((uint64_t)v << 60) | (2ull << 56) | ((uint64_t)foliage << 16) | (r->items.len & 0xFFFF);
    if (zb_array_push(&r->items, &it)) {
      r->stats.foliage_items++;
      r->stats.foliage_instances += count;
    }
  }
}

/* ---- Frame ------------------------------------------------------------------ */

/* A headset projection with the camera's depth range (perspective matrices only). */
static zb_mat4 with_depth_range(zb_mat4 p, float znear, float zfar) {
  if (p.m[11] < -0.5f && fabsf(p.m[15]) < 1e-6f && zfar > znear) {
    p.m[10] = (zfar + znear) / (znear - zfar);
    p.m[14] = 2.0f * zfar * znear / (znear - zfar);
  }
  return p;
}

/* The eye position of a rigid view matrix (its rows are the eye's axes, and eye = -R t). */
static zb_vec3 eye_of(const zb_mat4 *view) {
  const float *m = view->m;
  zb_vec3 right = zb_v3(m[0], m[4], m[8]), up = zb_v3(m[1], m[5], m[9]), back = zb_v3(m[2], m[6], m[10]);
  return zb_v3_scale(zb_v3_add(zb_v3_add(zb_v3_scale(right, m[12]), zb_v3_scale(up, m[13])), zb_v3_scale(back, m[14])),
                     -1.0f);
}

/* Text matrices of the explicit views: the canvas fills each view, or (xr_hud)
 * lies on a head-locked plane (see zb_render_set_xr_hud). */
static void explicit_text(zb_render *r) {
  int n = r->xr_view_count, ih = r->internal_h;
  float cw = (float)r->frame.views[0].width;
  float k = 0.0f; /* tangent per canvas pixel */
  for (int v = 0; v < n; ++v) {
    float m5 = r->xr_views[v].proj.m[5];
    k += fabsf(m5) > 1e-6f ? 2.0f / fabsf(m5) / (float)ih / (float)n : 0.0f;
  }
  for (int v = 0; v < n; ++v) {
    zb_view *vw = &r->frame.views[v];
    vw->text = zb_m4_ortho(0.0f, (float)vw->width, (float)ih, 0.0f, -1.0f, 1.0f);
  }
  if (r->xr_hud <= 0.0f || !(k > 0.0f)) {
    return;
  }
  zb_vec3 head = zb_v3(0.0f, 0.0f, 0.0f);
  for (int v = 0; v < n; ++v) {
    head = zb_v3_add(head, zb_v3_scale(eye_of(&r->xr_views[v].view), 1.0f / (float)n));
  }
  const float *m = r->xr_views[0].view.m;
  zb_vec3 right = zb_v3(m[0], m[4], m[8]), up = zb_v3(m[1], m[5], m[9]), back = zb_v3(m[2], m[6], m[10]);
  float d = r->xr_hud, s = r->xr_hud_pixel > 0.0f ? r->xr_hud_pixel : k * d;
  /* Canvas pixel (x, y) -> head + right (x - cw/2) s - up (y - ih/2) s - back d (reference space). */
  zb_vec3 origin = zb_v3_add(zb_v3_sub(head, zb_v3_scale(back, d)),
                             zb_v3_add(zb_v3_scale(right, -0.5f * cw * s), zb_v3_scale(up, 0.5f * (float)ih * s)));
  zb_mat4 canvas = zb_m4_identity();
  canvas.m[0] = right.x * s;
  canvas.m[1] = right.y * s;
  canvas.m[2] = right.z * s;
  canvas.m[4] = -up.x * s;
  canvas.m[5] = -up.y * s;
  canvas.m[6] = -up.z * s;
  canvas.m[10] = 0.0f;
  canvas.m[12] = origin.x;
  canvas.m[13] = origin.y;
  canvas.m[14] = origin.z;
  for (int v = 0; v < n; ++v) {
    const zb_xr_view *xv = &r->xr_views[v];
    zb_mat4 t = zb_m4_mul(xv->proj, zb_m4_mul(xv->view, canvas));
    t.m[2] = t.m[6] = t.m[10] = t.m[14] = 0.0f; /* z = 0: never clipped by near or far */
    r->frame.views[v].text = t;
  }
}

/* Views from explicit (headset) views relative to the camera: each at the
 * internal height with its viewport's aspect ratio, side by side. */
static void explicit_views(zb_render *r) {
  zb_layout *l = &r->frame.layout;
  int ih = r->internal_h, total = 0, scale = 0;
  int widths[ZB_RENDER_MAX_VIEWS];
  for (int v = 0; v < r->xr_view_count; ++v) {
    const zb_xr_view *xv = &r->xr_views[v];
    int vw = xv->width > 0 ? xv->width : 1, vh = xv->height > 0 ? xv->height : 1;
    long w = lroundf((float)ih * (float)vw / (float)vh);
    widths[v] = (int)(w < 1 ? 1 : (w > 4L * ih ? 4L * ih : w));
    total += widths[v];
    /* An integer upscale when every view is the same exact multiple. */
    int k = vh / ih;
    bool exact = k >= 1 && vh == k * ih && vw == k * widths[v];
    scale = exact && (v == 0 || k == scale) ? k : 0;
  }
  l->internal_w = total;
  l->internal_h = ih;
  l->out_x = 0;
  l->out_y = 0;
  l->out_w = l->drawable_w;
  l->out_h = l->drawable_h;
  l->scale = scale;
  zb_mat4 camera = zb_m4_look_at(r->eye, r->target, r->up);
  int x = 0;
  for (int v = 0; v < r->xr_view_count; ++v) {
    const zb_xr_view *xv = &r->xr_views[v];
    zb_view *vw = &r->frame.views[v];
    vw->x = x;
    vw->y = 0;
    vw->width = widths[v];
    vw->height = ih;
    x += widths[v];
    vw->out_x = xv->x;
    vw->out_y = xv->y;
    vw->out_w = xv->width;
    vw->out_h = xv->height;
    vw->view = zb_m4_mul(xv->view, camera);
    vw->proj = with_depth_range(xv->proj, r->znear, r->zfar);
    vw->view_proj = zb_m4_mul(vw->proj, vw->view);
    /* Rigid view matrix: its rows are the eye's axes. */
    const float *m = vw->view.m;
    vw->right = zb_v3(m[0], m[4], m[8]);
    vw->up = zb_v3(m[1], m[5], m[9]);
    vw->eye = eye_of(&vw->view);
    r->frustums[v] = zb_frustum_from_matrix(vw->view_proj);
  }
  r->frame.view_count = r->xr_view_count;
  r->frame.explicit_views = true;
  explicit_text(r);
}

void zb_render_begin(zb_render *r, int drawable_w, int drawable_h) {
  zb_array_clear(&r->items);
  zb_array_clear(&r->text);
  zb_array_clear(&r->text_batches);
  memset(&r->frame, 0, sizeof r->frame);
  if (r->target_w > 0) {
    drawable_w = r->target_w;
    drawable_h = r->target_h;
  }
  r->frame.layout = zb_render_compute_layout(drawable_w, drawable_h, r->internal_h);
  r->frame.target = r->target_fb;
  memcpy(r->frame.clear, r->clear, sizeof r->clear);
  r->frame.view_count = r->view_count;
  r->frame.light_dir = r->light_dir;
  r->frame.light_color = r->light_color;
  r->frame.ambient = r->ambient;

  int iw = r->frame.layout.internal_w, ih = r->frame.layout.internal_h;
  zb_vec3 fwd = zb_v3_normalize(zb_v3_sub(r->target, r->eye));
  zb_vec3 right = zb_v3_normalize(zb_v3_cross(fwd, r->up));
  zb_vec3 up = zb_v3_cross(right, fwd);
  for (int v = 0; v < r->view_count && r->xr_view_count == 0; ++v) {
    zb_view *vw = &r->frame.views[v];
    vw->width = iw / r->view_count;
    vw->height = ih;
    vw->x = v * vw->width;
    vw->y = 0;
    float offset = r->view_count == 2 ? (v == 0 ? -0.5f : 0.5f) * r->eye_sep : 0.0f;
    zb_vec3 eye = zb_v3_add(r->eye, zb_v3_scale(right, offset));
    zb_vec3 target = zb_v3_add(r->target, zb_v3_scale(right, offset));
    float aspect = (float)vw->width / (float)(vw->height > 0 ? vw->height : 1);
    vw->view = zb_m4_look_at(eye, target, r->up);
    vw->proj = zb_m4_perspective(r->fovy * ZB_PI / 180.0f, aspect, r->znear, r->zfar);
    vw->view_proj = zb_m4_mul(vw->proj, vw->view);
    vw->eye = eye;
    vw->right = right;
    vw->up = up;
    r->frustums[v] = zb_frustum_from_matrix(vw->view_proj);
  }
  if (r->xr_view_count > 0) {
    explicit_views(r);
  }
  memset(&r->stats, 0, sizeof(zb_render_stats) - sizeof(uint64_t));
  r->stats.layout = r->frame.layout;
  r->stats.views = r->frame.view_count;
  r->in_frame = true;
}

static float max_scale(zb_mat4 m) {
  float sx = zb_v3_length(zb_v3(m.m[0], m.m[1], m.m[2]));
  float sy = zb_v3_length(zb_v3(m.m[4], m.m[5], m.m[6]));
  float sz = zb_v3_length(zb_v3(m.m[8], m.m[9], m.m[10]));
  return fmaxf(sx, fmaxf(sy, sz));
}

void zb_render_draw_model(zb_render *r, uint32_t model, zb_mat4 transform) {
  const asset *as = asset_at(r, model);
  if (!as || as->kind != ASSET_MODEL || as->state == ZB_ASSET_PENDING) {
    return;
  }
  bool ready = as->state == ZB_ASSET_READY;
  const model_part *parts = ready ? as->parts : &r->cube_part;
  size_t part_count = ready ? as->part_count : 1;
  uint32_t mesh = ready ? as->mesh : r->cube_mesh;
  zb_vec3 center = zb_m4_transform_point(transform, ready ? as->center : r->cube_center);
  float radius = (ready ? as->radius : r->cube_radius) * max_scale(transform);
  for (int v = 0; v < r->frame.view_count; ++v) {
    if (!zb_frustum_sphere_visible(&r->frustums[v], center, radius)) {
      r->stats.culled++;
      continue;
    }
    for (size_t p = 0; p < part_count; ++p) {
      zb_draw_item it;
      memset(&it, 0, sizeof it);
      it.kind = ZB_DRAW_MODEL;
      it.view = (uint8_t)v;
      it.mesh = mesh;
      it.texture = parts[p].texture;
      it.first = parts[p].first;
      it.count = parts[p].count;
      it.model = transform;
      it.sort_key = ((uint64_t)v << 60) | ((uint64_t)it.texture << 32) | ((uint64_t)mesh << 8) | (r->items.len & 0xFF);
      if (zb_array_push(&r->items, &it)) {
        r->stats.model_items++;
      }
    }
  }
}

void zb_render_draw_mesh(zb_render *r, uint32_t mesh, uint32_t index_count, zb_mat4 transform, zb_vec3 center,
                         float radius) {
  zb_vec3 c = zb_m4_transform_point(transform, center);
  float rad = radius * max_scale(transform);
  for (int v = 0; v < r->frame.view_count; ++v) {
    if (!zb_frustum_sphere_visible(&r->frustums[v], c, rad)) {
      r->stats.culled++;
      continue;
    }
    zb_draw_item it;
    memset(&it, 0, sizeof it);
    it.kind = ZB_DRAW_MODEL;
    it.view = (uint8_t)v;
    it.mesh = mesh;
    it.texture = r->white_tex;
    it.first = 0;
    it.count = index_count;
    it.model = transform;
    it.sort_key = ((uint64_t)v << 60) | ((uint64_t)it.texture << 32) | ((uint64_t)mesh << 8) | (r->items.len & 0xFF);
    if (zb_array_push(&r->items, &it)) {
      r->stats.model_items++;
    }
  }
}

uint64_t zb_render_sprite_frame_number(const zb_render *r, uint32_t sprite, double time_s) {
  const asset *as = asset_at(r, sprite);
  if (!as || as->kind != ASSET_SPRITE || as->state != ZB_ASSET_READY || as->fps <= 0 || time_s < 0.0) {
    return 0;
  }
  return (uint64_t)floor(time_s * (double)as->fps + 1e-9);
}

void zb_render_draw_sprite(zb_render *r, uint32_t sprite, zb_vec3 position, float scale, double time_s) {
  const asset *as = asset_at(r, sprite);
  if (!as || as->kind != ASSET_SPRITE || as->state != ZB_ASSET_READY) {
    return;
  }
  const asset *tex = asset_at(r, as->texture_asset);
  if (!tex || tex->state == ZB_ASSET_PENDING) {
    return;
  }
  int frame = (int)(zb_render_sprite_frame_number(r, sprite, time_s) % (uint64_t)as->frames);
  int cols = tex->width / as->frame_w > 0 ? tex->width / as->frame_w : 1;
  float u0 = (float)((frame % cols) * as->frame_w) / (float)tex->width;
  float v0 = (float)((frame / cols) * as->frame_h) / (float)tex->height;
  float u1 = u0 + (float)as->frame_w / (float)tex->width;
  float v1 = v0 + (float)as->frame_h / (float)tex->height;
  float w = as->world_size[0] * scale, h = as->world_size[1] * scale;
  float radius = sqrtf(w * w + h * h);
  for (int v = 0; v < r->frame.view_count; ++v) {
    if (!zb_frustum_sphere_visible(&r->frustums[v], position, radius)) {
      r->stats.culled++;
      continue;
    }
    zb_draw_item it;
    memset(&it, 0, sizeof it);
    it.kind = ZB_DRAW_SPRITE;
    it.view = (uint8_t)v;
    it.texture = tex->tex;
    it.position = position;
    it.size[0] = w;
    it.size[1] = h;
    it.pivot[0] = as->pivot[0];
    it.pivot[1] = as->pivot[1];
    it.uv[0] = u0;
    it.uv[1] = v0;
    it.uv[2] = u1;
    it.uv[3] = v1;
    it.sort_key = ((uint64_t)v << 60) | (1ull << 56) | ((uint64_t)it.texture << 16) | (r->items.len & 0xFFFF);
    if (zb_array_push(&r->items, &it)) {
      r->stats.sprite_items++;
    }
  }
}

void zb_render_draw_text(zb_render *r, uint32_t font, const char *utf8, float x, float y, float scale,
                         const float rgba[4]) {
  const asset *f = asset_at(r, font);
  if (!f || f->kind != ASSET_FONT || f->state != ZB_ASSET_READY) {
    return;
  }
  const asset *tex = asset_at(r, f->texture_asset);
  if (!tex || tex->state == ZB_ASSET_PENDING || tex->width <= 0) {
    return;
  }
  uint32_t glyphs[512];
  size_t n = zb_render_text_glyphs(r, font, utf8, glyphs, ZB_ARRAY_LEN(glyphs));
  zb_text_batch batch = {tex->tex, (uint32_t)r->text.len, 0};
  float pen = x;
  float gw = (float)f->glyph_w * scale, gh = (float)f->glyph_h * scale;
  for (size_t i = 0; i < n; ++i) {
    int col = (int)glyphs[i] % f->columns, row = (int)glyphs[i] / f->columns;
    float u0 = (float)(col * f->glyph_w) / (float)tex->width;
    float v0 = (float)(row * f->glyph_h) / (float)tex->height;
    float u1 = u0 + (float)f->glyph_w / (float)tex->width;
    float v1 = v0 + (float)f->glyph_h / (float)tex->height;
    zb_text_vertex q[4] = {
        {pen, y, u0, v0, {rgba[0], rgba[1], rgba[2], rgba[3]}},
        {pen + gw, y, u1, v0, {rgba[0], rgba[1], rgba[2], rgba[3]}},
        {pen + gw, y + gh, u1, v1, {rgba[0], rgba[1], rgba[2], rgba[3]}},
        {pen, y + gh, u0, v1, {rgba[0], rgba[1], rgba[2], rgba[3]}},
    };
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; ++k) {
      if (!zb_array_push(&r->text, &q[order[k]])) {
        return;
      }
    }
    batch.count += 6;
    pen += f->advance * scale;
    r->stats.text_glyphs++;
  }
  if (batch.count) {
    zb_array_push(&r->text_batches, &batch);
  }
}

static int cmp_items(const void *a, const void *b) {
  const zb_draw_item *x = a, *y = b;
  return x->sort_key < y->sort_key ? -1 : (x->sort_key > y->sort_key ? 1 : 0);
}

void zb_render_end(zb_render *r) {
  if (!r->in_frame) {
    return;
  }
  if (r->items.len > 1) {
    qsort(r->items.data, r->items.len, sizeof(zb_draw_item), cmp_items);
  }
  r->frame.items = (const zb_draw_item *)r->items.data;
  r->frame.item_count = r->items.len;
  r->frame.text = (const zb_text_vertex *)r->text.data;
  r->frame.text_count = r->text.len;
  r->frame.text_batches = (const zb_text_batch *)r->text_batches.data;
  r->frame.text_batch_count = r->text_batches.len;
  /* The look as set by the end of the frame (scripts change it in frame hooks). */
  r->frame.atmosphere = r->atmosphere;
  r->frame.lut_texture = ready_texture(r, r->lut_asset);
  r->frame.bayer_texture = ready_texture(r, r->bayer_asset);
  r->frame.palette = r->frame.lut_texture != 0 && r->frame.bayer_texture != 0;
  r->frame.dither = r->dither;
  r->frame.vignette = r->vignette;
  r->frame.atlas_texture = ready_texture(r, r->atlas_asset);
  r->frame.atlas_layers = r->frame.atlas_texture ? r->atlas_layers : 0;
  r->frame.time = (float)r->time;
  memcpy(r->frame.foliage_tints, r->foliage_tints, sizeof r->foliage_tints);
  r->backend->execute(r->backend->ctx, &r->frame);
  r->stats.frames++;
  r->in_frame = false;
}

const zb_render_stats *zb_render_get_stats(const zb_render *r) { return &r->stats; }

zb_result zb_render_capture(zb_render *r, bool internal, const zb_allocator *a, zb_image *out) {
  return r->backend->capture(r->backend->ctx, internal, a, out);
}

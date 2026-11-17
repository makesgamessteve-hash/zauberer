/* SPDX-License-Identifier: MIT */
/* Render frontend: assets (textures, glTF models, sprites, bitmap fonts),
 * internal-resolution layout, multi-view cameras, culling and draw lists. */
#ifndef ZB_RENDER_RENDER_H
#define ZB_RENDER_RENDER_H

#include "render/backend.h"
#include "vfs/vfs.h"

typedef struct zb_render zb_render;

typedef enum zb_asset_state {
  ZB_ASSET_PENDING,
  ZB_ASSET_READY,
  ZB_ASSET_FAILED /* a placeholder is used */
} zb_asset_state;

typedef struct zb_render_stats {
  zb_layout layout;
  int views;
  size_t model_items;
  size_t sprite_items;
  size_t text_glyphs;
  size_t culled;
  uint64_t frames;
} zb_render_stats;

#define ZB_RENDER_DEFAULT_INTERNAL_HEIGHT 360

/* Takes ownership of backend. */
zb_render *zb_render_create(const zb_allocator *a, zb_vfs *vfs, zb_render_backend *backend);
void zb_render_destroy(zb_render *r);
zb_render_backend *zb_render_backend_of(zb_render *r);

void zb_render_set_internal_height(zb_render *r, int height);
int zb_render_internal_height(const zb_render *r);
void zb_render_set_view_count(zb_render *r, int views);
void zb_render_set_clear_color(zb_render *r, float red, float green, float blue);
void zb_render_set_camera(zb_render *r, zb_vec3 eye, zb_vec3 target, zb_vec3 up, float fovy_deg, float znear,
                          float zfar);
void zb_render_set_light(zb_render *r, zb_vec3 direction, zb_vec3 color, zb_vec3 ambient);
/* Distance between the eyes in stereo mode (metres). */
void zb_render_set_eye_separation(zb_render *r, float metres);

/* Asset handles are non-zero; repeated loads of a path return the same handle. */
uint32_t zb_render_load_texture(zb_render *r, const char *path);
uint32_t zb_render_load_model(zb_render *r, const char *path);
uint32_t zb_render_load_sprite(zb_render *r, const char *path);
uint32_t zb_render_load_font(zb_render *r, const char *path);
zb_asset_state zb_render_asset_state(const zb_render *r, uint32_t handle);
/* Number of textures, models, sprites and fonts known. */
size_t zb_render_asset_count(const zb_render *r);

/* Destination layout for a drawable size: integer scale when the drawable is
 * an exact multiple of the internal size, otherwise the largest fit. */
zb_layout zb_render_compute_layout(int drawable_w, int drawable_h, int internal_h);

void zb_render_begin(zb_render *r, int drawable_w, int drawable_h);
void zb_render_draw_model(zb_render *r, uint32_t model, zb_mat4 transform);
void zb_render_draw_sprite(zb_render *r, uint32_t sprite, zb_vec3 position, float scale, double time_s);
/* Screen-space text in internal pixels (origin top-left). */
void zb_render_draw_text(zb_render *r, uint32_t font, const char *utf8, float x, float y, float scale,
                         const float rgba[4]);
void zb_render_end(zb_render *r);

const zb_render_stats *zb_render_get_stats(const zb_render *r);
zb_result zb_render_capture(zb_render *r, bool internal, const zb_allocator *a, zb_image *out);
/* Number of frames the sprite animation has advanced at time_s (unwrapped). */
uint64_t zb_render_sprite_frame_number(const zb_render *r, uint32_t sprite, double time_s);
/* Maps text to glyph indices of a font (missing glyphs -> replacement). */
size_t zb_render_text_glyphs(const zb_render *r, uint32_t font, const char *utf8, uint32_t *out, size_t max);

/* Decodes one UTF-8 code point; returns bytes consumed (0 at the end of s).
 * Invalid sequences yield U+FFFD and consume one byte. */
size_t zb_utf8_decode(const char *s, uint32_t *cp);

#endif

/* SPDX-License-Identifier: MIT */
/* Render frontend: assets (textures, glTF models, sprites, bitmap fonts),
 * internal-resolution layout, multi-view cameras, culling and draw lists. */
#ifndef ZB_RENDER_RENDER_H
#define ZB_RENDER_RENDER_H

#include "core/xr.h"
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
  size_t terrain_items;
  size_t terrain_triangles;
  size_t water_items;
  size_t foliage_items;
  size_t foliage_instances;
  size_t text_glyphs;
  size_t ui_quads;   /* UI quads (rectangles, images, frames and glyphs) after clipping */
  size_t ui_batches; /* draw calls the UI and text took */
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
/* Sky and fog (copied; disabled by default). */
void zb_render_set_atmosphere(zb_render *r, const zb_atmosphere *a);
const zb_atmosphere *zb_render_atmosphere(const zb_render *r);
/* Palette quantisation with ordered dithering: texture assets of the 32^3 LUT
 * and the 8x8 Bayer matrix (0 switches the palette off). */
void zb_render_set_palette(zb_render *r, uint32_t lut_texture, uint32_t bayer_texture, float dither);
bool zb_render_palette_enabled(const zb_render *r);
/* Terrain material atlas: a texture asset with `layers` square layers stacked vertically. */
void zb_render_set_terrain_atlas(zb_render *r, uint32_t texture, int layers);
/* Distance between the eyes in stereo mode (metres). */
void zb_render_set_eye_separation(zb_render *r, float metres);
/* Explicit views (WebXR), copied, in effect from the next zb_render_begin
 * until cleared with count 0. They replace the camera-derived views: each
 * view's matrix is applied relative to the camera (view x camera view), its
 * projection keeps the field of view but takes the camera's near and far
 * planes, and its viewport is the destination in the target. Every view is
 * rendered at the internal height with its viewport's aspect ratio, side by
 * side in the internal framebuffer. */
void zb_render_set_views(zb_render *r, const zb_xr_view *views, int count);
/* Screen text in explicit views is drawn after the upscale, at the
 * resolution of each view's destination. distance 0 (the default) shows the
 * canvas (one view's internal width x the internal height) filling every
 * view; a distance > 0 (metres) puts it on a plane that far in front of the
 * head instead, its centre straight ahead, so both eyes see it in the same
 * direction. pixel is the size of a canvas pixel on that plane in metres
 * (<= 0: as wide as a pixel at the views' centre). The head is between the
 * eyes and looks along the first view. */
void zb_render_set_xr_hud(zb_render *r, float distance, float pixel);
/* The distance in effect (0: off), and the pixel size if pixel is not NULL. */
float zb_render_xr_hud(const zb_render *r, float *pixel);
/* Comfort vignette: darkens the edges of every view (0 .. 1, 0 = off; see
 * zb_vignette_factor), centred on each view's optical axis. */
void zb_render_set_vignette(zb_render *r, float strength);
/* The player's brightness: the image is multiplied by it (0.5 .. 2, default 1) before the palette. */
void zb_render_set_brightness(zb_render *r, float brightness);
float zb_render_brightness(const zb_render *r);
float zb_render_vignette(const zb_render *r);
/* Presents into a backend framebuffer of width x height pixels instead of
 * the window; a size of 0 returns to the window. */
void zb_render_set_target(zb_render *r, uint32_t framebuffer, int width, int height);

/* Asset handles are non-zero; repeated loads of a path return the same handle. */
uint32_t zb_render_load_texture(zb_render *r, const char *path);
uint32_t zb_render_load_model(zb_render *r, const char *path);
uint32_t zb_render_load_sprite(zb_render *r, const char *path);
uint32_t zb_render_load_font(zb_render *r, const char *path);
zb_asset_state zb_render_asset_state(const zb_render *r, uint32_t handle);
/* Runtime meshes (terrain): uploaded at once; handles are backend mesh handles. */
uint32_t zb_render_upload_mesh(zb_render *r, const zb_mesh_data *mesh);
void zb_render_release_mesh(zb_render *r, uint32_t mesh);
/* Number of textures, models, sprites and fonts known. */
size_t zb_render_asset_count(const zb_render *r);

/* Destination layout for a drawable size: integer scale when the drawable is
 * an exact multiple of the internal size, otherwise the largest fit. */
zb_layout zb_render_compute_layout(int drawable_w, int drawable_h, int internal_h);

/* Starts a frame for a window drawable of the given size (ignored while a
 * target is set). */
void zb_render_begin(zb_render *r, int drawable_w, int drawable_h);
void zb_render_draw_model(zb_render *r, uint32_t model, zb_mat4 transform);
/* An opaque, untextured mesh (from zb_render_upload_mesh; vertex colours) like
 * a model, with its bounding sphere in mesh coordinates. */
void zb_render_draw_mesh(zb_render *r, uint32_t mesh, uint32_t index_count, zb_mat4 transform, zb_vec3 center,
                         float radius);
/* A terrain mesh (from zb_render_upload_mesh) with its bounding sphere in mesh
 * coordinates, and how much snow (0 .. 1) lies on its flat ground. */
void zb_render_draw_terrain(zb_render *r, uint32_t mesh, uint32_t index_count, zb_mat4 transform, zb_vec3 center,
                            float radius, float snow);
/* A translucent water mesh (colour alpha = opacity, uv = metres for its
 * ripples), drawn after the opaque scene without writing depth. */
void zb_render_draw_water(zb_render *r, uint32_t mesh, uint32_t index_count, zb_mat4 transform, zb_vec3 center,
                          float radius);
/* The time that animates water and plants, in seconds. */
void zb_render_set_time(zb_render *r, double seconds);
/* Whether the engine lights the scene from the world's sky (day, night,
 * seasons) instead of the light and atmosphere the scripts set. */
void zb_render_set_daylight(zb_render *r, bool on);
bool zb_render_daylight(const zb_render *r);
/* Holds the animation time at seconds (photos and golden images), or lets it
 * run again when seconds is negative. */
void zb_render_pin_time(zb_render *r, double seconds);
/* The pinned time in seconds, or -1 when time is not pinned. */
double zb_render_pinned_time(const zb_render *r);
/* Plant billboards (zb_foliage_instance) uploaded once, e.g. per chunk. */
uint32_t zb_render_upload_foliage(zb_render *r, const zb_foliage_instance *instances, size_t count);
void zb_render_release_foliage(zb_render *r, uint32_t foliage);
/* Draws count billboards of a foliage buffer with a texture asset (the plant
 * atlas), with their bounding sphere in buffer coordinates. */
void zb_render_draw_foliage(zb_render *r, uint32_t foliage, uint32_t count, uint32_t texture, zb_mat4 transform,
                            zb_vec3 center, float radius);
/* Colours per species index for foliage (rgb triples; count <= ZB_FOLIAGE_TINTS),
 * e.g. the season's; unset species keep white. */
void zb_render_set_foliage_tints(zb_render *r, const float *rgb, int count);
/* The current camera eye (render coordinates). */
zb_vec3 zb_render_camera_eye(const zb_render *r);
void zb_render_draw_sprite(zb_render *r, uint32_t sprite, zb_vec3 position, float scale, double time_s);
/* Screen-space text in internal pixels (origin top-left). */
void zb_render_draw_text(zb_render *r, uint32_t font, const char *utf8, float x, float y, float scale,
                         const float rgba[4]);
void zb_render_end(zb_render *r);

const zb_render_stats *zb_render_get_stats(const zb_render *r);
/* ---- The UI layer: quads in internal pixels (origin top-left), drawn after the
 * world and the palette pass, before the upscale. Quads of one texture in a row
 * share a draw call; the clip stack cuts them on the CPU. ---- */

/* A quad of a texture asset's region (u, v normalised; texture 0 = white). */
void zb_render_draw_quad(zb_render *r, uint32_t texture, float x, float y, float w, float h, float u0, float v0,
                         float u1, float v1, const float rgba[4]);
/* A region of a texture asset (texels; sw or sh <= 0: the whole texture) drawn at x, y, w, h. */
void zb_render_draw_image(zb_render *r, uint32_t texture, float sx, float sy, float sw, float sh, float x, float y,
                          float w, float h, const float rgba[4]);
/* A nine-slice frame from a region of a texture asset with border widths l, t, rt, b (texels):
 * corners whole, edges and middle tiled in whole repeats (the last one cut), never stretched. */
void zb_render_draw_nine(zb_render *r, uint32_t texture, float sx, float sy, float sw, float sh, int l, int t, int rt,
                         int b, float x, float y, float w, float h, const float rgba[4]);
/* Clips later UI drawing to the rectangle (intersected with the one before); false when too deep. */
bool zb_render_push_clip(zb_render *r, float x, float y, float w, float h);
void zb_render_pop_clip(zb_render *r);
#define ZB_RENDER_MAX_SCRIPT_TEXTURE 1024
/* A texture asset made from RGBA8 pixels (w * h * 4 bytes, rows top to bottom); 0 when refused. */
uint32_t zb_render_texture_from_pixels(zb_render *r, int w, int h, const unsigned char *rgba);
/* Replaces a region of a texture made from pixels. */
zb_result zb_render_update_texture(zb_render *r, uint32_t texture, int x, int y, int w, int h,
                                   const unsigned char *rgba);
void zb_render_free_texture(zb_render *r, uint32_t texture);
/* The size of a ready texture asset (0, 0 otherwise). */
void zb_render_texture_size(const zb_render *r, uint32_t texture, int *w, int *h);
/* The layout of the last frame begun (or of the given drawable before any frame). */
zb_layout zb_render_last_layout(const zb_render *r);

zb_result zb_render_capture(zb_render *r, bool internal, const zb_allocator *a, zb_image *out);
/* Number of frames the sprite animation has advanced at time_s (unwrapped). */
uint64_t zb_render_sprite_frame_number(const zb_render *r, uint32_t sprite, double time_s);
/* Maps text to glyph indices of a font (missing glyphs -> replacement). */
size_t zb_render_text_glyphs(const zb_render *r, uint32_t font, const char *utf8, uint32_t *out, size_t max);

/* Decodes one UTF-8 code point; returns bytes consumed (0 at the end of s).
 * Invalid sequences yield U+FFFD and consume one byte. */
size_t zb_utf8_decode(const char *s, uint32_t *cp);

#endif

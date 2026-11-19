/* SPDX-License-Identifier: MIT */
#include "core/log.h"
#include "core/memfs.h"
#include "render/render.h"
#include "support/test_support.h"
#include "unity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static zb_memfs *g_fs;
static int g_warnings;

static void count_warn(zb_log_level level, const char *msg, void *user) {
  (void)msg;
  (void)user;
  if (level >= ZB_LOG_WARN) {
    g_warnings++;
  }
}

void setUp(void) {
  zb_test_begin();
  zb_log_reset();
  zb_log_set_stderr(false);
  zb_log_add_sink(count_warn, NULL);
  g_warnings = 0;
  g_fs = zb_memfs_create(zb_test_alloc());
}

void tearDown(void) {
  zb_memfs_destroy(g_fs);
  zb_log_reset();
  zb_test_end();
}

static void put_text(const char *path, const char *text) { zb_memfs_put(g_fs, path, text, strlen(text)); }

static void put_real(const char *memfs_path, const char *repo_path) {
  char full[1024];
  snprintf(full, sizeof full, "%s/%s", zb_test_source_dir(), repo_path);
  size_t len = 0;
  char *data = zb_test_read_file(full, &len);
  TEST_ASSERT_NOT_NULL_MESSAGE(data, full);
  zb_memfs_put(g_fs, memfs_path, data, len);
  free(data);
}

static void put_png(const char *path, int w, int h) {
  zb_image img = {w, h, zb_calloc(zb_test_alloc(), (size_t)w * (size_t)h * 4u)};
  for (int i = 0; i < w * h; ++i) {
    img.rgba[i * 4 + 3] = 255;
  }
  unsigned char *png;
  size_t len;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_image_encode_png(zb_test_alloc(), &img, &png, &len));
  zb_memfs_put(g_fs, path, png, len);
  zb_free(zb_test_alloc(), png);
  zb_image_free(zb_test_alloc(), &img);
}

typedef struct world {
  zb_vfs *vfs;
  zb_render *r;
  zb_render_backend *backend;
} world;

static world make_world(void) {
  put_text("assets/base/package.lua", "return { id = 'base', version = '0.1.0', api = '1.0', license = 'MIT' }");
  put_real("assets/base/models/carpet.glb", "assets/base/models/carpet.glb");
  put_real("assets/base/fonts/pixel.lua", "assets/base/fonts/pixel.lua");
  put_real("assets/base/fonts/pixel.png", "assets/base/fonts/pixel.png");
  put_png("assets/base/sprites/sheet.png", 64, 16);
  put_text("assets/base/sprites/anim.lua",
           "return { texture = 'sprites/sheet.png', frame_width = 16, frame_height = 16, frames = 4, fps = 8,"
           " pivot = { 0.5, 0 }, size = { 1, 1 } }");
  put_text("assets/base/sprites/broken.lua", "return { frames = ");
  put_text("assets/base/sprites/notexture.lua", "return { frames = 2 }");
  put_text("assets/base/models/corrupt.glb", "this is not a glb");
  world w;
  w.vfs = zb_vfs_create(zb_test_alloc(), zb_memfs_api(g_fs), NULL);
  zb_vfs_add_root(w.vfs, "assets");
  zb_vfs_mount(w.vfs, 1, 0);
  w.backend = zb_render_null_backend(zb_test_alloc());
  w.r = zb_render_create(zb_test_alloc(), w.vfs, w.backend);
  return w;
}

static void free_world(world *w) {
  zb_render_destroy(w->r);
  zb_vfs_destroy(w->vfs);
}

static void settle(world *w) {
  for (int i = 0; i < 4; ++i) {
    zb_vfs_poll(w->vfs);
  }
}

/* spec: rendering-core: Full HD window */
/* spec: rendering-core: Non-integer window size */
static void test_layout(void) {
  zb_layout l = zb_render_compute_layout(1920, 1080, 360);
  TEST_ASSERT_EQUAL_INT(640, l.internal_w);
  TEST_ASSERT_EQUAL_INT(360, l.internal_h);
  TEST_ASSERT_EQUAL_INT(3, l.scale);
  TEST_ASSERT_EQUAL_INT(1920, l.out_w);
  TEST_ASSERT_EQUAL_INT(0, l.out_x);
  l = zb_render_compute_layout(1000, 700, 360);
  TEST_ASSERT_EQUAL_INT(360, l.internal_h);
  TEST_ASSERT_EQUAL_INT(514, l.internal_w);
  TEST_ASSERT_EQUAL_INT(0, l.scale);
  TEST_ASSERT_TRUE(l.out_w >= 998 && l.out_w <= 1000);
  TEST_ASSERT_TRUE(l.out_h >= 698 && l.out_h <= 700);
  TEST_ASSERT_TRUE(l.out_x >= 0 && l.out_y >= 0);
  l = zb_render_compute_layout(0, 0, 360);
  TEST_ASSERT_EQUAL_INT(640, l.internal_w);
  l = zb_render_compute_layout(1, 4000, 360);
  TEST_ASSERT_EQUAL_INT(1, l.internal_w);
}

/* spec: rendering-core: Draw list inspection */
static void test_draw_list_and_culling(void) {
  world w = make_world();
  uint32_t carpet = zb_render_load_model(w.r, "models/carpet.glb");
  TEST_ASSERT_EQUAL_UINT32(carpet, zb_render_load_model(w.r, "models/carpet.glb"));
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_PENDING, zb_render_asset_state(w.r, carpet));
  zb_render_begin(w.r, 1280, 720);
  zb_render_draw_model(w.r, carpet, zb_m4_identity()); /* still pending: not drawn */
  zb_render_end(w.r);
  TEST_ASSERT_EQUAL_size_t(0, zb_render_null_item_count(w.backend, ZB_DRAW_MODEL));
  settle(&w);
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_READY, zb_render_asset_state(w.r, carpet));
  zb_render_set_camera(w.r, zb_v3(0, 0, 5), zb_v3(0, 0, 0), zb_v3(0, 1, 0), 60, 0.1f, 100);
  zb_render_begin(w.r, 1280, 720);
  zb_render_draw_model(w.r, carpet, zb_m4_translate(zb_v3(-1, 0, 0)));
  zb_render_draw_model(w.r, carpet, zb_m4_identity());
  zb_render_draw_model(w.r, carpet, zb_m4_translate(zb_v3(1, 0, 0)));
  zb_render_draw_model(w.r, carpet, zb_m4_translate(zb_v3(0, 0, 20))); /* behind the camera */
  zb_render_draw_model(w.r, 999, zb_m4_identity());                    /* invalid handle */
  zb_render_end(w.r);
  TEST_ASSERT_EQUAL_size_t(3, zb_render_null_item_count(w.backend, ZB_DRAW_MODEL));
  const zb_render_stats *st = zb_render_get_stats(w.r);
  TEST_ASSERT_EQUAL_size_t(3, st->model_items);
  TEST_ASSERT_EQUAL_size_t(1, st->culled);
  TEST_ASSERT_EQUAL_INT(1, st->views);
  zb_render_end(w.r); /* ignored outside a frame */
  TEST_ASSERT_EQUAL_size_t(2, zb_render_null_frames(w.backend));
  zb_image img;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_UNSUPPORTED, zb_render_capture(w.r, true, zb_test_alloc(), &img));
  TEST_ASSERT_TRUE(zb_render_asset_count(w.r) >= 1);
  TEST_ASSERT_EQUAL_PTR(w.backend, zb_render_backend_of(w.r));
  free_world(&w);
}

/* spec: rendering-core: Forced stereo */
static void test_two_views(void) {
  world w = make_world();
  uint32_t carpet = zb_render_load_model(w.r, "models/carpet.glb");
  settle(&w);
  zb_render_set_view_count(w.r, 2);
  zb_render_set_eye_separation(w.r, 0.1f);
  zb_render_begin(w.r, 1280, 720);
  zb_render_draw_model(w.r, carpet, zb_m4_identity());
  zb_render_end(w.r);
  TEST_ASSERT_EQUAL_INT(2, zb_render_get_stats(w.r)->views);
  TEST_ASSERT_EQUAL_size_t(2, zb_render_null_item_count(w.backend, ZB_DRAW_MODEL));
  zb_render_set_view_count(w.r, 7);
  zb_render_set_view_count(w.r, 0);
  zb_render_set_internal_height(w.r, 2);
  TEST_ASSERT_EQUAL_INT(16, zb_render_internal_height(w.r));
  zb_render_set_internal_height(w.r, 100000);
  TEST_ASSERT_EQUAL_INT(4320, zb_render_internal_height(w.r));
  free_world(&w);
}

static void assert_m4(zb_mat4 e, zb_mat4 a) {
  for (int k = 0; k < 16; ++k) {
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, e.m[k], a.m[k]);
  }
}

static size_t model_items_in_view(const zb_frame *f, int view) {
  size_t n = 0;
  for (size_t i = 0; i < f->item_count; ++i) {
    n += f->items[i].kind == ZB_DRAW_MODEL && f->items[i].view == view;
  }
  return n;
}

/* spec: webxr-session: Views from the headset */
static void test_explicit_views(void) {
  world w = make_world();
  uint32_t carpet = zb_render_load_model(w.r, "models/carpet.glb");
  settle(&w);
  zb_render_set_camera(w.r, zb_v3(0, 1.5f, 4), zb_v3(0, 1.5f, 0), zb_v3(0, 1, 0), 60, 0.2f, 300);
  zb_mat4 camera = zb_m4_look_at(zb_v3(0, 1.5f, 4), zb_v3(0, 1.5f, 0), zb_v3(0, 1, 0));
  /* An emulated left eye and a second view turned around (looking backwards),
   * in 400 x 400 viewports of an 820 x 420 framebuffer. */
  zb_xr_frame rig;
  zb_xr_emulate((zb_xr_pose){zb_v3(0, 0, 0), zb_quat_identity()}, 800, 400, 0.064f, &rig);
  zb_xr_view views[2] = {rig.views[0], rig.views[1]};
  views[0].x = 10;
  views[0].y = 10;
  views[0].width = 400;
  views[0].height = 400;
  views[1].x = 410;
  views[1].y = 10;
  views[1].width = 400;
  views[1].height = 400;
  views[1].view = zb_m4_from_quat(zb_quat_axis_angle(zb_v3(0, 1, 0), ZB_PI));
  zb_render_set_views(w.r, views, 2);
  zb_render_set_target(w.r, 7, 820, 420);
  zb_render_begin(w.r, 1280, 720);                                       /* the window size does not matter now */
  zb_render_draw_model(w.r, carpet, zb_m4_translate(zb_v3(0, 1.5f, 0))); /* ahead of the camera */
  zb_render_draw_model(w.r, carpet, zb_m4_translate(zb_v3(0, 1.5f, 8))); /* behind it */
  zb_render_end(w.r);
  const zb_frame *f = zb_render_null_last_frame(w.backend);
  TEST_ASSERT_TRUE(f->explicit_views);
  TEST_ASSERT_EQUAL_UINT32(7, f->target);
  TEST_ASSERT_EQUAL_INT(2, f->view_count);
  TEST_ASSERT_EQUAL_INT(820, f->layout.drawable_w);
  TEST_ASSERT_EQUAL_INT(420, f->layout.drawable_h);
  /* Each eye at the internal height with its viewport's aspect, side by side. */
  TEST_ASSERT_EQUAL_INT(360, f->layout.internal_h);
  TEST_ASSERT_EQUAL_INT(720, f->layout.internal_w);
  TEST_ASSERT_EQUAL_INT(0, f->layout.scale);
  for (int v = 0; v < 2; ++v) {
    const zb_view *vw = &f->views[v];
    TEST_ASSERT_EQUAL_INT(v * 360, vw->x);
    TEST_ASSERT_EQUAL_INT(0, vw->y);
    TEST_ASSERT_EQUAL_INT(360, vw->width);
    TEST_ASSERT_EQUAL_INT(360, vw->height);
    TEST_ASSERT_EQUAL_INT(views[v].x, vw->out_x);
    TEST_ASSERT_EQUAL_INT(views[v].y, vw->out_y);
    TEST_ASSERT_EQUAL_INT(400, vw->out_w);
    TEST_ASSERT_EQUAL_INT(400, vw->out_h);
    /* The headset's view relative to the camera. */
    assert_m4(zb_m4_mul(views[v].view, camera), vw->view);
    /* The headset's field of view with the camera's depth range. */
    TEST_ASSERT_EQUAL_FLOAT(views[v].proj.m[0], vw->proj.m[0]);
    TEST_ASSERT_EQUAL_FLOAT(views[v].proj.m[5], vw->proj.m[5]);
    TEST_ASSERT_EQUAL_FLOAT(views[v].proj.m[8], vw->proj.m[8]);
    TEST_ASSERT_EQUAL_FLOAT(views[v].proj.m[9], vw->proj.m[9]);
    zb_mat4 cam_proj = zb_m4_perspective(1.0f, 1.0f, 0.2f, 300.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, cam_proj.m[10], vw->proj.m[10]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, cam_proj.m[14], vw->proj.m[14]);
    assert_m4(zb_m4_mul(vw->proj, vw->view), vw->view_proj);
  }
  /* Eyes, axes and culling follow the composed matrices. */
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, -0.032f, f->views[0].eye.x);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.5f, f->views[0].eye.y);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.0f, f->views[0].eye.z);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, f->views[0].right.x);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, f->views[0].up.y);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, -1.0f, f->views[1].right.x);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.0f, f->views[1].eye.z);
  TEST_ASSERT_TRUE(model_items_in_view(f, 0) > 0);
  TEST_ASSERT_TRUE(model_items_in_view(f, 1) > 0);
  TEST_ASSERT_EQUAL_size_t(2, zb_render_get_stats(w.r)->culled); /* each carpet in one view only */
  TEST_ASSERT_EQUAL_INT(2, zb_render_get_stats(w.r)->views);

  /* Viewports twice the internal height: an integer upscale. Orthographic
   * projections keep their depth terms; viewless sizes stay drawable. */
  views[0].width = views[0].height = 720;
  views[1].width = views[1].height = 0;
  views[1].proj = zb_m4_ortho(-1, 1, -1, 1, 0.5f, 2.0f);
  zb_render_set_views(w.r, views, 5); /* clamped to two */
  zb_render_begin(w.r, 1280, 720);
  zb_render_end(w.r);
  f = zb_render_null_last_frame(w.backend);
  TEST_ASSERT_EQUAL_INT(2, f->view_count);
  TEST_ASSERT_EQUAL_INT(360, f->views[0].width);
  TEST_ASSERT_EQUAL_INT(360, f->views[1].width); /* 0 x 0 counts as square */
  TEST_ASSERT_EQUAL_INT(0, f->layout.scale);
  assert_m4(views[1].proj, f->views[1].proj);
  zb_render_set_views(w.r, views, 1);
  zb_render_begin(w.r, 1280, 720);
  zb_render_end(w.r);
  f = zb_render_null_last_frame(w.backend);
  TEST_ASSERT_EQUAL_INT(1, f->view_count);
  TEST_ASSERT_EQUAL_INT(2, f->layout.scale);

  /* Cleared: camera views again; a target alone takes the target's size. */
  zb_render_set_views(w.r, NULL, 2);
  zb_render_begin(w.r, 1280, 720);
  zb_render_end(w.r);
  f = zb_render_null_last_frame(w.backend);
  TEST_ASSERT_FALSE(f->explicit_views);
  TEST_ASSERT_EQUAL_INT(1, f->view_count);
  TEST_ASSERT_EQUAL_UINT32(7, f->target);
  TEST_ASSERT_EQUAL_INT(820, f->layout.drawable_w);
  zb_render_set_target(w.r, 7, 0, 420);
  zb_render_set_views(w.r, views, -1);
  zb_render_begin(w.r, 1280, 720);
  zb_render_end(w.r);
  f = zb_render_null_last_frame(w.backend);
  TEST_ASSERT_EQUAL_UINT32(0, f->target);
  TEST_ASSERT_EQUAL_INT(1280, f->layout.drawable_w);
  TEST_ASSERT_EQUAL_INT(640, f->layout.internal_w);
  free_world(&w);
}

/* m * (x, y, z, 1) in clip space, divided by w. */
static void project(zb_mat4 m, zb_vec3 p, float *nx, float *ny, float *w) {
  const float *a = m.m;
  float x = a[0] * p.x + a[4] * p.y + a[8] * p.z + a[12];
  float y = a[1] * p.x + a[5] * p.y + a[9] * p.z + a[13];
  *w = a[3] * p.x + a[7] * p.y + a[11] * p.z + a[15];
  *nx = x / *w;
  *ny = y / *w;
}

/* spec: webxr-play: HUD readable in the headset */
static void test_xr_hud_and_vignette(void) {
  world w = make_world();
  settle(&w);
  zb_render_set_camera(w.r, zb_v3(5, 1.5f, 4), zb_v3(5, 1.5f, 0), zb_v3(0, 1, 0), 60, 0.5f, 300);
  /* A head 1.2 m up, turned 40 degrees to the left: the canvas centre lies straight ahead of it. */
  zb_xr_pose head = {zb_v3(0.3f, 1.2f, -0.1f), zb_quat_axis_angle(zb_v3(0, 1, 0), 40.0f * ZB_PI / 180.0f)};
  zb_xr_frame rig;
  zb_xr_emulate(head, 640, 360, 0.064f, &rig);
  zb_render_set_views(w.r, rig.views, 2);
  /* The default: the canvas fills each view (the same place in both eyes). */
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_render_xr_hud(w.r, NULL));
  zb_render_begin(w.r, 640, 360);
  zb_render_end(w.r);
  const zb_frame *f = zb_render_null_last_frame(w.backend);
  int cw = f->views[0].width, ih = f->layout.internal_h;
  TEST_ASSERT_EQUAL_INT(320, cw);
  for (int v = 0; v < 2; ++v) {
    assert_m4(zb_m4_ortho(0.0f, (float)cw, (float)ih, 0.0f, -1.0f, 1.0f), f->views[v].text);
  }
  TEST_ASSERT_EQUAL_FLOAT(0.0f, f->vignette);
  /* Head-locked at 2 m: every canvas pixel is in the same direction from the head in both eyes. */
  zb_render_set_xr_hud(w.r, 2.0f, 0.0f);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, zb_render_xr_hud(w.r, NULL));
  zb_render_set_vignette(w.r, 0.6f);
  zb_render_begin(w.r, 640, 360);
  zb_render_end(w.r);
  f = zb_render_null_last_frame(w.backend);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.6f, f->vignette);
  zb_vec3 fwd = zb_quat_rotate(head.orientation, zb_v3(0, 0, -1));
  zb_vec3 right = zb_quat_rotate(head.orientation, zb_v3(1, 0, 0));
  zb_vec3 up = zb_quat_rotate(head.orientation, zb_v3(0, 1, 0));
  float k = 2.0f / rig.views[0].proj.m[5] / (float)ih; /* tangent per pixel */
  const float px[3][2] = {{160, 180}, {200, 240}, {10, 20}};
  for (int i = 0; i < 3; ++i) {
    zb_vec3 at = zb_v3_add(head.position, zb_v3_scale(fwd, 2.0f));
    at = zb_v3_add(at, zb_v3_scale(right, (px[i][0] - 0.5f * (float)cw) * k * 2.0f));
    at = zb_v3_add(at, zb_v3_scale(up, -(px[i][1] - 0.5f * (float)ih) * k * 2.0f));
    for (int v = 0; v < 2; ++v) {
      float ex, ey, ew, tx, ty, tw;
      project(zb_m4_mul(rig.views[v].proj, rig.views[v].view), at, &ex, &ey, &ew);
      project(f->views[v].text, zb_v3(px[i][0], px[i][1], 0.0f), &tx, &ty, &tw);
      TEST_ASSERT_FLOAT_WITHIN(1e-4f, ex, tx);
      TEST_ASSERT_FLOAT_WITHIN(1e-4f, ey, ty);
      TEST_ASSERT_TRUE(tw > 1.9f);
    }
  }
  /* The canvas centre is straight ahead: right of the left eye's viewport centre, left of the right one's. */
  float cx0, cy0, cx1, cy1, w0;
  project(f->views[0].text, zb_v3(160, 180, 0), &cx0, &cy0, &w0);
  project(f->views[1].text, zb_v3(160, 180, 0), &cx1, &cy1, &w0);
  TEST_ASSERT_TRUE(cx0 > 0.05f && cx1 < -0.05f);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, cy0);
  /* Never clipped by the depth range: z is 0. */
  for (int v = 0; v < 2; ++v) {
    const float *t = f->views[v].text.m;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, t[2] + t[6] + t[10] + t[14]);
  }
  /* An explicit pixel size: 3 mm per canvas pixel on a plane 1.5 m away. */
  zb_render_set_xr_hud(w.r, 1.5f, 0.003f);
  float pixel = 0.0f;
  TEST_ASSERT_EQUAL_FLOAT(1.5f, zb_render_xr_hud(w.r, &pixel));
  TEST_ASSERT_EQUAL_FLOAT(0.003f, pixel);
  zb_render_begin(w.r, 640, 360);
  zb_render_end(w.r);
  f = zb_render_null_last_frame(w.backend);
  {
    zb_vec3 at = zb_v3_add(zb_v3_add(head.position, zb_v3_scale(fwd, 1.5f)), zb_v3_scale(up, -100.0f * 0.003f));
    float ex, ey, ew, tx, ty, tw;
    project(zb_m4_mul(rig.views[1].proj, rig.views[1].view), at, &ex, &ey, &ew);
    project(f->views[1].text, zb_v3(160, 280, 0.0f), &tx, &ty, &tw);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, ex, tx);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, ey, ty);
  }
  /* Clamped and invalid settings. */
  zb_render_set_xr_hud(w.r, 5000.0f, 7.0f);
  TEST_ASSERT_EQUAL_FLOAT(1000.0f, zb_render_xr_hud(w.r, &pixel));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, pixel);
  zb_render_set_xr_hud(w.r, NAN, NAN);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_render_xr_hud(w.r, &pixel));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pixel);
  zb_render_set_vignette(w.r, 3.0f);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, zb_render_vignette(w.r));
  zb_render_set_vignette(w.r, -1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_render_vignette(w.r));
  zb_render_set_vignette(w.r, INFINITY);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, zb_render_vignette(w.r));
  /* A projection without a vertical scale cannot place the panel: the canvas fills the views. */
  zb_xr_view flat[2] = {rig.views[0], rig.views[1]};
  flat[0].proj.m[5] = flat[1].proj.m[5] = 0.0f;
  zb_render_set_xr_hud(w.r, 2.0f, 0.0f);
  zb_render_set_views(w.r, flat, 2);
  zb_render_begin(w.r, 640, 360);
  zb_render_end(w.r);
  f = zb_render_null_last_frame(w.backend);
  assert_m4(zb_m4_ortho(0.0f, (float)f->views[1].width, (float)ih, 0.0f, -1.0f, 1.0f), f->views[1].text);
  free_world(&w);
}

/* spec: rendering-core: Corrupt model */
static void test_corrupt_and_missing_model(void) {
  world w = make_world();
  uint32_t bad = zb_render_load_model(w.r, "models/corrupt.glb");
  uint32_t none = zb_render_load_model(w.r, "models/none.glb");
  settle(&w);
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_FAILED, zb_render_asset_state(w.r, bad));
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_FAILED, zb_render_asset_state(w.r, none));
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_FAILED, zb_render_asset_state(w.r, 12345));
  zb_render_begin(w.r, 640, 360);
  zb_render_draw_model(w.r, bad, zb_m4_identity());
  zb_render_end(w.r);
  TEST_ASSERT_EQUAL_size_t(1, zb_render_null_item_count(w.backend, ZB_DRAW_MODEL)); /* placeholder cube */
  TEST_ASSERT_TRUE(g_warnings >= 2);
  free_world(&w);
}

/* spec: asset-packages: Missing texture */
static void test_missing_texture_placeholder(void) {
  world w = make_world();
  uint32_t t1 = zb_render_load_texture(w.r, "textures/missing.png");
  uint32_t t2 = zb_render_load_texture(w.r, "textures/missing.png");
  TEST_ASSERT_EQUAL_UINT32(t1, t2);
  settle(&w);
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_FAILED, zb_render_asset_state(w.r, t1));
  TEST_ASSERT_EQUAL_INT(1, g_warnings);
  zb_render_load_texture(w.r, "textures/missing.png");
  settle(&w);
  TEST_ASSERT_EQUAL_INT(1, g_warnings); /* reported once */
  put_text("assets/base/textures/notpng.png", "nope");
  uint32_t t3 = zb_render_load_texture(w.r, "textures/notpng.png");
  settle(&w);
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_FAILED, zb_render_asset_state(w.r, t3));
  free_world(&w);
}

/* spec: rendering-core: Animated sprite */
static void test_sprites(void) {
  world w = make_world();
  uint32_t s = zb_render_load_sprite(w.r, "sprites/anim.lua");
  uint32_t broken = zb_render_load_sprite(w.r, "sprites/broken.lua");
  uint32_t notex = zb_render_load_sprite(w.r, "sprites/notexture.lua");
  uint32_t missing = zb_render_load_sprite(w.r, "sprites/missing.lua");
  TEST_ASSERT_EQUAL_UINT64(0, zb_render_sprite_frame_number(w.r, s, 1.0)); /* not loaded yet */
  settle(&w);
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_READY, zb_render_asset_state(w.r, s));
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_FAILED, zb_render_asset_state(w.r, broken));
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_FAILED, zb_render_asset_state(w.r, notex));
  TEST_ASSERT_EQUAL_INT(ZB_ASSET_FAILED, zb_render_asset_state(w.r, missing));
  uint64_t f0 = zb_render_sprite_frame_number(w.r, s, 1.25);
  uint64_t f1 = zb_render_sprite_frame_number(w.r, s, 1.75);
  TEST_ASSERT_EQUAL_UINT64(4, f1 - f0);
  TEST_ASSERT_EQUAL_UINT64(0, zb_render_sprite_frame_number(w.r, s, -1.0));
  zb_render_begin(w.r, 640, 360);
  zb_render_draw_sprite(w.r, s, zb_v3(0, 0, 0), 1.0f, 0.3);
  zb_render_draw_sprite(w.r, s, zb_v3(0, 0, 50), 1.0f, 0.3); /* behind */
  zb_render_draw_sprite(w.r, broken, zb_v3(0, 0, 0), 1.0f, 0.0);
  zb_render_end(w.r);
  TEST_ASSERT_EQUAL_size_t(1, zb_render_null_item_count(w.backend, ZB_DRAW_SPRITE));
  free_world(&w);
}

/* spec: rendering-core: Missing glyph */
static void test_text_and_glyphs(void) {
  world w = make_world();
  uint32_t font = zb_render_load_font(w.r, "fonts/pixel.lua");
  uint32_t glyphs[16];
  TEST_ASSERT_EQUAL_size_t(0, zb_render_text_glyphs(w.r, font, "abc", glyphs, 16)); /* pending */
  settle(&w);
  size_t n = zb_render_text_glyphs(w.r, font, "A\xE2\x82\xACz", glyphs, 16); /* A, euro (missing), z */
  TEST_ASSERT_EQUAL_size_t(3, n);
  uint32_t box[1];
  zb_render_text_glyphs(w.r, font, "\xE2\x96\xA1", box, 1); /* the replacement glyph itself */
  TEST_ASSERT_EQUAL_UINT32(box[0], glyphs[1]);
  TEST_ASSERT_NOT_EQUAL(glyphs[0], glyphs[1]);
  TEST_ASSERT_EQUAL_size_t(0, zb_render_text_glyphs(w.r, 999, "x", glyphs, 16));
  zb_render_begin(w.r, 640, 360);
  static const float white[4] = {1, 1, 1, 1};
  zb_render_draw_text(w.r, font, "Hallo \xC3\xBC", 4, 4, 1, white);
  zb_render_draw_text(w.r, 999, "x", 4, 4, 1, white);
  zb_render_end(w.r);
  TEST_ASSERT_EQUAL_size_t(7, zb_render_get_stats(w.r)->text_glyphs);
  free_world(&w);
}

static void test_utf8(void) {
  uint32_t cp;
  TEST_ASSERT_EQUAL_size_t(0, zb_utf8_decode("", &cp));
  TEST_ASSERT_EQUAL_size_t(1, zb_utf8_decode("A", &cp));
  TEST_ASSERT_EQUAL_UINT32('A', cp);
  TEST_ASSERT_EQUAL_size_t(2, zb_utf8_decode("\xC3\xBC", &cp));
  TEST_ASSERT_EQUAL_UINT32(0xFC, cp);
  TEST_ASSERT_EQUAL_size_t(3, zb_utf8_decode("\xE2\x82\xAC", &cp));
  TEST_ASSERT_EQUAL_UINT32(0x20AC, cp);
  TEST_ASSERT_EQUAL_size_t(4, zb_utf8_decode("\xF0\x9F\x98\x80", &cp));
  TEST_ASSERT_EQUAL_UINT32(0x1F600, cp);
  const char *invalid[] = {"\x80", "\xC3", "\xC0\x80", "\xE0\x80\x80", "\xF4\x90\x80\x80", "\xFF"};
  for (size_t i = 0; i < ZB_ARRAY_LEN(invalid); ++i) {
    TEST_ASSERT_EQUAL_size_t(1, zb_utf8_decode(invalid[i], &cp));
    TEST_ASSERT_EQUAL_UINT32(0xFFFD, cp);
  }
}

static void test_images(void) {
  zb_image a, b;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_image_checkerboard(zb_test_alloc(), 8, &a));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_image_checkerboard(zb_test_alloc(), 8, &b));
  zb_image_diff diff;
  TEST_ASSERT_TRUE(zb_image_compare(&a, &b, 0, 0.0, &diff));
  TEST_ASSERT_EQUAL_INT(0, diff.max_delta);
  b.rgba[0] = (unsigned char)(b.rgba[0] - 20);
  TEST_ASSERT_FALSE(zb_image_compare(&a, &b, 8, 0.0, &diff));
  TEST_ASSERT_TRUE(zb_image_compare(&a, &b, 8, 0.02, NULL));
  unsigned char *png;
  size_t len;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_image_encode_png(zb_test_alloc(), &a, &png, &len));
  zb_image c;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_image_decode_png(zb_test_alloc(), png, len, &c));
  TEST_ASSERT_TRUE(zb_image_compare(&a, &c, 0, 0.0, NULL));
  zb_image_flip_rows(&c);
  TEST_ASSERT_FALSE(zb_image_compare(&a, &c, 0, 0.0, NULL));
  zb_image small;
  zb_image_checkerboard(zb_test_alloc(), 4, &small);
  TEST_ASSERT_FALSE(zb_image_compare(&a, &small, 255, 1.0, &diff));
  TEST_ASSERT_FALSE(diff.same_size);
  zb_image_free(zb_test_alloc(), &c);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_PARSE, zb_image_decode_png(zb_test_alloc(), "junk", 4, &c));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_image_decode_png(zb_test_alloc(), NULL, 0, &c));
  zb_failing_allocator fa;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_image_decode_png(zb_failing_init(&fa, zb_test_alloc(), 0), png, len, &c));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_image_checkerboard(zb_failing_init(&fa, zb_test_alloc(), 0), 4, &c));
  zb_free(zb_test_alloc(), png);
  zb_image_free(zb_test_alloc(), &a);
  zb_image_free(zb_test_alloc(), &b);
  zb_image_free(zb_test_alloc(), &c);
  zb_image_free(zb_test_alloc(), &small);
}

static zb_result read_from_memfs(void *user, const char *path, const zb_allocator *a, char **data, size_t *len) {
  (void)user;
  const zb_fs_api *fs = zb_memfs_api(g_fs);
  return fs->read(fs->ctx, path, a, data, len);
}

static void test_gltf_variants(void) {
  char err[256];
  zb_mesh_data mesh;
  /* embedded .gltf with a data-URI buffer: one triangle, no normals, colours or uvs */
  const char *tri =
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
      "\"nodes\":[{\"mesh\":0,\"translation\":[1,0,0]}],"
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
      "\"buffers\":[{\"byteLength\":36,\"uri\":\"data:application/octet-stream;base64,"
      "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\"}],"
      "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
      "\"min\":[0,0,0],\"max\":[1,1,0]}]}";
  TEST_ASSERT_EQUAL_INT(ZB_OK,
                        zb_mesh_load_gltf(zb_test_alloc(), tri, strlen(tri), NULL, NULL, &mesh, err, sizeof err));
  TEST_ASSERT_EQUAL_size_t(3, mesh.vertex_count);
  TEST_ASSERT_EQUAL_size_t(3, mesh.index_count);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, mesh.vertices[0].pos[0]); /* node translation baked in */
  zb_mesh_free(zb_test_alloc(), &mesh);
  /* external buffer resolved through the read callback */
  zb_memfs_put(g_fs, "tri.bin", "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x80\x3f\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x80\x3f\0\0\0\0", 36);
  const char *ext =
      "{\"asset\":{\"version\":\"2.0\"},\"nodes\":[{\"mesh\":0}],"
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
      "\"buffers\":[{\"byteLength\":36,\"uri\":\"tri.bin\"}],"
      "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
      "\"min\":[0,0,0],\"max\":[1,1,0]}]}";
  TEST_ASSERT_EQUAL_INT(
      ZB_OK, zb_mesh_load_gltf(zb_test_alloc(), ext, strlen(ext), read_from_memfs, NULL, &mesh, err, sizeof err));
  zb_mesh_free(zb_test_alloc(), &mesh);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_PARSE,
                        zb_mesh_load_gltf(zb_test_alloc(), ext, strlen(ext), NULL, NULL, &mesh, err, sizeof err));
  const char *empty = "{\"asset\":{\"version\":\"2.0\"},\"nodes\":[{}]}";
  TEST_ASSERT_EQUAL_INT(ZB_ERR_PARSE,
                        zb_mesh_load_gltf(zb_test_alloc(), empty, strlen(empty), NULL, NULL, &mesh, err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "no triangle meshes"));
  zb_mesh_data cube;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_mesh_cube(zb_test_alloc(), &cube));
  TEST_ASSERT_EQUAL_size_t(36, cube.index_count);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.866f, cube.radius);
  zb_mesh_free(zb_test_alloc(), &cube);
  zb_mesh_data none;
  memset(&none, 0, sizeof none);
  zb_mesh_compute_bounds(&none);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, none.radius);
  zb_failing_allocator fa;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_mesh_cube(zb_failing_init(&fa, zb_test_alloc(), 1), &cube));
  for (long budget = 0; budget < 6; ++budget) {
    zb_mesh_data m;
    if (zb_mesh_load_gltf(zb_failing_init(&fa, zb_test_alloc(), budget), tri, strlen(tri), NULL, NULL, &m, err,
                          sizeof err) == ZB_OK) {
      zb_mesh_free(zb_failing_init(&fa, zb_test_alloc(), -1), &m);
    }
  }
}

/* Destroying the renderer while loads are pending frees their contexts. */
static void test_destroy_with_pending_loads(void) {
  world w = make_world();
  zb_render_load_model(w.r, "models/carpet.glb");
  zb_render_load_font(w.r, "fonts/pixel.lua");
  free_world(&w);
}

static void test_create_failure(void) {
  zb_failing_allocator fa;
  zb_render_backend *b = zb_render_null_backend(zb_test_alloc());
  TEST_ASSERT_NULL(zb_render_create(zb_failing_init(&fa, zb_test_alloc(), 0), NULL, b));
  TEST_ASSERT_NULL(zb_render_null_backend(zb_failing_init(&fa, zb_test_alloc(), 0)));
  zb_render_destroy(NULL);
}

static void test_gl_version_parsing(void) {
  bool es;
  int major, minor;
  TEST_ASSERT_TRUE(zb_gl_parse_version("4.6 (Core Profile) Mesa 25.2", &es, &major, &minor));
  TEST_ASSERT_FALSE(es);
  TEST_ASSERT_EQUAL_INT(4, major);
  TEST_ASSERT_TRUE(zb_gl_version_ok(es, major, minor));
  TEST_ASSERT_TRUE(zb_gl_parse_version("OpenGL ES 3.0 (WebGL 2.0 (OpenGL ES 3.0 Chromium))", &es, &major, &minor));
  TEST_ASSERT_TRUE(es);
  TEST_ASSERT_EQUAL_INT(3, major);
  TEST_ASSERT_TRUE(zb_gl_parse_version("WebGL 2.0 (OpenGL ES 3.0 Chromium)", &es, &major, &minor));
  TEST_ASSERT_EQUAL_INT(3, major);
  TEST_ASSERT_TRUE(es);
  TEST_ASSERT_TRUE(zb_gl_parse_version("2.1 Mesa", &es, &major, &minor));
  TEST_ASSERT_FALSE(zb_gl_version_ok(es, major, minor));
  TEST_ASSERT_FALSE(zb_gl_version_ok(false, 3, 2));
  TEST_ASSERT_TRUE(zb_gl_version_ok(false, 3, 3));
  TEST_ASSERT_FALSE(zb_gl_version_ok(true, 2, 0));
  TEST_ASSERT_FALSE(zb_gl_parse_version(NULL, &es, &major, &minor));
  TEST_ASSERT_FALSE(zb_gl_parse_version("no digits", &es, &major, &minor));
}

/* A fake GL: glGetString reports a chosen version, every other entry point
 * exists but is never called before the version check. */
static const char *g_fake_version;
static const char *g_missing_fn;

static const unsigned char *fake_get_string(unsigned int name) {
  (void)name;
  return (const unsigned char *)g_fake_version;
}

static void fake_noop(void) {}

static zb_gl_fn fake_loader(void *ctx, const char *name) {
  (void)ctx;
  if (g_missing_fn && strcmp(name, g_missing_fn) == 0) {
    return NULL;
  }
  if (strcmp(name, "glGetString") == 0) {
    return (zb_gl_fn)fake_get_string;
  }
  return fake_noop;
}

/* spec: platform-targets: Unsupported graphics context */
static void test_gl_backend_rejects_old_versions(void) {
  char err[256];
  g_missing_fn = NULL;
  g_fake_version = "2.1 Mesa 25.2";
  TEST_ASSERT_NULL(zb_render_gl_backend(zb_test_alloc(), fake_loader, NULL, false, NULL, err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "OpenGL 3.3 core profile is required"));
  TEST_ASSERT_NOT_NULL(strstr(err, "2.1 Mesa 25.2"));
  g_fake_version = "OpenGL ES 2.0 (WebGL 1.0)";
  TEST_ASSERT_NULL(zb_render_gl_backend(zb_test_alloc(), fake_loader, NULL, true, NULL, err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "OpenGL ES 3.0 / WebGL 2"));
  g_fake_version = NULL;
  TEST_ASSERT_NULL(zb_render_gl_backend(zb_test_alloc(), fake_loader, NULL, false, NULL, err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "unknown"));
  g_fake_version = "3.3 Mesa";
  g_missing_fn = "glBlitFramebuffer";
  TEST_ASSERT_NULL(zb_render_gl_backend(zb_test_alloc(), fake_loader, NULL, false, NULL, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("OpenGL function glBlitFramebuffer is missing (OpenGL 3.3 Mesa)", err);
  /* A legacy 1.1 context (Windows without a driver) reports its version. */
  g_missing_fn = "glGetString";
  TEST_ASSERT_NULL(zb_render_gl_backend(zb_test_alloc(), fake_loader, NULL, false, NULL, err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "detected version is unknown (unknown renderer)"));
  g_missing_fn = NULL;
  zb_failing_allocator fa;
  TEST_ASSERT_NULL(
      zb_render_gl_backend(zb_failing_init(&fa, zb_test_alloc(), 0), fake_loader, NULL, false, NULL, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("out of memory", err);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_layout);
  RUN_TEST(test_draw_list_and_culling);
  RUN_TEST(test_two_views);
  RUN_TEST(test_explicit_views);
  RUN_TEST(test_xr_hud_and_vignette);
  RUN_TEST(test_corrupt_and_missing_model);
  RUN_TEST(test_missing_texture_placeholder);
  RUN_TEST(test_sprites);
  RUN_TEST(test_text_and_glyphs);
  RUN_TEST(test_utf8);
  RUN_TEST(test_images);
  RUN_TEST(test_gltf_variants);
  RUN_TEST(test_destroy_with_pending_loads);
  RUN_TEST(test_create_failure);
  RUN_TEST(test_gl_version_parsing);
  RUN_TEST(test_gl_backend_rejects_old_versions);
  return UNITY_END();
}

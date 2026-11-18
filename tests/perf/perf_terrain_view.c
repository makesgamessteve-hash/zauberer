/* SPDX-License-Identifier: MIT */
/* Terrain drawing budgets with the desktop profile (release builds on Linux). */
#define _POSIX_C_SOURCE 199309L
#include "app/terrain_view.h"
#include "support/test_support.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

ZB_TEST_LEAK_CHECKED

#define FRAMES 30
#define MAX_ITEMS 4000
#define MAX_TRIANGLES 1000000
#define BUDGET_MS 3.0

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec * 1e-6;
}

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

static int cmp_double(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : (x > y);
}

static void desktop_frame(const zb_flora *flora) {
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_DESKTOP;
  d.flora = flora;
  d.clock_ns = frozen_clock; /* inline: every update generates all it can */
  char err[128];
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(w, err);
  zb_render *r = zb_render_create(zb_test_alloc(), NULL, zb_render_null_backend(zb_test_alloc()));
  zb_terrain_view *tv = zb_terrain_view_create(zb_test_alloc(), r, NULL, NULL, NULL);
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos.chunk = (zb_chunk){4, 9000, 300};
  f.pos.lx = f.pos.ly = 128.0f;
  f.pos.alt = 700.0f;
  zb_world_set_focus(w, 0, &f);
  zb_world_set_origin(w, f.pos.chunk);
  zb_vec3 eye = zb_origin_to_local(zb_world_origin(w), zb_world_planet(w), &f.pos);
  zb_vec3 target = zb_v3(eye.x + 1000.0f, eye.y - 60.0f, eye.z);
  double times[FRAMES];
  int measured = 0;
  for (int frame = 0; frame < 2000 && measured < FRAMES; ++frame) {
    zb_world_update(w);
    zb_render_begin(r, 640, 360);
    zb_render_set_camera(r, eye, target, zb_v3(0, 1, 0), 60, 1, 30000);
    double t0 = now_ms();
    zb_terrain_view_draw(tv, w, eye);
    double t = now_ms() - t0;
    zb_render_end(r);
    zb_terrain_view_stats st;
    zb_terrain_view_get_stats(tv, &st);
    if (st.built == 0 && st.foliage_built == 0) { /* steady state: everything meshed */
      times[measured++] = t;
    }
  }
  TEST_ASSERT_EQUAL_INT(FRAMES, measured);
  const zb_render_stats *rs = zb_render_get_stats(r);
  zb_terrain_view_stats st;
  zb_terrain_view_get_stats(tv, &st);
  qsort(times, FRAMES, sizeof times[0], cmp_double);
  double median = (times[FRAMES / 2 - 1] + times[FRAMES / 2]) * 0.5;
  printf(
      "terrain view%s: %zu chunks meshed, %zu items and %zu triangles drawn, %zu plants, %.2f ms per frame "
      "(budget %.1f ms)\n",
      flora ? " with plants" : "", st.meshes, rs->terrain_items, rs->terrain_triangles, st.plants, median, BUDGET_MS);
  TEST_ASSERT_TRUE(st.meshes > 5000);
  TEST_ASSERT_TRUE(rs->terrain_items <= MAX_ITEMS);
  TEST_ASSERT_TRUE(rs->terrain_triangles <= MAX_TRIANGLES);
  TEST_ASSERT_TRUE(median <= BUDGET_MS);
  zb_terrain_view_destroy(tv);
  zb_render_destroy(r);
  zb_world_destroy(w);
}

/* spec: retro-rendering: Frame budgets */
static void test_desktop_frame(void) { desktop_frame(NULL); }

static char *read_asset(const char *rel, size_t *len) {
  char path[1024];
  snprintf(path, sizeof path, "%s/assets/base/%s", getenv("ZB_SOURCE_DIR") ? getenv("ZB_SOURCE_DIR") : ".", rel);
  FILE *f = fopen(path, "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *data = malloc((size_t)n + 1);
  *len = fread(data, 1, (size_t)n, f);
  fclose(f);
  return data;
}

/* spec: vegetation: Vegetation budgets */
static void test_desktop_frame_with_plants(void) {
  size_t sl, al;
  char *species = read_asset("data/flora.lua", &sl), *atlas = read_asset("sprites/flora.lua", &al);
  static zb_flora flora;
  char err[160];
  TEST_ASSERT_EQUAL_INT_MESSAGE(ZB_OK, zb_flora_parse(&flora, zb_test_alloc(), species, sl, atlas, al, err, sizeof err),
                                err);
  free(species);
  free(atlas);
  desktop_frame(&flora);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_desktop_frame);
  RUN_TEST(test_desktop_frame_with_plants);
  return UNITY_END();
}

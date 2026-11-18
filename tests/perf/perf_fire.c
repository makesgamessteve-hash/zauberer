/* SPDX-License-Identifier: MIT */
/* Fire budget (release builds on Linux): a forest chunk burning across its
 * width, stepped at 20 Hz. */
#define _POSIX_C_SOURCE 199309L
#include "core/dmath.h"
#include "sim/fire.h"
#include "unity.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STEP_BUDGET_MS 2.0
#define SAMPLES 60

void setUp(void) {}
void tearDown(void) {}

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static int cmp(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : (x > y);
}

static char *read_asset(const char *rel, size_t *len) {
  char path[1024];
  snprintf(path, sizeof path, "%s/assets/base/%s", getenv("ZB_SOURCE_DIR") ? getenv("ZB_SOURCE_DIR") : ".", rel);
  FILE *f = fopen(path, "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *data = malloc((size_t)n + 1);
  TEST_ASSERT_EQUAL_size_t((size_t)n, fread(data, 1, (size_t)n, f));
  data[n] = '\0';
  fclose(f);
  *len = (size_t)n;
  return data;
}

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

/* spec: thermal-and-fire: Fire budget */
static void test_fire_budget(void) {
  size_t sl, al, ml;
  char *species = read_asset("data/flora.lua", &sl), *atlas = read_asset("sprites/flora.lua", &al);
  char *text = read_asset("data/materials.lua", &ml);
  char err[160];
  static zb_flora flora;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_flora_parse(&flora, zb_heap_allocator(), species, sl, atlas, al, err, sizeof err));
  zb_luon_doc doc;
  zb_luon_error le;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_luon_parse(&doc, zb_heap_allocator(), text, ml, &le));
  zb_registry r;
  zb_registry_init(&r, zb_heap_allocator(), "materials");
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_registry_merge(&r, "base", &doc.root));
  zb_luon_free(&doc);
  zb_materials *m = malloc(sizeof *m);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_materials_build(m, &r, err, sizeof err));

  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_heap_allocator();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  d.flora = &flora;
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL(w);
  const zb_planet *p = zb_world_planet(w);
  double lat = 46.0 * ZB_DPI / 180.0, lon = 0.3;
  zb_dvec3 dir = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos = zb_planet_from_cart(p, zb_dv_scale(dir, p->radius));
  f.pos.lx = f.pos.ly = 128.0f;
  zb_world_set_focus(w, 0, &f);
  for (int i = 0; i < 2000; ++i) {
    zb_world_update(w);
    const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(w), f.pos.chunk);
    if (ct && ct->stage >= ZB_STAGE_ECOLOGY) {
      break;
    }
  }
  zb_fire *fire = zb_fire_create(zb_heap_allocator(), w, m, 100.0);
  zb_fire_weather wind = {4.0, 3.0, 0.0};
  zb_fire_set_weather(fire, &wind);
  zb_fire_update(fire, &f.pos, 1, 0.0);
  /* A line of fire across the chunk, left to grow for a minute. */
  for (float y = 8.0f; y < 248.0f; y += 6.0f) {
    zb_planet_pos at = f.pos;
    at.lx = 20.0f;
    at.ly = y;
    zb_fire_ignite(fire, &at, 4.0);
  }
  for (int i = 0; i < 60 * 20; ++i) {
    zb_fire_update(fire, &f.pos, 1, ZB_FIRE_STEP);
  }
  zb_fire_stats st;
  zb_fire_get_stats(fire, &st);
  double t[SAMPLES];
  for (int i = 0; i < SAMPLES; ++i) {
    double t0 = now_ms();
    zb_fire_update(fire, &f.pos, 1, ZB_FIRE_STEP);
    t[i] = now_ms() - t0;
  }
  qsort(t, SAMPLES, sizeof(double), cmp);
  printf("fire: %d cells burning in %d awake chunk(s), %d burnt; step median %.2f ms (max %.2f, budget %.1f)\n",
         st.burning, st.awake, st.burnt, t[SAMPLES / 2], t[SAMPLES - 1], STEP_BUDGET_MS);
  TEST_ASSERT_TRUE_MESSAGE(st.burning > 500, "a large fire");
  TEST_ASSERT_TRUE(t[SAMPLES / 2] <= STEP_BUDGET_MS);
  zb_fire_destroy(fire);
  zb_world_destroy(w);
  zb_materials_free(m);
  free(m);
  free(species);
  free(atlas);
  free(text);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_fire_budget);
  return UNITY_END();
}

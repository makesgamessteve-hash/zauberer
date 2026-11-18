/* SPDX-License-Identifier: MIT */
/* Rigid-body budget (release builds on Linux): 200 boulders, crates and logs
 * tumbling down into a heap, stepped at 60 Hz. */
#define _POSIX_C_SOURCE 199309L
#include "core/dmath.h"
#include "sim/bodies.h"
#include "unity.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STEP_BUDGET_MS 1.5
#define BODIES 200
#define SAMPLES 120

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

/* spec: rigid-bodies: Bodies budget */
static void test_bodies_budget(void) {
  size_t ml;
  char *text = read_asset("data/materials.lua", &ml);
  char err[160];
  zb_luon_doc doc;
  zb_luon_error le;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_luon_parse(&doc, zb_heap_allocator(), text, ml, &le));
  free(text);
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
    if (ct && ct->stage >= ZB_STAGE_SURFACE) {
      break;
    }
  }
  zb_bodies *b = zb_bodies_create(zb_heap_allocator(), w, m, NULL);
  const char *mats[3] = {"rock", "wood", "pine_wood"};
  for (int i = 0; i < BODIES; ++i) {
    zb_body_desc bd;
    memset(&bd, 0, sizeof bd);
    bd.shape = (zb_shape)(i % 3);
    bd.material = zb_materials_find(m, mats[i % 3]);
    bd.radius = 0.4;
    bd.half_length = 0.8;
    bd.half[0] = bd.half[1] = bd.half[2] = 0.4;
    bd.pos = f.pos;
    bd.pos.lx = 118.0f + (float)(i % 10) * 2.0f;
    bd.pos.ly = 118.0f + (float)(i / 10 % 10) * 2.0f;
    bd.pos.alt = (float)zb_world_ground_height(w, &bd.pos) + 1.0f + 2.0f * (float)(i / 100);
    bd.spin[0] = 0.3 * (i % 5);
    TEST_ASSERT_TRUE(zb_bodies_add(b, &bd) > 0);
  }
  for (int i = 0; i < 60; ++i) { /* a second into the fall: piling up */
    zb_bodies_update(b, 1.0 / ZB_BODIES_HZ);
  }
  double t[SAMPLES];
  int contacts = 0, awake = 0;
  for (int i = 0; i < SAMPLES; ++i) {
    double t0 = now_ms();
    zb_bodies_update(b, 1.0 / ZB_BODIES_HZ);
    t[i] = now_ms() - t0;
    zb_bodies_stats st;
    zb_bodies_get_stats(b, &st);
    contacts = st.contacts > contacts ? st.contacts : contacts;
    awake = st.awake > awake ? st.awake : awake;
  }
  qsort(t, SAMPLES, sizeof(double), cmp);
  printf("bodies: %d bodies, up to %d awake and %d contacts; step median %.3f ms (max %.3f, budget %.1f)\n", BODIES,
         awake, contacts, t[SAMPLES / 2], t[SAMPLES - 1], STEP_BUDGET_MS);
  TEST_ASSERT_TRUE_MESSAGE(awake > BODIES / 2 && contacts > BODIES, "a busy heap");
  TEST_ASSERT_TRUE(t[SAMPLES / 2] <= STEP_BUDGET_MS);
  zb_bodies_destroy(b);
  zb_world_destroy(w);
  zb_materials_free(m);
  free(m);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_bodies_budget);
  return UNITY_END();
}

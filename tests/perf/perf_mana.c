/* SPDX-License-Identifier: MIT */
/* Mana budget (release builds on Linux): the field over a whole desktop
 * active tier (384 m), with its sources looked at every two seconds. */
#define _POSIX_C_SOURCE 199309L
#include "core/dmath.h"
#include "sim/tier.h"
#include "unity.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TICK_BUDGET_MS 0.5 /* averaged over 60 Hz ticks */
#define TICKS 240

void setUp(void) {}
void tearDown(void) {}

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
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

/* spec: mana: Mana budget */
static void test_mana_budget(void) {
  size_t sl, al, ml;
  char *species = read_asset("data/flora.lua", &sl), *atlas = read_asset("sprites/flora.lua", &al);
  char err[160];
  static zb_flora flora;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_flora_parse(&flora, zb_heap_allocator(), species, sl, atlas, al, err, sizeof err));
  free(species);
  free(atlas);
  char *text = read_asset("data/materials.lua", &ml);
  zb_luon_doc doc;
  zb_luon_error le;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_luon_parse(&doc, zb_heap_allocator(), text, ml, &le));
  free(text);
  zb_registry r;
  zb_registry_init(&r, zb_heap_allocator(), "materials");
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_registry_merge(&r, "base", &doc.root));
  zb_luon_free(&doc);
  zb_materials *mats = malloc(sizeof *mats);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_materials_build(mats, &r, err, sizeof err));
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_heap_allocator();
  d.seed = 1;
  d.profile = ZB_STREAM_DESKTOP;
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
  for (int i = 0; i < 20000; ++i) {
    zb_world_update(w);
    zb_stream_stats ss;
    zb_stream_get_stats(zb_world_stream(w), &ss);
    if (ss.queued == 0 && ss.jobs == 0 && i > 100) {
      break;
    }
  }
  /* The whole tier, so that the field reads fire, water and air. */
  zb_tier_config tc = {zb_heap_allocator(), w, mats, 384.0, 1, NULL, NULL};
  zb_tier *tier = zb_tier_create(&tc);
  TEST_ASSERT_NOT_NULL(tier);
  zb_weather wt = {0.4, 2.0, 0.0, 0.2, 3.0, 1.0, 18.0};
  zb_tier_weather(tier)->overridden = true;
  zb_tier_weather(tier)->now = wt;
  zb_tier_apply_weather(tier, &wt);
  for (int i = 0; i < 120; ++i) {
    zb_tier_update(tier, 1.0 / 60.0);
  }
  zb_mana *m = zb_tier_mana(tier);
  zb_mana_stats st;
  zb_mana_get_stats(m, &st);
  double t0 = now_ms();
  for (int i = 0; i < TICKS; ++i) {
    zb_mana_update(m, &f.pos, 1, 1.0 / 60.0);
  }
  double per_tick = (now_ms() - t0) / TICKS;
  printf("mana: %d chunks (%d cells); %.3f ms per tick averaged (budget %.1f)\n", st.chunks,
         st.chunks * ZB_MANA_RES * ZB_MANA_RES, per_tick, TICK_BUDGET_MS);
  TEST_ASSERT_TRUE_MESSAGE(st.chunks >= 9, "a full active tier");
  TEST_ASSERT_TRUE(per_tick <= TICK_BUDGET_MS);
  zb_tier_destroy(tier);
  zb_world_destroy(w);
  zb_materials_free(mats);
  free(mats);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mana_budget);
  return UNITY_END();
}

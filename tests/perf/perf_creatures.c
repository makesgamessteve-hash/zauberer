/* SPDX-License-Identifier: MIT */
/* Creature budget (release builds on Linux): 200 creatures of three kinds,
 * packs among them, around a mage on foot over a whole desktop active tier. */
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
#define TICKS 300
#define CREATURES 200

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

static zb_animal kind(const char *id, double run, zb_diet diet, bool pack, bool flies) {
  zb_animal a;
  memset(&a, 0, sizeof a);
  snprintf(a.id, sizeof a.id, "%s", id);
  a.length = 1.5;
  a.mass = 50.0;
  a.walk = 1.5;
  a.run = run;
  a.diet = diet;
  a.pack = pack;
  a.flies = flies;
  a.cruise = 20.0;
  a.sight = 100.0;
  a.hearing = 150.0;
  a.smell = 200.0;
  a.health = 60.0;
  a.aggression = diet == ZB_DIET_CARNIVORE ? 0.8 : 0.0;
  a.fear_of_fire = 1.0;
  a.fear_of_player = 0.6;
  a.mana_color = -1;
  return a;
}

/* spec: creatures: Creature budget */
static void test_creature_budget(void) {
  size_t ml;
  char err[160];
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
  zb_tier_config tc = {zb_heap_allocator(), w, mats, 384.0, 1, NULL, NULL};
  zb_tier *tier = zb_tier_create(&tc);
  TEST_ASSERT_NOT_NULL(tier);
  zb_weather wt = {0.2, 3.0, 0.0, 0.0, 3.0, 1.0, 15.0};
  zb_tier_weather(tier)->overridden = true;
  zb_tier_weather(tier)->now = wt;
  zb_tier_apply_weather(tier, &wt);
  for (int i = 0; i < 60; ++i) {
    zb_tier_update(tier, 1.0 / 60.0);
  }
  zb_creatures *c = zb_tier_creatures(tier);
  zb_animal kinds[3] = {kind("wolf", 10.0, ZB_DIET_CARNIVORE, true, false),
                        kind("deer", 12.0, ZB_DIET_HERBIVORE, true, false),
                        kind("bird", 18.0, ZB_DIET_CARNIVORE, false, true)};
  for (int k = 0; k < 3; ++k) {
    TEST_ASSERT_EQUAL_INT(k, zb_creatures_define(c, &kinds[k]));
  }
  for (int i = 0; i < CREATURES; ++i) {
    double heading = (double)i * 2.399963, range = 20.0 + (double)(i % 17) * 9.0;
    double turned;
    zb_planet_pos at = zb_planet_move(p, &f.pos, heading, range, 0.0, 0.0, &turned);
    TEST_ASSERT_TRUE(zb_creatures_spawn(c, i % 3, &at) > 0);
  }
  zb_creatures_set_player(c, &f.pos, true);
  zb_creatures_stimulus(c, ZB_STIMULUS_NOISE, &f.pos, 0.5, 60.0);
  for (int i = 0; i < 60; ++i) {
    zb_creatures_update(c, 1.0 / 60.0);
  }
  double t0 = now_ms();
  for (int i = 0; i < TICKS; ++i) {
    zb_creatures_update(c, 1.0 / 60.0);
  }
  double per_tick = (now_ms() - t0) / TICKS;
  zb_creatures_stats st;
  zb_creatures_get_stats(c, &st);
  printf("creatures: %d alive, %d frozen; %.3f ms per tick averaged (budget %.1f)\n", st.alive, st.frozen, per_tick,
         TICK_BUDGET_MS);
  TEST_ASSERT_TRUE_MESSAGE(st.alive >= CREATURES * 9 / 10, "most in the active tier");
  TEST_ASSERT_TRUE(per_tick <= TICK_BUDGET_MS);
  zb_tier_destroy(tier);
  zb_world_destroy(w);
  zb_materials_free(mats);
  free(mats);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_creature_budget);
  return UNITY_END();
}

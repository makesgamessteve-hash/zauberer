/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "sim/tier.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/delta.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static zb_flora g_flora;
static zb_materials *g_materials;

static char *read_asset(const char *rel, size_t *len) {
  char path[512];
  snprintf(path, sizeof path, "%s/assets/base/%s", zb_test_source_dir(), rel);
  char *text = zb_test_read_file(path, len);
  TEST_ASSERT_NOT_NULL_MESSAGE(text, path);
  return text;
}

/* The base game's flora and materials, once for every test. */
static void load_data(void) {
  if (g_materials) {
    return;
  }
  size_t sl, al, ml;
  char *species = read_asset("data/flora.lua", &sl), *atlas = read_asset("sprites/flora.lua", &al);
  char err[160];
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      ZB_OK, zb_flora_parse(&g_flora, zb_heap_allocator(), species, sl, atlas, al, err, sizeof err), err);
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
  g_materials = malloc(sizeof *g_materials);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_materials_build(g_materials, &r, err, sizeof err));
}

static uint64_t g_ns;
static uint64_t fake_clock(void *ctx) {
  (void)ctx;
  return g_ns += 100000; /* 0.1 ms between readings */
}

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

typedef struct scene {
  zb_world *world;
  zb_fire *fire; /* the tier's */
  zb_tier *tier;
  zb_planet_pos centre;
} scene;

/* Ground shapes for a test: flat meadow, a slope rising east, or a meadow
 * with a channel of water running north-south east of the centre. */
typedef enum ground { FLAT, SLOPE, CHANNEL, FOREST } ground;

/* Reshapes a loaded chunk: soil everywhere, no plants, heights by shape. */
static void reshape(zb_chunk_terrain *ct, ground g) {
  for (int y = 0; y < ZB_HEIGHT_RES; ++y) {
    for (int x = 0; x < ZB_HEIGHT_RES; ++x) {
      float h = 50.0f;
      if (g == SLOPE) {
        h = 50.0f + 0.4f * (float)x;
      } else if (g == CHANNEL && x >= 150 && x < 170) {
        h = -3.0f; /* below sea level: water */
      }
      ct->height[y * ZB_HEIGHT_RES + x] = h;
    }
  }
  memset(ct->material, ZB_MAT_SOIL, ZB_SURFACE_RES * ZB_SURFACE_RES);
  ct->plant_count = 0;
}

/* A world with the chunk at the focus loaded with its plants (reshaped
 * unless FOREST) and the fire simulation over it. */
static void scene_open(scene *s, ground g, double wind_east, double rain) {
  load_data();
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  d.flora = &g_flora;
  char err[128];
  s->world = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(s->world, err);
  /* A temperate forest of the noise world. */
  const zb_planet *p = zb_world_planet(s->world);
  double lat = 46.0 * ZB_DPI / 180.0, lon = 0.3;
  zb_dvec3 dir = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos = zb_planet_from_cart(p, zb_dv_scale(dir, p->radius));
  f.pos.lx = f.pos.ly = 128.0f;
  s->centre = f.pos;
  zb_world_set_focus(s->world, 0, &f);
  for (int i = 0; i < 2000; ++i) {
    zb_world_update(s->world);
    const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(s->world), f.pos.chunk);
    if (ct && ct->stage >= ZB_STAGE_ECOLOGY) {
      break;
    }
  }
  zb_chunk_terrain *ct = zb_stream_edit(zb_world_stream(s->world), f.pos.chunk);
  TEST_ASSERT_NOT_NULL(ct);
  TEST_ASSERT_TRUE(ct->stage >= ZB_STAGE_ECOLOGY);
  if (g != FOREST) {
    reshape(ct, g);
  }
  zb_tier_config tc = {zb_test_alloc(), s->world, g_materials, 100.0, 7, fake_clock, NULL};
  s->tier = zb_tier_create(&tc);
  TEST_ASSERT_NOT_NULL(s->tier);
  s->fire = zb_tier_fire(s->tier);
  zb_weather_control *wc = zb_tier_weather(s->tier);
  wc->overridden = true;
  wc->now = (zb_weather){0.0, rain, 0.0, 0.0, wind_east, 0.0, 15.0};
  zb_tier_apply_weather(s->tier, &wc->now);
  zb_tier_update(s->tier, 0.0);
}

/* A wind of speed m/s blowing along the chunk's x axis (towards larger lx). */
static void scene_close(scene *s) {
  zb_tier_destroy(s->tier);
  zb_world_destroy(s->world);
}

static void run(scene *s, double seconds) {
  for (double t = 0.0; t < seconds; t += 1.0 / 60.0) {
    zb_tier_update(s->tier, 1.0 / 60.0);
  }
}

/* spec: simulation-tiers: The tier couples its simulations */
static void test_coupling(void) {
  scene s;
  scene_open(&s, FLAT, 0.0, 0.0);
  /* Fire smokes into the air. */
  TEST_ASSERT_TRUE(zb_tier_lightning(s.tier, &s.centre) > 0);
  TEST_ASSERT_EQUAL_INT(1, zb_tier_weather(s.tier)->strikes);
  run(&s, 5.0);
  zb_air_stats as;
  zb_air_get_stats(zb_tier_air(s.tier), &as);
  TEST_ASSERT_TRUE(as.emitted[ZB_GAS_SMOKE] > 0.0 && as.mass[ZB_GAS_SMOKE] > 0.0);
  /* Water poured on the fire puts it out. */
  zb_fire_stats fs;
  zb_fire_get_stats(s.fire, &fs);
  TEST_ASSERT_TRUE(fs.burning > 0);
  zb_surface_water_add(zb_tier_water(s.tier), &s.centre, 40.0, 4000.0);
  run(&s, 2.0);
  zb_fire_cell c;
  zb_fire_cell_at(s.fire, &s.centre, &c);
  TEST_ASSERT_TRUE(!c.burning && c.moisture >= 1.0);
  /* Every simulation is timed against the budget. */
  zb_tier_stats st;
  zb_tier_get_stats(s.tier, &st);
  TEST_ASSERT_EQUAL_DOUBLE(ZB_TIER_BUDGET_MS, st.budget_ms);
  TEST_ASSERT_TRUE(st.ms[ZB_TIER_FIRE] > 0.0 && st.total_ms > 0.0);
  scene_close(&s);
}

/* spec: simulation-tiers: The world's weather drives the tier */
static void test_weather(void) {
  scene s;
  scene_open(&s, FLAT, 0.0, 0.0);
  zb_weather_control *wc = zb_tier_weather(s.tier);
  wc->overridden = false; /* the world's own, from the next second */
  run(&s, 1.1);
  zb_weather nat;
  zb_planet_pos here = s.centre;
  here.alt = (float)zb_world_ground_height(s.world, &s.centre);
  zb_weather_at(zb_world_terrain(s.world), zb_world_calendar(s.world), &here, zb_world_time(s.world), &nat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, nat.cloud, wc->now.cloud);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, nat.wind_east, zb_fire_weather_get(s.fire)->wind_east);
  /* A storm the scripts set sends lightning within a few minutes. */
  wc->overridden = true;
  wc->now.storm = 1.0;
  run(&s, 240.0);
  TEST_ASSERT_TRUE(wc->strikes > 0);
  /* Another world: everything starts over. */
  zb_tier_set_world(s.tier, s.world);
  zb_air_stats as;
  zb_air_get_stats(zb_tier_air(s.tier), &as);
  TEST_ASSERT_EQUAL_INT(0, as.chunks);
  scene_close(&s);
}

/* spec: simulation-tiers: The tier couples its simulations */
static void test_without_fire(void) {
  load_data();
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  char err[128];
  zb_world *w = zb_world_create(&d, err, sizeof err);
  zb_tier_config tc = {zb_test_alloc(), w, NULL, 100.0, 7, NULL, NULL};
  zb_tier *t = zb_tier_create(&tc);
  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_NULL(zb_tier_fire(t));
  zb_planet_pos p = {{0, 0, 0}, 128.0f, 128.0f, 0.0f};
  TEST_ASSERT_EQUAL_INT(0, zb_tier_lightning(t, &p));
  zb_tier_update(t, 1.0 / 60.0);
  zb_tier_destroy(t);
  zb_tier_destroy(NULL);
  zb_world_destroy(w);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_coupling);
  RUN_TEST(test_weather);
  RUN_TEST(test_without_fire);
  int failures = UNITY_END();
  if (g_materials) {
    zb_materials_free(g_materials);
    free(g_materials);
  }
  return failures;
}

/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "sim/fire.h"
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

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

typedef struct scene {
  zb_world *world;
  zb_fire *fire;
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
  s->fire = zb_fire_create(zb_test_alloc(), s->world, g_materials, 100.0);
  TEST_ASSERT_NOT_NULL(s->fire);
  zb_fire_weather w = {wind_east, 0.0, rain};
  zb_fire_set_weather(s->fire, &w);
  zb_fire_update(s->fire, &s->centre, 1, 0.0);
}

/* A wind of speed m/s blowing along the chunk's x axis (towards larger lx). */
static void wind_along_x(scene *s, double speed) {
  const zb_planet *p = zb_world_planet(s->world);
  zb_planet_pos a = s->centre, b = s->centre;
  b.lx += 1.0f;
  zb_dvec3 ca = zb_planet_to_cart(p, &a), ax = zb_dv_norm(zb_dv_sub(zb_planet_to_cart(p, &b), ca));
  zb_dvec3 east, north, up;
  zb_planet_frame(zb_dv_norm(ca), &east, &north, &up);
  zb_fire_weather w = {speed * zb_dv_dot(ax, east), speed * zb_dv_dot(ax, north), 0.0};
  zb_fire_set_weather(s->fire, &w);
}

static void scene_close(scene *s) {
  zb_fire_destroy(s->fire);
  zb_world_destroy(s->world);
}

static void run(scene *s, double seconds) {
  for (double t = 0.0; t < seconds; t += 0.25) {
    zb_fire_update(s->fire, &s->centre, 1, 0.25);
  }
}

static zb_planet_pos offset(const scene *s, float dx, float dy) {
  zb_planet_pos p = s->centre;
  p.lx += dx;
  p.ly += dy;
  return p;
}

/* How far east and west of the centre the fire got (metres, along the row). */
static void extent(const scene *s, float *east, float *west) {
  *east = *west = 0.0f;
  for (float d = 1.0f; d < 120.0f; d += 1.0f) {
    zb_fire_cell c;
    zb_planet_pos e = offset(s, d, 0.0f), w = offset(s, -d, 0.0f);
    zb_fire_cell_at(s->fire, &e, &c);
    if (c.burning || c.burnt) {
      *east = d;
    }
    zb_fire_cell_at(s->fire, &w, &c);
    if (c.burning || c.burnt) {
      *west = d;
    }
  }
}

/* spec: thermal-and-fire: Fire spreads and burns out */
static void test_spread(void) {
  scene s;
  scene_open(&s, FLAT, 0.0, 0.0);
  zb_fire_stats st;
  zb_fire_get_stats(s.fire, &st);
  TEST_ASSERT_TRUE(st.chunks >= 1);
  TEST_ASSERT_EQUAL_INT(0, st.awake); /* nothing hot: nothing to do */
  zb_fire_cell c;
  zb_fire_cell_at(s.fire, &s.centre, &c);
  TEST_ASSERT_TRUE(c.loaded && !c.burning && c.fuel > 0.2 && c.temperature > -40.0 && c.temperature < 50.0);
  TEST_ASSERT_EQUAL_INT(zb_materials_find(g_materials, "grass"), c.material);
  TEST_ASSERT_TRUE(zb_fire_ignite(s.fire, &s.centre, 3.0) >= 5);
  run(&s, 30.0);
  zb_fire_get_stats(s.fire, &st);
  TEST_ASSERT_TRUE(st.burning > 10);
  TEST_ASSERT_TRUE(st.max_temperature > 500.0);
  TEST_ASSERT_TRUE(st.awake >= 1);
  float east, west;
  extent(&s, &east, &west);
  TEST_ASSERT_TRUE(east > 4.0f && west > 4.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.5f * (east + west), east, west); /* no wind, flat: round */
  /* It burns out, leaving burnt ground that the world remembers. */
  run(&s, 600.0);
  zb_fire_get_stats(s.fire, &st);
  TEST_ASSERT_TRUE(st.burnt > 100);
  zb_fire_cell_at(s.fire, &s.centre, &c);
  TEST_ASSERT_TRUE(c.burnt && c.fuel < 0.1);
  size_t n;
  zb_deltas_get(zb_world_deltas(s.world), s.centre.chunk, &n);
  TEST_ASSERT_TRUE(n > 100);
  const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(s.world), s.centre.chunk);
  TEST_ASSERT_EQUAL_UINT8(ZB_MAT_ASH, ct->material[128 * ZB_SURFACE_RES + 128]);
  scene_close(&s);
}

/* spec: thermal-and-fire: Wind and slope lean the fire */
static void test_wind_and_slope(void) {
  scene s;
  float east, west;
  scene_open(&s, FLAT, 0.0, 0.0);
  wind_along_x(&s, 6.0);
  zb_fire_ignite(s.fire, &s.centre, 3.0);
  run(&s, 40.0);
  extent(&s, &east, &west);
  TEST_ASSERT_TRUE_MESSAGE(east > 2.5f * west, "downwind");
  scene_close(&s);
  scene_open(&s, SLOPE, 0.0, 0.0);
  zb_fire_ignite(s.fire, &s.centre, 3.0);
  run(&s, 40.0);
  extent(&s, &east, &west);
  TEST_ASSERT_TRUE_MESSAGE(east > 1.5f * west, "uphill");
  scene_close(&s);
}

/* spec: thermal-and-fire: Water and rain stop fire */
static void test_water_and_rain(void) {
  scene s;
  scene_open(&s, CHANNEL, 0.0, 0.0);
  zb_planet_pos river = offset(&s, 32.0f, 0.0f);
  zb_fire_cell c;
  zb_fire_cell_at(s.fire, &river, &c);
  TEST_ASSERT_TRUE(c.water && c.fuel == 0.0);
  zb_fire_ignite(s.fire, &s.centre, 3.0);
  run(&s, 400.0);
  zb_planet_pos beyond = offset(&s, 50.0f, 0.0f), before = offset(&s, 18.0f, 0.0f);
  zb_fire_cell_at(s.fire, &beyond, &c);
  TEST_ASSERT_FALSE(c.burning || c.burnt);
  zb_fire_cell_at(s.fire, &before, &c);
  TEST_ASSERT_TRUE(c.burnt);
  scene_close(&s);
  /* Heavy rain: the fire dies and wet fuel does not catch. */
  scene_open(&s, FLAT, 0.0, 0.0);
  zb_fire_ignite(s.fire, &s.centre, 3.0);
  run(&s, 20.0);
  zb_fire_weather wet = {0.0, 0.0, 40.0};
  zb_fire_set_weather(s.fire, &wet);
  TEST_ASSERT_EQUAL_FLOAT(40.0f, (float)zb_fire_weather_get(s.fire)->rain);
  run(&s, 120.0);
  zb_fire_stats st;
  zb_fire_get_stats(s.fire, &st);
  TEST_ASSERT_EQUAL_INT(0, st.burning);
  zb_planet_pos far = offset(&s, 60.0f, 60.0f);
  zb_fire_cell_at(s.fire, &far, &c);
  TEST_ASSERT_TRUE(!c.burnt && c.moisture > 0.3);
  TEST_ASSERT_EQUAL_INT(0, zb_fire_ignite(s.fire, &far, 3.0));
  scene_close(&s);
}

/* spec: thermal-and-fire: Embers start spot fires */
static void test_embers(void) {
  scene s;
  scene_open(&s, FOREST, 12.0, 0.0);
  zb_fire_ignite(s.fire, &s.centre, 6.0);
  run(&s, 240.0);
  zb_fire_stats st;
  zb_fire_get_stats(s.fire, &st);
  TEST_ASSERT_TRUE(st.embers > 0);
  TEST_ASSERT_TRUE(st.spots > 0);
  scene_close(&s);
}

/* spec: thermal-and-fire: Heat without fire */
static void test_heat(void) {
  scene s;
  scene_open(&s, FLAT, 0.0, 0.0);
  zb_planet_pos p = offset(&s, -40.0f, 0.0f);
  zb_fire_cell before, c;
  zb_fire_cell_at(s.fire, &p, &before);
  zb_fire_add_heat(s.fire, &p, 2.0, 2e5); /* a warm stone, not a fire */
  zb_fire_cell_at(s.fire, &p, &c);
  TEST_ASSERT_TRUE(c.temperature > before.temperature + 5.0 && !c.burning);
  run(&s, 600.0);
  zb_fire_cell_at(s.fire, &p, &c);
  TEST_ASSERT_FLOAT_WITHIN(0.6, before.temperature, c.temperature); /* back to the air's temperature */
  zb_fire_stats st;
  zb_fire_get_stats(s.fire, &st);
  TEST_ASSERT_EQUAL_INT(0, st.awake);
  zb_fire_add_heat(s.fire, &p, 2.0, 5e7); /* a fire spell */
  run(&s, 1.0);
  zb_fire_cell_at(s.fire, &p, &c);
  TEST_ASSERT_TRUE(c.burning);
  /* Outside the simulation nothing happens. */
  zb_planet_pos away = s.centre;
  away.chunk.x += 40;
  zb_fire_cell_at(s.fire, &away, &c);
  TEST_ASSERT_FALSE(c.loaded);
  TEST_ASSERT_EQUAL_INT(0, zb_fire_ignite(s.fire, &away, 5.0));
  zb_fire_add_heat(s.fire, &away, 5.0, 1e9);
  scene_close(&s);
}

static void count_cell(void *ctx, const zb_planet_pos *pos, double intensity, double fuel) {
  (void)pos;
  TEST_ASSERT_TRUE(intensity > 0.0 && fuel >= 0.0);
  (*(int *)ctx)++;
}

/* spec: thermal-and-fire: Deterministic fire */
static void test_deterministic(void) {
  uint64_t h[2];
  for (int k = 0; k < 2; ++k) {
    scene s;
    scene_open(&s, FOREST, 5.0, 0.0);
    zb_fire_ignite(s.fire, &s.centre, 4.0);
    run(&s, 60.0);
    h[k] = zb_fire_hash(s.fire);
    int seen = 0;
    zb_fire_each_burning(s.fire, count_cell, &seen);
    TEST_ASSERT_TRUE(seen > 0);
    scene_close(&s);
  }
  TEST_ASSERT_EQUAL_UINT64(h[0], h[1]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_spread);
  RUN_TEST(test_wind_and_slope);
  RUN_TEST(test_water_and_rain);
  RUN_TEST(test_embers);
  RUN_TEST(test_heat);
  RUN_TEST(test_deterministic);
  int failures = UNITY_END();
  if (g_materials) {
    zb_materials_free(g_materials);
    free(g_materials);
  }
  return failures;
}

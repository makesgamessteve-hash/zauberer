/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/delta.h"
#include "world/dvec.h"
#include "world/stream.h"
#include "world/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

static zb_flora g_flora;
static bool g_flora_loaded;

static char *read_asset(const char *rel, size_t *len) {
  char path[512];
  snprintf(path, sizeof path, "%s/assets/base/%s", zb_test_source_dir(), rel);
  char *text = zb_test_read_file(path, len);
  TEST_ASSERT_NOT_NULL_MESSAGE(text, path);
  return text;
}

/* The base game's plants. */
static const zb_flora *flora(void) {
  if (!g_flora_loaded) {
    size_t sl, al;
    char *species = read_asset("data/flora.lua", &sl), *atlas = read_asset("sprites/flora.lua", &al);
    char err[160];
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        ZB_OK, zb_flora_parse(&g_flora, zb_heap_allocator(), species, sl, atlas, al, err, sizeof err), err);
    free(species);
    free(atlas);
    g_flora_loaded = true;
  }
  return &g_flora;
}

/* A world with the chunks around a land place loaded to their heights (and
 * plants, given a flora). */
static zb_world *open_world_with(zb_planet_pos *centre, const zb_flora *fl) {
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.flora = fl;
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  char err[128];
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(w, err);
  const zb_planet *p = zb_world_planet(w);
  double lat = 46.0 * ZB_DPI / 180.0, lon = 0.3;
  zb_dvec3 dir = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos = zb_planet_from_cart(p, zb_dv_scale(dir, p->radius));
  f.pos.lx = f.pos.ly = 128.0f;
  *centre = f.pos;
  zb_world_set_focus(w, 0, &f);
  for (int i = 0; i < 2000; ++i) {
    zb_world_update(w);
  }
  return w;
}

static zb_world *open_world(zb_planet_pos *centre) { return open_world_with(centre, NULL); }

/* spec: terrain-deformation: Craters and mounds */
static void test_crater(void) {
  zb_planet_pos c;
  zb_world *w = open_world(&c);
  const zb_stream *s = zb_world_stream(w);
  zb_chunk east;
  TEST_ASSERT_TRUE(zb_planet_neighbor(zb_world_planet(w), c.chunk, 1, 0, &east));
  const zb_chunk_terrain *a = zb_stream_find(s, c.chunk), *b = zb_stream_find(s, east);
  TEST_ASSERT_TRUE(a && b && a->stage >= ZB_STAGE_TERRAIN && b->stage >= ZB_STAGE_TERRAIN);
  zb_planet_pos edge = c;
  edge.lx = 250.0f;
  float before = a->height[128 * ZB_HEIGHT_RES + 250], far = a->height[128 * ZB_HEIGHT_RES + 200];
  float seam = b->height[128 * ZB_HEIGHT_RES + 0];
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, a->height[128 * ZB_HEIGHT_RES + 256], seam);
  zb_chunk changed[9];
  int n = zb_world_deform(w, &edge, 20.0, -5.0, changed, 9);
  TEST_ASSERT_TRUE_MESSAGE(n >= 2, "the crater crosses into the next chunk");
  a = zb_stream_find(s, c.chunk);
  b = zb_stream_find(s, east);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, before - 5.0f, a->height[128 * ZB_HEIGHT_RES + 250]); /* the bottom */
  TEST_ASSERT_EQUAL_FLOAT(far, a->height[128 * ZB_HEIGHT_RES + 200]);                   /* outside: untouched */
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, a->height[128 * ZB_HEIGHT_RES + 256], b->height[128 * ZB_HEIGHT_RES + 0]);
  TEST_ASSERT_TRUE(b->height[128 * ZB_HEIGHT_RES + 0] < seam - 1.0f);
  TEST_ASSERT_TRUE(a->min_height <= a->height[128 * ZB_HEIGHT_RES + 250]);
  TEST_ASSERT_FLOAT_WITHIN(0.05, before - 5.0, zb_world_ground_height(w, &edge));
  size_t edits;
  zb_deltas_get(zb_world_deltas(w), c.chunk, &edits);
  TEST_ASSERT_TRUE(edits > 500);
  /* A mound elsewhere, and nothing where the ground is not loaded. */
  zb_planet_pos hill = c;
  hill.lx = 60.0f;
  float base = a->height[128 * ZB_HEIGHT_RES + 60];
  TEST_ASSERT_EQUAL_INT(1, zb_world_deform(w, &hill, 10.0, 3.0, changed, 9));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, base + 3.0f, zb_stream_find(s, c.chunk)->height[128 * ZB_HEIGHT_RES + 60]);
  zb_planet_pos away = c;
  away.chunk.x += 40;
  TEST_ASSERT_EQUAL_INT(0, zb_world_deform(w, &away, 10.0, 3.0, changed, 9));
  zb_world_destroy(w);
}

typedef struct plant_count {
  int present, taller, ash;
} plant_count;

static plant_count look(const zb_chunk_terrain *ct) {
  plant_count n = {0, 0, 0};
  for (uint32_t k = 0; k < ct->plant_count; ++k) {
    n.present += ct->plants[k].layer < ZB_PLANT_LAYERS ? 1 : 0;
    n.taller += ct->plants[k].layer < ZB_PLANT_GROUND ? 1 : 0;
  }
  for (int i = 0; i < ZB_SURFACE_RES * ZB_SURFACE_RES; ++i) {
    n.ash += ct->material[i] == ZB_MAT_ASH ? 1 : 0;
  }
  return n;
}

/* A loaded chunk at its plants (stage 3), waiting for it after a regeneration. */
static const zb_chunk_terrain *at_plants(zb_world *w, zb_chunk c) {
  for (int i = 0; i < 4000; ++i) {
    const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(w), c);
    if (ct && ct->stage >= ZB_STAGE_ECOLOGY) {
      return ct;
    }
    zb_world_update(w);
  }
  TEST_FAIL_MESSAGE("the chunk never got its plants");
  return NULL;
}

/* spec: world-persistence: Burnt ground heals */
static void test_heal(void) {
  zb_planet_pos c;
  zb_world *w = open_world_with(&c, flora());
  const zb_chunk_terrain *ct = at_plants(w, c.chunk);
  plant_count before = look(ct);
  TEST_ASSERT_TRUE(before.present > 100 && before.taller > 0);
  /* The whole chunk burns while it stays loaded. */
  static uint16_t cells[ZB_BURN_RES * ZB_BURN_RES];
  for (int i = 0; i < ZB_BURN_RES * ZB_BURN_RES; ++i) {
    cells[i] = (uint16_t)i;
  }
  zb_world_update(w);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_world_burn(w, c.chunk, cells, ZB_BURN_RES * ZB_BURN_RES, zb_world_time(w)));
  plant_count burnt = look(at_plants(w, c.chunk));
  TEST_ASSERT_EQUAL_INT(0, burnt.present);
  TEST_ASSERT_TRUE(burnt.ash > 0);
  /* A month on, still loaded: the ash is gone and the ground cover back. */
  zb_world_set_time(w, zb_world_time(w) + 30.0 * 86400.0);
  uint32_t version = zb_stream_version(zb_world_stream(w), c.chunk);
  zb_world_update(w);
  TEST_ASSERT_TRUE_MESSAGE(zb_stream_version(zb_world_stream(w), c.chunk) != version, "generated again");
  plant_count month = look(at_plants(w, c.chunk));
  TEST_ASSERT_EQUAL_INT(0, month.ash);
  TEST_ASSERT_TRUE(month.present > 0);
  TEST_ASSERT_EQUAL_INT(0, month.taller);
  /* Five years on, everything is back. */
  zb_world_set_time(w, zb_world_time(w) + 5.0 * 365.0 * 86400.0);
  zb_world_update(w);
  plant_count years = look(at_plants(w, c.chunk));
  TEST_ASSERT_EQUAL_INT(before.present, years.present);
  /* Nothing more is due: no more regeneration. */
  version = zb_stream_version(zb_world_stream(w), c.chunk);
  zb_world_set_time(w, zb_world_time(w) + 7200.0);
  zb_world_update(w);
  TEST_ASSERT_EQUAL_UINT32(version, zb_stream_version(zb_world_stream(w), c.chunk));
  TEST_ASSERT_FALSE(zb_stream_regenerate(zb_world_stream(w), (zb_chunk){c.chunk.face, c.chunk.x + 40, c.chunk.y}, 2));
  zb_world_destroy(w);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_crater);
  RUN_TEST(test_heal);
  return UNITY_END();
}

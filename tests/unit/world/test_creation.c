/* SPDX-License-Identifier: MIT */
#include "core/hash.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/creation.h"
#include "world/dvec.h"
#include "world/world.h"

#include <stdio.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

/* One small world shared by the tests (made with an untracked allocator). */
static zb_world_creation *g_creation;
static zb_world_bundle g_bundle;
static zb_array g_file;

static void small_options(zb_world_options *o, uint64_t seed) {
  zb_world_default_options(o);
  o->seed = seed;
  o->radius = 400000.0;
  o->quality = ZB_WORLD_QUICK;
}

/* spec: world-creation: Creation in stages */
static void test_stages(void) {
  zb_world_options o;
  small_options(&o, 7);
  char err[128];
  zb_world_creation *c = zb_world_creation_create(zb_test_alloc(), &o, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(c, err);
  zb_creation_progress p, last;
  zb_world_creation_get_progress(c, &last);
  TEST_ASSERT_EQUAL_INT(ZB_CREATION_DEEP_TIME, last.stage);
  TEST_ASSERT_EQUAL_STRING("Drifting continents", last.label);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, last.fraction);
  bool saw[ZB_CREATION_DONE + 1] = {false};
  int pieces = 0;
  while (!zb_world_creation_advance(c)) {
    zb_world_creation_get_progress(c, &p);
    TEST_ASSERT_TRUE(p.stage >= last.stage);
    if (p.attempt == last.attempt) {
      TEST_ASSERT_TRUE(p.fraction >= last.fraction);
    }
    if (p.stage == ZB_CREATION_DEEP_TIME && last.stage == ZB_CREATION_DEEP_TIME && p.attempt == last.attempt) {
      TEST_ASSERT_TRUE(p.myr_ago <= last.myr_ago);
    }
    saw[p.stage] = true;
    last = p;
    pieces++;
  }
  zb_world_creation_get_progress(c, &p);
  TEST_ASSERT_EQUAL_INT(ZB_CREATION_DONE, p.stage);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, p.fraction);
  TEST_ASSERT_TRUE(saw[ZB_CREATION_DEEP_TIME] && saw[ZB_CREATION_RELIEF] && saw[ZB_CREATION_CLIMATE]);
  TEST_ASSERT_TRUE(pieces > 80);
  TEST_ASSERT_TRUE(zb_world_creation_advance(c)); /* stays done */
  TEST_ASSERT_NOT_NULL(zb_world_creation_climate(c));
  TEST_ASSERT_EQUAL_UINT64(7, zb_world_creation_options(c)->seed);
  /* The time-lapse: one frame per step, the land drifting between them. */
  TEST_ASSERT_EQUAL_INT(80, p.frames);
  TEST_ASSERT_NOT_NULL(zb_world_creation_frame_elevation(c, 0));
  TEST_ASSERT_NOT_NULL(zb_world_creation_frame_plates(c, 79));
  TEST_ASSERT_NULL(zb_world_creation_frame_elevation(c, 80));
  TEST_ASSERT_NULL(zb_world_creation_frame_plates(c, -1));
  const int16_t *first = zb_world_creation_frame_elevation(c, 0), *final = zb_world_creation_frame_elevation(c, 79);
  int changed = 0, land_first = 0;
  for (int i = 0; i < 12000; ++i) {
    changed += (first[i] > 0) != (final[i] > 0);
    land_first += first[i] > 0;
  }
  TEST_ASSERT_TRUE(changed > 500);
  TEST_ASSERT_TRUE(land_first > 2000);
  zb_world_creation_destroy(c);
}

/* spec: world-creation: Cancelling */
static void test_cancel(void) {
  zb_world_options o;
  small_options(&o, 9);
  char err[128];
  zb_world_creation *c = zb_world_creation_create(zb_test_alloc(), &o, err, sizeof err);
  for (int k = 0; k < 20; ++k) {
    zb_world_creation_advance(c);
  }
  zb_world_bundle b;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, zb_world_creation_bundle(c, zb_test_alloc(), &b)); /* not done */
  zb_world_creation_destroy(c); /* the leak check proves everything was freed */
  zb_world_creation_destroy(NULL);
}

/* spec: world-creation: Options and quality */
static void test_options(void) {
  zb_world_options o;
  zb_world_default_options(&o);
  TEST_ASSERT_EQUAL_DOUBLE(ZB_PLANET_DEFAULT_RADIUS, o.radius);
  TEST_ASSERT_EQUAL_INT(ZB_WORLD_NORMAL, o.quality);
  TEST_ASSERT_EQUAL_DOUBLE(23.0, o.axial_tilt);
  zb_geology_options g;
  zb_world_geology_options(&o, &g);
  TEST_ASSERT_EQUAL_INT(40000, g.points);
  TEST_ASSERT_EQUAL_INT(524, g.macro_res); /* about 6 km cells */
  o.quality = ZB_WORLD_QUICK;
  zb_world_geology_options(&o, &g);
  TEST_ASSERT_EQUAL_INT(12000, g.points);
  TEST_ASSERT_EQUAL_INT(262, g.macro_res); /* about 12 km */
  o.quality = ZB_WORLD_HIGH;
  zb_world_geology_options(&o, &g);
  TEST_ASSERT_EQUAL_INT(80000, g.points);
  TEST_ASSERT_EQUAL_INT(785, g.macro_res);
  o.radius = 6371000.0;
  zb_world_geology_options(&o, &g);
  TEST_ASSERT_EQUAL_INT(1024, g.macro_res); /* capped */
  TEST_ASSERT_EQUAL_STRING("quick", zb_world_quality_name(ZB_WORLD_QUICK));
  TEST_ASSERT_EQUAL_STRING("?", zb_world_quality_name(ZB_WORLD_QUALITY_COUNT));
  char err[128];
  zb_world_default_options(&o);
  o.quality = ZB_WORLD_QUALITY_COUNT;
  TEST_ASSERT_NULL(zb_world_creation_create(zb_test_alloc(), &o, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("unknown quality", err);
  zb_world_default_options(&o);
  o.temperature_offset = 20.0;
  TEST_ASSERT_NULL(zb_world_creation_create(zb_test_alloc(), &o, err, sizeof err));
  zb_world_default_options(&o);
  o.axial_tilt = 80.0;
  TEST_ASSERT_NULL(zb_world_creation_create(zb_test_alloc(), &o, err, sizeof err));
  zb_world_default_options(&o);
  o.radius = 1.0; /* geology refuses */
  TEST_ASSERT_NULL(zb_world_creation_create(zb_test_alloc(), &o, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("radius out of range", err);
  for (long budget = 0; budget < 30; ++budget) { /* out of memory at every step of creation */
    zb_failing_allocator fa;
    small_options(&o, 1);
    zb_world_creation *c = zb_world_creation_create(zb_failing_init(&fa, zb_test_alloc(), budget), &o, err, sizeof err);
    if (c) {
      zb_world_creation_destroy(c);
      break;
    }
    TEST_ASSERT_EQUAL_STRING("out of memory", err);
  }
}

/* spec: world-creation: The world bundle */
static void test_bundle_contents(void) {
  const zb_world_bundle *b = &g_bundle;
  const zb_geology *geo = zb_world_creation_geology(g_creation);
  const zb_geology_macro *m = zb_geology_get_macro(geo);
  TEST_ASSERT_EQUAL_UINT32(ZB_GENERATOR_VERSION, b->generator_version);
  TEST_ASSERT_EQUAL_INT(m->res, b->grid.res);
  TEST_ASSERT_EQUAL_INT(12000, b->points);
  TEST_ASSERT_EQUAL_MEMORY(m->height, b->height, sizeof(float) * b->grid.cells);
  TEST_ASSERT_EQUAL_MEMORY(zb_climate_biomes(zb_world_creation_climate(g_creation)), b->biome, b->grid.cells);
  TEST_ASSERT_TRUE(b->ley_node_count > 0 && b->ley_edge_count > 0 && b->ley_path_len > 0);
  TEST_ASSERT_TRUE(b->province_count > 0);
  TEST_ASSERT_EQUAL_INT(80, b->era_count);
  TEST_ASSERT_TRUE(b->metrics.attempts >= 1);
  /* Queries: a cell centre samples its own height; the rock record lies under it. */
  for (int32_t c = 0; c < (int32_t)b->grid.cells; c += 211) {
    zb_dvec3 d = zb_cubegrid_dir(&b->grid, c);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, (double)b->height[c], zb_world_bundle_height(b, d));
    TEST_ASSERT_EQUAL_INT(b->biome[c], zb_world_bundle_biome(b, d));
    TEST_ASSERT_EQUAL_INT(b->point[c], zb_world_bundle_point(b, d));
    double t, p;
    zb_world_bundle_climate(b, d, 6, 0.0, &t, &p);
    TEST_ASSERT_TRUE(t > -80.0 && t < 60.0 && p >= 0.0);
  }
  /* No seams: heights just either side of a cube edge are close. */
  double worst = 0.0;
  for (int k = 0; k < 64; ++k) {
    double v = -0.9 + 1.8 * (double)k / 63.0;
    zb_dvec3 inside = zb_planet_face_dir(ZB_FACE_PX, 0.9999, v), outside = zb_planet_face_dir(ZB_FACE_PX, 1.0001, v);
    double gap = fabs(zb_world_bundle_height(b, inside) - zb_world_bundle_height(b, outside));
    worst = gap > worst ? gap : worst;
  }
  TEST_ASSERT_TRUE(worst < 50.0);
}

/* spec: world-creation: Files survive the round trip */
static void test_round_trip(void) {
  TEST_ASSERT_EQUAL_MEMORY("ZBWB", g_file.data, 4);
  size_t raw = g_bundle.grid.cells * 33 + g_bundle.climate_grid.cells * 96 + (size_t)g_bundle.points * 74;
  TEST_ASSERT_TRUE(g_file.len < raw); /* compressed */
  zb_world_bundle back;
  char err[128];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_world_bundle_read(zb_test_alloc(), g_file.data, g_file.len, &back, err, sizeof err));
  TEST_ASSERT_EQUAL_UINT64(zb_world_bundle_hash(&g_bundle), zb_world_bundle_hash(&back));
  TEST_ASSERT_EQUAL_UINT64(g_bundle.options.seed, back.options.seed);
  TEST_ASSERT_EQUAL_DOUBLE(g_bundle.options.radius, back.options.radius);
  TEST_ASSERT_EQUAL_INT(g_bundle.metrics.continents, back.metrics.continents);
  for (int k = 0; k < back.era_count; ++k) { /* field by field: the structs have padding */
    TEST_ASSERT_EQUAL_FLOAT(g_bundle.eras[k].myr_ago, back.eras[k].myr_ago);
    TEST_ASSERT_EQUAL_FLOAT(g_bundle.eras[k].sea_level, back.eras[k].sea_level);
    TEST_ASSERT_EQUAL_FLOAT(g_bundle.eras[k].temperature, back.eras[k].temperature);
    TEST_ASSERT_EQUAL_FLOAT(g_bundle.eras[k].humidity, back.eras[k].humidity);
    TEST_ASSERT_EQUAL(g_bundle.eras[k].ice_age, back.eras[k].ice_age);
    TEST_ASSERT_EQUAL_UINT8(g_bundle.eras[k].plates, back.eras[k].plates);
    TEST_ASSERT_EQUAL_FLOAT(g_bundle.eras[k].land_fraction, back.eras[k].land_fraction);
  }
  for (int k = 0; k < back.ley_edge_count; ++k) {
    const zb_ley_edge *x = &g_bundle.ley_edges[k], *y = &back.ley_edges[k];
    TEST_ASSERT_TRUE(x->a == y->a && x->b == y->b && x->path == y->path && x->length == y->length &&
                     x->loop == y->loop);
  }
  for (int k = 0; k < back.ley_node_count; ++k) {
    TEST_ASSERT_EQUAL_INT32(g_bundle.ley_nodes[k].point, back.ley_nodes[k].point);
    TEST_ASSERT_EQUAL_FLOAT(g_bundle.ley_nodes[k].strength, back.ley_nodes[k].strength);
  }
  TEST_ASSERT_EQUAL_MEMORY(g_bundle.provinces, back.provinces, sizeof(zb_province) * (size_t)back.province_count);
  TEST_ASSERT_EQUAL_MEMORY(g_bundle.contact, back.contact, sizeof(float) * (size_t)(back.lineages * back.lineages));
  zb_world_bundle_free(&back);
  zb_world_bundle_free(&back); /* twice is harmless */
}

static char g_err[128];

static zb_result read_copy(const uint8_t *data, size_t len) {
  zb_world_bundle b;
  zb_result r = zb_world_bundle_read(zb_test_alloc(), data, len, &b, g_err, sizeof g_err);
  zb_world_bundle_free(&b);
  return r;
}

/* spec: world-creation: Damaged files are refused */
static void test_damaged(void) {
  uint8_t *copy = zb_alloc(zb_test_alloc(), g_file.len + 64);
  memcpy(copy, g_file.data, g_file.len);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, read_copy(copy, 3));
  TEST_ASSERT_EQUAL_STRING("not a world bundle", g_err);
  copy[4] = 99; /* a newer format */
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, read_copy(copy, g_file.len));
  TEST_ASSERT_EQUAL_STRING("the world bundle comes from a newer version of the game", g_err);
  copy[4] = ZB_BUNDLE_VERSION;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, read_copy(copy, g_file.len / 2));
  TEST_ASSERT_EQUAL_STRING("the world bundle is cut short", g_err);
  copy[g_file.len - 10] ^= 0x55; /* inside the last section's compressed data */
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, read_copy(copy, g_file.len));
  TEST_ASSERT_EQUAL_STRING("the world bundle is damaged (checksum)", g_err);
  copy[g_file.len - 10] ^= 0x55;
  /* An unknown section from a newer version is skipped. */
  uint32_t sections = 9;
  memcpy(copy + 12, &sections, 4);
  static const uint8_t extra[28] = {'N', 'E', 'W', '!', 0}; /* zero sizes, checksum 0 */
  memcpy(copy + g_file.len, extra, sizeof extra);
  TEST_ASSERT_EQUAL_INT(ZB_OK, read_copy(copy, g_file.len + sizeof extra));
  /* A missing section. */
  sections = 7;
  memcpy(copy + 12, &sections, 4);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, read_copy(copy, g_file.len));
  TEST_ASSERT_EQUAL_STRING("the world bundle misses a section", g_err);
  zb_free(zb_test_alloc(), copy);
  /* Writing and reading without memory. */
  for (long budget = 0; budget < 60; budget += 7) {
    zb_failing_allocator fa;
    zb_world_bundle b;
    zb_result r = zb_world_bundle_read(zb_failing_init(&fa, zb_test_alloc(), budget), g_file.data, g_file.len, &b,
                                       g_err, sizeof g_err);
    zb_world_bundle_free(&b);
    TEST_ASSERT_TRUE(r == ZB_OK || r == ZB_ERR_INVALID);
    zb_array out;
    zb_array_init(&out, zb_failing_init(&fa, zb_test_alloc(), budget), 1);
    r = zb_world_bundle_write(&g_bundle, &out);
    TEST_ASSERT_TRUE(r == ZB_OK || (r == ZB_ERR_NOMEM && out.len == 0));
    zb_array_free(&out);
  }
}

/* spec: world-creation: Terrain from the created world */
static void test_terrain_v2(void) {
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.bundle = &g_bundle;
  d.seed = 999; /* replaced by the bundle's */
  char err[128];
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(w, err);
  TEST_ASSERT_EQUAL_PTR(&g_bundle, zb_world_get_bundle(w));
  TEST_ASSERT_EQUAL_DOUBLE(400000.0, zb_world_planet(w)->radius);
  const zb_terrain *t = zb_world_terrain(w);
  /* Heights follow the macro grid within the hills and crags below its cells. */
  for (int32_t c = 0; c < (int32_t)g_bundle.grid.cells; c += 97) {
    zb_dvec3 dir = zb_cubegrid_dir(&g_bundle.grid, c);
    TEST_ASSERT_DOUBLE_WITHIN(400.0, (double)g_bundle.height[c], zb_terrain_macro_height(t, dir));
  }
  /* Materials follow the biomes: find a sand desert and a glacier chunk. */
  int found_sand = 0, found_ice = 0;
  for (int32_t c = 0; c < (int32_t)g_bundle.grid.cells && (!found_sand || !found_ice); ++c) {
    zb_biome b = (zb_biome)g_bundle.biome[c];
    if ((b != ZB_BIOME_SAND_DESERT || found_sand) && (b != ZB_BIOME_ICE_SHEET || found_ice)) {
      continue;
    }
    zb_planet_pos pos = zb_planet_from_cart(
        zb_world_planet(w), zb_dv_scale(zb_cubegrid_dir(&g_bundle.grid, c), zb_world_planet(w)->radius));
    zb_chunk_terrain ct;
    zb_chunk_terrain_init(&ct, pos.chunk);
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(t, zb_test_alloc(), &ct, ZB_STAGE_SURFACE));
    int want = b == ZB_BIOME_SAND_DESERT ? ZB_MAT_SAND : ZB_MAT_ICE, hits = 0;
    for (int k = 0; k < ZB_SURFACE_RES * ZB_SURFACE_RES; ++k) {
      hits += ct.material[k] == want;
    }
    if (hits > ZB_SURFACE_RES * ZB_SURFACE_RES / 2) {
      *(b == ZB_BIOME_SAND_DESERT ? &found_sand : &found_ice) = 1;
    }
    zb_chunk_terrain_release(zb_test_alloc(), &ct);
  }
  TEST_ASSERT_TRUE(found_sand || found_ice);
  zb_world_destroy(w);
  /* The biome rule itself. */
  TEST_ASSERT_EQUAL_INT(ZB_MAT_MUD, zb_terrain_biome_material(ZB_BIOME_TAIGA, -100.0, 0.0, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_SAND, zb_terrain_biome_material(ZB_BIOME_TAIGA, 1.0, 0.0, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_ROCK, zb_terrain_biome_material(ZB_BIOME_TAIGA, 100.0, 1.0, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_GRAVEL, zb_terrain_biome_material(ZB_BIOME_SCREE, 900.0, 0.1, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_ROCK, zb_terrain_biome_material(ZB_BIOME_SCREE, 900.0, 0.6, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_ICE, zb_terrain_biome_material(ZB_BIOME_GLACIER, 900.0, 0.1, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_MUD, zb_terrain_biome_material(ZB_BIOME_MARSH, 20.0, 0.1, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_SNOW, zb_terrain_biome_material(ZB_BIOME_ALPINE_MEADOW, 4000.0, 0.1, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_SOIL, zb_terrain_biome_material(ZB_BIOME_ALPINE_MEADOW, 2000.0, 0.1, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_GRAVEL, zb_terrain_biome_material(ZB_BIOME_TAIGA, 300.0, 0.65, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_SOIL, zb_terrain_biome_material(ZB_BIOME_TAIGA, 300.0, 0.1, 0.0));
}

/* spec: world-creation: Deterministic worlds */
static void test_golden(void) {
  char hex[17];
  zb_hash_hex(zb_world_bundle_hash(&g_bundle), hex);
  printf("world bundle golden hash: %s\n", hex);
  TEST_ASSERT_EQUAL_STRING("60ecad6cb056df0f", hex); /* the same on every platform */
}

int main(void) {
  /* The shared world. */
  zb_world_options o;
  small_options(&o, 42);
  char err[128];
  g_creation = zb_world_creation_create(zb_heap_allocator(), &o, err, sizeof err);
  while (g_creation && !zb_world_creation_advance(g_creation)) {
  }
  zb_array_init(&g_file, zb_heap_allocator(), 1);
  if (!g_creation || zb_world_creation_bundle(g_creation, zb_heap_allocator(), &g_bundle) != ZB_OK ||
      zb_world_bundle_write(&g_bundle, &g_file) != ZB_OK) {
    fprintf(stderr, "could not make the shared world: %s\n", err);
    return 1;
  }
  UNITY_BEGIN();
  RUN_TEST(test_stages);
  RUN_TEST(test_cancel);
  RUN_TEST(test_options);
  RUN_TEST(test_bundle_contents);
  RUN_TEST(test_round_trip);
  RUN_TEST(test_damaged);
  RUN_TEST(test_terrain_v2);
  RUN_TEST(test_golden);
  int result = UNITY_END();
  zb_array_free(&g_file);
  zb_world_bundle_free(&g_bundle);
  zb_world_creation_destroy(g_creation);
  return result;
}

/* SPDX-License-Identifier: MIT */
#include "support/test_support.h"
#include "unity.h"
#include "world/delta.h"
#include "world/flora.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static int regions_seen;
static void count_region(void *ctx, int face, int32_t rx, int32_t ry) {
  (void)ctx;
  (void)face;
  (void)rx;
  (void)ry;
  regions_seen++;
}

/* spec: world-persistence: Edits are kept per chunk */
static void test_store(void) {
  zb_deltas *d = zb_deltas_create(zb_test_alloc());
  TEST_ASSERT_NOT_NULL(d);
  zb_chunk a = {2, 100, 200}, b = {2, 117, 200}, c = {5, -3, 7};
  zb_edit plant = {ZB_EDIT_PLANT_REMOVED, 0, 0, 0, 42};
  zb_edit paint = {ZB_EDIT_MATERIAL, ZB_MAT_SAND, 10, 20, 0};
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_add(d, a, &plant));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_add(d, a, &paint));
  paint.value = ZB_MAT_GRAVEL; /* the same cell again: replaced */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_add(d, a, &paint));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_add(d, b, &plant));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_add(d, c, &plant));
  size_t n;
  const zb_edit *e = zb_deltas_get(d, a, &n);
  TEST_ASSERT_EQUAL_size_t(2, n);
  TEST_ASSERT_EQUAL_UINT8(ZB_MAT_GRAVEL, e[1].value);
  TEST_ASSERT_NULL(zb_deltas_get(d, (zb_chunk){0, 0, 0}, &n));
  TEST_ASSERT_EQUAL_size_t(0, n);
  TEST_ASSERT_EQUAL_size_t(3, zb_deltas_chunks(d));
  /* Invalid edits are refused. */
  zb_edit bad = {ZB_EDIT_MATERIAL, ZB_MAT_NONE, 1, 1, 0};
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_add(d, a, &bad));
  bad.value = ZB_MAT_SOIL;
  bad.x = ZB_SURFACE_RES;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_add(d, a, &bad));
  bad.kind = 9;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_add(d, a, &bad));
  /* Three regions changed (chunk x 100 and 117 lie in different regions of 16). */
  regions_seen = 0;
  zb_deltas_each_dirty(d, count_region, NULL);
  TEST_ASSERT_EQUAL_INT(3, regions_seen);
  zb_deltas_clear_dirty(d);
  regions_seen = 0;
  zb_deltas_each_dirty(d, count_region, NULL);
  TEST_ASSERT_EQUAL_INT(0, regions_seen);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_each_region(d, count_region, NULL));
  TEST_ASSERT_EQUAL_INT(3, regions_seen);
  char name[64];
  zb_region_file_name(5, -1, 0, name, sizeof name);
  TEST_ASSERT_EQUAL_STRING("5.-1.0.bin", name);
  zb_deltas_destroy(d);
  zb_deltas_destroy(NULL);
}

/* spec: world-persistence: Edits are kept per chunk */
static void test_apply(void) {
  zb_deltas *d = zb_deltas_create(zb_test_alloc());
  zb_chunk_terrain ct;
  zb_chunk_terrain_init(&ct, (zb_chunk){1, 2, 3});
  zb_plant plants[3];
  memset(plants, 0, sizeof plants);
  uint8_t material[ZB_SURFACE_RES * ZB_SURFACE_RES];
  memset(material, ZB_MAT_SOIL, sizeof material);
  zb_edit plant = {ZB_EDIT_PLANT_REMOVED, 0, 0, 0, 1};
  zb_edit far = {ZB_EDIT_PLANT_REMOVED, 0, 0, 0, 7}; /* beyond the chunk's plants: ignored */
  zb_edit paint = {ZB_EDIT_MATERIAL, ZB_MAT_ROCK, 5, 6, 0};
  zb_deltas_add(d, ct.chunk, &plant);
  zb_deltas_add(d, ct.chunk, &far);
  zb_deltas_add(d, ct.chunk, &paint);
  TEST_ASSERT_FALSE(zb_deltas_apply(d, &ct, 0.0)); /* nothing generated yet */
  ct.material = material;
  ct.plants = plants;
  ct.plant_count = 3;
  TEST_ASSERT_TRUE(zb_deltas_apply(d, &ct, 0.0));
  TEST_ASSERT_EQUAL_UINT8(ZB_PLANT_REMOVED, plants[1].layer);
  TEST_ASSERT_EQUAL_UINT8(0, plants[0].layer);
  TEST_ASSERT_EQUAL_UINT8(ZB_MAT_ROCK, material[6 * ZB_SURFACE_RES + 5]);
  TEST_ASSERT_FALSE(zb_deltas_apply(d, &ct, 0.0)); /* already applied */
  zb_deltas_destroy(d);
}

/* spec: world-persistence: Region files */
static void test_regions(void) {
  zb_deltas *d = zb_deltas_create(zb_test_alloc());
  zb_chunk a = {3, -20, 33}, b = {3, -17, 40}, other = {3, 100, 100};
  for (uint32_t i = 0; i < 50; ++i) {
    zb_edit e = {ZB_EDIT_PLANT_REMOVED, 0, 0, 0, i * 3};
    zb_deltas_add(d, a, &e);
    zb_edit m = {ZB_EDIT_MATERIAL, ZB_MAT_MUD, (uint16_t)i, (uint16_t)(255 - i), 0};
    zb_deltas_add(d, b, &m);
  }
  zb_edit e = {ZB_EDIT_PLANT_REMOVED, 0, 0, 0, 1};
  zb_deltas_add(d, other, &e);
  zb_array file;
  zb_array_init(&file, zb_test_alloc(), 1);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_write_region(d, 3, -2, 2, &file));
  TEST_ASSERT_EQUAL_MEMORY("ZBRD", file.data, 4);
  /* Read back into a fresh store: the region's chunks only, the same edits. */
  zb_deltas *back = zb_deltas_create(zb_test_alloc());
  char err[160];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_read_region(back, file.data, file.len, err, sizeof err));
  TEST_ASSERT_EQUAL_size_t(2, zb_deltas_chunks(back));
  size_t n1, n2;
  const zb_edit *x = zb_deltas_get(d, b, &n1), *y = zb_deltas_get(back, b, &n2);
  TEST_ASSERT_EQUAL_size_t(n1, n2);
  for (size_t k = 0; k < n1; ++k) { /* field by field: the struct has padding */
    TEST_ASSERT_TRUE(x[k].kind == y[k].kind && x[k].value == y[k].value && x[k].x == y[k].x && x[k].y == y[k].y &&
                     x[k].index == y[k].index);
  }
  regions_seen = 0;
  zb_deltas_each_dirty(back, count_region, NULL); /* reading is not an edit */
  TEST_ASSERT_EQUAL_INT(0, regions_seen);
  /* Damaged files are refused. */
  unsigned char *copy = malloc(file.len);
  memcpy(copy, file.data, file.len);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_read_region(back, copy, 3, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("not a region file", err);
  copy[4] = 99;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_read_region(back, copy, file.len, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("the region file comes from a newer version of the game", err);
  copy[4] = 1;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_read_region(back, copy, file.len - 5, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("the region file is cut short or damaged", err);
  copy[file.len - 6] ^= 0x5a;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_read_region(back, copy, file.len, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("the region file is damaged (checksum)", err);
  free(copy);
  /* Running out of memory is reported. */
  for (int fail = 0; fail < 12; ++fail) {
    zb_failing_allocator fa;
    const zb_allocator *fail_alloc = zb_failing_init(&fa, zb_test_alloc(), fail);
    zb_deltas *t = zb_deltas_create(fail_alloc);
    if (!t) {
      continue;
    }
    zb_result r = zb_deltas_read_region(t, file.data, file.len, err, sizeof err);
    TEST_ASSERT_TRUE(r == ZB_OK || r == ZB_ERR_NOMEM);
    zb_array out;
    zb_array_init(&out, fail_alloc, 1);
    r = zb_deltas_write_region(d, 3, -2, 2, &out);
    TEST_ASSERT_TRUE(r == ZB_OK || r == ZB_ERR_NOMEM);
    zb_array_free(&out);
    zb_deltas_destroy(t);
  }
  zb_array_free(&file);
  zb_deltas_destroy(back);
  zb_deltas_destroy(d);
}

/* A chunk as generated: soil everywhere, a tree, a shrub and ground cover on
 * the 2 m cell (10, 10), and a tree elsewhere. */
static void fresh_chunk(zb_chunk_terrain *ct, uint8_t *material, zb_plant *plants) {
  zb_chunk_terrain_init(ct, (zb_chunk){4, 7, 9});
  memset(material, ZB_MAT_SOIL, ZB_SURFACE_RES * ZB_SURFACE_RES);
  memset(plants, 0, sizeof(zb_plant) * 4);
  for (int i = 0; i < 3; ++i) {
    plants[i].x = plants[i].y = 21 * 256; /* 21 m: in the 2 m cell 10 */
    plants[i].layer = (uint8_t)i;         /* tree, shrub, ground cover */
  }
  plants[3].x = plants[3].y = 100 * 256;
  ct->material = material;
  ct->plants = plants;
  ct->plant_count = 4;
}

/* spec: world-persistence: Burnt ground heals */
static void test_burnt(void) {
  zb_deltas *d = zb_deltas_create(zb_test_alloc());
  zb_chunk_terrain ct;
  uint8_t material[ZB_SURFACE_RES * ZB_SURFACE_RES];
  zb_plant plants[4];
  fresh_chunk(&ct, material, plants);
  double fire = 1000.0 * 60.0; /* game minute 1000 */
  zb_edit burnt = {ZB_EDIT_BURNT, 0, 10, 10, 1000};
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_add(d, ct.chunk, &burnt));
  burnt.x = ZB_BURN_RES;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_add(d, ct.chunk, &burnt));
  /* Right after the fire: ash, and nothing stands there. */
  TEST_ASSERT_TRUE(zb_deltas_apply(d, &ct, fire + 3600.0));
  TEST_ASSERT_EQUAL_UINT8(ZB_MAT_ASH, material[20 * ZB_SURFACE_RES + 20]);
  TEST_ASSERT_EQUAL_UINT8(ZB_MAT_ASH, material[21 * ZB_SURFACE_RES + 21]);
  TEST_ASSERT_EQUAL_UINT8(ZB_MAT_SOIL, material[22 * ZB_SURFACE_RES + 22]);
  for (int i = 0; i < 3; ++i) {
    TEST_ASSERT_EQUAL_UINT8(ZB_PLANT_REMOVED, plants[i].layer);
  }
  TEST_ASSERT_EQUAL_UINT8(ZB_PLANT_TREE, plants[3].layer);
  TEST_ASSERT_FALSE(zb_deltas_apply(d, &ct, fire + 3600.0)); /* already applied */
  /* 30 days later: ground cover back, no ash; shrubs and trees still gone. */
  fresh_chunk(&ct, material, plants);
  zb_deltas_apply(d, &ct, fire + 30.0 * 86400.0);
  TEST_ASSERT_EQUAL_UINT8(ZB_MAT_SOIL, material[20 * ZB_SURFACE_RES + 20]);
  TEST_ASSERT_EQUAL_UINT8(ZB_PLANT_REMOVED, plants[0].layer);
  TEST_ASSERT_EQUAL_UINT8(ZB_PLANT_REMOVED, plants[1].layer);
  TEST_ASSERT_EQUAL_UINT8(ZB_PLANT_GROUND, plants[2].layer);
  /* Five years on, the forest is back. */
  fresh_chunk(&ct, material, plants);
  TEST_ASSERT_FALSE(zb_deltas_apply(d, &ct, fire + 241.0 * 86400.0));
  TEST_ASSERT_EQUAL_UINT8(ZB_PLANT_TREE, plants[0].layer);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_regrowth_seconds(-1));
  TEST_ASSERT_EQUAL_DOUBLE(48.0 * 86400.0, zb_regrowth_seconds(ZB_PLANT_SHRUB));
  /* Burnt cells travel in region files like every edit. */
  zb_array file;
  zb_array_init(&file, zb_test_alloc(), 1);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_write_region(d, 4, 0, 0, &file));
  zb_deltas *back = zb_deltas_create(zb_test_alloc());
  char err[160];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_read_region(back, file.data, file.len, err, sizeof err));
  size_t n;
  const zb_edit *e = zb_deltas_get(back, ct.chunk, &n);
  TEST_ASSERT_EQUAL_size_t(1, n);
  TEST_ASSERT_TRUE(e[0].kind == ZB_EDIT_BURNT && e[0].x == 10 && e[0].y == 10 && e[0].index == 1000);
  zb_array_free(&file);
  zb_deltas_destroy(back);
  zb_deltas_destroy(d);
}

/* spec: terrain-deformation: Heights are kept as edits */
/* spec: mana: Drawing depletes and overdrawing blights */
static void test_heights(void) {
  zb_deltas *d = zb_deltas_create(zb_test_alloc());
  zb_chunk_terrain ct;
  zb_chunk_terrain_init(&ct, (zb_chunk){2, 5, 6});
  static float height[ZB_HEIGHT_RES * ZB_HEIGHT_RES];
  for (int i = 0; i < ZB_HEIGHT_RES * ZB_HEIGHT_RES; ++i) {
    height[i] = 10.0f;
  }
  ct.min_height = ct.max_height = 10.0f;
  float low = 4.5f, nan = NAN, huge = 30000.0f;
  zb_edit e = {ZB_EDIT_HEIGHT, 0, 256, 256, 0};
  memcpy(&e.index, &low, sizeof low);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_add(d, ct.chunk, &e));
  zb_edit bad = e;
  bad.x = ZB_HEIGHT_RES;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_add(d, ct.chunk, &bad));
  bad = e;
  memcpy(&bad.index, &nan, sizeof nan);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_add(d, ct.chunk, &bad));
  memcpy(&bad.index, &huge, sizeof huge);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_add(d, ct.chunk, &bad));
  /* Blighted mana cells: 8 per chunk edge; they change nothing in the chunk. */
  zb_edit blight = {ZB_EDIT_BLIGHT, 0, 7, 7, 100};
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_add(d, ct.chunk, &blight));
  blight.x = ZB_BLIGHT_RES;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_deltas_add(d, ct.chunk, &blight));
  TEST_ASSERT_FALSE(zb_deltas_apply(d, &ct, 0.0)); /* no heights yet */
  ct.height = height;
  TEST_ASSERT_TRUE(zb_deltas_apply(d, &ct, 0.0));
  TEST_ASSERT_EQUAL_FLOAT(4.5f, height[256 * ZB_HEIGHT_RES + 256]);
  TEST_ASSERT_EQUAL_FLOAT(4.5f, ct.min_height);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, ct.max_height);
  TEST_ASSERT_FALSE(zb_deltas_apply(d, &ct, 0.0)); /* the same heights again: nothing changes */
  zb_array file;
  zb_array_init(&file, zb_test_alloc(), 1);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_write_region(d, 2, 0, 0, &file));
  zb_deltas *back = zb_deltas_create(zb_test_alloc());
  char err[160];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_deltas_read_region(back, file.data, file.len, err, sizeof err));
  size_t n;
  const zb_edit *got = zb_deltas_get(back, ct.chunk, &n);
  TEST_ASSERT_EQUAL_size_t(2, n);
  TEST_ASSERT_TRUE(got[0].kind == ZB_EDIT_HEIGHT && got[0].index == e.index);
  TEST_ASSERT_TRUE(got[1].kind == ZB_EDIT_BLIGHT && got[1].index == 100);
  zb_array_free(&file);
  zb_deltas_destroy(back);
  zb_deltas_destroy(d);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_store);
  RUN_TEST(test_apply);
  RUN_TEST(test_regions);
  RUN_TEST(test_burnt);
  RUN_TEST(test_heights);
  return UNITY_END();
}

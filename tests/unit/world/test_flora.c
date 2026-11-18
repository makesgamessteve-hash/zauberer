/* SPDX-License-Identifier: MIT */
#include "core/hash.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/climate.h"
#include "world/dvec.h"
#include "world/flora.h"
#include "world/terrain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static char *g_species, *g_atlas;
static size_t g_species_len, g_atlas_len;
static zb_flora g_flora;
static zb_planet g_planet;
static zb_terrain g_terrain;

static char *read_asset(const char *rel, size_t *len) {
  char path[1024];
  snprintf(path, sizeof path, "%s/assets/base/%s", getenv("ZB_SOURCE_DIR") ? getenv("ZB_SOURCE_DIR") : ".", rel);
  FILE *f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *data = malloc((size_t)n + 1);
  *len = fread(data, 1, (size_t)n, f);
  data[*len] = '\0';
  fclose(f);
  return data;
}

static zb_result parse(const char *species, char *err, size_t err_len) {
  zb_flora f;
  return zb_flora_parse(&f, zb_test_alloc(), species, strlen(species), g_atlas, g_atlas_len, err, err_len);
}

/* spec: vegetation: The species registry */
static void test_registry(void) {
  TEST_ASSERT_TRUE(g_flora.count >= 19);
  int oak = zb_flora_find(&g_flora, "oak"), reeds = zb_flora_find(&g_flora, "reeds");
  TEST_ASSERT_TRUE(oak >= 0 && reeds >= 0);
  TEST_ASSERT_EQUAL_INT(-1, zb_flora_find(&g_flora, "triffid"));
  const zb_species *s = &g_flora.species[oak];
  TEST_ASSERT_EQUAL_STRING("Oak", s->name);
  TEST_ASSERT_EQUAL_INT(ZB_PLANT_TREE, s->layer);
  TEST_ASSERT_EQUAL_INT(0, s->sprite);
  TEST_ASSERT_TRUE(s->clump > 0);
  TEST_ASSERT_TRUE(s->grows_on & (1u << ZB_MAT_SOIL));
  TEST_ASSERT_FALSE(s->grows_on & (1u << ZB_MAT_ROCK));
  TEST_ASSERT_TRUE(g_flora.species[reeds].wet);
  TEST_ASSERT_EQUAL_INT(ZB_PLANT_GROUND, g_flora.species[reeds].layer);
  TEST_ASSERT_EQUAL_INT(8, g_flora.columns);
  TEST_ASSERT_EQUAL_INT(32, g_flora.frame_width);
  TEST_ASSERT_EQUAL_INT(48, g_flora.frame_height);
  TEST_ASSERT_TRUE(g_flora.frames > 20);
  /* Every layer is present. */
  int layers[ZB_PLANT_LAYERS] = {0};
  for (int i = 0; i < g_flora.count; ++i) {
    layers[g_flora.species[i].layer]++;
  }
  TEST_ASSERT_TRUE(layers[ZB_PLANT_TREE] >= 8 && layers[ZB_PLANT_SHRUB] >= 2 && layers[ZB_PLANT_GROUND] >= 3);
}

/* spec: vegetation: Broken registries are refused */
static void test_registry_errors(void) {
  char err[160];
  static const char *const good =
      "{ id = 'x', layer = 'tree', sprite = 'oak', height = { 1, 2 }, width = 1, temperature = { 0, 10, 20 },"
      " rain = { 100, 500, 900 }, grows_on = { 'soil' }, seasons = { {1,1,1}, {1,1,1}, {1,1,1}, {1,1,1} } }";
  char doc[1024];
  snprintf(doc, sizeof doc, "return { %s }", good);
  TEST_ASSERT_EQUAL_INT(ZB_OK, parse(doc, err, sizeof err));
  static const struct {
    const char *from, *to, *message;
  } cases[] = {
      {"id = 'x'", "id = ''", "a species needs an id"},
      {"layer = 'tree'", "layer = 'vine'", "species 'x': bad or missing layer"},
      {"sprite = 'oak'", "sprite = 'rose'", "species 'x': bad or missing sprite"},
      {"sprite = 'oak'", "sprite = 'oak', clump = 'bush'", "species 'x': bad or missing clump"},
      {"height = { 1, 2 }", "height = { 2, 1 }", "species 'x': bad or missing height"},
      {"width = 1", "width = 0", "species 'x': bad or missing width"},
      {"temperature = { 0, 10, 20 }", "temperature = { 0, 30, 20 }", "species 'x': bad or missing temperature"},
      {"rain = { 100, 500, 900 }", "rain = { 100, 500 }", "species 'x': bad or missing rain"},
      {"width = 1", "width = 1, density = -1", "species 'x': bad or missing density"},
      {"grows_on = { 'soil' }", "grows_on = { 'lava' }", "species 'x': bad or missing grows_on"},
      {"grows_on = { 'soil' }", "grows_on = { }", "species 'x': bad or missing grows_on"},
      {"{1,1,1}, {1,1,1}, {1,1,1}, {1,1,1}", "{1,1,1}, {1,1,1}, {1,1,1}", "species 'x': bad or missing seasons"},
      {"{1,1,1}, {1,1,1}, {1,1,1}, {1,1,1}", "{1,1,1}, {1,1,1}, {1,1,1}, {1,'a',1}",
       "species 'x': bad or missing seasons"},
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
    char changed[1024];
    const char *at = strstr(good, cases[i].from);
    TEST_ASSERT_NOT_NULL(at);
    snprintf(changed, sizeof changed, "return { %.*s%s%s }", (int)(at - good), good, cases[i].to,
             at + strlen(cases[i].from));
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZB_ERR_INVALID, parse(changed, err, sizeof err), cases[i].to);
    TEST_ASSERT_EQUAL_STRING_LEN(cases[i].message, err, strlen(cases[i].message));
  }
  snprintf(doc, sizeof doc, "return { %s, %s }", good, good);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse(doc, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("species 'x' is listed twice", err);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse("return {", err, sizeof err));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, parse("return {}", err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("flora: between 1 and 64 species", err);
  zb_flora f;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_flora_parse(&f, zb_test_alloc(), doc, strlen(doc), "x", 1, err, sizeof err));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID,
                        zb_flora_parse(&f, zb_test_alloc(), doc, strlen(doc), "return {}", 9, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("flora atlas: no frames", err);
  TEST_ASSERT_EQUAL_INT(0, f.count);
}

/* spec: vegetation: Plants grow where their climate suits them */
static void test_suitability(void) {
  const zb_species *oak = &g_flora.species[zb_flora_find(&g_flora, "oak")];
  const zb_species *palm = &g_flora.species[zb_flora_find(&g_flora, "palm")];
  const zb_species *reeds = &g_flora.species[zb_flora_find(&g_flora, "reeds")];
  zb_site temperate = {11.0, -2.0, 900.0, 0.1, ZB_MAT_SOIL, ZB_BIOME_DECIDUOUS_FOREST, false};
  zb_site tropical = {27.0, 25.0, 2000.0, 0.1, ZB_MAT_SAND, ZB_BIOME_BEACH, false};
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, zb_species_suitability(oak, &temperate));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_species_suitability(oak, &tropical));
  TEST_ASSERT_TRUE(zb_species_suitability(palm, &tropical) > 0.9);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_species_suitability(palm, &temperate));
  zb_site frost = temperate;
  frost.coldest = -30.0;
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_species_suitability(oak, &frost));
  zb_site steep = temperate;
  steep.slope = 1.5;
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_species_suitability(oak, &steep));
  zb_site rock = temperate;
  rock.material = ZB_MAT_ROCK;
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_species_suitability(oak, &rock));
  rock.material = ZB_MAT_NONE;
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_species_suitability(oak, &rock));
  zb_site marsh = temperate;
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_species_suitability(reeds, &marsh)); /* reeds need water */
  marsh.wet = true;
  TEST_ASSERT_TRUE(zb_species_suitability(reeds, &marsh) > 0.5);
  /* Half-way to a limit, a quarter of the way down the parabola. */
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.75, zb_species_climate(oak, 7.0, 0.0, 900.0));
}

/* spec: vegetation: Biomes carry their plants */
static void test_cover(void) {
  TEST_ASSERT_TRUE(zb_biome_cover(ZB_BIOME_RAINFOREST, ZB_PLANT_TREE) >
                   zb_biome_cover(ZB_BIOME_SAVANNA, ZB_PLANT_TREE));
  TEST_ASSERT_TRUE(zb_biome_cover(ZB_BIOME_SAVANNA, ZB_PLANT_TREE) >
                   zb_biome_cover(ZB_BIOME_SAND_DESERT, ZB_PLANT_TREE));
  TEST_ASSERT_TRUE(zb_biome_cover(ZB_BIOME_STEPPE, ZB_PLANT_GROUND) > zb_biome_cover(ZB_BIOME_STEPPE, ZB_PLANT_TREE));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_biome_cover(ZB_BIOME_ICE_SHEET, ZB_PLANT_GROUND));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_biome_cover(ZB_BIOME_DEEP_OCEAN, ZB_PLANT_TREE));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_biome_cover(ZB_BIOME_RAINFOREST, ZB_PLANT_LAYERS));
  TEST_ASSERT_TRUE(zb_biome_is_wet(ZB_BIOME_MARSH) && zb_biome_is_wet(ZB_BIOME_SWAMP_FOREST));
  TEST_ASSERT_FALSE(zb_biome_is_wet(ZB_BIOME_STEPPE));
}

/* A land chunk at about a latitude (degrees) in the noise world, generated to stage 3. */
static bool land_chunk(double lat_deg, zb_chunk_terrain *ct) {
  for (int k = 0; k < 400; ++k) {
    double lat = (lat_deg + 0.02 * (k % 20 - 10)) * ZB_DPI / 180.0, lon = 0.05 * k;
    zb_dvec3 d = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
    if (zb_terrain_macro_height(&g_terrain, d) < 60.0) {
      continue;
    }
    zb_planet_pos pos = zb_planet_from_cart(&g_planet, zb_dv_scale(d, g_planet.radius));
    zb_chunk_terrain_init(ct, pos.chunk);
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_test_alloc(), ct, ZB_STAGE_ECOLOGY));
    if (ct->min_height > 5.0f) {
      return true;
    }
    zb_chunk_terrain_release(zb_test_alloc(), ct);
  }
  return false;
}

static int count_species(const zb_chunk_terrain *ct, const char *id) {
  int s = zb_flora_find(&g_flora, id), n = 0;
  for (uint32_t i = 0; i < ct->plant_count; ++i) {
    n += ct->plants[i].species == s;
  }
  return n;
}

/* spec: vegetation: Plants grow where their climate suits them */
static void test_climate_by_latitude(void) {
  double mean, coldest, warmest, rain;
  zb_chunk equator = zb_planet_from_cart(&g_planet, zb_dv(g_planet.radius, 0.0, 0.0)).chunk;
  zb_terrain_chunk_climate(&g_terrain, equator, &mean, &coldest, &warmest, &rain);
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 27.0, mean);
  TEST_ASSERT_TRUE(warmest > coldest && rain > 100.0);
  zb_chunk pole = zb_planet_from_cart(&g_planet, zb_dv(0.0, 0.0, g_planet.radius)).chunk;
  zb_terrain_chunk_climate(&g_terrain, pole, &mean, &coldest, &warmest, &rain);
  TEST_ASSERT_TRUE(mean < -10.0);
  /* Boreal land: conifers and birch, no palms; tropical land: no spruce. */
  zb_chunk_terrain boreal, tropic;
  TEST_ASSERT_TRUE(land_chunk(58.0, &boreal));
  TEST_ASSERT_TRUE(land_chunk(5.0, &tropic));
  int cold_trees = count_species(&boreal, "spruce") + count_species(&boreal, "fir") + count_species(&boreal, "pine") +
                   count_species(&boreal, "birch");
  printf("boreal chunk: %u plants, %d conifers and birches; tropical chunk: %u plants\n", boreal.plant_count,
         cold_trees, tropic.plant_count);
  TEST_ASSERT_TRUE(cold_trees > 0);
  TEST_ASSERT_EQUAL_INT(0, count_species(&boreal, "palm") + count_species(&boreal, "kapok"));
  TEST_ASSERT_EQUAL_INT(0, count_species(&tropic, "spruce") + count_species(&tropic, "birch"));
  TEST_ASSERT_TRUE(tropic.plant_count > 0);
  zb_chunk_terrain_release(zb_test_alloc(), &boreal);
  zb_chunk_terrain_release(zb_test_alloc(), &tropic);
}

/* spec: vegetation: Plants on every chunk near the player */
static void test_placement(void) {
  zb_chunk_terrain ct;
  TEST_ASSERT_TRUE(land_chunk(45.0, &ct));
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_ECOLOGY, ct.stage);
  TEST_ASSERT_TRUE(ct.plant_count > 50 && ct.plant_count <= ZB_PLANTS_MAX);
  /* Layers in order, on the ground they need, never under water. */
  int last_layer = 0;
  for (uint32_t i = 0; i < ct.plant_count; ++i) {
    const zb_plant *p = &ct.plants[i];
    TEST_ASSERT_TRUE(p->layer >= last_layer);
    last_layer = p->layer;
    const zb_species *sp = &g_flora.species[p->species];
    TEST_ASSERT_EQUAL_INT(sp->layer, p->layer);
    int x = p->x / 256, y = p->y / 256;
    TEST_ASSERT_TRUE(sp->grows_on & (1u << ct.material[y * ZB_SURFACE_RES + x]));
    TEST_ASSERT_TRUE(zb_chunk_height_at(&ct, (float)p->x / 256.0f, (float)p->y / 256.0f) >= 0.0f);
  }
  /* The same plants every time, on every platform. */
  zb_chunk_terrain again;
  zb_chunk_terrain_init(&again, ct.chunk);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_test_alloc(), &again, ZB_STAGE_ECOLOGY));
  TEST_ASSERT_EQUAL_UINT64(zb_chunk_terrain_hash(&ct), zb_chunk_terrain_hash(&again));
  zb_chunk_terrain_release(zb_test_alloc(), &again);
  /* Footprint: at most 330 KB of stage 2 plus 64 KB of plants. */
  TEST_ASSERT_TRUE(zb_chunk_terrain_bytes(&ct) <= 330u * 1024u + sizeof(zb_plant) * ZB_PLANTS_MAX);
  /* Trimming to stage 2 drops the plants, and stage 3 comes back the same. */
  uint64_t full = zb_chunk_terrain_hash(&ct);
  zb_chunk_terrain_trim(zb_test_alloc(), &ct, ZB_STAGE_SURFACE);
  TEST_ASSERT_NULL(ct.plants);
  TEST_ASSERT_EQUAL_UINT32(0, ct.plant_count);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_test_alloc(), &ct, ZB_STAGE_ECOLOGY));
  TEST_ASSERT_EQUAL_UINT64(full, zb_chunk_terrain_hash(&ct));
  /* The canopy of stage 0 shows the trees. */
  int covered = 0;
  for (int i = 0; i < ZB_CANOPY_RES * ZB_CANOPY_RES; ++i) {
    covered += ct.canopy[i] > 0;
  }
  int trees = 0;
  for (uint32_t i = 0; i < ct.plant_count; ++i) {
    trees += ct.plants[i].layer == ZB_PLANT_TREE;
  }
  TEST_ASSERT_TRUE(trees == 0 || (covered > 0 && ct.clump > 0));
  zb_chunk_terrain_release(zb_test_alloc(), &ct);
  /* Running out of memory is reported. */
  TEST_ASSERT_TRUE(land_chunk(45.0, &ct));
  zb_chunk c = ct.chunk;
  zb_chunk_terrain_release(zb_test_alloc(), &ct);
  for (int fail = 0; fail < 6; ++fail) {
    zb_chunk_terrain_init(&ct, c);
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_test_alloc(), &ct, ZB_STAGE_SURFACE));
    zb_failing_allocator fa;
    zb_result r = zb_terrain_generate(&g_terrain, zb_failing_init(&fa, zb_test_alloc(), fail), &ct, ZB_STAGE_ECOLOGY);
    TEST_ASSERT_TRUE(r == ZB_OK || r == ZB_ERR_NOMEM);
    zb_chunk_terrain_release(zb_test_alloc(), &ct);
  }
  /* Without a flora, stage 3 is empty. */
  zb_terrain bare = g_terrain;
  zb_terrain_use_flora(&bare, NULL);
  zb_chunk_terrain_init(&ct, c);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&bare, zb_test_alloc(), &ct, ZB_STAGE_ECOLOGY));
  TEST_ASSERT_EQUAL_UINT32(0, ct.plant_count);
  TEST_ASSERT_EQUAL_UINT8(0, ct.clump);
  zb_chunk_terrain_release(zb_test_alloc(), &ct);
}

/* spec: vegetation: Deterministic plants */
static void test_golden(void) {
  zb_hasher h;
  zb_hasher_init(&h, 0x706c616e7473);
  for (int k = 0; k < 4; ++k) {
    zb_chunk_terrain ct;
    TEST_ASSERT_TRUE(land_chunk(-50.0 + 30.0 * k, &ct));
    zb_hasher_u64(&h, zb_chunk_terrain_hash(&ct));
    zb_chunk_terrain_release(zb_test_alloc(), &ct);
  }
  char hex[17];
  zb_hash_hex(zb_hasher_final(&h), hex);
  printf("plants golden hash: %s\n", hex);
  TEST_ASSERT_EQUAL_STRING("cad77a7c071dd7bf", hex); /* the same on every platform */
}

int main(void) {
  g_species = read_asset("data/flora.lua", &g_species_len);
  g_atlas = read_asset("sprites/flora.lua", &g_atlas_len);
  char err[160];
  if (!g_species || !g_atlas ||
      zb_flora_parse(&g_flora, zb_heap_allocator(), g_species, g_species_len, g_atlas, g_atlas_len, err, sizeof err) !=
          ZB_OK ||
      !zb_planet_init(&g_planet, ZB_PLANET_DEFAULT_RADIUS, err, sizeof err)) {
    fprintf(stderr, "could not read the flora: %s\n", g_species ? err : "files missing");
    return 1;
  }
  zb_terrain_init(&g_terrain, &g_planet, 7, NULL);
  zb_terrain_use_flora(&g_terrain, &g_flora);
  UNITY_BEGIN();
  RUN_TEST(test_registry);
  RUN_TEST(test_registry_errors);
  RUN_TEST(test_suitability);
  RUN_TEST(test_cover);
  RUN_TEST(test_climate_by_latitude);
  RUN_TEST(test_placement);
  RUN_TEST(test_golden);
  int result = UNITY_END();
  free(g_species);
  free(g_atlas);
  return result;
}

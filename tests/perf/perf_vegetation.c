/* SPDX-License-Identifier: MIT */
/* Vegetation budgets (release builds on Linux): placing a chunk's plants
 * (stage 3) and building its billboards. */
#define _POSIX_C_SOURCE 199309L
#include "app/terrain_view.h"
#include "unity.h"
#include "world/dvec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHUNKS 24
#define STAGE_BUDGET_MS 3.0 /* stage 3, so that stages 1-3 stay within 8 ms */
#define BUILD_BUDGET_MS 3.0 /* every plant of a chunk as billboards */

static zb_flora g_flora;
static zb_planet g_planet;
static zb_terrain g_terrain;

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
  if (!f) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *data = malloc((size_t)n + 1);
  *len = fread(data, 1, (size_t)n, f);
  fclose(f);
  return data;
}

/* spec: vegetation: Vegetation budgets */
static void test_plant_budgets(void) {
  double stage[CHUNKS], build[CHUNKS];
  size_t plants = 0;
  int n = 0;
  for (int k = 0; n < CHUNKS && k < 4000; ++k) {
    double lat = (-55.0 + 0.0277 * k) * ZB_DPI / 180.0, lon = 0.37 * k;
    zb_dvec3 d = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
    if (zb_terrain_macro_height(&g_terrain, d) < 50.0) {
      continue;
    }
    zb_chunk_terrain ct;
    zb_chunk_terrain_init(&ct, zb_planet_from_cart(&g_planet, zb_dv_scale(d, g_planet.radius)).chunk);
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_heap_allocator(), &ct, ZB_STAGE_SURFACE));
    double t0 = now_ms();
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_heap_allocator(), &ct, ZB_STAGE_ECOLOGY));
    stage[n] = now_ms() - t0;
    zb_foliage_instance *inst;
    size_t count;
    zb_vec3 center;
    float radius;
    t0 = now_ms();
    TEST_ASSERT_EQUAL_INT(ZB_OK,
                          zb_foliage_build(zb_heap_allocator(), &g_terrain, &ct, 0, &inst, &count, &center, &radius));
    build[n] = now_ms() - t0;
    free(inst);
    plants += ct.plant_count;
    zb_chunk_terrain_release(zb_heap_allocator(), &ct);
    n++;
  }
  TEST_ASSERT_EQUAL_INT(CHUNKS, n);
  qsort(stage, CHUNKS, sizeof(double), cmp);
  qsort(build, CHUNKS, sizeof(double), cmp);
  printf(
      "plants: %zu per chunk; stage 3 median %.2f ms (max %.2f, budget %.1f), billboards median %.2f ms (max "
      "%.2f, budget %.1f)\n",
      plants / CHUNKS, stage[CHUNKS / 2], stage[CHUNKS - 1], STAGE_BUDGET_MS, build[CHUNKS / 2], build[CHUNKS - 1],
      BUILD_BUDGET_MS);
  TEST_ASSERT_TRUE(stage[CHUNKS / 2] <= STAGE_BUDGET_MS);
  TEST_ASSERT_TRUE(build[CHUNKS / 2] <= BUILD_BUDGET_MS);
}

int main(void) {
  size_t sl = 0, al = 0;
  char *species = read_asset("data/flora.lua", &sl), *atlas = read_asset("sprites/flora.lua", &al);
  char err[160];
  if (!species || !atlas ||
      zb_flora_parse(&g_flora, zb_heap_allocator(), species, sl, atlas, al, err, sizeof err) != ZB_OK ||
      !zb_planet_init(&g_planet, ZB_PLANET_DEFAULT_RADIUS, err, sizeof err)) {
    fprintf(stderr, "could not read the flora\n");
    return 1;
  }
  free(species);
  free(atlas);
  zb_terrain_init(&g_terrain, &g_planet, 7, NULL);
  zb_terrain_use_flora(&g_terrain, &g_flora);
  UNITY_BEGIN();
  RUN_TEST(test_plant_budgets);
  return UNITY_END();
}

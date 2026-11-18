/* SPDX-License-Identifier: MIT */
/* Water budgets in a created world (release builds on Linux): generating
 * chunks that rivers cross, and building their water meshes. */
#define _POSIX_C_SOURCE 199309L
#include "app/terrain_view.h"
#include "unity.h"
#include "world/creation.h"
#include "world/water.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHUNKS 24
#define STAGE_BUDGET_MS 8.0 /* stages 1-2 of a river chunk: the whole stage 1-3 budget at most */
#define MESH_BUDGET_MS 2.0  /* a 4 m water mesh */

static zb_world_bundle g_bundle;
static zb_terrain g_terrain;
static zb_planet g_planet;

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

/* spec: water-bodies: Water budgets */
static void test_river_chunks(void) {
  /* Chunks under the middle of river channels, all over the world. */
  zb_chunk chunks[CHUNKS];
  int n = 0;
  zb_river_set s;
  zb_river_set_init(&s, zb_heap_allocator());
  for (int32_t c = 0; c < (int32_t)g_bundle.grid.cells && n < CHUNKS; c += 7) {
    s.count = 0;
    if (!(g_bundle.cell_flags[c] & ZB_CELL_OCEAN) && zb_rivers_add_cell(&g_bundle, c, &s) == ZB_OK && s.count) {
      zb_dvec3 d = s.piece[s.count / 2].a;
      zb_dvec3 cart = {d.x * g_planet.radius, d.y * g_planet.radius, d.z * g_planet.radius};
      chunks[n++] = zb_planet_from_cart(&g_planet, cart).chunk;
    }
  }
  zb_river_set_free(&s);
  TEST_ASSERT_EQUAL_INT(CHUNKS, n);
  double stage[CHUNKS], mesh[CHUNKS];
  for (int i = 0; i < CHUNKS; ++i) {
    zb_chunk_terrain ct;
    zb_chunk_terrain_init(&ct, chunks[i]);
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_heap_allocator(), &ct, ZB_STAGE_COARSE));
    double t0 = now_ms();
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_heap_allocator(), &ct, ZB_STAGE_SURFACE));
    stage[i] = now_ms() - t0;
    zb_mesh_data m;
    t0 = now_ms();
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_water_mesh_build(zb_heap_allocator(), &g_terrain, &ct, 0, &m));
    mesh[i] = now_ms() - t0;
    zb_mesh_free(zb_heap_allocator(), &m);
    zb_chunk_terrain_release(zb_heap_allocator(), &ct);
  }
  qsort(stage, CHUNKS, sizeof(double), cmp);
  qsort(mesh, CHUNKS, sizeof(double), cmp);
  printf(
      "river chunks: stages 1-2 median %.2f ms (max %.2f, budget %.1f), water mesh median %.2f ms (max %.2f, "
      "budget %.1f)\n",
      stage[CHUNKS / 2], stage[CHUNKS - 1], STAGE_BUDGET_MS, mesh[CHUNKS / 2], mesh[CHUNKS - 1], MESH_BUDGET_MS);
  TEST_ASSERT_TRUE(stage[CHUNKS / 2] <= STAGE_BUDGET_MS);
  TEST_ASSERT_TRUE(mesh[CHUNKS / 2] <= MESH_BUDGET_MS);
}

int main(void) {
  zb_world_options o;
  zb_world_default_options(&o);
  o.seed = 42;
  o.radius = 400000.0;
  o.quality = ZB_WORLD_QUICK;
  char err[128];
  zb_world_creation *c = zb_world_creation_create(zb_heap_allocator(), &o, err, sizeof err);
  while (c && !zb_world_creation_advance(c)) {
  }
  if (!c || zb_world_creation_bundle(c, zb_heap_allocator(), &g_bundle) != ZB_OK ||
      !zb_planet_init(&g_planet, o.radius, err, sizeof err)) {
    fprintf(stderr, "could not make the world: %s\n", err);
    return 1;
  }
  zb_world_creation_destroy(c);
  zb_terrain_init(&g_terrain, &g_planet, o.seed, NULL);
  zb_terrain_use_bundle(&g_terrain, &g_bundle);
  UNITY_BEGIN();
  RUN_TEST(test_river_chunks);
  int result = UNITY_END();
  zb_world_bundle_free(&g_bundle);
  return result;
}

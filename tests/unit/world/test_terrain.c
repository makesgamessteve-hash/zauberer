/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "core/hash.h"
#include "core/jobs.h"
#include "support/test_support.h"
#include "support/test_threads.h"
#include "unity.h"
#include "world/noise.h"
#include "world/terrain.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static zb_planet g_planet;
static zb_terrain g_terrain; /* default planet, seed 1 */

static void init_world(void) {
  static bool done = false;
  if (!done) {
    TEST_ASSERT_TRUE(zb_planet_init(&g_planet, ZB_PLANET_DEFAULT_RADIUS, NULL, 0));
    zb_terrain_init(&g_terrain, &g_planet, 1, NULL);
    done = true;
  }
}

static void test_noise(void) {
  double lo = 1e9, hi = -1e9, sum = 0.0;
  for (int i = 0; i < 20000; ++i) {
    double x = i * 0.137, y = i * 0.0731 - 50.0, z = i * 0.0193 + 3.0;
    double n = zb_noise3(7, x, y, z);
    lo = fmin(lo, n);
    hi = fmax(hi, n);
    sum += n;
    /* Continuous: a tiny step changes the value by a tiny amount. */
    TEST_ASSERT_TRUE(fabs(zb_noise3(7, x + 1e-6, y, z) - n) < 1e-4);
  }
  TEST_ASSERT_TRUE(lo >= -1.1 && hi <= 1.1);
  TEST_ASSERT_TRUE(lo < -0.5 && hi > 0.5);
  TEST_ASSERT_TRUE(fabs(sum / 20000.0) < 0.05);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_noise3(3, 5.0, -2.0, 11.0)); /* zero at lattice points */
  TEST_ASSERT_TRUE(zb_noise3(1, 0.3, 0.6, 0.2) != zb_noise3(2, 0.3, 0.6, 0.2));
  TEST_ASSERT_EQUAL_DOUBLE(zb_noise3(9, -1e6 + 0.3, 2e6 + 0.7, -3e5 + 0.1),
                           zb_noise3(9, -1e6 + 0.3, 2e6 + 0.7, -3e5 + 0.1));
  for (int i = 0; i < 2000; ++i) {
    double f = zb_fbm3(5, i * 0.31, i * 0.17, 1.0, 5, 0.5);
    double r = zb_ridged3(5, i * 0.31, i * 0.17, 1.0, 5);
    TEST_ASSERT_TRUE(f >= -1.1 && f <= 1.1);
    TEST_ASSERT_TRUE(r >= 0.0 && r <= 1.0);
  }
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_fbm3(5, 1.0, 2.0, 3.0, 0, 0.5));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_ridged3(5, 1.0, 2.0, 3.0, 0));
}

static zb_dvec3 random_dir(uint64_t *s) {
  double z = (double)(zb_mix64(++*s) >> 11) * (2.0 / 9007199254740992.0) - 1.0;
  double phi = (double)(zb_mix64(++*s) >> 11) * (2.0 * ZB_DPI / 9007199254740992.0);
  double r = sqrt(1.0 - z * z);
  zb_dvec3 d = {r * zb_dcos(phi), r * zb_dsin(phi), z};
  return d;
}

/* spec: terrain-generation: Land fraction */
/* spec: terrain-generation: Ocean and mountains */
static void test_relief(void) {
  init_world();
  uint64_t s = 12345;
  int land = 0, deep = 0, high = 0;
  for (int i = 0; i < 20000; ++i) {
    double h = zb_terrain_height(&g_terrain, random_dir(&s));
    land += h > 0.0;
    deep += h < -2000.0;
    high += h > 1500.0;
  }
  printf("land %.3f, deep %d, high %d\n", land / 20000.0, deep, high);
  TEST_ASSERT_TRUE(land >= 6000 && land <= 8000);
  TEST_ASSERT_TRUE(deep > 0);
  TEST_ASSERT_TRUE(high > 0);
  zb_terrain_params params;
  zb_terrain_default_params(&params);
  params.land_fraction = 0.6;
  zb_terrain wet;
  zb_terrain_init(&wet, &g_planet, 1, &params);
  TEST_ASSERT_TRUE(wet.continent_threshold < g_terrain.continent_threshold);
}

static double sample_u(const zb_chunk_terrain *ct, double step, int i) {
  return -1.0 + ((double)ct->chunk.x + i * step / ZB_CHUNK_SIZE) * g_planet.chunk_uv;
}
static double sample_v(const zb_chunk_terrain *ct, double step, int j) {
  return -1.0 + ((double)ct->chunk.y + j * step / ZB_CHUNK_SIZE) * g_planet.chunk_uv;
}

/* spec: terrain-generation: Stage dependencies */
/* spec: terrain-generation: Stages agree */
static void test_stages(void) {
  init_world();
  const zb_allocator *a = zb_test_alloc();
  const zb_chunk chunks[] = {{0, 6000, 6100}, {2, 100, 12000}, {4, 9000, 300}, {1, 4000, 4000}};
  for (size_t k = 0; k < sizeof chunks / sizeof chunks[0]; ++k) {
    zb_chunk_terrain ct;
    zb_chunk_terrain_init(&ct, chunks[k]);
    TEST_ASSERT_EQUAL_INT(ZB_STAGE_NONE, ct.stage);
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, a, &ct, ZB_STAGE_SURFACE));
    TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, ct.stage);
    TEST_ASSERT_NOT_NULL(ct.height);
    TEST_ASSERT_NOT_NULL(ct.material);
    /* A stage-0 copy for the coarse comparison. */
    zb_chunk_terrain coarse = ct;
    coarse.height = NULL;
    coarse.material = NULL;
    double worst_coarse = 0.0, worst_exact = 0.0;
    for (int y = 0; y < ZB_HEIGHT_RES; y += 3) {
      for (int x = 0; x < ZB_HEIGHT_RES; x += 5) {
        double h = ct.height[y * ZB_HEIGHT_RES + x];
        worst_coarse = fmax(worst_coarse, fabs(h - zb_chunk_height_at(&coarse, (float)x, (float)y)));
        zb_dvec3 d = zb_planet_face_dir(ct.chunk.face, sample_u(&ct, 1.0, x), sample_v(&ct, 1.0, y));
        worst_exact = fmax(worst_exact, fabs(h - zb_terrain_height(&g_terrain, d)));
      }
    }
    printf("chunk %zu: coarse %.2f m, exact %.3f m, %.0f..%.0f m\n", k, worst_coarse, worst_exact, ct.min_height,
           ct.max_height);
    TEST_ASSERT_TRUE(worst_coarse <= 6.0);
    TEST_ASSERT_TRUE(worst_exact <= 0.5);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, ct.height[0], zb_chunk_height_at(&ct, 0.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, ct.height[ZB_HEIGHT_RES * ZB_HEIGHT_RES - 1],
                             zb_chunk_height_at(&ct, 256.0f, 300.0f)); /* clamped */
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, zb_chunk_coarse_height(&coarse, 16, 16),
                             zb_chunk_height_at(&coarse, 256.0f, 256.0f));
    zb_chunk_terrain_release(a, &ct);
    TEST_ASSERT_EQUAL_INT(ZB_STAGE_NONE, ct.stage);
  }
  /* Stage by stage, and asking for an earlier stage is a no-op. */
  zb_chunk_terrain ct;
  zb_chunk_terrain_init(&ct, chunks[0]);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, a, &ct, ZB_STAGE_COARSE));
  TEST_ASSERT_NULL(ct.height);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, a, &ct, ZB_STAGE_TERRAIN));
  TEST_ASSERT_NULL(ct.material);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, a, &ct, ZB_STAGE_COARSE));
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_TERRAIN, ct.stage);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_terrain_generate(&g_terrain, a, &ct, 3));
  zb_chunk_terrain_release(a, &ct);
  zb_chunk_terrain bad;
  zb_chunk_terrain_init(&bad, (zb_chunk){0, g_planet.n, 0});
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_terrain_generate(&g_terrain, a, &bad, 0));
}

/* Height in chunk nb at the planet direction d (nb's local coordinates). */
static float height_in(const zb_chunk_terrain *nb, zb_dvec3 d) {
  double u, v;
  zb_planet_dir_uv(nb->chunk.face, d, &u, &v);
  double lx = ((u + 1.0) / g_planet.chunk_uv - nb->chunk.x) * ZB_CHUNK_SIZE;
  double ly = ((v + 1.0) / g_planet.chunk_uv - nb->chunk.y) * ZB_CHUNK_SIZE;
  TEST_ASSERT_TRUE(lx > -0.01 && lx < 256.01 && ly > -0.01 && ly < 256.01);
  return zb_chunk_height_at(nb, (float)lx, (float)ly);
}

static void check_edge(const zb_allocator *a, zb_chunk c, int dx, int dy) {
  zb_chunk n;
  TEST_ASSERT_TRUE(zb_planet_neighbor(&g_planet, c, dx, dy, &n));
  zb_chunk_terrain ct, nt;
  zb_chunk_terrain_init(&ct, c);
  zb_chunk_terrain_init(&nt, n);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, a, &ct, ZB_STAGE_TERRAIN));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, a, &nt, ZB_STAGE_TERRAIN));
  for (int i = 0; i < ZB_HEIGHT_RES; i += 4) {
    int x = dx > 0 ? 256 : (dx < 0 ? 0 : i), y = dy > 0 ? 256 : (dy < 0 ? 0 : i);
    zb_dvec3 d = zb_planet_face_dir(c.face, sample_u(&ct, 1.0, x), sample_v(&ct, 1.0, y));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ct.height[y * ZB_HEIGHT_RES + x], height_in(&nt, d));
  }
  zb_chunk_terrain_release(a, &ct);
  zb_chunk_terrain_release(a, &nt);
}

/* spec: terrain-generation: Shared edges */
static void test_shared_edges(void) {
  init_world();
  const zb_allocator *a = zb_test_alloc();
  int32_t last = g_planet.n - 1;
  check_edge(a, (zb_chunk){0, 500, 700}, 1, 0);
  check_edge(a, (zb_chunk){0, 500, 700}, 0, -1);
  for (int face = 0; face < ZB_PLANET_FACES; ++face) { /* across every kind of seam */
    check_edge(a, (zb_chunk){face, last, 3000}, 1, 0);
    check_edge(a, (zb_chunk){face, 0, 3000}, -1, 0);
    check_edge(a, (zb_chunk){face, 4000, last}, 0, 1);
    check_edge(a, (zb_chunk){face, 4000, 0}, 0, -1);
  }
  check_edge(a, (zb_chunk){3, last, last}, 1, 0); /* at a cube corner */
}

/* spec: terrain-generation: Material rules */
static void test_material_rules(void) {
  double polar80 = zb_dsin(80.0 * ZB_DPI / 180.0);
  TEST_ASSERT_EQUAL_INT(ZB_MAT_SAND, zb_terrain_material_rule(1.0, 0.05, 0.0, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_ROCK, zb_terrain_material_rule(300.0, 1.0, 0.0, 0.0)); /* 45 deg */
  TEST_ASSERT_EQUAL_INT(ZB_MAT_SOIL, zb_terrain_material_rule(300.0, 0.1, 0.0, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_SNOW, zb_terrain_material_rule(4000.0, 0.2, 0.3, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_ICE, zb_terrain_material_rule(50.0, 0.05, polar80, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_MUD, zb_terrain_material_rule(-100.0, 0.0, 0.0, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_GRAVEL, zb_terrain_material_rule(300.0, 0.65, 0.0, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_ROCK, zb_terrain_material_rule(4000.0, 0.7, 0.0, 0.0));
  TEST_ASSERT_EQUAL_INT(ZB_MAT_SNOW, zb_terrain_material_rule(500.0, 0.1, -polar80, 0.0));
  TEST_ASSERT_EQUAL_STRING("gravel", zb_material_name(ZB_MAT_GRAVEL));
  TEST_ASSERT_EQUAL_STRING("none", zb_material_name(ZB_MAT_COUNT));
  TEST_ASSERT_EQUAL_STRING("none", zb_material_name((zb_material)-1));
}

/* The chunk around the highest (or the coast-nearest) of a set of samples. */
static zb_chunk find_chunk(bool coast) {
  uint64_t s = 99;
  double best = 1e18;
  zb_dvec3 best_dir = {1, 0, 0};
  for (int i = 0; i < 30000; ++i) {
    zb_dvec3 d = random_dir(&s);
    double h = zb_terrain_macro_height(&g_terrain, d);
    double score = coast ? fabs(h - 3.0) : -h; /* coast: where beach turns to soil */
    if (score < best) {
      best = score;
      best_dir = d;
    }
  }
  zb_dvec3 c = {best_dir.x * g_planet.radius, best_dir.y * g_planet.radius, best_dir.z * g_planet.radius};
  return zb_planet_from_cart(&g_planet, c).chunk;
}

/* spec: terrain-generation: Materials in a generated chunk */
static void test_chunk_materials(void) {
  init_world();
  const zb_allocator *a = zb_test_alloc();
  for (int coast = 0; coast < 2; ++coast) {
    zb_chunk_terrain ct;
    zb_chunk_terrain_init(&ct, find_chunk(coast != 0));
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, a, &ct, ZB_STAGE_SURFACE));
    int counts[ZB_MAT_COUNT] = {0};
    for (int i = 0; i < ZB_SURFACE_RES * ZB_SURFACE_RES; ++i) {
      TEST_ASSERT_TRUE(ct.material[i] > ZB_MAT_NONE && ct.material[i] < ZB_MAT_COUNT);
      counts[ct.material[i]]++;
    }
    int kinds = 0;
    for (int m = 0; m < ZB_MAT_COUNT; ++m) {
      kinds += counts[m] > 0;
    }
    printf("%s chunk: %d materials, %.0f..%.0f m\n", coast ? "coastal" : "mountain", kinds, ct.min_height,
           ct.max_height);
    TEST_ASSERT_TRUE(kinds >= 2);
    for (int i = 0; i < ZB_COARSE_RES * ZB_COARSE_RES; ++i) {
      TEST_ASSERT_TRUE(ct.coarse_material[i] > ZB_MAT_NONE);
    }
    zb_chunk_terrain_release(a, &ct);
  }
}

/* spec: terrain-generation: Footprint by stage */
static void test_footprint(void) {
  init_world();
  const zb_allocator *a = zb_test_alloc();
  zb_chunk_terrain ct;
  zb_chunk_terrain_init(&ct, (zb_chunk){5, 77, 88});
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, a, &ct, ZB_STAGE_COARSE));
  TEST_ASSERT_TRUE(zb_chunk_terrain_bytes(&ct) <= 2048);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, a, &ct, ZB_STAGE_TERRAIN));
  TEST_ASSERT_TRUE(zb_chunk_terrain_bytes(&ct) <= 262 * 1024);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, a, &ct, ZB_STAGE_SURFACE));
  TEST_ASSERT_TRUE(zb_chunk_terrain_bytes(&ct) <= 330 * 1024);
  zb_chunk_terrain_release(a, &ct);
  TEST_ASSERT_EQUAL_size_t(sizeof ct, zb_chunk_terrain_bytes(&ct));
  /* Out of memory in either stage leaves the chunk at its previous stage. */
  for (long budget = 0; budget < 4; ++budget) {
    zb_failing_allocator fa;
    const zb_allocator *fail = zb_failing_init(&fa, a, budget);
    zb_chunk_terrain_init(&ct, (zb_chunk){5, 77, 88});
    zb_result r = zb_terrain_generate(&g_terrain, fail, &ct, ZB_STAGE_SURFACE);
    TEST_ASSERT_TRUE(r == ZB_ERR_NOMEM || r == ZB_OK);
    TEST_ASSERT_TRUE(r == ZB_OK || ct.stage < ZB_STAGE_SURFACE);
    zb_chunk_terrain_release(fail, &ct);
  }
}

static const zb_chunk GOLDEN_CHUNKS[] = {{0, 6136, 6136}, {1, 100, 200},   {2, 12271, 0},    {3, 0, 12271},
                                         {4, 5000, 7000}, {5, 3000, 3000}, {0, 12271, 6000}, {2, 6000, 6000}};
#define GOLDEN_COUNT (sizeof GOLDEN_CHUNKS / sizeof GOLDEN_CHUNKS[0])

static uint64_t golden_hash(const zb_terrain *t, const zb_allocator *a) {
  uint64_t h = 0;
  for (size_t i = 0; i < GOLDEN_COUNT; ++i) {
    zb_chunk_terrain ct;
    zb_chunk_terrain_init(&ct, GOLDEN_CHUNKS[i]);
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(t, a, &ct, ZB_STAGE_SURFACE));
    h = zb_hash_combine(h, zb_chunk_terrain_hash(&ct));
    zb_chunk_terrain_release(a, &ct);
  }
  return h;
}

/* Update only when terrain generation changes on purpose (worlds change with it). */
#define GOLDEN "73daf5681c45c1de"

/* spec: terrain-generation: Golden worldgen hashes */
/* spec: terrain-generation: Two peers */
static void test_golden(void) {
  init_world();
  char hex[17];
  zb_hash_hex(golden_hash(&g_terrain, zb_test_alloc()), hex);
  printf("terrain golden hash: %s\n", hex);
  TEST_ASSERT_EQUAL_STRING(GOLDEN, hex);
  zb_planet p2;
  TEST_ASSERT_TRUE(zb_planet_init(&p2, ZB_PLANET_DEFAULT_RADIUS, NULL, 0));
  zb_terrain peer;
  zb_terrain_init(&peer, &p2, 1, NULL);
  TEST_ASSERT_EQUAL_HEX64(golden_hash(&g_terrain, zb_test_alloc()), golden_hash(&peer, zb_test_alloc()));
}

typedef struct gen_job {
  const zb_terrain *t;
  zb_chunk_terrain ct;
  zb_result result;
} gen_job;

static void run_gen(void *arg) {
  gen_job *g = arg;
  g->result = zb_terrain_generate(g->t, zb_heap_allocator(), &g->ct, ZB_STAGE_SURFACE);
}

/* spec: terrain-generation: Same seed, same terrain */
/* spec: terrain-generation: Different seeds differ */
static void test_determinism(void) {
  init_world();
  enum { N = 16 };
  uint64_t first[N];
  for (int i = 0; i < N; ++i) {
    zb_chunk_terrain ct;
    zb_chunk_terrain_init(&ct, (zb_chunk){i % 6, 2000 + i * 311, 9000 - i * 177});
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_test_alloc(), &ct, ZB_STAGE_SURFACE));
    first[i] = zb_chunk_terrain_hash(&ct);
    zb_chunk_terrain_release(zb_test_alloc(), &ct);
  }
  /* Reverse order on worker threads (inline when the platform has none). */
  static gen_job jobs[N];
  zb_jobs *js = zb_jobs_create(zb_heap_allocator(), zb_test_thread_api(), 4);
  TEST_ASSERT_NOT_NULL(js);
  zb_job *handles[N];
  for (int i = N - 1; i >= 0; --i) {
    jobs[i].t = &g_terrain;
    zb_chunk_terrain_init(&jobs[i].ct, (zb_chunk){i % 6, 2000 + i * 311, 9000 - i * 177});
    handles[i] = zb_job_submit(js, run_gen, &jobs[i], ZB_JOB_NORMAL);
    TEST_ASSERT_NOT_NULL(handles[i]);
  }
  for (int i = 0; i < N; ++i) {
    TEST_ASSERT_EQUAL_INT(ZB_JOB_DONE, zb_job_wait(js, handles[i]));
    zb_job_release(js, handles[i]);
    TEST_ASSERT_EQUAL_INT(ZB_OK, jobs[i].result);
    TEST_ASSERT_EQUAL_HEX64(first[i], zb_chunk_terrain_hash(&jobs[i].ct));
    zb_chunk_terrain_release(zb_heap_allocator(), &jobs[i].ct);
  }
  zb_jobs_destroy(js);
  /* Another seed gives another heightfield. */
  zb_terrain other;
  zb_terrain_init(&other, &g_planet, 2, NULL);
  zb_chunk_terrain a1, a2;
  zb_chunk_terrain_init(&a1, (zb_chunk){0, 6000, 6000});
  zb_chunk_terrain_init(&a2, (zb_chunk){0, 6000, 6000});
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_test_alloc(), &a1, ZB_STAGE_TERRAIN));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&other, zb_test_alloc(), &a2, ZB_STAGE_TERRAIN));
  TEST_ASSERT_TRUE(memcmp(a1.height, a2.height, sizeof(float) * ZB_HEIGHT_RES * ZB_HEIGHT_RES) != 0);
  zb_chunk_terrain_release(zb_test_alloc(), &a1);
  zb_chunk_terrain_release(zb_test_alloc(), &a2);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_noise);
  RUN_TEST(test_relief);
  RUN_TEST(test_stages);
  RUN_TEST(test_shared_edges);
  RUN_TEST(test_material_rules);
  RUN_TEST(test_chunk_materials);
  RUN_TEST(test_footprint);
  RUN_TEST(test_golden);
  RUN_TEST(test_determinism);
  return UNITY_END();
}

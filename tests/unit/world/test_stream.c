/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "support/test_support.h"
#include "support/test_threads.h"
#include "unity.h"
#include "world/stream.h"
#include "world/world.h"

#include <math.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static zb_planet g_planet;
static zb_terrain g_terrain;

static void init_world(void) {
  static bool done = false;
  if (!done) {
    TEST_ASSERT_TRUE(zb_planet_init(&g_planet, ZB_PLANET_DEFAULT_RADIUS, NULL, 0));
    zb_terrain_init(&g_terrain, &g_planet, 1, NULL);
    done = true;
  }
}

/* Small rings keep the tests fast: near 512 m, far 1024 m. */
static zb_stream_config small_config(void) {
  zb_stream_config c;
  zb_stream_default_config(&c, ZB_STREAM_DESKTOP);
  c.a = zb_test_alloc();
  c.chunk_alloc = zb_test_alloc();
  c.near_radius = 512.0f;
  c.far_radius = 1024.0f;
  c.far_per_altitude = 0.0f;
  return c;
}

/* A clock that never advances: inline updates run every pending job. */
static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

static zb_stream_focus focus_at(zb_chunk c, float lx, float ly, float alt) {
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos.chunk = c;
  f.pos.lx = lx;
  f.pos.ly = ly;
  f.pos.alt = alt;
  return f;
}

/* Updates until no work is left (sleeping briefly while worker threads run). */
static void settle(zb_stream *s, const zb_stream_focus *f, int count) {
  for (int i = 0; i < 200000; ++i) {
    zb_stream_update(s, f, count);
    zb_stream_stats st;
    zb_stream_get_stats(s, &st);
    if (st.queued == 0 && st.jobs == 0) {
      return;
    }
    if (st.jobs > 0 && zb_test_thread_api()) {
      zb_test_sleep_ms(1);
    }
  }
  TEST_FAIL_MESSAGE("streaming did not settle");
}

/* Moves a position by metres east (approximately, along the face's u axis). */
static zb_stream_focus moved(const zb_stream_focus *f, double east, double north) {
  zb_origin o;
  zb_origin_init(&o, &g_planet, f->pos.chunk);
  zb_vec3 l = zb_origin_to_local(&o, &g_planet, &f->pos);
  l.x += (float)east;
  l.z -= (float)north;
  zb_stream_focus r = *f;
  float alt = f->pos.alt;
  r.pos = zb_origin_to_pos(&o, &g_planet, l);
  r.pos.alt = alt;
  return r;
}

static double centre_distance(zb_chunk a, const zb_planet_pos *p) {
  zb_planet_pos c = zb_planet_chunk_center(a);
  return zb_planet_distance(&g_planet, &c, p);
}

/* spec: chunk-streaming: Settled rings */
static void test_settled_rings(void) {
  init_world();
  zb_stream_focus f = focus_at((zb_chunk){0, 5000, 7000}, 100.0f, 30.0f, 100.0f);
  /* 1. The desktop rings themselves (bookkeeping only: ~7,600 chunks). */
  zb_stream_config c;
  zb_stream_default_config(&c, ZB_STREAM_DESKTOP);
  c.a = zb_test_alloc();
  c.max_jobs = 0;
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  TEST_ASSERT_NOT_NULL(s);
  zb_stream_update(s, &f, 1);
  int near = 0, far = 0;
  for (int32_t y = 7000 - 60; y <= 7000 + 60; ++y) {
    for (int32_t x = 5000 - 60; x <= 5000 + 60; ++x) {
      zb_chunk ch = {0, x, y};
      double d = centre_distance(ch, &f.pos);
      int target = zb_stream_target(s, ch);
      if (d <= 1536.0) {
        TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, target);
        near++;
      } else if (d <= 12400.0) {
        TEST_ASSERT_TRUE(target >= ZB_STAGE_COARSE);
        far++;
      } else if (d > 12400.0 * 1.25 + 400.0) {
        TEST_ASSERT_EQUAL_INT(ZB_STAGE_NONE, target);
      }
    }
  }
  printf("desktop rings: %d near, %d far\n", near, far);
  TEST_ASSERT_TRUE(near > 100 && far > 5000);
  zb_stream_destroy(s);
  /* 2. Generating them: the desktop near ring with a shorter far ring, on workers. */
  c.max_jobs = 8;
  c.far_radius = 2048.0f;
  c.clock_ns = frozen_clock;
  zb_jobs *js = zb_jobs_create(zb_heap_allocator(), zb_test_thread_api(), 4);
  c.jobs = js;
  s = zb_stream_create(&g_terrain, &c);
  settle(s, &f, 1);
  for (int32_t y = 7000 - 10; y <= 7000 + 10; ++y) {
    for (int32_t x = 5000 - 10; x <= 5000 + 10; ++x) {
      zb_chunk ch = {0, x, y};
      double d = centre_distance(ch, &f.pos);
      const zb_chunk_terrain *ct = zb_stream_find(s, ch);
      if (d <= 1536.0) {
        TEST_ASSERT_NOT_NULL(ct);
        TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, ct->stage);
      } else if (d <= 2048.0) {
        TEST_ASSERT_NOT_NULL(ct);
      }
    }
  }
  zb_stream_stats st;
  zb_stream_get_stats(s, &st);
  TEST_ASSERT_TRUE(st.bytes < c.memory_cap);
  zb_stream_destroy(s);
  zb_jobs_destroy(js);
}

/* spec: chunk-streaming: Higher means farther */
static void test_altitude(void) {
  init_world();
  zb_stream_config c;
  zb_stream_default_config(&c, ZB_STREAM_DESKTOP);
  c.a = zb_test_alloc();
  c.max_jobs = 0; /* rings only */
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  zb_stream_focus f = focus_at((zb_chunk){2, 6000, 6000}, 128.0f, 128.0f, 100.0f);
  zb_stream_update(s, &f, 1);
  zb_chunk at16 = {2, 6000, 6000}; /* the first chunk east beyond 16 km */
  while (centre_distance(at16, &f.pos) < 16000.0) {
    at16.x++;
  }
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_NONE, zb_stream_target(s, at16));
  f.pos.alt = 2000.0f;
  zb_stream_update(s, &f, 1);
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_COARSE, zb_stream_target(s, at16));
  zb_stream_stats st;
  zb_stream_get_stats(s, &st);
  TEST_ASSERT_TRUE(st.wanted > 15000);
  /* Capped at the profile's maximum (24 km) however high. */
  f.pos.alt = 100000.0f;
  zb_stream_update(s, &f, 1);
  zb_chunk at27 = at16;
  while (centre_distance(at27, &f.pos) < 24000.0 * 1.25 + 400.0) {
    at27.x++;
  }
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_NONE, zb_stream_target(s, at27));
  zb_stream_destroy(s);
}

static void count_faces(void *ctx, const zb_chunk_terrain *ct, uint32_t version) {
  (void)version;
  int *faces = ctx;
  faces[ct->chunk.face]++;
}

/* spec: chunk-streaming: Around a cube corner */
static void test_corner(void) {
  init_world();
  zb_stream_config c = small_config();
  c.clock_ns = frozen_clock;
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  double k = g_planet.radius / sqrt(3.0);
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos = zb_planet_from_cart(&g_planet, (zb_dvec3){k, k, k});
  settle(s, &f, 1);
  int faces[ZB_PLANET_FACES] = {0};
  zb_stream_each(s, count_faces, faces);
  int touched = 0, total = 0;
  for (int i = 0; i < ZB_PLANET_FACES; ++i) {
    touched += faces[i] > 0;
    total += faces[i];
  }
  TEST_ASSERT_EQUAL_INT(3, touched);
  TEST_ASSERT_TRUE(faces[ZB_FACE_NX] == 0 && faces[ZB_FACE_NY] == 0 && faces[ZB_FACE_NZ] == 0);
  zb_stream_stats st;
  zb_stream_get_stats(s, &st);
  TEST_ASSERT_EQUAL_size_t(st.chunks[0] + st.chunks[1] + st.chunks[2], (size_t)total);
  /* Every neighbour of the corner chunk is inside the near ring. */
  zb_chunk nb[8];
  int n = zb_planet_neighbors(&g_planet, f.pos.chunk, nb);
  TEST_ASSERT_EQUAL_INT(7, n);
  for (int i = 0; i < n; ++i) {
    const zb_chunk_terrain *ct = zb_stream_find(s, nb[i]);
    TEST_ASSERT_NOT_NULL(ct);
    TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, ct->stage);
  }
  zb_stream_destroy(s);
}

/* Records the order in which chunks get loaded. */
typedef struct order_log {
  zb_chunk seen[4096];
  int count;
} order_log;

static bool logged(const order_log *o, zb_chunk c) {
  for (int i = 0; i < o->count; ++i) {
    if (zb_chunk_equal(o->seen[i], c)) {
      return true;
    }
  }
  return false;
}

static void log_new(void *ctx, const zb_chunk_terrain *ct, uint32_t version) {
  (void)version;
  order_log *o = ctx;
  if (o->count < 4096 && !logged(o, ct->chunk)) {
    o->seen[o->count++] = ct->chunk;
  }
}

/* Of the first 20 chunks loaded, how many lie along the local direction (east, north)? */
static int count_ahead(const zb_stream_focus *f, double east, double north) {
  init_world();
  zb_stream_config c = small_config();
  c.clock_ns = NULL; /* one job per update */
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  static order_log log;
  log.count = 0;
  while (log.count < 20) {
    zb_stream_update(s, f, 1);
    zb_stream_each(s, log_new, &log);
  }
  zb_origin o;
  zb_origin_init(&o, &g_planet, f->pos.chunk);
  zb_vec3 me = zb_origin_to_local(&o, &g_planet, &f->pos);
  int ahead = 0;
  for (int i = 0; i < 20; ++i) {
    zb_planet_pos cp = zb_planet_chunk_center(log.seen[i]);
    zb_vec3 l = zb_origin_to_local(&o, &g_planet, &cp);
    double along = (double)(l.x - me.x) * east - (double)(l.z - me.z) * north;
    ahead += along > 0.0;
  }
  zb_stream_destroy(s);
  return ahead;
}

/* spec: chunk-streaming: Ahead first */
/* spec: chunk-streaming: In view first */
static void test_priority(void) {
  zb_stream_focus f = focus_at((zb_chunk){4, 3000, 3000}, 128.0f, 128.0f, 50.0f);
  f.velocity = zb_v3(150.0f, 0.0f, 0.0f);
  int east = count_ahead(&f, 1.0, 0.0);
  f.velocity = zb_v3(0.0f, 0.0f, 0.0f);
  f.view = zb_v3(0.0f, 0.0f, -1.0f); /* looking north */
  int north = count_ahead(&f, 0.0, 1.0);
  printf("ahead: %d/20 east, %d/20 north\n", east, north);
  TEST_ASSERT_TRUE(east > 10); /* most */
  TEST_ASSERT_TRUE(north > 10);
}

/* spec: chunk-streaming: Job limit */
static void test_job_limit(void) {
  init_world();
  zb_stream_config c = small_config();
  c.chunk_alloc = zb_heap_allocator(); /* used by worker threads */
  c.max_jobs = 3;
  zb_jobs *js = zb_jobs_create(zb_heap_allocator(), zb_test_thread_api(), 4);
  c.jobs = js;
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  zb_stream_focus f = focus_at((zb_chunk){1, 800, 900}, 10.0f, 10.0f, 10.0f);
  int peak = 0;
  for (int i = 0; i < 200000; ++i) {
    zb_stream_update(s, &f, 1);
    zb_stream_stats st;
    zb_stream_get_stats(s, &st);
    TEST_ASSERT_TRUE(st.jobs <= 3);
    peak = st.jobs > peak ? st.jobs : peak;
    if (st.queued == 0 && st.jobs == 0) {
      break;
    }
    if (zb_test_thread_api()) {
      zb_test_sleep_ms(1);
    }
  }
  TEST_ASSERT_TRUE(peak >= 1);
  zb_stream_destroy(s);
  zb_jobs_destroy(js);
}

static uint64_t g_fake_ns;
static uint64_t stepping_clock(void *ctx) {
  (void)ctx;
  uint64_t t = g_fake_ns;
  g_fake_ns += 2000000; /* 2 ms per reading: one reading after each chunk */
  return t;
}

/* spec: chunk-streaming: Inline time budget */
static void test_inline_budget(void) {
  init_world();
  zb_stream_config c = small_config();
  c.clock_ns = stepping_clock;
  c.inline_budget_ms = 4.0f;
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  zb_stream_focus f = focus_at((zb_chunk){5, 100, 100}, 0.0f, 0.0f, 0.0f);
  uint64_t before = 0;
  for (int i = 0; i < 10; ++i) {
    zb_stream_update(s, &f, 1);
    zb_stream_stats st;
    zb_stream_get_stats(s, &st);
    TEST_ASSERT_TRUE(st.generated - before <= 3);
    TEST_ASSERT_TRUE(st.generated - before >= 1);
    before = st.generated;
  }
  zb_stream_destroy(s);
}

/* spec: chunk-streaming: Flying along a ring boundary */
static void test_hysteresis(void) {
  init_world();
  zb_stream_config c = small_config();
  c.clock_ns = frozen_clock;
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  zb_stream_focus a = focus_at((zb_chunk){3, 2000, 2000}, 128.0f, 128.0f, 0.0f);
  zb_stream_focus b = moved(&a, 40.0, 0.0);
  settle(s, &a, 1);
  settle(s, &b, 1);
  zb_stream_stats st0;
  zb_stream_get_stats(s, &st0);
  for (int i = 0; i < 200; ++i) {
    zb_stream_update(s, (i & 1) ? &b : &a, 1);
  }
  zb_stream_stats st1;
  zb_stream_get_stats(s, &st1);
  TEST_ASSERT_EQUAL_UINT64(st0.generated, st1.generated);
  TEST_ASSERT_EQUAL_UINT64(0, st1.evicted);
  TEST_ASSERT_TRUE(st1.refreshes > st0.refreshes + 150);
  zb_stream_destroy(s);
}

/* spec: chunk-streaming: Teleport */
static void test_teleport(void) {
  init_world();
  zb_stream_config c = small_config();
  c.clock_ns = NULL;
  c.max_jobs = 8;
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  zb_stream_focus f = focus_at((zb_chunk){0, 6000, 6000}, 0.0f, 0.0f, 0.0f);
  zb_stream_update(s, &f, 1); /* one job ran, its slot was refilled: 8 queued */
  zb_stream_stats st;
  zb_stream_get_stats(s, &st);
  TEST_ASSERT_EQUAL_INT(8, st.jobs);
  TEST_ASSERT_EQUAL_UINT64(1, st.generated);
  zb_stream_focus far = f;
  far.pos.chunk.x += 2000; /* about 500 km */
  zb_stream_update(s, &far, 1);
  zb_stream_get_stats(s, &st);
  TEST_ASSERT_EQUAL_UINT64(8, st.cancelled);
  c.clock_ns = frozen_clock;
  settle(s, &far, 1);
  const zb_chunk_terrain *ct = zb_stream_find(s, far.pos.chunk);
  TEST_ASSERT_NOT_NULL(ct);
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, ct->stage);
  zb_chunk old_neighbour = {0, 6001, 6001};
  TEST_ASSERT_NULL(zb_stream_find(s, old_neighbour)); /* its job was cancelled */
  zb_stream_destroy(s);
}

/* spec: chunk-streaming: Long flight with a small cap */
static void test_memory_cap(void) {
  init_world();
  zb_stream_config c = small_config();
  c.clock_ns = frozen_clock;
  c.near_radius = 256.0f;
  c.far_radius = 2048.0f;
  c.memory_cap = (size_t)5 << 20;
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  zb_stream_focus start = focus_at((zb_chunk){4, 7000, 7000}, 128.0f, 128.0f, 0.0f);
  zb_stream_focus f = start;
  for (int i = 0; i < 200; ++i) { /* 10 km east in 50 m steps */
    f = moved(&f, 50.0, 0.0);
    zb_stream_update(s, &f, 1);
    zb_stream_stats st;
    zb_stream_get_stats(s, &st);
    TEST_ASSERT_TRUE(st.bytes <= c.memory_cap);
  }
  settle(s, &f, 1);
  zb_stream_stats st;
  zb_stream_get_stats(s, &st);
  printf("cap: %.1f MB, evicted %llu, cached %zu\n", (double)st.bytes / 1048576.0, (unsigned long long)st.evicted,
         st.cached);
  TEST_ASSERT_TRUE(st.evicted > 0);
  TEST_ASSERT_NULL(zb_stream_find(s, start.pos.chunk));
  const zb_chunk_terrain *here = zb_stream_find(s, f.pos.chunk);
  TEST_ASSERT_NOT_NULL(here);
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, here->stage);
  zb_stream_destroy(s);
}

/* spec: chunk-streaming: Thirty seconds at full speed */
static void test_endless_flight(void) {
  init_world();
  zb_stream_config c = small_config();
  c.far_radius = 2048.0f;
  c.clock_ns = NULL; /* one generation job per update */
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  zb_stream_focus f = focus_at((zb_chunk){2, 9000, 9000}, 128.0f, 128.0f, 80.0f);
  f.velocity = zb_v3(150.0f, 0.0f, 0.0f);
  f.view = zb_v3(1.0f, 0.0f, 0.0f);
  int misses = 0;
  for (int tick = 0; tick < 30 * 60; ++tick) {
    f = moved(&f, 150.0 / 60.0, 0.0);
    zb_stream_update(s, &f, 1);
    if (tick >= 2 * 60) {
      const zb_chunk_terrain *ct = zb_stream_find(s, f.pos.chunk);
      misses += !ct || ct->stage != ZB_STAGE_SURFACE;
    }
  }
  TEST_ASSERT_EQUAL_INT(0, misses);
  zb_stream_destroy(s);
}

/* spec: chunk-streaming: Profile values */
static void test_profiles(void) {
  zb_stream_config d, w, m;
  zb_stream_default_config(&d, ZB_STREAM_DESKTOP);
  zb_stream_default_config(&w, ZB_STREAM_WEB);
  zb_stream_default_config(&m, ZB_STREAM_MOBILE);
  TEST_ASSERT_EQUAL_FLOAT(1536.0f, d.near_radius);
  TEST_ASSERT_EQUAL_FLOAT(1024.0f, w.near_radius);
  TEST_ASSERT_EQUAL_FLOAT(640.0f, m.near_radius);
  TEST_ASSERT_EQUAL_size_t((size_t)512 << 20, d.memory_cap);
  TEST_ASSERT_EQUAL_size_t((size_t)256 << 20, w.memory_cap);
  TEST_ASSERT_EQUAL_size_t((size_t)128 << 20, m.memory_cap);
  TEST_ASSERT_TRUE(d.max_jobs > w.max_jobs && w.max_jobs > m.max_jobs);
  TEST_ASSERT_TRUE(d.far_radius > w.far_radius && w.far_radius > m.far_radius);
}

/* spec: chunk-streaming: Two focus points */
static void test_two_focus(void) {
  init_world();
  zb_stream_config c = small_config();
  c.clock_ns = frozen_clock;
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  zb_stream_focus f[2] = {focus_at((zb_chunk){1, 5000, 5000}, 0.0f, 0.0f, 0.0f),
                          focus_at((zb_chunk){1, 5120, 5000}, 0.0f, 0.0f, 0.0f)};
  TEST_ASSERT_TRUE(centre_distance(f[1].pos.chunk, &f[0].pos) > 29000.0);
  settle(s, f, 2);
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, zb_stream_find(s, f[0].pos.chunk)->stage);
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, zb_stream_find(s, f[1].pos.chunk)->stage);
  settle(s, f, 1);
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, zb_stream_target(s, f[0].pos.chunk));
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_NONE, zb_stream_target(s, f[1].pos.chunk));
  TEST_ASSERT_NOT_NULL(zb_stream_find(s, f[1].pos.chunk)); /* cached until memory is needed */
  /* More focus points than supported are ignored; none at all wants nothing. */
  zb_stream_focus many[6] = {f[0], f[0], f[0], f[0], f[1], f[1]};
  zb_stream_update(s, many, 6);
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_NONE, zb_stream_target(s, f[1].pos.chunk));
  zb_stream_update(s, NULL, 0);
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_NONE, zb_stream_target(s, f[0].pos.chunk));
  zb_stream_destroy(s);
}

/* spec: chunk-streaming: Same content as direct generation */
static void test_same_content(void) {
  init_world();
  zb_stream_config c = small_config();
  c.chunk_alloc = zb_heap_allocator();
  zb_jobs *js = zb_jobs_create(zb_heap_allocator(), zb_test_thread_api(), 3);
  c.jobs = js;
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  zb_stream_focus f = focus_at((zb_chunk){5, 4321, 1234}, 0.0f, 0.0f, 0.0f);
  settle(s, &f, 1);
  const zb_chunk_terrain *streamed = zb_stream_find(s, f.pos.chunk);
  TEST_ASSERT_NOT_NULL(streamed);
  TEST_ASSERT_TRUE(zb_stream_version(s, f.pos.chunk) > 0);
  TEST_ASSERT_EQUAL_UINT32(0, zb_stream_version(s, (zb_chunk){0, 0, 0}));
  zb_chunk_terrain direct;
  zb_chunk_terrain_init(&direct, f.pos.chunk);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_test_alloc(), &direct, ZB_STAGE_SURFACE));
  TEST_ASSERT_EQUAL_HEX64(zb_chunk_terrain_hash(&direct), zb_chunk_terrain_hash(streamed));
  zb_chunk_terrain_release(zb_test_alloc(), &direct);
  zb_stream_destroy(s); /* with jobs possibly still running elsewhere: none here */
  zb_jobs_destroy(js);
}

/* Destroying a stream with work in flight, and running out of memory. */
static void test_teardown_and_oom(void) {
  init_world();
  zb_stream_config c = small_config();
  c.chunk_alloc = zb_heap_allocator();
  zb_jobs *js = zb_jobs_create(zb_heap_allocator(), zb_test_thread_api(), 2);
  c.jobs = js;
  zb_stream *s = zb_stream_create(&g_terrain, &c);
  zb_stream_focus f = focus_at((zb_chunk){0, 100, 100}, 0.0f, 0.0f, 0.0f);
  zb_stream_update(s, &f, 1);
  zb_stream_destroy(s);
  zb_jobs_destroy(js);
  zb_stream_destroy(NULL);
  /* Bookkeeping and chunk allocations that fail are retried later. */
  for (long budget = 0; budget < 40; budget += 3) {
    zb_failing_allocator fa;
    zb_stream_config fc = small_config();
    fc.a = zb_failing_init(&fa, zb_test_alloc(), budget);
    fc.chunk_alloc = fc.a;
    fc.clock_ns = frozen_clock;
    zb_stream *t = zb_stream_create(&g_terrain, &fc);
    if (t) {
      for (int i = 0; i < 40; ++i) {
        zb_stream_update(t, &f, 1);
      }
      zb_stream_destroy(t);
    }
  }
}

/* spec: chunk-streaming: Streaming from Lua */
static void test_world_object(void) {
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE; /* one job per update: the nearest chunk comes first */
  char err[128];
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(w, err);
  TEST_ASSERT_EQUAL_DOUBLE(ZB_PLANET_DEFAULT_RADIUS, zb_world_planet(w)->radius);
  zb_stream_focus f = focus_at((zb_chunk){0, 300, 300}, 64.0f, 64.0f, 30.0f);
  double analytic = zb_world_ground_height(w, &f.pos);
  TEST_ASSERT_TRUE(zb_world_set_focus(w, 0, &f));
  TEST_ASSERT_FALSE(zb_world_set_focus(w, 4, &f));
  TEST_ASSERT_FALSE(zb_world_set_focus(w, -1, NULL));
  zb_world_update(w);
  const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(w), f.pos.chunk);
  TEST_ASSERT_NOT_NULL(ct);
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, ct->stage);
  TEST_ASSERT_DOUBLE_WITHIN(0.5, analytic, zb_world_ground_height(w, &f.pos));
  TEST_ASSERT_TRUE(zb_world_set_focus(w, 0, NULL));
  zb_world_update(w);
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_NONE, zb_stream_target(zb_world_stream(w), f.pos.chunk));
  TEST_ASSERT_EQUAL_DOUBLE(1, zb_world_terrain(w)->seed);
  zb_world_destroy(w);
  zb_world_destroy(NULL);
  d.radius = 10.0;
  TEST_ASSERT_NULL(zb_world_create(&d, err, sizeof err));
  d.radius = 0.0;
  for (long budget = 0; budget < 3; ++budget) {
    zb_failing_allocator fa;
    d.a = zb_failing_init(&fa, zb_test_alloc(), budget);
    zb_world *fw = zb_world_create(&d, err, sizeof err);
    TEST_ASSERT_NULL(fw);
    TEST_ASSERT_EQUAL_STRING("out of memory", err);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_settled_rings);
  RUN_TEST(test_altitude);
  RUN_TEST(test_corner);
  RUN_TEST(test_priority);
  RUN_TEST(test_job_limit);
  RUN_TEST(test_inline_budget);
  RUN_TEST(test_hysteresis);
  RUN_TEST(test_teleport);
  RUN_TEST(test_memory_cap);
  RUN_TEST(test_endless_flight);
  RUN_TEST(test_profiles);
  RUN_TEST(test_two_focus);
  RUN_TEST(test_same_content);
  RUN_TEST(test_teardown_and_oom);
  RUN_TEST(test_world_object);
  return UNITY_END();
}

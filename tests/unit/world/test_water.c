/* SPDX-License-Identifier: MIT */
#include "core/hash.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/creation.h"
#include "world/dvec.h"
#include "world/terrain.h"
#include "world/water.h"
#include "world/world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

/* One small created world shared by the tests (made with an untracked allocator). */
static zb_world_bundle g_bundle;
static zb_world *g_world;

static const zb_terrain *terrain(void) { return zb_world_terrain(g_world); }

/* The land cell with the most discharge whose channel does not start in a lake. */
static int32_t biggest_river(void) {
  int32_t best = -1;
  for (int32_t c = 0; c < (int32_t)g_bundle.grid.cells; ++c) {
    if (zb_water_cell_has_channel(&g_bundle, c) && !(g_bundle.cell_flags[c] & ZB_CELL_LAKE) &&
        (best < 0 || g_bundle.discharge[c] > g_bundle.discharge[best])) {
      best = c;
    }
  }
  return best;
}

/* spec: water-bodies: Channels follow their discharge */
static void test_geometry(void) {
  double qs[] = {8.0, 100.0, 2000.0}, last_w = 0.0, last_d = 0.0;
  for (int i = 0; i < 3; ++i) {
    double w, d, v;
    zb_river_geometry(qs[i], &w, &d, &v);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9 * qs[i], qs[i], w * d * v);
    TEST_ASSERT_TRUE(w > last_w && d > last_d);
    TEST_ASSERT_TRUE(v > 0.5 && v < 3.0);
    last_w = w;
    last_d = d;
  }
  double w, d, v;
  zb_river_geometry(8.0, &w, &d, &v);
  TEST_ASSERT_TRUE(w > 7.0 && w < 10.0); /* a creek */
  zb_river_geometry(2000.0, &w, &d, &v);
  TEST_ASSERT_TRUE(w > 100.0 && w < 160.0 && d > 4.0 && d < 9.0); /* a great river */
  zb_river_geometry(0.0, &w, &d, &v);                             /* no water: a trickle, not a division by zero */
  TEST_ASSERT_TRUE(w > 0.0 && d > 0.0 && v > 0.0);
}

/* spec: water-bodies: A connected river network */
static void test_network(void) {
  zb_river_set s;
  zb_river_set_init(&s, zb_test_alloc());
  int channels = 0, to_sea = 0, to_lake = 0;
  for (int32_t c = 0; c < (int32_t)g_bundle.grid.cells; ++c) {
    if (!zb_water_cell_has_channel(&g_bundle, c)) {
      s.count = 0;
      TEST_ASSERT_EQUAL_INT(ZB_OK, zb_rivers_add_cell(&g_bundle, c, &s));
      TEST_ASSERT_EQUAL_size_t(0, s.count);
      continue;
    }
    channels++;
    s.count = 0;
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_rivers_add_cell(&g_bundle, c, &s));
    TEST_ASSERT_EQUAL_size_t(ZB_RIVER_PIECES, s.count);
    int32_t r = g_bundle.receiver[c];
    zb_dvec3 from = zb_cubegrid_dir(&g_bundle.grid, c), to = zb_cubegrid_dir(&g_bundle.grid, r);
    /* It starts at the cell centre and ends at the receiver's, so tributaries meet it. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, zb_dv_len(zb_dv_sub(s.piece[0].a, from)));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, zb_dv_len(zb_dv_sub(s.piece[ZB_RIVER_PIECES - 1].b, to)));
    TEST_ASSERT_EQUAL_DOUBLE(zb_water_cell_level(&g_bundle, c), s.piece[0].surface_a);
    for (size_t k = 0; k < s.count; ++k) {
      const zb_river_piece *p = &s.piece[k];
      TEST_ASSERT_TRUE(p->surface_b <= p->surface_a); /* water never climbs */
      if (k + 1 < s.count) {
        TEST_ASSERT_EQUAL_DOUBLE(p->b.x, s.piece[k + 1].a.x);
        TEST_ASSERT_EQUAL_DOUBLE(p->surface_b, s.piece[k + 1].surface_a);
      }
      /* Meanders stay near the line between the centres. */
      double off = zb_dv_len(zb_dv_cross(zb_dv_norm(zb_dv_sub(to, from)), zb_dv_sub(p->b, from)));
      TEST_ASSERT_TRUE(off * g_bundle.options.radius < 0.3 * zb_dv_angle(from, to) * g_bundle.options.radius);
    }
    /* Downstream it reaches the sea or a lake. */
    int32_t at = c;
    for (int guard = 0; guard < 100000 && zb_water_cell_has_channel(&g_bundle, at); ++guard) {
      at = g_bundle.receiver[at];
    }
    to_sea += (g_bundle.cell_flags[at] & ZB_CELL_OCEAN) != 0;
    to_lake += (g_bundle.cell_flags[at] & (ZB_CELL_LAKE | ZB_CELL_SALT_LAKE)) != 0;
    TEST_ASSERT_TRUE_MESSAGE(g_bundle.cell_flags[at] & (ZB_CELL_OCEAN | ZB_CELL_LAKE | ZB_CELL_SALT_LAKE),
                             "a river ends in the sea or a lake");
  }
  printf("channel cells %d, ending in the sea %d, in lakes %d\n", channels, to_sea, to_lake);
  TEST_ASSERT_TRUE(channels > 20);
  TEST_ASSERT_TRUE(to_sea > 0);
  zb_river_set_free(&s);
}

/* spec: water-bodies: Rivers carve their beds */
static void test_carve(void) {
  zb_river_hit h = {0.0, 100.0, 10.0, 2.0, 1.0, {1.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 97.0, zb_river_carve(&h, 130.0)); /* mid-stream: 1.5 x the mean depth */
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 90.0, zb_river_carve(&h, 90.0));  /* never raises the bed */
  h.distance = 5.0;
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 100.0 - 1.5 * 2.0 * 0.75, zb_river_carve(&h, 130.0));
  h.distance = 10.0; /* the water's edge: just above the water */
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 100.3, zb_river_carve(&h, 130.0));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 100.3, zb_river_carve(&h, 80.0)); /* a levee over low land */
  TEST_ASSERT_EQUAL_DOUBLE(-5.0, zb_river_carve(&h, -5.0));         /* but not out of the sea */
  h.distance = 17.0;
  double bank = zb_river_carve(&h, 130.0);
  TEST_ASSERT_TRUE(bank > 100.3 && bank < 130.0);
  h.distance = 24.0; /* past the bank */
  TEST_ASSERT_EQUAL_DOUBLE(130.0, zb_river_carve(&h, 130.0));
  TEST_ASSERT_EQUAL_DOUBLE(24.0, zb_river_reach(&h));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_river_hill_factor(&h));
  h.distance = 24.0 + 75.0 + 30.0;
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, zb_river_hill_factor(&h));
  h.distance = 1000.0;
  TEST_ASSERT_EQUAL_DOUBLE(1.0, zb_river_hill_factor(&h));
}

/* spec: water-bodies: Lakes at their fill level */
static void test_lakes(void) {
  int lakes = 0, outlets = 0;
  for (int32_t c = 0; c < (int32_t)g_bundle.grid.cells; ++c) {
    double level;
    if (g_bundle.cell_flags[c] & ZB_CELL_LAKE) {
      lakes++;
      TEST_ASSERT_TRUE(zb_lake_level(&g_bundle, zb_cubegrid_dir(&g_bundle.grid, c), &level));
      TEST_ASSERT_TRUE(level >= zb_water_cell_level(&g_bundle, c));
      continue;
    }
    if (g_bundle.cell_flags[c] & ZB_CELL_OCEAN) {
      continue;
    }
    /* A lake never spills into its lower outlet cell. */
    for (int k = 0; k < ZB_CUBEGRID_NBRS; ++k) {
      int32_t n = zb_cubegrid_nbr(&g_bundle.grid, c, k);
      if (n >= 0 && (g_bundle.cell_flags[n] & ZB_CELL_LAKE) &&
          (double)g_bundle.height[c] < zb_water_cell_level(&g_bundle, n)) {
        outlets++;
        if (zb_lake_level(&g_bundle, zb_cubegrid_dir(&g_bundle.grid, c), &level)) {
          TEST_ASSERT_TRUE(level <= (double)g_bundle.height[c] || level != zb_water_cell_level(&g_bundle, n));
        }
      }
    }
  }
  printf("lake cells %d, outlet cells %d\n", lakes, outlets);
  TEST_ASSERT_TRUE(lakes > 0);
  /* Far from any lake there is none. */
  double level;
  int32_t sea = -1;
  for (int32_t c = 0; c < (int32_t)g_bundle.grid.cells && sea < 0; ++c) {
    if (g_bundle.height[c] < -2000.0f) {
      sea = c;
    }
  }
  TEST_ASSERT_FALSE(zb_lake_level(&g_bundle, zb_cubegrid_dir(&g_bundle.grid, sea), &level));
}

/* spec: water-bodies: Water at any place */
static void test_water_at(void) {
  const zb_terrain *t = terrain();
  zb_water_sample w;
  /* A river at its cell centre: wet, flowing towards the receiver. */
  int32_t c = biggest_river();
  zb_dvec3 d = zb_cubegrid_dir(&g_bundle.grid, c), down = zb_cubegrid_dir(&g_bundle.grid, g_bundle.receiver[c]);
  zb_terrain_water(t, d, &w);
  TEST_ASSERT_EQUAL_INT(ZB_WATER_RIVER, w.kind);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, zb_water_cell_level(&g_bundle, c), w.surface);
  TEST_ASSERT_TRUE(w.depth > 0.5);
  TEST_ASSERT_TRUE(zb_dv_len(w.flow) > 0.5);
  TEST_ASSERT_TRUE(zb_dv_dot(w.flow, zb_dv_sub(down, d)) > 0.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, zb_dv_dot(w.flow, d)); /* along the ground */
  /* The deepest lake. */
  int32_t lake = -1;
  for (int32_t k = 0; k < (int32_t)g_bundle.grid.cells; ++k) {
    if ((g_bundle.cell_flags[k] & ZB_CELL_LAKE) && (lake < 0 || g_bundle.lake[k] > g_bundle.lake[lake])) {
      lake = k;
    }
  }
  zb_terrain_water(t, zb_cubegrid_dir(&g_bundle.grid, lake), &w);
  TEST_ASSERT_EQUAL_INT(ZB_WATER_LAKE, w.kind);
  /* Flat: the fill levels of a lake's cells differ only by the filling's epsilon. */
  TEST_ASSERT_DOUBLE_WITHIN(0.5, zb_water_cell_level(&g_bundle, lake), w.surface);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_dv_len(w.flow));
  /* The open sea. */
  for (int32_t k = 0; k < (int32_t)g_bundle.grid.cells; ++k) {
    if (g_bundle.height[k] < -2000.0f) {
      zb_terrain_water(t, zb_cubegrid_dir(&g_bundle.grid, k), &w);
      TEST_ASSERT_EQUAL_INT(ZB_WATER_OCEAN, w.kind);
      TEST_ASSERT_EQUAL_DOUBLE(0.0, w.surface);
      TEST_ASSERT_TRUE(w.depth > 1000.0);
      break;
    }
  }
  /* Dry land: a high cell far from rivers and lakes. */
  int32_t dry = -1;
  for (int32_t k = 0; k < (int32_t)g_bundle.grid.cells && dry < 0; ++k) {
    bool near = false;
    for (int n = 0; n < ZB_CUBEGRID_NBRS && !near; ++n) {
      int32_t m = zb_cubegrid_nbr(&g_bundle.grid, k, n);
      near = m >= 0 && (zb_water_cell_has_channel(&g_bundle, m) || (g_bundle.cell_flags[m] & ZB_CELL_LAKE));
    }
    if (!near && g_bundle.height[k] > 300.0f && !zb_water_cell_has_channel(&g_bundle, k) &&
        !(g_bundle.cell_flags[k] & ZB_CELL_LAKE)) {
      dry = k;
    }
  }
  zb_terrain_water(t, zb_cubegrid_dir(&g_bundle.grid, dry), &w);
  TEST_ASSERT_EQUAL_INT(ZB_WATER_NONE, w.kind);
  /* The noise world only has the sea. */
  zb_terrain noise;
  zb_planet p;
  char err[64];
  TEST_ASSERT_TRUE(zb_planet_init(&p, ZB_PLANET_DEFAULT_RADIUS, err, sizeof err));
  zb_terrain_init(&noise, &p, 7, NULL);
  int sea = 0, land = 0;
  for (int k = 0; k < 64; ++k) {
    zb_dvec3 dir = zb_planet_face_dir(k % 6, -0.9 + 0.03 * k, 0.5 - 0.01 * k);
    zb_terrain_water(&noise, dir, &w);
    if (zb_terrain_height(&noise, dir) < 0.0) {
      TEST_ASSERT_EQUAL_INT(ZB_WATER_OCEAN, w.kind);
      sea++;
    } else {
      TEST_ASSERT_EQUAL_INT(ZB_WATER_NONE, w.kind);
      land++;
    }
  }
  TEST_ASSERT_TRUE(sea > 0 && land > 0);
}

/* The channel cell farthest upstream from where its river ends. */
static int32_t longest_river(void) {
  int32_t best = -1;
  int best_len = -1;
  for (int32_t c = 0; c < (int32_t)g_bundle.grid.cells; ++c) {
    int len = 0;
    for (int32_t at = c; zb_water_cell_has_channel(&g_bundle, at) && len < 100000; at = g_bundle.receiver[at]) {
      len++;
    }
    if (len > best_len) {
      best = c;
      best_len = len;
    }
  }
  return best;
}

/* spec: water-bodies: Follow a river to the sea */
static void test_follow_river(void) {
  const zb_terrain *t = terrain();
  zb_river_set s;
  zb_river_set_init(&s, zb_test_alloc());
  int32_t at = longest_river();
  int pieces = 0, wet = 0;
  zb_water_sample w;
  while (zb_water_cell_has_channel(&g_bundle, at)) {
    s.count = 0;
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_rivers_add_cell(&g_bundle, at, &s));
    for (size_t k = 0; k < s.count; ++k) {
      zb_dvec3 mid = zb_dv_norm(zb_dv_add(s.piece[k].a, s.piece[k].b));
      zb_terrain_water(t, mid, &w);
      pieces++;
      wet += w.kind != ZB_WATER_NONE;
    }
    at = g_bundle.receiver[at];
  }
  zb_river_set_free(&s);
  printf("followed %d river pieces, %d wet\n", pieces, wet);
  TEST_ASSERT_TRUE(pieces >= 3 * ZB_RIVER_PIECES);
  TEST_ASSERT_EQUAL_INT(pieces, wet); /* the channel never runs dry */
  /* It ends in the sea or a lake, and the water goes on there. */
  TEST_ASSERT_TRUE(g_bundle.cell_flags[at] & (ZB_CELL_OCEAN | ZB_CELL_LAKE | ZB_CELL_SALT_LAKE));
  zb_terrain_water(t, zb_cubegrid_dir(&g_bundle.grid, at), &w);
  TEST_ASSERT_TRUE(w.kind != ZB_WATER_NONE);
}

/* The chunk that holds a direction. */
static zb_chunk chunk_at(zb_dvec3 d) {
  const zb_planet *p = zb_world_planet(g_world);
  return zb_planet_from_cart(p, zb_dv_scale(d, p->radius)).chunk;
}

/* spec: water-bodies: Rivers carve their beds */
static void test_chunks(void) {
  const zb_terrain *t = terrain();
  int32_t c = biggest_river();
  zb_dvec3 d = zb_cubegrid_dir(&g_bundle.grid, c);
  const zb_planet *p = zb_world_planet(g_world);
  zb_planet_pos pos = zb_planet_from_cart(p, zb_dv_scale(d, p->radius));
  zb_chunk_terrain ct;
  zb_chunk_terrain_init(&ct, pos.chunk);
  /* Stage 0 already shows the river (raised over the uncarved ground). */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(t, zb_test_alloc(), &ct, ZB_STAGE_COARSE));
  uint8_t kind[65 * 65];
  float surface[65 * 65];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_water_grid(t, zb_test_alloc(), &ct, 16, kind, surface));
  int river = 0;
  for (int i = 0; i < 17 * 17; ++i) {
    river += kind[i] == ZB_WATER_RIVER;
  }
  TEST_ASSERT_TRUE(river > 0);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_terrain_water_grid(t, zb_test_alloc(), &ct, 3, kind, surface));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_terrain_water_grid(t, zb_test_alloc(), &ct, 128, kind, surface));
  /* Stage 1: the carved bed agrees with the analytic height, below the water. */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(t, zb_test_alloc(), &ct, ZB_STAGE_SURFACE));
  double analytic = zb_terrain_height(t, d);
  double sampled = (double)zb_chunk_height_at(&ct, pos.lx, pos.ly);
  zb_water_sample w;
  zb_terrain_water(t, d, &w);
  printf("river bed: analytic %.2f, chunk %.2f, surface %.2f\n", analytic, sampled, w.surface);
  TEST_ASSERT_DOUBLE_WITHIN(2.0, analytic, sampled);
  TEST_ASSERT_TRUE(sampled < w.surface);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_water_grid(t, zb_test_alloc(), &ct, 4, kind, surface));
  river = 0;
  for (int i = 0; i < 65 * 65; ++i) {
    river += kind[i] == ZB_WATER_RIVER;
  }
  TEST_ASSERT_TRUE(river > 0);
  /* The bed is mud or gravel, its banks sand. */
  int x = (int)pos.lx, y = (int)pos.ly;
  x = x > 255 ? 255 : x;
  y = y > 255 ? 255 : y;
  int m = ct.material[y * ZB_SURFACE_RES + x];
  TEST_ASSERT_TRUE(m == ZB_MAT_MUD || m == ZB_MAT_GRAVEL);
  int sand = 0;
  for (int i = 0; i < ZB_SURFACE_RES * ZB_SURFACE_RES; ++i) {
    sand += ct.material[i] == ZB_MAT_SAND;
  }
  TEST_ASSERT_TRUE(sand > 0);
  zb_chunk_terrain_release(zb_test_alloc(), &ct);
  /* Running out of memory is reported at every stage. */
  for (int stage = ZB_STAGE_COARSE; stage <= ZB_STAGE_SURFACE; ++stage) {
    for (int fail = 0; fail < 8; ++fail) {
      zb_chunk_terrain_init(&ct, chunk_at(d));
      if (stage > ZB_STAGE_COARSE) {
        TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(t, zb_test_alloc(), &ct, stage - 1));
      }
      zb_failing_allocator fa;
      zb_result r = zb_terrain_generate(t, zb_failing_init(&fa, zb_test_alloc(), fail), &ct, stage);
      TEST_ASSERT_TRUE(r == ZB_OK || r == ZB_ERR_NOMEM);
      zb_chunk_terrain_release(zb_test_alloc(), &ct);
    }
  }
  for (int fail = 0; fail < 4; ++fail) {
    zb_chunk_terrain_init(&ct, chunk_at(d));
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(t, zb_test_alloc(), &ct, ZB_STAGE_COARSE));
    zb_failing_allocator fa;
    zb_result r = zb_terrain_water_grid(t, zb_failing_init(&fa, zb_test_alloc(), fail), &ct, 16, kind, surface);
    TEST_ASSERT_TRUE(r == ZB_OK || r == ZB_ERR_NOMEM);
    zb_chunk_terrain_release(zb_test_alloc(), &ct);
  }
}

/* spec: water-bodies: Deterministic water */
static void test_golden(void) {
  const zb_terrain *t = terrain();
  zb_hasher h;
  zb_hasher_init(&h, 0x7761746572);
  for (int32_t c = 0; c < (int32_t)g_bundle.grid.cells; c += 37) {
    zb_water_sample w;
    zb_terrain_water(t, zb_cubegrid_dir(&g_bundle.grid, c), &w);
    zb_hasher_u64(&h, (uint64_t)w.kind);
    zb_hasher_f64(&h, w.surface);
    zb_hasher_f64(&h, w.depth);
    zb_hasher_f64(&h, w.flow.x + w.flow.y + w.flow.z);
  }
  zb_chunk_terrain ct;
  zb_chunk_terrain_init(&ct, chunk_at(zb_cubegrid_dir(&g_bundle.grid, biggest_river())));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(t, zb_test_alloc(), &ct, ZB_STAGE_SURFACE));
  zb_hasher_u64(&h, zb_chunk_terrain_hash(&ct));
  zb_chunk_terrain_release(zb_test_alloc(), &ct);
  char hex[17];
  zb_hash_hex(zb_hasher_final(&h), hex);
  printf("water golden hash: %s\n", hex);
  TEST_ASSERT_EQUAL_STRING("05bb5d31d2f0bfe7", hex); /* the same on every platform */
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
  if (!c || zb_world_creation_bundle(c, zb_heap_allocator(), &g_bundle) != ZB_OK) {
    fprintf(stderr, "could not make the shared world: %s\n", err);
    return 1;
  }
  zb_world_creation_destroy(c);
  zb_world_desc wd;
  memset(&wd, 0, sizeof wd);
  wd.a = zb_heap_allocator();
  wd.bundle = &g_bundle;
  g_world = zb_world_create(&wd, err, sizeof err);
  if (!g_world) {
    fprintf(stderr, "could not make the world: %s\n", err);
    return 1;
  }
  UNITY_BEGIN();
  RUN_TEST(test_geometry);
  RUN_TEST(test_network);
  RUN_TEST(test_carve);
  RUN_TEST(test_lakes);
  RUN_TEST(test_water_at);
  RUN_TEST(test_follow_river);
  RUN_TEST(test_chunks);
  RUN_TEST(test_golden);
  int result = UNITY_END();
  zb_world_destroy(g_world);
  zb_world_bundle_free(&g_bundle);
  return result;
}

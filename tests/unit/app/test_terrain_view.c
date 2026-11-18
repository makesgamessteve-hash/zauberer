/* SPDX-License-Identifier: MIT */
#include "app/terrain_view.h"
#include "core/dmath.h"
#include "support/test_support.h"
#include "unity.h"

#include <math.h>
#include <stdio.h>
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

static const zb_vec3 COLORS[ZB_MAT_COUNT] = {{0, 0, 0},          {0.3f, 0.5f, 0.2f}, {0.8f, 0.7f, 0.5f},
                                             {0.4f, 0.4f, 0.4f}, {0.6f, 0.6f, 0.5f}, {1, 1, 1},
                                             {0.8f, 0.9f, 1.0f}, {0.3f, 0.2f, 0.1f}};
static const int LAYERS[ZB_MAT_COUNT] = {-1, 0, 4, 5, 6, 7, 8, 9};

static zb_chunk_terrain generate(zb_chunk c, int stage) {
  zb_chunk_terrain ct;
  zb_chunk_terrain_init(&ct, c);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&g_terrain, zb_test_alloc(), &ct, stage));
  return ct;
}

/* spec: retro-rendering: Levels of detail */
/* spec: retro-rendering: No cracks between levels */
static void test_mesh_levels(void) {
  init_world();
  zb_chunk_terrain ct = generate((zb_chunk){4, 9000, 300}, ZB_STAGE_SURFACE);
  TEST_ASSERT_TRUE(ct.min_height > 0.5f); /* land: no water surface */
  for (int lod = 0; lod < ZB_TERRAIN_LODS; ++lod) {
    zb_mesh_data m;
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_mesh_build(zb_test_alloc(), &g_planet, &ct, lod, COLORS, LAYERS, &m));
    size_t n = (size_t)(256 / (4 << lod) + 1);
    TEST_ASSERT_EQUAL_size_t(n * n + 4 * n, m.vertex_count);
    TEST_ASSERT_EQUAL_size_t((n - 1) * (n - 1) * 6 + 4 * (n - 1) * 12, m.index_count);
    /* Grid triangles face up (counter-clockwise seen from above). */
    for (size_t t = 0; t < (n - 1) * (n - 1) * 6; t += 3) {
      const float *a = m.vertices[m.indices[t]].pos, *b = m.vertices[m.indices[t + 1]].pos,
                  *c = m.vertices[m.indices[t + 2]].pos;
      zb_vec3 e1 = zb_v3(b[0] - a[0], b[1] - a[1], b[2] - a[2]), e2 = zb_v3(c[0] - a[0], c[1] - a[1], c[2] - a[2]);
      TEST_ASSERT_TRUE(zb_v3_cross(e1, e2).y > 0.0f);
    }
    /* Skirts hang below every edge vertex. */
    for (size_t k = n * n; k < m.vertex_count; ++k) {
      TEST_ASSERT_TRUE(m.vertices[k].pos[1] < -1.0f + ct.max_height + 1000.0f);
    }
    const zb_vertex *corner = &m.vertices[0], *skirt = &m.vertices[n * n];
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, corner->pos[0], skirt->pos[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, (float)(2 * (4 << lod) + 8), corner->pos[1] - skirt->pos[1]);
    /* Only the nearest level is textured; heights follow the data. */
    float alpha = m.vertices[n + 1].color[3];
    TEST_ASSERT_TRUE(lod == 0 ? alpha < 0.5f : alpha == 1.0f);
    TEST_ASSERT_TRUE(m.radius > 150.0f && m.radius < 400.0f);
    zb_mesh_free(zb_test_alloc(), &m);
  }
  /* Without a heightfield the fine levels fall back to the coarse grid. */
  zb_chunk_terrain coarse = generate((zb_chunk){4, 9000, 300}, ZB_STAGE_COARSE);
  zb_mesh_data m;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_mesh_build(zb_test_alloc(), &g_planet, &coarse, 0, COLORS, LAYERS, &m));
  TEST_ASSERT_EQUAL_size_t(17 * 17 + 4 * 17, m.vertex_count);
  zb_mesh_free(zb_test_alloc(), &m);
  zb_chunk_terrain none;
  zb_chunk_terrain_init(&none, (zb_chunk){0, 1, 1});
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID,
                        zb_terrain_mesh_build(zb_test_alloc(), &g_planet, &none, 0, COLORS, LAYERS, &m));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_terrain_mesh_build(zb_test_alloc(), &g_planet, &ct, 9, COLORS, LAYERS, &m));
  zb_failing_allocator fa;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_terrain_mesh_build(zb_failing_init(&fa, zb_test_alloc(), 1), &g_planet, &ct, 0,
                                                            COLORS, LAYERS, &m));
  zb_chunk_terrain_release(zb_test_alloc(), &ct);
}

/* spec: retro-rendering: Water at sea level */
static void test_water(void) {
  init_world();
  zb_chunk_terrain ct = generate((zb_chunk){1, 100, 200}, ZB_STAGE_TERRAIN);
  TEST_ASSERT_TRUE(ct.max_height < 0.0f); /* deep sea */
  zb_mesh_data m;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_mesh_build(zb_test_alloc(), &g_planet, &ct, 2, COLORS, LAYERS, &m));
  size_t grid = 17 * 17 + 4 * 17;
  TEST_ASSERT_EQUAL_size_t(grid + 25, m.vertex_count);
  zb_origin frame;
  zb_origin_init(&frame, &g_planet, ct.chunk);
  for (size_t k = grid; k < m.vertex_count; ++k) {
    zb_vec3 p = zb_v3(m.vertices[k].pos[0], m.vertices[k].pos[1], m.vertices[k].pos[2]);
    zb_planet_pos pos = zb_planet_from_cart(&g_planet, zb_origin_to_cart(&frame, p));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, pos.alt); /* exactly at sea level */
    TEST_ASSERT_TRUE(m.vertices[k].pos[1] > ct.max_height);
  }
  zb_mesh_free(zb_test_alloc(), &m);
  zb_chunk_terrain_release(zb_test_alloc(), &ct);
}

/* spec: retro-rendering: Curvature */
static void test_curvature(void) {
  init_world();
  zb_origin origin, far;
  zb_origin_init(&origin, &g_planet, (zb_chunk){4, 5000, 5000});
  zb_origin_init(&far, &g_planet, (zb_chunk){4, 5000, 5083}); /* about 20 km along the face */
  zb_mat4 t = zb_origin_transform(&far, &origin);
  zb_vec3 c = zb_m4_transform_point(t, zb_v3(0, 0, 0));
  float d = sqrtf(c.x * c.x + c.z * c.z);
  TEST_ASSERT_TRUE(d > 19000.0f && d < 22000.0f);
  /* The far chunk's centre sits d^2 / 2R below the origin's horizontal plane. */
  TEST_ASSERT_FLOAT_WITHIN(1.0f, -(d * d) / (2.0f * (float)g_planet.radius), c.y);
  /* And its up direction is tilted by d / R. */
  zb_vec3 up = zb_v3_normalize(zb_v3_sub(zb_m4_transform_point(t, zb_v3(0, 1, 0)), c));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, cosf(d / (float)g_planet.radius), up.y);
}

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

static float eye_distance(const zb_world *w, zb_chunk c, zb_vec3 eye) {
  const zb_planet *p = zb_world_planet(w);
  zb_dvec3 d = zb_planet_chunk_dir(p, c);
  zb_dvec3 cart = {d.x * p->radius, d.y * p->radius, d.z * p->radius};
  return zb_v3_length(zb_v3_sub(zb_origin_from_cart(zb_world_origin(w), cart), eye));
}

/* spec: retro-rendering: Mesh build budget */
/* spec: retro-rendering: Origin follows the player */
static void test_view(void) {
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  char err[128];
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(w, err);
  zb_stream_set_radii(zb_world_stream(w), 600.0f, 2500.0f);
  zb_render *r = zb_render_create(zb_test_alloc(), NULL, zb_render_null_backend(zb_test_alloc()));
  zb_terrain_view_config cfg;
  zb_terrain_view_default_config(&cfg);
  cfg.fine_builds = 2;
  cfg.coarse_builds = 10;
  zb_terrain_view *tv = zb_terrain_view_create(zb_test_alloc(), r, NULL, NULL, &cfg);
  TEST_ASSERT_NOT_NULL(tv);
  TEST_ASSERT_EQUAL_INT(0, zb_terrain_view_lod(tv, 300.0f));
  TEST_ASSERT_EQUAL_INT(1, zb_terrain_view_lod(tv, 1000.0f));
  TEST_ASSERT_EQUAL_INT(2, zb_terrain_view_lod(tv, 3000.0f));
  TEST_ASSERT_EQUAL_INT(3, zb_terrain_view_lod(tv, 6000.0f));
  TEST_ASSERT_EQUAL_INT(4, zb_terrain_view_lod(tv, 10000.0f));
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos.chunk = (zb_chunk){0, 6000, 6000};
  f.pos.lx = f.pos.ly = 128.0f;
  f.pos.alt = 300.0f;
  zb_world_set_focus(w, 0, &f);
  zb_world_set_origin(w, f.pos.chunk);
  zb_vec3 eye = zb_origin_to_local(zb_world_origin(w), zb_world_planet(w), &f.pos);
  zb_terrain_view_stats st;
  size_t loaded = 0;
  for (int frame = 0; frame < 400; ++frame) {
    zb_world_update(w);
    zb_render_begin(r, 640, 360);
    zb_render_set_camera(r, eye, zb_v3(eye.x + 100, eye.y - 30, eye.z), zb_v3(0, 1, 0), 60, 1, 20000);
    zb_terrain_view_draw(tv, w, eye);
    zb_render_end(r);
    zb_terrain_view_get_stats(tv, &st);
    TEST_ASSERT_TRUE(st.built <= 12);
    zb_stream_stats ss;
    zb_stream_get_stats(zb_world_stream(w), &ss);
    loaded = ss.wanted; /* settled: every chunk in the rings is loaded */
    if (ss.queued == 0 && ss.jobs == 0 && st.built == 0 && st.meshes == loaded) {
      break;
    }
    if (frame == 399) {
      printf("not settled: loaded %zu meshes %zu queued %zu jobs %d built %zu\n", loaded, st.meshes, ss.queued, ss.jobs,
             st.built);
    }
  }
  TEST_ASSERT_EQUAL_size_t(loaded, st.meshes);
  /* The renderer's own placeholder cube is the one extra mesh. */
  TEST_ASSERT_EQUAL_size_t(st.meshes + 1, zb_render_null_meshes(zb_render_backend_of(r)));
  TEST_ASSERT_TRUE(st.per_lod[0] > 0 && st.per_lod[1] > 0 && st.per_lod[2] > 0);
  TEST_ASSERT_TRUE(zb_render_get_stats(r)->terrain_items > 0);
  TEST_ASSERT_TRUE(zb_render_get_stats(r)->culled > 0); /* chunks behind the camera */
  /* The nearest chunk got the finest level. */
  TEST_ASSERT_TRUE(eye_distance(w, f.pos.chunk, eye) < 600.0f);
  /* Fly 3 km: the origin follows, meshes are re-placed and the far ones dropped. */
  uint32_t version = zb_world_origin_version(w);
  f.pos.chunk.x += 12;
  zb_world_set_focus(w, 0, &f);
  zb_world_update(w);
  TEST_ASSERT_TRUE(zb_world_origin_version(w) > version);
  TEST_ASSERT_TRUE(zb_chunk_equal(zb_world_origin(w)->chunk, f.pos.chunk));
  eye = zb_origin_to_local(zb_world_origin(w), zb_world_planet(w), &f.pos);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 300.0f, eye.y); /* the camera is above the new origin */
  for (int frame = 0; frame < 400; ++frame) {
    zb_world_update(w);
    zb_render_begin(r, 640, 360);
    zb_terrain_view_draw(tv, w, eye);
    zb_render_end(r);
  }
  zb_terrain_view_get_stats(tv, &st);
  zb_stream_stats ss;
  zb_stream_get_stats(zb_world_stream(w), &ss);
  TEST_ASSERT_EQUAL_size_t(ss.wanted, st.meshes); /* chunks left behind are cached, not drawn */
  TEST_ASSERT_TRUE(ss.cached > 0);
  zb_terrain_view_destroy(tv);
  TEST_ASSERT_EQUAL_size_t(1, zb_render_null_meshes(zb_render_backend_of(r)));
  zb_terrain_view_destroy(NULL);
  zb_render_destroy(r);
  zb_world_destroy(w);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mesh_levels);
  RUN_TEST(test_water);
  RUN_TEST(test_curvature);
  RUN_TEST(test_view);
  return UNITY_END();
}

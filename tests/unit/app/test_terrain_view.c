/* SPDX-License-Identifier: MIT */
#include "app/terrain_view.h"
#include "core/dmath.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/dvec.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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
  /* The land has no water in it; the water is a mesh of its own. */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_mesh_build(zb_test_alloc(), &g_planet, &ct, 2, COLORS, LAYERS, &m));
  TEST_ASSERT_EQUAL_size_t(17 * 17 + 4 * 17, m.vertex_count);
  zb_mesh_free(zb_test_alloc(), &m);
  zb_origin frame;
  zb_origin_init(&frame, &g_planet, ct.chunk);
  for (int lod = 0; lod <= 2; lod += 2) {
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_water_mesh_build(zb_test_alloc(), &g_terrain, &ct, lod, &m));
    size_t n = lod == 0 ? 65 : 17;
    TEST_ASSERT_EQUAL_size_t(n * n, m.vertex_count);
    TEST_ASSERT_EQUAL_size_t((n - 1) * (n - 1) * 6, m.index_count); /* all of it is sea */
    for (size_t k = 0; k < m.vertex_count; ++k) {
      zb_vec3 p = zb_v3(m.vertices[k].pos[0], m.vertices[k].pos[1], m.vertices[k].pos[2]);
      zb_planet_pos pos = zb_planet_from_cart(&g_planet, zb_origin_to_cart(&frame, p));
      TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, pos.alt); /* exactly at sea level */
      TEST_ASSERT_TRUE(m.vertices[k].pos[1] > ct.max_height);
      TEST_ASSERT_TRUE(m.vertices[k].color[3] > 0.5f && m.vertices[k].color[3] < 1.0f); /* translucent */
      TEST_ASSERT_TRUE(m.vertices[k].normal[1] > 0.99f);
    }
    zb_mesh_free(zb_test_alloc(), &m);
  }
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_water_mesh_build(zb_test_alloc(), &g_terrain, &ct, 7, &m));
  zb_failing_allocator fa;
  for (int fail = 0; fail < 6; ++fail) {
    zb_result r = zb_water_mesh_build(zb_failing_init(&fa, zb_test_alloc(), fail), &g_terrain, &ct, 0, &m);
    TEST_ASSERT_TRUE(r == ZB_OK || r == ZB_ERR_NOMEM);
    zb_mesh_free(zb_test_alloc(), &m);
  }
  zb_chunk_terrain_release(zb_test_alloc(), &ct);
  /* Dry land: an empty water mesh. */
  ct = generate((zb_chunk){0, 1000, 1000}, ZB_STAGE_COARSE);
  if (ct.min_height > 1.0f) {
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_water_mesh_build(zb_test_alloc(), &g_terrain, &ct, 2, &m));
    TEST_ASSERT_EQUAL_size_t(0, m.vertex_count);
  }
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
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_SURFACE, zb_stream_target(zb_world_stream(w), f.pos.chunk)); /* no flora */
  /* Water meshes over the sea, and the renderer's own placeholder cube. */
  TEST_ASSERT_TRUE(st.water_meshes > 0 && st.water == st.water_meshes);
  TEST_ASSERT_TRUE(zb_render_get_stats(r)->water_items > 0);
  TEST_ASSERT_EQUAL_size_t(st.meshes + st.water_meshes + 1, zb_render_null_meshes(zb_render_backend_of(r)));
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

/* ---- Plants ---------------------------------------------------------------------------- */

static char *read_asset(const char *rel, size_t *len) {
  char path[1024];
  snprintf(path, sizeof path, "%s/assets/base/%s", getenv("ZB_SOURCE_DIR") ? getenv("ZB_SOURCE_DIR") : ".", rel);
  FILE *f = fopen(path, "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *data = malloc((size_t)n + 1);
  *len = fread(data, 1, (size_t)n, f);
  fclose(f);
  return data;
}

static zb_flora g_flora;

static void load_flora(void) {
  static bool done = false;
  if (!done) {
    size_t sl, al;
    char *species = read_asset("data/flora.lua", &sl), *atlas = read_asset("sprites/flora.lua", &al);
    char err[160];
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        ZB_OK, zb_flora_parse(&g_flora, zb_heap_allocator(), species, sl, atlas, al, err, sizeof err), err);
    free(species);
    free(atlas);
    done = true;
  }
}

/* A forested land chunk of the noise world around 45 degrees north. */
static zb_chunk forest_chunk(const zb_terrain *t, const zb_planet *p) {
  zb_chunk best = {0, 0, 0};
  int most = -1;
  for (int k = 0; k < 120; ++k) {
    double lat = (40.0 + 0.1 * k) * ZB_DPI / 180.0, lon = 0.07 * k;
    zb_dvec3 d = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
    if (zb_terrain_macro_height(t, d) < 40.0) {
      continue;
    }
    zb_chunk_terrain ct;
    zb_chunk_terrain_init(&ct, zb_planet_from_cart(p, zb_dv_scale(d, p->radius)).chunk);
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(t, zb_test_alloc(), &ct, ZB_STAGE_COARSE));
    int cover = 0;
    for (int i = 0; i < ZB_CANOPY_RES * ZB_CANOPY_RES; ++i) {
      cover += ct.canopy[i];
    }
    if (cover > most) {
      most = cover;
      best = ct.chunk;
    }
    zb_chunk_terrain_release(zb_test_alloc(), &ct);
  }
  TEST_ASSERT_TRUE(most > 0);
  return best;
}

/* spec: vegetation: Four levels of forest */
static void test_foliage_bands(void) {
  init_world();
  load_flora();
  zb_terrain t = g_terrain;
  zb_terrain_use_flora(&t, &g_flora);
  zb_chunk_terrain ct;
  zb_chunk_terrain_init(&ct, forest_chunk(&t, &g_planet));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_generate(&t, zb_test_alloc(), &ct, ZB_STAGE_ECOLOGY));
  TEST_ASSERT_TRUE(ct.plant_count > 0 && ct.clump > 0);
  size_t ground = 0, canopy = 0;
  for (uint32_t i = 0; i < ct.plant_count; ++i) {
    ground += ct.plants[i].layer == ZB_PLANT_GROUND;
  }
  for (int i = 0; i < ZB_CANOPY_RES * ZB_CANOPY_RES; ++i) {
    canopy += ct.canopy[i] >= 39; /* 0.15 */
  }
  size_t expect[4] = {ct.plant_count, ct.plant_count - ground, canopy, 0};
  for (int band = 0; band < 4; ++band) {
    zb_foliage_instance *inst;
    size_t n;
    zb_vec3 center;
    float radius;
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_foliage_build(zb_test_alloc(), &t, &ct, band, &inst, &n, &center, &radius));
    TEST_ASSERT_EQUAL_size_t(expect[band], n);
    int mirrored = 0;
    for (size_t i = 0; i < n; ++i) {
      TEST_ASSERT_TRUE(inst[i].size[0] > 0.0f && inst[i].size[1] > 0.0f);
      TEST_ASSERT_TRUE(inst[i].uv[1] < inst[i].uv[3]);
      for (int k = 0; k < 4; ++k) {
        TEST_ASSERT_TRUE(inst[i].uv[k] > 0.0f && inst[i].uv[k] < 1.0f);
      }
      mirrored += inst[i].uv[0] > inst[i].uv[2];
      TEST_ASSERT_TRUE(inst[i].tint[3] >= 0.0f && inst[i].tint[3] < (float)g_flora.count);
      zb_vec3 p = zb_v3(inst[i].pos[0], inst[i].pos[1], inst[i].pos[2]);
      TEST_ASSERT_TRUE(zb_v3_length(zb_v3_sub(p, center)) <= radius);
    }
    TEST_ASSERT_TRUE(n == 0 || mirrored > 0);
    zb_free(zb_test_alloc(), inst);
  }
  /* Far away the canopy raises the terrain mesh and colours it. */
  zb_mesh_data m;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_terrain_mesh_build(zb_test_alloc(), &g_planet, &ct, 3, COLORS, LAYERS, &m));
  int green = 0;
  for (size_t k = 0; k < 9 * 9; ++k) {
    green += m.vertices[k].color[1] > m.vertices[k].color[0] * 1.5f;
  }
  TEST_ASSERT_TRUE(green > 0);
  zb_mesh_free(zb_test_alloc(), &m);
  zb_failing_allocator fa;
  zb_foliage_instance *inst;
  size_t n;
  zb_vec3 center;
  float radius;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_foliage_build(zb_failing_init(&fa, zb_test_alloc(), 0), &t, &ct, 0, &inst, &n,
                                                       &center, &radius));
  /* No flora: no plants. */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_foliage_build(zb_test_alloc(), &g_terrain, &ct, 0, &inst, &n, &center, &radius));
  TEST_ASSERT_EQUAL_size_t(0, n);
  zb_chunk_terrain_release(zb_test_alloc(), &ct);
}

/* spec: vegetation: Four levels of forest */
/* spec: vegetation: Seasons colour the plants */
/* spec: vegetation: Plants on every chunk near the player */
static void test_foliage_view(void) {
  init_world();
  load_flora();
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  d.flora = &g_flora;
  char err[128];
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(w, err);
  zb_render *r = zb_render_create(zb_test_alloc(), NULL, zb_render_null_backend(zb_test_alloc()));
  zb_terrain_view_config cfg;
  zb_terrain_view_default_config(&cfg);
  cfg.fine_builds = 8;
  zb_terrain_view *tv = zb_terrain_view_create(zb_test_alloc(), r, NULL, NULL, &cfg);
  TEST_ASSERT_EQUAL_INT(0, zb_terrain_view_foliage_band(tv, 100.0f));
  TEST_ASSERT_EQUAL_INT(1, zb_terrain_view_foliage_band(tv, 500.0f));
  TEST_ASSERT_EQUAL_INT(2, zb_terrain_view_foliage_band(tv, 2000.0f));
  TEST_ASSERT_EQUAL_INT(3, zb_terrain_view_foliage_band(tv, 9000.0f));
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos.chunk = forest_chunk(zb_world_terrain(w), zb_world_planet(w));
  f.pos.lx = f.pos.ly = 128.0f;
  f.pos.alt = 200.0f;
  zb_world_set_focus(w, 0, &f);
  zb_world_set_origin(w, f.pos.chunk);
  zb_vec3 eye = zb_origin_to_local(zb_world_origin(w), zb_world_planet(w), &f.pos);
  zb_terrain_view_set_season(tv, 2.0f); /* autumn */
  zb_terrain_view_stats st;
  for (int frame = 0; frame < 600; ++frame) {
    zb_world_update(w);
    zb_render_begin(r, 640, 360);
    zb_render_set_camera(r, eye, zb_v3(eye.x + 100, eye.y - 60, eye.z), zb_v3(0, 1, 0), 60, 1, 20000);
    zb_terrain_view_draw(tv, w, eye);
    zb_render_end(r);
    zb_terrain_view_get_stats(tv, &st);
    zb_stream_stats ss;
    zb_stream_get_stats(zb_world_stream(w), &ss);
    if (ss.queued == 0 && ss.jobs == 0 && st.built == 0 && st.foliage_per_band[0] > 0 && frame > 50) {
      break;
    }
  }
  printf("plants: %zu chunks (%zu near, %zu mid, %zu far), %zu billboards\n", st.foliage, st.foliage_per_band[0],
         st.foliage_per_band[1], st.foliage_per_band[2], st.plants);
  TEST_ASSERT_TRUE(st.foliage_per_band[0] > 0 && st.foliage_per_band[2] > 0);
  /* The chunk under the focus has its plants; those far away only their canopy. */
  TEST_ASSERT_EQUAL_INT(ZB_STAGE_ECOLOGY, zb_stream_target(zb_world_stream(w), f.pos.chunk));
  zb_chunk far = f.pos.chunk;
  far.x += 6;
  TEST_ASSERT_TRUE(zb_stream_target(zb_world_stream(w), far) < ZB_STAGE_ECOLOGY);
  TEST_ASSERT_TRUE(st.plants > 100);
  TEST_ASSERT_EQUAL_size_t(st.foliage, zb_render_null_foliage(zb_render_backend_of(r)));
  /* The season's colours reach the frame. */
  const zb_frame *fr = zb_render_null_last_frame(zb_render_backend_of(r));
  int oak = zb_flora_find(&g_flora, "oak");
  TEST_ASSERT_EQUAL_FLOAT(g_flora.species[oak].seasons[2][0], fr->foliage_tints[oak][0]);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, fr->foliage_tints[ZB_FOLIAGE_TINTS - 1][0]); /* beyond the species */
  zb_terrain_view_set_season(tv, 3.5f);                                      /* half-way from winter to spring */
  zb_render_begin(r, 640, 360);
  zb_terrain_view_draw(tv, w, eye);
  zb_render_end(r);
  fr = zb_render_null_last_frame(zb_render_backend_of(r));
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f * (g_flora.species[oak].seasons[3][1] + g_flora.species[oak].seasons[0][1]),
                           fr->foliage_tints[oak][1]);
  zb_terrain_view_destroy(tv);
  TEST_ASSERT_EQUAL_size_t(0, zb_render_null_foliage(zb_render_backend_of(r)));
  zb_render_destroy(r);
  zb_world_destroy(w);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mesh_levels);
  RUN_TEST(test_water);
  RUN_TEST(test_curvature);
  RUN_TEST(test_view);
  RUN_TEST(test_foliage_bands);
  RUN_TEST(test_foliage_view);
  return UNITY_END();
}

/* SPDX-License-Identifier: MIT */
#include "app/water_view.h"
#include "core/dmath.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <string.h>

ZB_TEST_LEAK_CHECKED

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

/* spec: shallow-water: Water from Lua */
/* spec: shallow-water: Water for magic */
static void test_standing_water(void) {
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  char err[128];
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(w, err);
  const zb_planet *p = zb_world_planet(w);
  double lat = 46.0 * ZB_DPI / 180.0, lon = 0.3;
  zb_dvec3 dir = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos = zb_planet_from_cart(p, zb_dv_scale(dir, p->radius));
  f.pos.lx = f.pos.ly = 128.0f;
  zb_world_set_focus(w, 0, &f);
  zb_world_set_origin(w, f.pos.chunk);
  for (int i = 0; i < 2000; ++i) {
    zb_world_update(w);
    const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(w), f.pos.chunk);
    if (ct && ct->stage >= ZB_STAGE_SURFACE) {
      break;
    }
  }
  zb_surface_water *water = zb_surface_water_create(zb_test_alloc(), w, 1.0);
  zb_surface_water_update(water, &f.pos, 1, 0.0);
  zb_render *r = zb_render_create(zb_test_alloc(), NULL, zb_render_null_backend(zb_test_alloc()));
  zb_water_view *v = zb_water_view_create(zb_test_alloc(), r);
  /* Dry: nothing to draw, an empty mesh. */
  zb_render_begin(r, 640, 360);
  TEST_ASSERT_EQUAL_INT(0, zb_water_view_draw(v, water, w, 0.0));
  zb_render_end(r);
  zb_water_chunk_view c;
  TEST_ASSERT_EQUAL_INT(1, zb_surface_water_chunk_count(water));
  zb_surface_water_chunk(water, 0, &c);
  zb_mesh_data md;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_standing_water_mesh(zb_test_alloc(), p, &c, &md));
  TEST_ASSERT_EQUAL_size_t(0, md.index_count);
  /* Water poured: a mesh of quads at the water's surface. */
  zb_surface_water_add(water, &f.pos, 6.0, 300.0);
  zb_surface_water_chunk(water, 0, &c);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_standing_water_mesh(zb_test_alloc(), p, &c, &md));
  TEST_ASSERT_TRUE(md.index_count > 0 && md.index_count % 6 == 0);
  TEST_ASSERT_TRUE(md.vertices[0].color[3] > 0.3f);
  zb_mesh_free(zb_test_alloc(), &md);
  /* Frozen: ice, pale and nearly opaque. */
  TEST_ASSERT_TRUE(zb_surface_water_freeze(water, &f.pos, 4.0, 60.0) > 0);
  zb_surface_water_chunk(water, 0, &c);
  TEST_ASSERT_NOT_NULL(c.ice);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_standing_water_mesh(zb_test_alloc(), p, &c, &md));
  bool icy = false;
  for (size_t k = 0; k < md.vertex_count; ++k) {
    icy = icy || (md.vertices[k].color[3] > 0.9f && md.vertices[k].color[0] > 0.7f);
  }
  TEST_ASSERT_TRUE(icy);
  zb_mesh_free(zb_test_alloc(), &md);
  TEST_ASSERT_TRUE(zb_surface_water_thaw(water, &f.pos, 4.0) > 0);
  for (int k = 0; k < 4; ++k) {
    zb_surface_water_update(water, &f.pos, 1, 0.25);
    zb_render_begin(r, 640, 360);
    TEST_ASSERT_EQUAL_INT(1, zb_water_view_draw(v, water, w, 1.0 + 0.25 * k)); /* rebuilt: half a second on */
    zb_render_end(r);
  }
  /* The chunk leaves the simulation: its mesh goes. */
  zb_surface_water_set_world(water, w);
  zb_render_begin(r, 640, 360);
  TEST_ASSERT_EQUAL_INT(0, zb_water_view_draw(v, water, w, 2.0));
  TEST_ASSERT_EQUAL_INT(0, zb_water_view_draw(v, NULL, w, 3.0));
  zb_render_end(r);
  zb_water_view_destroy(v);
  zb_water_view_destroy(NULL);
  zb_render_destroy(r);
  zb_surface_water_destroy(water);
  zb_world_destroy(w);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_standing_water);
  return UNITY_END();
}

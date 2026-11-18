/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "sim/bodies.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/dvec.h"
#include "world/stream.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static zb_materials *g_materials;

/* The base game's materials, once for every test. */
static void load_materials(void) {
  if (g_materials) {
    return;
  }
  char path[512];
  snprintf(path, sizeof path, "%s/assets/base/data/materials.lua", zb_test_source_dir());
  size_t len;
  char *text = zb_test_read_file(path, &len);
  TEST_ASSERT_NOT_NULL_MESSAGE(text, path);
  zb_luon_doc doc;
  zb_luon_error le;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_luon_parse(&doc, zb_heap_allocator(), text, len, &le));
  free(text);
  zb_registry r;
  zb_registry_init(&r, zb_heap_allocator(), "materials");
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_registry_merge(&r, "base", &doc.root));
  zb_luon_free(&doc);
  g_materials = malloc(sizeof *g_materials);
  char err[160];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_materials_build(g_materials, &r, err, sizeof err));
}

static int mat(const char *name) {
  int m = zb_materials_find(g_materials, name);
  TEST_ASSERT_TRUE_MESSAGE(m >= 0, name);
  return m;
}

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

typedef struct scene {
  zb_world *world;
  zb_surface_water *water;
  zb_bodies *bodies;
  zb_planet_pos centre;
} scene;

/* Flat ground at 50 m, a slope rising 0.45 m per metre along the chunk's x
 * (24 degrees), or a pit 10 m deep and 60 m across. */
typedef enum ground { FLAT, SLOPE, PIT } ground;

static float height_of(ground g, int x, int y) {
  double dx = x - 128.0, dy = y - 128.0;
  switch (g) {
    case SLOPE: return (float)(50.0 + 0.45 * (x - 128));
    case PIT: return dx * dx + dy * dy < 30.0 * 30.0 ? 40.0f : 50.0f;
    default: return 50.0f;
  }
}

static void scene_open(scene *s, ground g) {
  load_materials();
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  char err[128];
  s->world = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(s->world, err);
  const zb_planet *p = zb_world_planet(s->world);
  double lat = 46.0 * ZB_DPI / 180.0, lon = 0.3;
  zb_dvec3 dir = {zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat)};
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos = zb_planet_from_cart(p, zb_dv_scale(dir, p->radius));
  f.pos.lx = f.pos.ly = 128.0f;
  s->centre = f.pos;
  zb_world_set_focus(s->world, 0, &f);
  for (int i = 0; i < 2000; ++i) {
    zb_world_update(s->world);
    const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(s->world), f.pos.chunk);
    if (ct && ct->stage >= ZB_STAGE_SURFACE) {
      break;
    }
  }
  zb_chunk_terrain *ct = zb_stream_edit(zb_world_stream(s->world), f.pos.chunk);
  TEST_ASSERT_NOT_NULL(ct);
  for (int y = 0; y < ZB_HEIGHT_RES; ++y) {
    for (int x = 0; x < ZB_HEIGHT_RES; ++x) {
      ct->height[y * ZB_HEIGHT_RES + x] = height_of(g, x, y);
    }
  }
  memset(ct->material, ZB_MAT_ROCK, ZB_SURFACE_RES * ZB_SURFACE_RES);
  s->water = zb_surface_water_create(zb_test_alloc(), s->world, 1.0); /* the reshaped chunk alone */
  zb_surface_water_update(s->water, &s->centre, 1, 0.0);
  s->bodies = zb_bodies_create(zb_test_alloc(), s->world, g_materials, s->water);
  TEST_ASSERT_NOT_NULL(s->bodies);
}

static void scene_close(scene *s) {
  zb_bodies_destroy(s->bodies);
  zb_surface_water_destroy(s->water);
  zb_world_destroy(s->world);
}

static void run(scene *s, double seconds) {
  for (double t = 0.0; t < seconds - 1e-9; t += 1.0 / 60.0) {
    zb_bodies_update(s->bodies, 1.0 / 60.0);
  }
}

static void run_water(scene *s, double seconds) {
  for (double t = 0.0; t < seconds - 1e-9; t += 1.0 / 60.0) {
    zb_surface_water_update(s->water, &s->centre, 1, 1.0 / 60.0);
    zb_bodies_update(s->bodies, 1.0 / 60.0);
  }
}

static zb_planet_pos at(const scene *s, float x, float y, double alt) {
  zb_planet_pos p = s->centre;
  p.lx = x;
  p.ly = y;
  p.alt = (float)alt;
  return p;
}

/* The chunk's x axis in east and north terms. */
static void chunk_x(const scene *s, double out[2]) {
  const zb_planet *p = zb_world_planet(s->world);
  zb_planet_pos a = at(s, 128.0f, 128.0f, 50.0), b = at(s, 129.0f, 128.0f, 50.0);
  zb_dvec3 ca = zb_planet_to_cart(p, &a), d = zb_dv_norm(zb_dv_sub(zb_planet_to_cart(p, &b), ca)), e, n, u;
  zb_planet_frame(zb_dv_norm(ca), &e, &n, &u);
  out[0] = zb_dv_dot(d, e);
  out[1] = zb_dv_dot(d, n);
}

/* Lines a body's own x axis up with the chunk's x, tilted up by slope (rise
 * per metre), its z axis square to that. */
static void align(const scene *s, zb_body_desc *d, double slope) {
  double cx[2];
  chunk_x(s, cx);
  double l = sqrt(1.0 + slope * slope);
  d->axis_x[0] = cx[0] / l;
  d->axis_x[1] = cx[1] / l;
  d->axis_x[2] = slope / l;
  d->axis_z[0] = -slope * cx[0] / l;
  d->axis_z[1] = -slope * cx[1] / l;
  d->axis_z[2] = 1.0 / l;
}

static zb_body_desc desc(const scene *s, zb_shape shape, const char *material, float x, float y, double alt) {
  zb_body_desc d;
  memset(&d, 0, sizeof d);
  d.shape = shape;
  d.material = mat(material);
  d.pos = at(s, x, y, alt);
  d.radius = 0.5;
  d.half_length = 1.0;
  d.half[0] = d.half[1] = d.half[2] = 0.5;
  return d;
}

static uint32_t spawn(scene *s, zb_shape shape, const char *material, float x, float y, double alt) {
  zb_body_desc d = desc(s, shape, material, x, y, alt);
  uint32_t id = zb_bodies_add(s->bodies, &d);
  TEST_ASSERT_TRUE(id > 0);
  return id;
}

static zb_body_state state(const scene *s, uint32_t id) {
  zb_body_state st;
  TEST_ASSERT_TRUE(zb_bodies_get(s->bodies, id, &st));
  return st;
}

/* spec: rigid-bodies: Bodies fall and come to rest */
static void test_fall_and_rest(void) {
  scene s;
  scene_open(&s, FLAT);
  uint32_t ball = spawn(&s, ZB_SHAPE_SPHERE, "rock", 100.0f, 128.0f, 60.0);
  uint32_t crate = spawn(&s, ZB_SHAPE_BOX, "wood", 128.0f, 128.0f, 55.0);
  zb_body_desc d = desc(&s, ZB_SHAPE_CAPSULE, "wood", 156.0f, 128.0f, 53.0);
  d.radius = 0.3;
  d.spin[0] = 0.5; /* tips over */
  uint32_t log = zb_bodies_add(s.bodies, &d);
  TEST_ASSERT_TRUE(log > ball && crate > ball);
  /* In free fall for a second: about 4.9 m. */
  run(&s, 1.0);
  zb_body_state b = state(&s, ball);
  TEST_ASSERT_DOUBLE_WITHIN(0.1, 60.0 - 0.5 * 9.81, b.pos.alt);
  TEST_ASSERT_TRUE(b.velocity[2] < -9.0 && !b.grounded && !b.asleep);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0 / 3.0 * ZB_DPI * 0.125 * 2700.0, b.mass);
  run(&s, 9.0);
  b = state(&s, ball);
  zb_body_state c = state(&s, crate), l = state(&s, log);
  TEST_ASSERT_DOUBLE_WITHIN(0.03, 50.5, b.pos.alt);
  TEST_ASSERT_DOUBLE_WITHIN(0.03, 50.5, c.pos.alt);
  TEST_ASSERT_DOUBLE_WITHIN(0.03, 50.3, l.pos.alt);
  TEST_ASSERT_TRUE_MESSAGE(fabs(l.axis_z[2]) < 0.1, "the log lies on its side");
  TEST_ASSERT_TRUE_MESSAGE(b.asleep && c.asleep && l.asleep, "all at rest");
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 100.0, b.pos.lx); /* it did not wander */
  zb_bodies_stats st;
  zb_bodies_get_stats(s.bodies, &st);
  TEST_ASSERT_EQUAL_INT(3, st.count);
  TEST_ASSERT_EQUAL_INT(0, st.awake);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, st.kinetic);
  scene_close(&s);
}

/* spec: rigid-bodies: Friction holds or lets go */
static void test_slope(void) {
  scene s;
  scene_open(&s, SLOPE);
  /* Ground at x is 50 + 0.45 (x - 128); start each just above it. */
  uint32_t ball = spawn(&s, ZB_SHAPE_SPHERE, "rock", 128.0f, 100.0f, 50.6);
  zb_body_desc d = desc(&s, ZB_SHAPE_BOX, "rock", 128.0f, 128.0f, 50.56); /* flat on the slope */
  align(&s, &d, 0.45);
  uint32_t stone = zb_bodies_add(s.bodies, &d);
  d = desc(&s, ZB_SHAPE_BOX, "ice", 128.0f, 156.0f, 50.56);
  align(&s, &d, 0.45);
  uint32_t ice = zb_bodies_add(s.bodies, &d);
  run(&s, 3.0);
  zb_body_state b = state(&s, ball), st = state(&s, stone), i = state(&s, ice);
  TEST_ASSERT_TRUE_MESSAGE(b.pos.lx < 118.0f, "the ball rolls down");
  TEST_ASSERT_TRUE_MESSAGE(fabs(b.spin[0]) + fabs(b.spin[1]) > 1.0, "rolling, not sliding");
  TEST_ASSERT_TRUE_MESSAGE(fabs(st.pos.lx - 128.0f) < 0.3f, "granite on granite holds");
  TEST_ASSERT_TRUE_MESSAGE(i.pos.lx < 120.0f, "ice slides");
  TEST_ASSERT_TRUE(i.grounded);
  scene_close(&s);
}

/* spec: rigid-bodies: Bodies collide */
static void test_collide(void) {
  scene s;
  scene_open(&s, FLAT);
  /* Two balls meet head-on high above the ground: momentum is kept. */
  zb_body_desc a = desc(&s, ZB_SHAPE_SPHERE, "rock", 120.0f, 128.0f, 120.0);
  zb_body_desc b = desc(&s, ZB_SHAPE_SPHERE, "wood", 124.0f, 128.0f, 120.0);
  double cx[2];
  chunk_x(&s, cx);
  double ax = cx[0], ay = cx[1];
  a.velocity[0] = 6.0 * ax;
  a.velocity[1] = 6.0 * ay;
  uint32_t ia = zb_bodies_add(s.bodies, &a);
  uint32_t ib = zb_bodies_add(s.bodies, &b);
  double ma = state(&s, ia).mass, mb = state(&s, ib).mass;
  run(&s, 1.0);
  zb_body_state sa = state(&s, ia), sb = state(&s, ib);
  double pa = sa.velocity[0] * ax + sa.velocity[1] * ay, pb = sb.velocity[0] * ax + sb.velocity[1] * ay;
  TEST_ASSERT_TRUE_MESSAGE(pb > 3.0, "the struck ball flies on");
  TEST_ASSERT_DOUBLE_WITHIN(0.02 * ma * 6.0, ma * 6.0, ma * pa + mb * pb);
  zb_bodies_clear(s.bodies);
  /* A stack of crates stands, and a capsule rests against a ball. */
  uint32_t low = spawn(&s, ZB_SHAPE_BOX, "wood", 128.0f, 128.0f, 50.55);
  uint32_t high = spawn(&s, ZB_SHAPE_BOX, "wood", 128.0f, 128.0f, 51.65);
  uint32_t ball = spawn(&s, ZB_SHAPE_SPHERE, "rock", 140.0f, 128.0f, 50.6);
  zb_body_desc lean = desc(&s, ZB_SHAPE_CAPSULE, "wood", 141.2f, 128.0f, 52.0);
  lean.radius = 0.2;
  uint32_t pole = zb_bodies_add(s.bodies, &lean);
  run(&s, 1.6);
  run(&s, 6.4);
  zb_body_state lo = state(&s, low), hi = state(&s, high);
  TEST_ASSERT_DOUBLE_WITHIN(0.05, 50.5, lo.pos.alt);
  TEST_ASSERT_DOUBLE_WITHIN(0.08, 51.5, hi.pos.alt);
  TEST_ASSERT_TRUE(fabs(hi.pos.lx - 128.0f) < 0.2f && lo.asleep && hi.asleep);
  TEST_ASSERT_TRUE(state(&s, pole).pos.alt < 52.0 && state(&s, ball).pos.alt < 50.6);
  zb_bodies_stats st;
  zb_bodies_get_stats(s.bodies, &st);
  TEST_ASSERT_EQUAL_INT(4, st.count);
  scene_close(&s);
}

/* spec: rigid-bodies: Bodies collide */
static void test_shapes(void) {
  scene s;
  scene_open(&s, FLAT);
  zb_body_desc d = desc(&s, ZB_SHAPE_BOX, "rock", 128.0f, 128.0f, 50.5); /* a slab 4 m across */
  d.half[0] = d.half[1] = 2.0;
  d.asleep = true;
  uint32_t base = zb_bodies_add(s.bodies, &d);
  zb_body_desc tilted = desc(&s, ZB_SHAPE_BOX, "wood", 128.3f, 128.0f, 53.0);
  tilted.axis_x[0] = 1.0;
  tilted.axis_x[2] = 1.0;
  tilted.axis_z[0] = -1.0;
  tilted.axis_z[2] = 1.0; /* tipped 45 degrees: an edge comes down first */
  uint32_t top = zb_bodies_add(s.bodies, &tilted);
  zb_body_state ts = state(&s, top);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, sqrt(0.5), ts.axis_z[2]);
  /* Two logs side by side along the chunk's x, a third dropped across them. */
  double cx[2];
  chunk_x(&s, cx);
  zb_body_desc c1 = desc(&s, ZB_SHAPE_CAPSULE, "wood", 100.0f, 128.0f, 50.31), c2 = c1, c3;
  c1.radius = c2.radius = 0.3;
  c1.axis_x[2] = c2.axis_x[2] = 1.0;
  c1.axis_z[0] = c2.axis_z[0] = cx[0];
  c1.axis_z[1] = c2.axis_z[1] = cx[1];
  c2.pos = at(&s, 100.0f, 128.59f, 50.31);
  c3 = c1;
  c3.axis_z[0] = -cx[1];
  c3.axis_z[1] = cx[0];
  c3.pos = at(&s, 100.0f, 128.3f, 51.5);
  uint32_t under = zb_bodies_add(s.bodies, &c1), beside = zb_bodies_add(s.bodies, &c2);
  uint32_t over = zb_bodies_add(s.bodies, &c3);
  run(&s, 8.0);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, top).pos.alt > 51.4, "it landed on the base, not through it");
  TEST_ASSERT_TRUE(state(&s, under).asleep && state(&s, beside).asleep);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, over).pos.alt > 50.75, "one log across the others");
  TEST_ASSERT_TRUE(fabs(state(&s, beside).pos.ly - state(&s, under).pos.ly) > 0.59f);
  zb_bodies_get(s.bodies, base, &ts);
  TEST_ASSERT_DOUBLE_WITHIN(0.05, 50.5, ts.pos.alt);
  TEST_ASSERT_EQUAL_STRING("capsule", zb_shape_name(ZB_SHAPE_CAPSULE));
  TEST_ASSERT_EQUAL_STRING("?", zb_shape_name(ZB_SHAPES));
  scene_close(&s);
}

/* spec: rigid-bodies: Sleeping bodies wake */
static void test_wake(void) {
  scene s;
  scene_open(&s, FLAT);
  uint32_t a = spawn(&s, ZB_SHAPE_SPHERE, "rock", 100.0f, 128.0f, 50.5);
  uint32_t b = spawn(&s, ZB_SHAPE_BOX, "rock", 128.0f, 128.0f, 50.5);
  uint32_t c = spawn(&s, ZB_SHAPE_BOX, "rock", 160.0f, 128.0f, 50.5);
  run(&s, 2.0);
  TEST_ASSERT_TRUE(state(&s, a).asleep && state(&s, b).asleep && state(&s, c).asleep);
  /* A push. */
  double kick[3] = {0.0, 0.0, 3000.0};
  TEST_ASSERT_TRUE(zb_bodies_push(s.bodies, a, kick));
  TEST_ASSERT_FALSE(state(&s, a).asleep);
  run(&s, 0.2);
  TEST_ASSERT_TRUE(state(&s, a).pos.alt > 50.6);
  /* The ground opens under it. */
  zb_planet_pos hole = at(&s, 128.0f, 128.0f, 0.0);
  zb_chunk changed[9];
  int n = zb_world_deform(s.world, &hole, 4.0, -2.0, changed, 9);
  for (int k = 0; k < n; ++k) {
    zb_bodies_ground_changed(s.bodies, changed[k]);
  }
  run(&s, 3.0);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, b).pos.alt < 49.5, "it fell into the hole");
  /* Another body knocks it. */
  zb_body_desc ball = desc(&s, ZB_SHAPE_SPHERE, "iron", 150.0f, 128.0f, 50.6);
  TEST_ASSERT_TRUE(state(&s, c).asleep);
  double cx[2];
  chunk_x(&s, cx);
  ball.velocity[0] = 8.0 * cx[0];
  ball.velocity[1] = 8.0 * cx[1];
  zb_bodies_add(s.bodies, &ball);
  run(&s, 2.0);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, c).pos.lx > 160.5f, "knocked along");
  scene_close(&s);
}

/* spec: rigid-bodies: Wood floats and stone sinks */
static void test_water(void) {
  scene s;
  scene_open(&s, PIT);
  zb_planet_pos middle = at(&s, 128.0f, 128.0f, 0.0);
  zb_surface_water_add(s.water, &middle, 29.0, ZB_DPI * 30.0 * 30.0 * 6.0); /* a pond several metres deep */
  run_water(&s, 30.0);
  zb_water_cell wc;
  zb_planet_pos probe = at(&s, 128.0f, 128.0f, 0.0);
  zb_surface_water_cell_at(s.water, &probe, &wc);
  double level = wc.level;
  TEST_ASSERT_TRUE(level > 44.0 && level < 49.5);
  zb_body_desc logd = desc(&s, ZB_SHAPE_BOX, "wood", 120.0f, 128.0f, level + 2.0);
  logd.half[0] = 1.5;
  logd.half[1] = logd.half[2] = 0.4;
  uint32_t log = zb_bodies_add(s.bodies, &logd);
  uint32_t stone = spawn(&s, ZB_SHAPE_SPHERE, "rock", 136.0f, 128.0f, level + 2.0);
  uint32_t buoy = spawn(&s, ZB_SHAPE_SPHERE, "pine_wood", 128.0f, 120.0f, level + 2.0);
  run_water(&s, 25.0);
  zb_body_state l = state(&s, log), st = state(&s, stone), bu = state(&s, buoy);
  TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(0.1, 0.7, l.submerged, "oak floats seven tenths under");
  TEST_ASSERT_DOUBLE_WITHIN(0.15, level - 0.4 + 0.8 * 0.7, l.pos.alt + 0.4);
  TEST_ASSERT_DOUBLE_WITHIN(0.1, 0.5, bu.submerged);
  TEST_ASSERT_TRUE_MESSAGE(st.pos.alt < 41.0 && st.submerged > 0.99, "granite sinks to the bottom");
  scene_close(&s);
  /* Flowing water carries a floating body away from where it pours in. */
  scene_open(&s, FLAT);
  zb_planet_pos spring = at(&s, 100.0f, 128.0f, 0.0);
  zb_surface_water_add(s.water, &spring, 6.0, 400.0);
  run_water(&s, 1.0);
  zb_body_desc ball = desc(&s, ZB_SHAPE_SPHERE, "pine_wood", 108.0f, 128.0f, 50.3);
  ball.radius = 0.3;
  uint32_t drift = zb_bodies_add(s.bodies, &ball);
  run_water(&s, 5.0);
  zb_body_state ds = state(&s, drift);
  TEST_ASSERT_TRUE_MESSAGE(ds.pos.lx > 109.0f, "carried along by the flow");
  scene_close(&s);
}

/* spec: rigid-bodies: Projectiles never pass through */
static void test_projectiles(void) {
  scene s;
  scene_open(&s, FLAT);
  /* A thin board and a fast bolt aimed at it. */
  zb_body_desc board = desc(&s, ZB_SHAPE_BOX, "wood", 140.0f, 128.0f, 51.0);
  board.half[0] = 0.02;
  board.half[2] = 1.0;
  align(&s, &board, 0.0);
  uint32_t target = zb_bodies_add(s.bodies, &board);
  double cx[2];
  chunk_x(&s, cx);
  double v[3] = {300.0 * cx[0], 300.0 * cx[1], 0.0};
  zb_planet_pos from = at(&s, 100.0f, 128.0f, 51.0);
  uint32_t bolt = zb_bodies_launch(s.bodies, &from, v, 0.02, 0.05);
  TEST_ASSERT_TRUE(bolt > 0);
  zb_bodies_stats st;
  zb_bodies_get_stats(s.bodies, &st);
  TEST_ASSERT_EQUAL_INT(1, st.projectiles);
  run(&s, 0.5);
  zb_projectile_state ps;
  TEST_ASSERT_TRUE(zb_bodies_projectile(s.bodies, bolt, &ps));
  TEST_ASSERT_TRUE(ps.exists && !ps.flying && ps.hit.hit);
  TEST_ASSERT_EQUAL_UINT32(target, ps.hit.body);
  TEST_ASSERT_DOUBLE_WITHIN(0.05, 140.0 - 0.04, ps.pos.lx);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, target).speed > 0.0 || !state(&s, target).asleep, "the board was struck");
  /* A stone thrown up comes down on the ground. */
  double up[3] = {0.0, 0.0, 15.0};
  zb_planet_pos hand = at(&s, 60.0f, 60.0f, 52.0);
  uint32_t stone = zb_bodies_launch(s.bodies, &hand, up, 0.05, 0.5);
  run(&s, 1.0);
  TEST_ASSERT_TRUE(zb_bodies_projectile(s.bodies, stone, &ps) && ps.flying && ps.velocity[2] > 0.0);
  run(&s, 3.0);
  TEST_ASSERT_TRUE(zb_bodies_projectile(s.bodies, stone, &ps) && !ps.flying);
  TEST_ASSERT_EQUAL_UINT32(0, ps.hit.body);
  TEST_ASSERT_DOUBLE_WITHIN(0.05, 50.05, ps.pos.alt);
  TEST_ASSERT_TRUE(ps.hit.normal[2] > 0.99);
  run(&s, 11.0);
  TEST_ASSERT_FALSE_MESSAGE(zb_bodies_projectile(s.bodies, stone, &ps), "forgotten after it stopped");
  /* Sweeps: a ray down hits the ground, a ray through the air nothing. */
  zb_planet_pos top = at(&s, 30.0f, 30.0f, 80.0), below = at(&s, 30.0f, 30.0f, 20.0);
  zb_hit h = zb_bodies_sweep(s.bodies, &top, &below, 0.0);
  TEST_ASSERT_TRUE(h.hit && h.body == 0);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 30.0, h.distance);
  zb_planet_pos a1 = at(&s, 30.0f, 30.0f, 80.0), a2 = at(&s, 60.0f, 30.0f, 80.0);
  h = zb_bodies_sweep(s.bodies, &a1, &a2, 0.5);
  TEST_ASSERT_FALSE(h.hit);
  zb_body_state now = state(&s, target); /* knocked over by now */
  zb_planet_pos b1 = now.pos, b2 = now.pos;
  b1.alt += 10.0f;
  b2.alt -= 10.0f;
  h = zb_bodies_sweep(s.bodies, &b1, &b2, 0.1);
  TEST_ASSERT_TRUE(h.hit);
  TEST_ASSERT_EQUAL_UINT32(target, h.body);
  TEST_ASSERT_TRUE(h.normal[2] > 0.9);
  h = zb_bodies_sweep(s.bodies, &b2, &b2, 0.1); /* starts in the ground */
  TEST_ASSERT_TRUE(h.hit && h.distance == 0.0);
  /* Out of range. */
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_launch(s.bodies, &from, v, 0.0, 0.05));
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_launch(s.bodies, &from, v, 0.02, 1000.0));
  TEST_ASSERT_FALSE(zb_bodies_projectile(s.bodies, 999, &ps));
  scene_close(&s);
}

static void count_pose(void *ctx, const zb_body_pose *p) {
  TEST_ASSERT_TRUE(p->id > 0 && fabs(zb_dv_len(p->axis[0]) - 1.0) < 1e-9);
  (*(int *)ctx)++;
}

/* spec: rigid-bodies: Bodies outside the active tier are frozen */
static void test_bookkeeping(void) {
  scene s;
  scene_open(&s, FLAT);
  zb_body_desc d = desc(&s, ZB_SHAPE_SPHERE, "rock", 128.0f, 128.0f, 70.0);
  d.radius = 0.0;
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_add(s.bodies, &d));
  d.radius = 80.0;
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_add(s.bodies, &d));
  d = desc(&s, ZB_SHAPE_BOX, "rock", 128.0f, 128.0f, 70.0);
  d.half[1] = 0.0;
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_add(s.bodies, &d));
  d = desc(&s, ZB_SHAPE_CAPSULE, "rock", 128.0f, 128.0f, 70.0);
  d.half_length = 0.0;
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_add(s.bodies, &d));
  d = desc(&s, ZB_SHAPE_SPHERE, "water", 128.0f, 128.0f, 70.0); /* not solid */
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_add(s.bodies, &d));
  d.material = 9999;
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_add(s.bodies, &d));
  d = desc(&s, ZB_SHAPES, "rock", 128.0f, 128.0f, 70.0);
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_add(s.bodies, &d));
  /* One far away, out of the active tier: it waits in the air. */
  zb_body_desc far = desc(&s, ZB_SHAPE_SPHERE, "rock", 128.0f, 128.0f, 70.0);
  far.pos.chunk.x += 40;
  uint32_t idle = zb_bodies_add(s.bodies, &far);
  uint32_t here = spawn(&s, ZB_SHAPE_SPHERE, "rock", 128.0f, 128.0f, 70.0);
  run(&s, 1.0);
  zb_body_state st = state(&s, idle);
  TEST_ASSERT_TRUE(st.frozen);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, 70.0, st.pos.alt);
  TEST_ASSERT_FALSE(state(&s, here).frozen);
  zb_bodies_stats stats;
  zb_bodies_get_stats(s.bodies, &stats);
  TEST_ASSERT_EQUAL_INT(1, stats.frozen);
  TEST_ASSERT_EQUAL_INT(1, stats.awake);
  TEST_ASSERT_TRUE(stats.kinetic > 0.0);
  uint32_t ids[4];
  zb_planet_pos c = at(&s, 128.0f, 128.0f, 60.0);
  TEST_ASSERT_EQUAL_INT(1, zb_bodies_near(s.bodies, &c, 20.0, ids, 4));
  TEST_ASSERT_EQUAL_UINT32(here, ids[0]);
  int poses = 0;
  zb_bodies_each(s.bodies, count_pose, &poses);
  TEST_ASSERT_EQUAL_INT(2, poses);
  TEST_ASSERT_TRUE(zb_bodies_remove(s.bodies, idle));
  TEST_ASSERT_FALSE(zb_bodies_remove(s.bodies, idle));
  TEST_ASSERT_FALSE(zb_bodies_get(s.bodies, idle, &st));
  double kick[3] = {1, 0, 0};
  TEST_ASSERT_FALSE(zb_bodies_push(s.bodies, idle, kick));
  zb_bodies_update(s.bodies, 5.0); /* far behind: catches up ten steps, drops the rest */
  zb_bodies_set_world(s.bodies, s.world, s.water);
  zb_bodies_get_stats(s.bodies, &stats);
  TEST_ASSERT_EQUAL_INT(0, stats.count);
  /* Without water, the ground's stage decides. */
  zb_bodies *dry = zb_bodies_create(zb_test_alloc(), s.world, g_materials, NULL);
  zb_body_desc dd = desc(&s, ZB_SHAPE_SPHERE, "rock", 128.0f, 128.0f, 50.5);
  uint32_t id = zb_bodies_add(dry, &dd);
  for (int k = 0; k < 60; ++k) {
    zb_bodies_update(dry, 1.0 / 60.0);
  }
  TEST_ASSERT_TRUE(zb_bodies_get(dry, id, &st) && !st.frozen && st.submerged == 0.0);
  zb_bodies_destroy(dry);
  zb_bodies_destroy(NULL);
  scene_close(&s);
}

/* spec: rigid-bodies: Bodies collide */
static void test_odd_cases(void) {
  scene s;
  scene_open(&s, FLAT);
  /* A slab wider than the grid's cells carries a ball. */
  zb_body_desc slab = desc(&s, ZB_SHAPE_BOX, "rock", 128.0f, 128.0f, 50.5);
  slab.half[0] = slab.half[1] = 10.0;
  align(&s, &slab, 0.0);
  uint32_t big = zb_bodies_add(s.bodies, &slab);
  uint32_t ball = spawn(&s, ZB_SHAPE_SPHERE, "rock", 132.0f, 128.0f, 52.0);
  /* A log dropped flat on the slab. */
  zb_body_desc log = desc(&s, ZB_SHAPE_CAPSULE, "wood", 124.0f, 124.0f, 52.5);
  log.radius = 0.3;
  log.half_length = 2.0;
  log.axis_x[2] = 1.0;
  log.axis_z[0] = 1.0;
  uint32_t lying = zb_bodies_add(s.bodies, &log);
  /* Two boxes meeting edge to edge, crossed. */
  zb_body_desc lower = desc(&s, ZB_SHAPE_BOX, "rock", 60.0f, 60.0f, 50.0 + sqrt(0.5));
  lower.axis_x[0] = 1.0;
  lower.axis_x[2] = 1.0;
  lower.axis_z[0] = -1.0;
  lower.axis_z[2] = 1.0;
  lower.asleep = true;
  uint32_t under = zb_bodies_add(s.bodies, &lower);
  zb_body_desc upper = desc(&s, ZB_SHAPE_BOX, "wood", 60.0f, 60.0f, 50.0 + 3.0 * sqrt(0.5) + 0.05);
  upper.axis_x[1] = 1.0;
  upper.axis_x[2] = 1.0;
  upper.axis_z[1] = -1.0;
  upper.axis_z[2] = 1.0;
  uint32_t over = zb_bodies_add(s.bodies, &upper);
  /* A pebble caught inside a crate is pushed out. */
  zb_body_desc crate = desc(&s, ZB_SHAPE_BOX, "wood", 40.0f, 200.0f, 50.5);
  crate.asleep = true;
  uint32_t box = zb_bodies_add(s.bodies, &crate);
  zb_body_desc pebble = desc(&s, ZB_SHAPE_SPHERE, "rock", 40.1f, 200.0f, 50.6);
  pebble.radius = 0.1;
  uint32_t caught = zb_bodies_add(s.bodies, &pebble);
  /* A ball at the chunk's far edge, and one turned every which way. */
  uint32_t edge = spawn(&s, ZB_SHAPE_SPHERE, "rock", 255.6f, 255.6f, 50.5);
  for (int k = 0; k < 4; ++k) {
    zb_body_desc t = desc(&s, ZB_SHAPE_BOX, "rock", 200.0f, 40.0f + 4.0f * (float)k, 50.5);
    double flips[4][2][3] = {
        {{-1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {0, 0, 1}}, {{0, 1, 0}, {-1, 0, 0}}, {{0, 0, 1}, {0, 1, 0}}};
    memcpy(t.axis_x, flips[k][0], sizeof t.axis_x);
    memcpy(t.axis_z, flips[k][1], sizeof t.axis_z);
    uint32_t id = zb_bodies_add(s.bodies, &t);
    zb_body_state ts = state(&s, id);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, flips[k][1][2], ts.axis_z[2]);
  }
  run(&s, 6.0);
  TEST_ASSERT_DOUBLE_WITHIN(0.05, 50.5, state(&s, big).pos.alt);
  TEST_ASSERT_DOUBLE_WITHIN(0.08, 51.5, state(&s, ball).pos.alt);
  TEST_ASSERT_DOUBLE_WITHIN(0.08, 51.3, state(&s, lying).pos.alt);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, over).pos.alt > state(&s, under).pos.alt + 0.5, "edge on edge, not through");
  zb_body_state cs = state(&s, caught), bs = state(&s, box);
  TEST_ASSERT_TRUE_MESSAGE(cs.pos.alt > bs.pos.alt + 0.55, "out of the crate, on top");
  TEST_ASSERT_DOUBLE_WITHIN(0.05, 50.5, state(&s, edge).pos.alt);
  /* A sweep through the log, and one starting inside the slab. */
  zb_body_state ls = state(&s, lying);
  zb_planet_pos above = ls.pos, deep = state(&s, big).pos;
  above.alt += 5.0f;
  zb_planet_pos down = above;
  down.alt -= 10.0f;
  zb_hit h = zb_bodies_sweep(s.bodies, &above, &down, 0.05);
  TEST_ASSERT_TRUE(h.hit && h.body == lying);
  h = zb_bodies_sweep(s.bodies, &deep, &above, 0.0);
  TEST_ASSERT_TRUE(h.hit && h.body == big && h.distance == 0.0);
  scene_close(&s);
}

/* spec: rigid-bodies: Sleeping bodies wake */
static void test_wake_by_water(void) {
  scene s;
  scene_open(&s, PIT);
  zb_body_desc d = desc(&s, ZB_SHAPE_SPHERE, "pine_wood", 128.0f, 128.0f, 40.3);
  d.radius = 0.3;
  uint32_t ball = zb_bodies_add(s.bodies, &d);
  run_water(&s, 2.0);
  TEST_ASSERT_TRUE(state(&s, ball).asleep);
  /* The pit floods around it: it wakes and floats. */
  zb_planet_pos c = at(&s, 128.0f, 128.0f, 0.0);
  zb_surface_water_add(s.water, &c, 25.0, 3000.0);
  run_water(&s, 10.0);
  zb_body_state st = state(&s, ball);
  TEST_ASSERT_TRUE(st.submerged > 0.2 && st.pos.alt > 40.5);
  scene_close(&s);
}

static void test_limits(void) {
  scene s;
  scene_open(&s, FLAT);
  /* Bodies and projectiles up to their limits, all far outside the tier. */
  zb_body_desc far = desc(&s, ZB_SHAPE_SPHERE, "rock", 128.0f, 128.0f, 70.0);
  far.pos.chunk.x += 40;
  for (int k = 0; k < ZB_BODIES_MAX; ++k) {
    TEST_ASSERT_TRUE(zb_bodies_add(s.bodies, &far) > 0);
  }
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_add(s.bodies, &far));
  double v[3] = {1.0, 0.0, 0.0};
  for (int k = 0; k < ZB_PROJECTILES_MAX; ++k) {
    TEST_ASSERT_TRUE(zb_bodies_launch(s.bodies, &far.pos, v, 0.05, 1.0) > 0);
  }
  TEST_ASSERT_EQUAL_UINT32(0, zb_bodies_launch(s.bodies, &far.pos, v, 0.05, 1.0));
  run(&s, 0.5);
  zb_bodies_stats st;
  zb_bodies_get_stats(s.bodies, &st);
  TEST_ASSERT_EQUAL_INT(ZB_BODIES_MAX, st.frozen);
  TEST_ASSERT_EQUAL_INT(ZB_PROJECTILES_MAX, st.projectiles); /* frozen in flight */
  scene_close(&s);
}

/* spec: rigid-bodies: Deterministic bodies */
static void test_deterministic(void) {
  uint64_t h[2];
  for (int k = 0; k < 2; ++k) {
    scene s;
    scene_open(&s, SLOPE);
    for (int i = 0; i < 12; ++i) {
      zb_body_desc d = desc(&s, (zb_shape)(i % 3), i % 2 ? "rock" : "wood", 120.0f + (float)(i % 4) * 1.5f,
                            120.0f + (float)(i / 4) * 1.5f, 60.0 + i);
      d.spin[0] = 0.1 * i;
      TEST_ASSERT_TRUE(zb_bodies_add(s.bodies, &d) > 0);
    }
    double v[3] = {5.0, 0.0, 5.0};
    zb_planet_pos from = at(&s, 100.0f, 121.0f, 70.0);
    zb_bodies_launch(s.bodies, &from, v, 0.1, 2.0);
    run(&s, 6.0);
    h[k] = zb_bodies_hash(s.bodies);
    scene_close(&s);
  }
  TEST_ASSERT_EQUAL_UINT64(h[0], h[1]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_fall_and_rest);
  RUN_TEST(test_slope);
  RUN_TEST(test_collide);
  RUN_TEST(test_shapes);
  RUN_TEST(test_wake);
  RUN_TEST(test_water);
  RUN_TEST(test_projectiles);
  RUN_TEST(test_bookkeeping);
  RUN_TEST(test_odd_cases);
  RUN_TEST(test_wake_by_water);
  RUN_TEST(test_limits);
  RUN_TEST(test_deterministic);
  int failures = UNITY_END();
  if (g_materials) {
    zb_materials_free(g_materials);
    free(g_materials);
  }
  return failures;
}

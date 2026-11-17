/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/planet.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static zb_planet planet(double radius) {
  zb_planet p;
  char err[128];
  TEST_ASSERT_TRUE_MESSAGE(zb_planet_init(&p, radius, err, sizeof err), err);
  return p;
}

static uint64_t rng_state = 1;
static uint64_t next_u64(void) {
  uint64_t z = (rng_state += 0x9e3779b97f4a7c15ull);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}
static double uniform(double lo, double hi) {
  return lo + (hi - lo) * ((double)(next_u64() >> 11) * (1.0 / 9007199254740992.0));
}

static double dist3(zb_dvec3 a, zb_dvec3 b) {
  double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return sqrt(dx * dx + dy * dy + dz * dz);
}

/* spec: planet-coordinates: Default planet size */
static void test_default_size(void) {
  zb_planet p = planet(ZB_PLANET_DEFAULT_RADIUS);
  TEST_ASSERT_EQUAL_INT(12272, p.n);
  double lo = 1e9, hi = 0.0;
  for (int face = 0; face < ZB_PLANET_FACES; ++face) {
    for (int i = 0; i < 400; ++i) {
      zb_chunk c = {face, 0, 0};
      c.x = (int32_t)(next_u64() % (uint64_t)p.n);
      c.y = (int32_t)(next_u64() % (uint64_t)p.n);
      if (i < 4) { /* the corners */
        c.x = (i & 1) ? p.n - 1 : 0;
        c.y = (i & 2) ? p.n - 1 : 0;
      } else if (i < 8) { /* face centre and edge midpoints */
        c.x = i == 5 ? 0 : (i == 6 ? p.n - 1 : p.n / 2);
        c.y = i == 7 ? 0 : p.n / 2;
      }
      double w, h;
      zb_planet_chunk_size(&p, c, &w, &h);
      lo = fmin(lo, fmin(w, h));
      hi = fmax(hi, fmax(w, h));
    }
  }
  TEST_ASSERT_TRUE(lo >= 175.0 && hi <= 260.0);
  TEST_ASSERT_TRUE(lo < 200.0 && hi > 250.0); /* corners shrink, centres stay ~256 m */
  /* Earth-size planets fit the 30-bit key fields. */
  zb_planet earth = planet(ZB_PLANET_MAX_RADIUS);
  TEST_ASSERT_TRUE(earth.n < (1 << 30));
  TEST_ASSERT_EQUAL_INT(307, planet(ZB_PLANET_MIN_RADIUS).n);
}

/* spec: planet-coordinates: Radius out of range */
static void test_radius_range(void) {
  zb_planet p;
  char err[128] = "";
  TEST_ASSERT_FALSE(zb_planet_init(&p, 10000.0, err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "50000..6371000"));
  TEST_ASSERT_FALSE(zb_planet_init(&p, 10000000.0, err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "10000000"));
  TEST_ASSERT_FALSE(zb_planet_init(&p, (double)NAN, NULL, 0));
}

static zb_planet_pos random_pos(const zb_planet *p, int i) {
  zb_planet_pos pos;
  pos.chunk.face = i % ZB_PLANET_FACES;
  int32_t n = p->n;
  switch ((i / ZB_PLANET_FACES) % 4) {
    case 0: /* anywhere */
      pos.chunk.x = (int32_t)(next_u64() % (uint64_t)n);
      pos.chunk.y = (int32_t)(next_u64() % (uint64_t)n);
      break;
    case 1: /* face edge */
      pos.chunk.x = (next_u64() & 1) ? n - 1 : 0;
      pos.chunk.y = (int32_t)(next_u64() % (uint64_t)n);
      break;
    case 2: /* other face edge */
      pos.chunk.x = (int32_t)(next_u64() % (uint64_t)n);
      pos.chunk.y = (next_u64() & 1) ? n - 1 : 0;
      break;
    default: /* cube corner */
      pos.chunk.x = (next_u64() & 1) ? n - 1 : 0;
      pos.chunk.y = (next_u64() & 1) ? n - 1 : 0;
      break;
  }
  pos.lx = (float)uniform(0.001, 255.999);
  pos.ly = (float)uniform(0.001, 255.999);
  pos.alt = (float)uniform(-500.0, 8000.0);
  return pos;
}

/* spec: planet-coordinates: Round trip everywhere */
static void test_round_trip(void) {
  const double radii[] = {ZB_PLANET_DEFAULT_RADIUS, ZB_PLANET_MAX_RADIUS, ZB_PLANET_MIN_RADIUS};
  for (size_t r = 0; r < 3; ++r) {
    zb_planet p = planet(radii[r]);
    for (int i = 0; i < 10000; ++i) {
      zb_planet_pos a = random_pos(&p, i);
      zb_planet_pos b = zb_planet_from_cart(&p, zb_planet_to_cart(&p, &a));
      TEST_ASSERT_TRUE(zb_chunk_equal(a.chunk, b.chunk));
      TEST_ASSERT_FLOAT_WITHIN(0.001f, a.lx, b.lx);
      TEST_ASSERT_FLOAT_WITHIN(0.001f, a.ly, b.ly);
      TEST_ASSERT_FLOAT_WITHIN(0.001f, a.alt, b.alt);
    }
  }
}

/* spec: planet-coordinates: Every direction has one owner */
static void test_owner(void) {
  zb_planet p = planet(ZB_PLANET_DEFAULT_RADIUS);
  const double s = p.radius / sqrt(2.0), t = p.radius / sqrt(3.0);
  const zb_dvec3 dirs[] = {{s, s, 0},  {-s, s, 0}, {s, -s, 0}, {0, s, s},    {0, -s, -s},
                           {s, 0, -s}, {t, t, t},  {-t, t, t}, {-t, -t, -t}, {t, -t, t}};
  const int owners[] = {ZB_FACE_PX, ZB_FACE_NX, ZB_FACE_PX, ZB_FACE_PY, ZB_FACE_NY,
                        ZB_FACE_PX, ZB_FACE_PX, ZB_FACE_NX, ZB_FACE_NX, ZB_FACE_PX};
  for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; ++i) {
    zb_planet_pos a = zb_planet_from_cart(&p, dirs[i]);
    zb_planet_pos b = zb_planet_from_cart(&p, dirs[i]);
    TEST_ASSERT_EQUAL_INT(owners[i], a.chunk.face);
    TEST_ASSERT_TRUE(zb_chunk_valid(&p, a.chunk));
    TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof a);
    TEST_ASSERT_TRUE(dist3(dirs[i], zb_planet_to_cart(&p, &a)) < 0.001);
  }
  /* The planet centre maps somewhere valid instead of dividing by zero. */
  zb_dvec3 zero = {0, 0, 0};
  TEST_ASSERT_TRUE(zb_chunk_valid(&p, zb_planet_from_cart(&p, zero).chunk));
}

static int cmp_u64(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return x < y ? -1 : (x > y);
}

/* spec: planet-coordinates: Unique keys */
static void test_unique_keys(void) {
  zb_planet p = planet(ZB_PLANET_MIN_RADIUS);
  size_t count = (size_t)ZB_PLANET_FACES * (size_t)p.n * (size_t)p.n;
  uint64_t *keys = malloc(count * sizeof *keys);
  TEST_ASSERT_NOT_NULL(keys);
  size_t k = 0;
  for (int f = 0; f < ZB_PLANET_FACES; ++f) {
    for (int32_t y = 0; y < p.n; ++y) {
      for (int32_t x = 0; x < p.n; ++x) {
        zb_chunk c = {f, x, y};
        keys[k] = zb_chunk_key(c);
        TEST_ASSERT_TRUE(zb_chunk_equal(c, zb_chunk_from_key(keys[k])));
        ++k;
      }
    }
  }
  qsort(keys, count, sizeof *keys, cmp_u64);
  for (size_t i = 1; i < count; ++i) {
    TEST_ASSERT_TRUE(keys[i] != keys[i - 1]);
  }
  free(keys);
  /* Stable across platforms: fixed bit layout. */
  zb_chunk c = {5, 39000, 7};
  TEST_ASSERT_EQUAL_HEX64(0x5000000000000000ull | (39000ull << 30) | 7ull, zb_chunk_key(c));
  zb_chunk bad = {-1, 0, 0}, off = {0, p.n, 0};
  TEST_ASSERT_FALSE(zb_chunk_valid(&p, bad));
  TEST_ASSERT_FALSE(zb_chunk_valid(&p, off));
}

static bool contains(const zb_chunk *list, int n, zb_chunk c) {
  for (int i = 0; i < n; ++i) {
    if (zb_chunk_equal(list[i], c)) {
      return true;
    }
  }
  return false;
}

/* Every neighbour is close, valid and lists the chunk back. */
static void check_neighbors(const zb_planet *p, zb_chunk c, double max_dist) {
  zb_chunk nb[8], back[8];
  int n = zb_planet_neighbors(p, c, nb);
  zb_dvec3 dc = zb_planet_chunk_dir(p, c);
  for (int i = 0; i < n; ++i) {
    TEST_ASSERT_TRUE(zb_chunk_valid(p, nb[i]));
    TEST_ASSERT_FALSE(zb_chunk_equal(nb[i], c));
    double d = dist3(dc, zb_planet_chunk_dir(p, nb[i])) * p->radius;
    TEST_ASSERT_TRUE(d <= max_dist);
    int m = zb_planet_neighbors(p, nb[i], back);
    TEST_ASSERT_TRUE(contains(back, m, c));
  }
}

/* spec: planet-coordinates: Crossing a seam */
static void test_seams(void) {
  zb_planet p = planet(ZB_PLANET_DEFAULT_RADIUS);
  for (int face = 0; face < ZB_PLANET_FACES; ++face) {
    for (int i = 0; i < 50; ++i) {
      int32_t row = i == 0 ? 0 : (i == 1 ? p.n - 1 : (int32_t)(next_u64() % (uint64_t)p.n));
      zb_chunk c = {face, p.n - 1, row}, east;
      TEST_ASSERT_TRUE(zb_planet_neighbor(&p, c, 1, 0, &east));
      TEST_ASSERT_TRUE(east.face != face);
      double d = dist3(zb_planet_chunk_dir(&p, c), zb_planet_chunk_dir(&p, east)) * p.radius;
      TEST_ASSERT_TRUE(d <= 1.5 * ZB_CHUNK_SIZE);
      zb_chunk back[8];
      TEST_ASSERT_TRUE(contains(back, zb_planet_neighbors(&p, east, back), c));
      /* The other three edges as well. */
      zb_chunk w = {face, 0, row}, s = {face, row, 0}, n = {face, row, p.n - 1};
      check_neighbors(&p, c, 1.5 * ZB_CHUNK_SIZE * 1.5);
      check_neighbors(&p, w, 1.5 * ZB_CHUNK_SIZE * 1.5);
      check_neighbors(&p, s, 1.5 * ZB_CHUNK_SIZE * 1.5);
      check_neighbors(&p, n, 1.5 * ZB_CHUNK_SIZE * 1.5);
    }
  }
  /* Exhaustive along all seams of a small planet. */
  zb_planet small = planet(ZB_PLANET_MIN_RADIUS);
  for (int face = 0; face < ZB_PLANET_FACES; ++face) {
    for (int32_t i = 0; i < small.n; ++i) {
      zb_chunk edge[4] = {{face, 0, i}, {face, small.n - 1, i}, {face, i, 0}, {face, i, small.n - 1}};
      for (int e = 0; e < 4; ++e) {
        check_neighbors(&small, edge[e], 2.0 * ZB_CHUNK_SIZE);
      }
    }
  }
}

/* spec: planet-coordinates: Cube corner */
static void test_corner(void) {
  zb_planet p = planet(ZB_PLANET_DEFAULT_RADIUS);
  zb_chunk nb[8];
  for (int face = 0; face < ZB_PLANET_FACES; ++face) {
    for (int k = 0; k < 4; ++k) {
      zb_chunk c = {face, (k & 1) ? p.n - 1 : 0, (k & 2) ? p.n - 1 : 0};
      TEST_ASSERT_EQUAL_INT(7, zb_planet_neighbors(&p, c, nb));
      check_neighbors(&p, c, 2.0 * ZB_CHUNK_SIZE);
      zb_chunk none;
      TEST_ASSERT_FALSE(zb_planet_neighbor(&p, c, (k & 1) ? 1 : -1, (k & 2) ? 1 : -1, &none));
    }
    zb_chunk mid = {face, p.n / 2, p.n / 2}, next_to_corner = {face, 1, 0};
    TEST_ASSERT_EQUAL_INT(8, zb_planet_neighbors(&p, mid, nb));
    TEST_ASSERT_EQUAL_INT(8, zb_planet_neighbors(&p, next_to_corner, nb));
  }
}

static zb_planet_pos pos_at(const zb_planet *p, double lon_deg, double lat_deg) {
  double lon = lon_deg * ZB_DPI / 180.0, lat = lat_deg * ZB_DPI / 180.0;
  zb_dvec3 c = {p->radius * zb_dcos(lat) * zb_dcos(lon), p->radius * zb_dcos(lat) * zb_dsin(lon),
                p->radius * zb_dsin(lat)};
  return zb_planet_from_cart(p, c);
}

/* spec: planet-coordinates: Distance between chunks */
static void test_distance(void) {
  zb_planet p = planet(ZB_PLANET_DEFAULT_RADIUS);
  const double expected = 34906.585;
  zb_planet_pos a = pos_at(&p, 10.0, 0.0), b = pos_at(&p, 11.0, 0.0);
  TEST_ASSERT_DOUBLE_WITHIN(0.1, expected, zb_planet_distance(&p, &a, &b));
  /* Across the +X/+Y seam and along a meridian. */
  a = pos_at(&p, 44.5, 0.0);
  b = pos_at(&p, 45.5, 0.0);
  TEST_ASSERT_TRUE(a.chunk.face != b.chunk.face);
  TEST_ASSERT_DOUBLE_WITHIN(0.1, expected, zb_planet_distance(&p, &a, &b));
  a = pos_at(&p, 100.0, 60.0);
  b = pos_at(&p, 100.0, 61.0);
  TEST_ASSERT_DOUBLE_WITHIN(0.1, expected, zb_planet_distance(&p, &a, &b));
  a = pos_at(&p, 0.0, 0.0);
  b = pos_at(&p, 180.0, 0.0);
  TEST_ASSERT_DOUBLE_WITHIN(0.1, ZB_DPI * p.radius, zb_planet_distance(&p, &a, &b));
  /* Frames: right-handed, up along the position, east horizontal. */
  zb_dvec3 e, n, u, d = {0.3, -0.5, 0.8};
  zb_planet_frame(d, &e, &n, &u);
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, e.z);
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, e.x * u.x + e.y * u.y + e.z * u.z);
  TEST_ASSERT_TRUE(n.z > 0.0);
  zb_dvec3 pole = {0, 0, 1};
  zb_planet_frame(pole, &e, &n, &u);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, e.y);
  TEST_ASSERT_EQUAL_DOUBLE(-1.0, n.x);
}

/* spec: planet-coordinates: Precision far from the zero meridian */
static void test_origin_precision(void) {
  zb_planet p = planet(ZB_PLANET_MAX_RADIUS);
  zb_chunk anchor = {ZB_FACE_NX, p.n / 3, p.n / 2 + 17};
  zb_origin o;
  zb_origin_init(&o, &p, anchor);
  const zb_vec3 offsets[] = {{6, 0, -8}, {0, 10, 0}, {-3.5f, 1.25f, 9.2f}, {0.0001f, 0, 0}};
  for (size_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
    zb_planet_pos pos = zb_origin_to_pos(&o, &p, offsets[i]);
    zb_vec3 back = zb_origin_to_local(&o, &p, &pos);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, offsets[i].x, back.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, offsets[i].y, back.y);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, offsets[i].z, back.z);
  }
  /* Single-precision planet coordinates would be ~0.5 m off here. */
  zb_dvec3 c = zb_planet_to_cart(&p, &(zb_planet_pos){anchor, 128, 128, 0});
  TEST_ASSERT_TRUE(fabs((double)(float)c.x - c.x) + fabs((double)(float)c.y - c.y) > 1e-3);
}

/* spec: planet-coordinates: Curvature below the horizon */
static void test_curvature(void) {
  zb_planet p = planet(ZB_PLANET_DEFAULT_RADIUS);
  zb_chunk anchor = {ZB_FACE_PZ, 4000, 9000};
  zb_origin o;
  zb_origin_init(&o, &p, anchor);
  double theta = 20000.0 / p.radius;
  zb_dvec3 d = {o.up.x * zb_dcos(theta) + o.north.x * zb_dsin(theta),
                o.up.y * zb_dcos(theta) + o.north.y * zb_dsin(theta),
                o.up.z * zb_dcos(theta) + o.north.z * zb_dsin(theta)};
  zb_dvec3 c = {d.x * p.radius, d.y * p.radius, d.z * p.radius};
  zb_planet_pos pos = zb_planet_from_cart(&p, c);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, pos.alt);
  zb_vec3 l = zb_origin_to_local(&o, &p, &pos);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, -100.0f, l.y);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, -19999.7f, l.z); /* north is -z */
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, l.x);
}

static float len3(zb_vec3 v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }

/* spec: planet-coordinates: Re-anchoring without a jump */
static void test_rebase(void) {
  zb_planet p = planet(ZB_PLANET_DEFAULT_RADIUS);
  zb_origin o;
  zb_origin_init(&o, &p, (zb_chunk){ZB_FACE_PY, p.n - 3, 1200});
  zb_vec3 cam = {300.0f, 40.0f, -1060.0f};
  zb_vec3 obj = {300.0f, 40.0f, -1065.0f};
  TEST_ASSERT_TRUE(zb_origin_should_rebase(cam, ZB_ORIGIN_REBASE_DISTANCE));
  zb_planet_pos cam_pos = zb_origin_to_pos(&o, &p, cam);
  zb_origin old = o;
  zb_mat4 m = zb_origin_rebase(&o, &p, cam_pos.chunk);
  TEST_ASSERT_TRUE(zb_chunk_equal(cam_pos.chunk, o.chunk));
  zb_vec3 cam2 = zb_m4_transform_point(m, cam), obj2 = zb_m4_transform_point(m, obj);
  /* The object stays where it is on the planet relative to the camera (the
   * local axes turn with the planet's curvature, so compare in planet space). */
  zb_dvec3 cam_w = zb_origin_to_cart(&old, cam), obj_w = zb_origin_to_cart(&old, obj);
  zb_dvec3 cam2_w = zb_origin_to_cart(&o, cam2), obj2_w = zb_origin_to_cart(&o, obj2);
  zb_dvec3 before = {obj_w.x - cam_w.x, obj_w.y - cam_w.y, obj_w.z - cam_w.z};
  zb_dvec3 after = {obj2_w.x - cam2_w.x, obj2_w.y - cam2_w.y, obj2_w.z - cam2_w.z};
  TEST_ASSERT_TRUE(dist3(before, after) < 0.001);
  TEST_ASSERT_TRUE(dist3(cam_w, cam2_w) < 0.001);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, len3(zb_v3_sub(obj2, cam2)));
  /* The transform agrees with converting through the planet position. */
  zb_vec3 direct = zb_origin_to_local(&o, &p, &cam_pos);
  TEST_ASSERT_TRUE(len3(zb_v3_sub(direct, cam2)) < 0.001f);
  TEST_ASSERT_FALSE(zb_origin_should_rebase(cam2, ZB_ORIGIN_REBASE_DISTANCE));
  /* Rebasing across a face seam keeps the local up direction continuous. */
  zb_origin_init(&o, &p, (zb_chunk){ZB_FACE_PY, p.n - 1, 1200});
  zb_vec3 across = {0.0f, 0.0f, 0.0f};
  zb_chunk east;
  TEST_ASSERT_TRUE(zb_planet_neighbor(&p, o.chunk, 1, 0, &east));
  zb_vec3 east_local = zb_origin_to_local(&o, &p, &(zb_planet_pos){east, 128, 128, 0});
  m = zb_origin_rebase(&o, &p, east);
  zb_vec3 moved = zb_m4_transform_point(m, east_local);
  TEST_ASSERT_TRUE(len3(zb_v3_sub(moved, across)) < 0.001f);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_default_size);
  RUN_TEST(test_radius_range);
  RUN_TEST(test_round_trip);
  RUN_TEST(test_owner);
  RUN_TEST(test_unique_keys);
  RUN_TEST(test_seams);
  RUN_TEST(test_corner);
  RUN_TEST(test_distance);
  RUN_TEST(test_origin_precision);
  RUN_TEST(test_curvature);
  RUN_TEST(test_rebase);
  return UNITY_END();
}

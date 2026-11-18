/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "core/hash.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/climate.h"
#include "world/dvec.h"
#include "world/geology.h"

#include <stdio.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

#define RADIUS 2000000.0
#define DEG (3.141592653589793 / 180.0)

static zb_dvec3 at(double lat_deg, double lon_deg) {
  double la = lat_deg * DEG, lo = lon_deg * DEG;
  return zb_dv(zb_dcos(la) * zb_dcos(lo), zb_dcos(la) * zb_dsin(lo), zb_dsin(la));
}

static double lat_of(zb_dvec3 d) { return zb_dasin(d.z) / DEG; }
static double lon_of(zb_dvec3 d) { return zb_datan2(d.y, d.x) / DEG; }

/* ---- Synthetic worlds -------------------------------------------------------------------- */

typedef double (*height_fn)(zb_dvec3 d);

typedef struct synth {
  zb_cubegrid grid;
  float *height;
  int32_t *receiver;
  zb_climate *climate;
} synth;

static void synth_make(synth *s, int res, height_fn f, double tilt) {
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_cubegrid_init(&s->grid, zb_test_alloc(), res, RADIUS / 1000.0));
  s->height = zb_alloc(zb_test_alloc(), sizeof(float) * s->grid.cells);
  s->receiver = zb_alloc(zb_test_alloc(), sizeof(int32_t) * s->grid.cells);
  for (size_t c = 0; c < s->grid.cells; ++c) {
    s->height[c] = (float)f(zb_cubegrid_dir(&s->grid, (int32_t)c));
  }
  for (size_t c = 0; c < s->grid.cells; ++c) { /* steepest descent */
    int32_t best = -1;
    float low = s->height[c];
    for (int k = 0; k < ZB_CUBEGRID_NBRS && s->height[c] > 0.0f; ++k) {
      int32_t nb = zb_cubegrid_nbr(&s->grid, (int32_t)c, k);
      if (nb >= 0 && s->height[nb] < low) {
        low = s->height[nb];
        best = nb;
      }
    }
    s->receiver[c] = best;
  }
  zb_climate_input in;
  memset(&in, 0, sizeof in);
  in.grid = &s->grid;
  in.height = s->height;
  in.receiver = s->receiver;
  in.radius = RADIUS;
  in.axial_tilt = tilt;
  in.seed = 3;
  char err[128];
  s->climate = zb_climate_create(zb_test_alloc(), &in, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(s->climate, err);
  zb_climate_run(s->climate);
}

static void synth_free(synth *s) {
  zb_climate_destroy(s->climate);
  zb_free(zb_test_alloc(), s->height);
  zb_free(zb_test_alloc(), s->receiver);
  zb_cubegrid_free(&s->grid);
}

static double ocean_world(zb_dvec3 d) {
  (void)d;
  return -4000.0;
}

/* A continent from 60 S to 60 N between 30 W and 70 E. */
static double continent(zb_dvec3 d) {
  double la = lat_of(d), lo = lon_of(d);
  return la > -60.0 && la < 60.0 && lo > -30.0 && lo < 70.0 ? 200.0 : -4000.0;
}

/* The continent with a north-south range 2.5 km high at 20 E. */
static double ranged(zb_dvec3 d) {
  double h = continent(d), lo = lon_of(d);
  if (h > 0.0 && lo > 15.0 && lo < 25.0) {
    h += 2500.0;
  }
  return h;
}

/* The continent with a 3 km plateau in its middle. */
static double plateau(zb_dvec3 d) {
  double h = continent(d), la = lat_of(d), lo = lon_of(d);
  return h > 0.0 && la > 20.0 && la < 30.0 && lo > 30.0 && lo < 40.0 ? 3000.0 : h;
}

static double annual_t(const zb_climate *c, double la, double lo, double h) {
  double t, p;
  zb_climate_annual(c, at(la, lo), h, &t, &p);
  return t;
}

static double annual_p(const zb_climate *c, double la, double lo) {
  double t, p;
  zb_climate_annual(c, at(la, lo), 0.0, &t, &p);
  return p;
}

static double month_t(const zb_climate *c, double la, double lo, int m) {
  double t, p;
  zb_climate_month(c, at(la, lo), m, 0.0, &t, &p);
  return t;
}

/* spec: climate-and-biomes: Insolation and seasons */
static void test_insolation_and_seasons(void) {
  /* Insolation: the equator gets most over the year; polar night and midnight sun. */
  double tilt = 23.0 * DEG;
  TEST_ASSERT_TRUE(zb_insolation(0.0, 0.0) > zb_insolation(60.0 * DEG, 0.0));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_insolation(80.0 * DEG, -tilt));
  TEST_ASSERT_TRUE(zb_insolation(80.0 * DEG, tilt) > zb_insolation(0.0, tilt));
  synth s;
  synth_make(&s, 24, ocean_world, 23.0);
  double eq = annual_t(s.climate, 0, 0, 0), mid = annual_t(s.climate, 45, 0, 0), pole = annual_t(s.climate, 85, 0, 0);
  TEST_ASSERT_TRUE(eq > 24.0 && eq < 30.0);
  TEST_ASSERT_TRUE(eq > mid && mid > pole);
  /* Seasons are opposite in the two hemispheres. */
  TEST_ASSERT_TRUE(month_t(s.climate, 45, 0, 7) > month_t(s.climate, 45, 0, 1));
  TEST_ASSERT_TRUE(month_t(s.climate, -45, 0, 1) > month_t(s.climate, -45, 0, 7));
  synth_free(&s);
  /* Without tilt there are no seasons. */
  synth_make(&s, 16, ocean_world, 0.0);
  TEST_ASSERT_FLOAT_WITHIN(0.05, month_t(s.climate, 45, 0, 1), month_t(s.climate, 45, 0, 7));
  synth_free(&s);
}

/* spec: climate-and-biomes: Continents swing, oceans moderate */
static void test_continentality(void) {
  synth s;
  synth_make(&s, 32, continent, 23.0);
  double inland = month_t(s.climate, 45, 20, 7) - month_t(s.climate, 45, 20, 1);
  double coast = month_t(s.climate, 45, -28, 7) - month_t(s.climate, 45, -28, 1);
  double ocean = month_t(s.climate, 45, -120, 7) - month_t(s.climate, 45, -120, 1);
  char msg[96];
  snprintf(msg, sizeof msg, "seasonal range inland %.1f coast %.1f ocean %.1f", inland, coast, ocean);
  TEST_ASSERT_TRUE_MESSAGE(inland > coast && coast > ocean && ocean > 0.0, msg);
  synth_free(&s);
}

/* spec: climate-and-biomes: Altitude cools */
static void test_lapse_rate(void) {
  synth s;
  synth_make(&s, 32, plateau, 23.0);
  double low = annual_t(s.climate, 25, 35, 0.0), high = annual_t(s.climate, 25, 35, 3000.0);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 19.5, low - high);
  /* The climate grid itself knows the plateau: a macro-cell biome there is cold. */
  zb_biome b = zb_climate_biome_at(s.climate, at(25, 35));
  TEST_ASSERT_TRUE(zb_biome_group_of(b) == ZB_GROUP_MOUNTAIN || zb_biome_group_of(b) == ZB_GROUP_COLD ||
                   zb_biome_group_of(b) == ZB_GROUP_ARID || zb_biome_group_of(b) == ZB_GROUP_TEMPERATE);
  synth_free(&s);
}

/* spec: climate-and-biomes: Rain shadows */
static void test_rain_shadow(void) {
  synth s;
  synth_make(&s, 40, ranged, 23.0);
  /* Westerlies at 45 N: wet on the western (windward) side, dry on the lee. */
  double windward = annual_p(s.climate, 45, 12), lee = annual_p(s.climate, 45, 30);
  char msg[80];
  snprintf(msg, sizeof msg, "windward %.0f mm, lee %.0f mm", windward, lee);
  TEST_ASSERT_TRUE_MESSAGE(windward > 1.5 * lee, msg);
  synth_free(&s);
}

/* spec: climate-and-biomes: Belts of rain and drought */
static void test_rain_belts(void) {
  synth s;
  synth_make(&s, 32, continent, 23.0);
  double tropics = annual_p(s.climate, 2, 20), subtropics = annual_p(s.climate, 24, 20);
  char msg[80];
  snprintf(msg, sizeof msg, "equator %.0f mm, 24 N %.0f mm", tropics, subtropics);
  TEST_ASSERT_TRUE_MESSAGE(tropics > 2.0 * subtropics, msg);
  /* Cold currents: the subtropical west coast is cooler and drier than the east coast. */
  double west_t = annual_t(s.climate, 25, -28, 0), east_t = annual_t(s.climate, 25, 68, 0);
  double west_p = annual_p(s.climate, 25, -28), east_p = annual_p(s.climate, 25, 68);
  snprintf(msg, sizeof msg, "west %.1f C %.0f mm, east %.1f C %.0f mm", west_t, west_p, east_t, east_p);
  TEST_ASSERT_TRUE_MESSAGE(west_t < east_t && west_p < east_p, msg);
  synth_free(&s);
}

/* spec: climate-and-biomes: Whittaker biomes */
static void test_classify(void) {
  struct {
    double t, p, warm, dry, rocky;
    zb_biome want;
  } cases[] = {
      {-20, 100, -5, 0.4, 0.5, ZB_BIOME_POLAR_DESERT},
      {-15, 400, -2, 0.4, 0.5, ZB_BIOME_ICE_SHEET},
      {-5, 300, 8, 0.4, 0.5, ZB_BIOME_TUNDRA},
      {0, 500, 15, 0.4, 0.5, ZB_BIOME_TAIGA},
      {0, 150, 15, 0.4, 0.5, ZB_BIOME_TUNDRA},
      {25, 100, 35, 0.3, 0.2, ZB_BIOME_SAND_DESERT},
      {25, 100, 35, 0.3, 0.8, ZB_BIOME_ROCKY_DESERT},
      {10, 400, 22, 0.4, 0.5, ZB_BIOME_STEPPE},
      {17, 400, 26, 0.4, 0.5, ZB_BIOME_XERIC_SHRUBLAND},
      {15, 700, 25, 0.1, 0.5, ZB_BIOME_MEDITERRANEAN},
      {12, 900, 22, 0.4, 0.5, ZB_BIOME_DECIDUOUS_FOREST},
      {6, 800, 18, 0.4, 0.5, ZB_BIOME_MIXED_FOREST},
      {10, 3000, 18, 0.4, 0.5, ZB_BIOME_TEMPERATE_RAINFOREST},
      {25, 400, 30, 0.3, 0.5, ZB_BIOME_XERIC_SHRUBLAND},
      {25, 900, 30, 0.3, 0.5, ZB_BIOME_SAVANNA},
      {25, 1500, 28, 0.3, 0.5, ZB_BIOME_SEASONAL_FOREST},
      {26, 3000, 28, 0.1, 0.5, ZB_BIOME_SEASONAL_FOREST},
      {26, 3000, 28, 0.4, 0.5, ZB_BIOME_RAINFOREST},
  };
  for (size_t k = 0; k < sizeof cases / sizeof cases[0]; ++k) {
    zb_biome got = zb_biome_classify(cases[k].t, cases[k].p, cases[k].warm, cases[k].dry, cases[k].rocky);
    char msg[96];
    snprintf(msg, sizeof msg, "case %u: %s instead of %s", (unsigned)k, zb_biome_name(got),
             zb_biome_name(cases[k].want));
    TEST_ASSERT_EQUAL_INT_MESSAGE(cases[k].want, got, msg);
  }
  /* Every biome has a name and a group. */
  int groups[ZB_GROUP_COUNT] = {0};
  for (int b = 1; b < ZB_BIOME_COUNT; ++b) {
    TEST_ASSERT_NOT_EQUAL(0, strcmp("?", zb_biome_name((zb_biome)b)));
    groups[zb_biome_group_of((zb_biome)b)]++;
  }
  for (int gi = 0; gi < ZB_GROUP_COUNT; ++gi) {
    TEST_ASSERT_TRUE(groups[gi] >= 4);
  }
  TEST_ASSERT_EQUAL_INT(8, groups[ZB_GROUP_MAGICAL]);
  TEST_ASSERT_EQUAL_STRING("?", zb_biome_name(ZB_BIOME_COUNT));
}

/* ---- A real world --------------------------------------------------------------------------- */

typedef struct real {
  zb_geology *geo;
  zb_climate_link link;
  zb_climate *climate;
  const zb_cubegrid *grid;
} real;

static void real_make(real *r, uint64_t seed, int res) {
  zb_geology_options o;
  zb_geology_default_options(&o, RADIUS);
  o.points = 5000;
  o.steps = 100;
  o.macro_res = res;
  o.erosion_iterations = 4;
  char err[128];
  r->geo = zb_geology_create(zb_test_alloc(), seed, &o, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(r->geo, err);
  zb_geology_run(r->geo);
  zb_climate_input in;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_climate_input_from_geology(&in, &r->link, zb_test_alloc(), r->geo, seed));
  r->grid = in.grid;
  r->climate = zb_climate_create(zb_test_alloc(), &in, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(r->climate, err);
  zb_climate_run(r->climate);
}

static void real_free(real *r) {
  zb_climate_destroy(r->climate);
  zb_climate_link_free(&r->link);
  zb_geology_destroy(r->geo);
}

/* spec: climate-and-biomes: Rivers and lakes */
/* spec: climate-and-biomes: Ice and groundwater */
static void test_hydrology(void) {
  real r;
  real_make(&r, 2, 64);
  const uint16_t *f = zb_climate_flags(r.climate);
  const float *q = zb_climate_discharge(r.climate), *gw = zb_climate_groundwater(r.climate);
  const uint8_t *order = zb_climate_river_order(r.climate);
  const zb_geology_macro *m = zb_geology_get_macro(r.geo);
  int mouths = 0, rivers = 0, max_order = 0;
  for (size_t c = 0; c < r.grid->cells; ++c) {
    if (f[c] & ZB_CELL_OCEAN) {
      TEST_ASSERT_EQUAL_UINT8(0, order[c]);
      continue;
    }
    int32_t down = m->receiver[c];
    if (down >= 0 && !(f[c] & ZB_CELL_LAKE) && !(f[down] & ZB_CELL_LAKE)) {
      TEST_ASSERT_TRUE(q[down] >= q[c] - 1e-3f); /* rivers grow downstream (lakes may evaporate) */
    }
    if (f[c] & ZB_CELL_RIVER) {
      rivers++;
      TEST_ASSERT_TRUE(order[c] >= 1);
      TEST_ASSERT_TRUE(q[c] >= 8.0f);
      TEST_ASSERT_EQUAL_FLOAT(1.0f, gw[c]);
      max_order = order[c] > max_order ? order[c] : max_order;
      if (down >= 0 && (f[down] & ZB_CELL_RIVER)) {
        TEST_ASSERT_TRUE(order[down] >= order[c]); /* order never drops downstream */
      }
    }
    if (f[c] & ZB_CELL_RIVER_MOUTH) {
      mouths++;
      TEST_ASSERT_TRUE(f[c] & ZB_CELL_RIVER);
      TEST_ASSERT_TRUE(down >= 0 && (f[down] & ZB_CELL_OCEAN));
    }
    if (f[c] & ZB_CELL_SALT_LAKE) {
      TEST_ASSERT_TRUE(f[c] & ZB_CELL_LAKE);
    }
    if (f[c] & ZB_CELL_OASIS) {
      double t, p;
      zb_climate_annual(r.climate, zb_cubegrid_dir(r.grid, (int32_t)c), m->height[c], &t, &p);
      TEST_ASSERT_TRUE(p < 260.0 && gw[c] < 25.0f);
    }
    if (f[c] & ZB_CELL_GLACIER) {
      double t, p;
      zb_climate_annual(r.climate, zb_cubegrid_dir(r.grid, (int32_t)c), m->height[c], &t, &p);
      TEST_ASSERT_TRUE(t < 2.0);
    }
    TEST_ASSERT_TRUE(gw[c] >= 0.0f);
  }
  TEST_ASSERT_TRUE(rivers > 50);
  TEST_ASSERT_TRUE(mouths > 5);
  TEST_ASSERT_TRUE(max_order >= 3);
  zb_climate_stats st;
  zb_climate_get_stats(r.climate, &st);
  TEST_ASSERT_TRUE(st.glacier_area > 0.0);
  TEST_ASSERT_TRUE(st.lake_area >= st.salt_lake_area);
  real_free(&r);
}

/* spec: climate-and-biomes: Biomes everywhere */
/* spec: climate-and-biomes: Coasts, mountains and wetlands */
static void test_world_biomes(void) {
  real r;
  real_make(&r, 2, 64);
  const uint8_t *b = zb_climate_biomes(r.climate);
  const uint16_t *f = zb_climate_flags(r.climate);
  const zb_geology_macro *m = zb_geology_get_macro(r.geo);
  zb_climate_stats st;
  zb_climate_get_stats(r.climate, &st);
  double land = st.land_area, group_area[ZB_GROUP_COUNT] = {0};
  for (int k = 0; k < ZB_BIOME_COUNT; ++k) {
    group_area[zb_biome_group_of((zb_biome)k)] += st.biome_area[k];
  }
  char msg[160];
  snprintf(msg, sizeof msg, "tropical %.3f arid %.3f temperate %.3f cold %.3f of land",
           group_area[ZB_GROUP_TROPICAL] / land, group_area[ZB_GROUP_ARID] / land,
           group_area[ZB_GROUP_TEMPERATE] / land, group_area[ZB_GROUP_COLD] / land);
  TEST_ASSERT_TRUE_MESSAGE(group_area[ZB_GROUP_TROPICAL] > 0.02 * land && group_area[ZB_GROUP_ARID] > 0.02 * land &&
                               group_area[ZB_GROUP_TEMPERATE] > 0.05 * land && group_area[ZB_GROUP_COLD] > 0.02 * land,
                           msg);
  TEST_ASSERT_TRUE(st.biome_area[ZB_BIOME_DEEP_OCEAN] > 0.0);
  for (size_t c = 0; c < r.grid->cells; ++c) {
    zb_biome bi = (zb_biome)b[c];
    zb_biome_group gr = zb_biome_group_of(bi);
    TEST_ASSERT_TRUE(bi > ZB_BIOME_NONE && bi < ZB_BIOME_COUNT);
    if (f[c] & ZB_CELL_OCEAN) {
      TEST_ASSERT_TRUE(bi <= ZB_BIOME_SEA_ICE); /* oceans keep ocean biomes */
      continue;
    }
    TEST_ASSERT_TRUE(bi > ZB_BIOME_SEA_ICE);
    if (gr == ZB_GROUP_COASTAL) {
      TEST_ASSERT_TRUE(f[c] & ZB_CELL_COAST);
    }
    if (bi == ZB_BIOME_ALPINE_MEADOW || bi == ZB_BIOME_SCREE || bi == ZB_BIOME_MONTANE_FOREST ||
        bi == ZB_BIOME_CLOUD_FOREST) {
      TEST_ASSERT_TRUE(m->height[c] > 1200.0f);
    }
    if (gr == ZB_GROUP_WETLAND) {
      TEST_ASSERT_TRUE(m->height[c] < 250.0f);
    }
    if (bi == ZB_BIOME_LAKE || bi == ZB_BIOME_SALT_LAKE) {
      TEST_ASSERT_TRUE(f[c] & ZB_CELL_LAKE);
    }
  }
  real_free(&r);
}

/* spec: climate-and-biomes: Magical biomes */
static void test_magical_biomes(void) {
  synth s;
  memset(&s, 0, sizeof s);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_cubegrid_init(&s.grid, zb_test_alloc(), 48, RADIUS / 1000.0));
  s.height = zb_alloc(zb_test_alloc(), sizeof(float) * s.grid.cells);
  uint16_t *gf = zb_calloc(zb_test_alloc(), sizeof(uint16_t) * s.grid.cells);
  for (size_t c = 0; c < s.grid.cells; ++c) {
    s.height[c] = (float)continent(zb_cubegrid_dir(&s.grid, (int32_t)c));
  }
  zb_dvec3 nexus[4] = {at(10, 10), at(-20, 40), at(40, 0), at(0, -100)};
  float strength[4] = {1.0f, 0.9f, 0.5f, 1.0f}; /* the third is too weak; the last is at sea */
  uint8_t kind[4] = {0, 1, 2, 0};
  zb_climate_input in;
  memset(&in, 0, sizeof in);
  in.grid = &s.grid;
  in.height = s.height;
  in.geo_flags = gf;
  in.radius = RADIUS;
  in.axial_tilt = 23.0;
  in.nexus = nexus;
  in.nexus_strength = strength;
  in.nexus_kind = kind;
  in.nexus_count = 4;
  char err[64];
  zb_climate *c = zb_climate_create(zb_test_alloc(), &in, err, sizeof err);
  TEST_ASSERT_NOT_NULL(c);
  zb_climate_run(c);
  TEST_ASSERT_EQUAL_INT(ZB_BIOME_HOLLOWS, zb_climate_biome_at(c, nexus[0]));  /* the strongest triple junction */
  TEST_ASSERT_EQUAL_INT(ZB_BIOME_ASHLANDS, zb_climate_biome_at(c, nexus[1])); /* a volcano */
  TEST_ASSERT_NOT_EQUAL(ZB_GROUP_MAGICAL, zb_biome_group_of(zb_climate_biome_at(c, nexus[2])));
  TEST_ASSERT_NOT_EQUAL(ZB_GROUP_MAGICAL, zb_biome_group_of(zb_climate_biome_at(c, at(10, 30)))); /* far away */
  zb_climate_stats st;
  zb_climate_get_stats(c, &st);
  TEST_ASSERT_EQUAL_INT(2, st.magical_patches);
  /* Magical land lies within 15 + 45 * strength km of a strong nexus. */
  const uint8_t *b = zb_climate_biomes(c);
  int magical = 0;
  for (size_t k = 0; k < s.grid.cells; ++k) {
    if (zb_biome_group_of((zb_biome)b[k]) != ZB_GROUP_MAGICAL) {
      continue;
    }
    magical++;
    zb_dvec3 d = zb_cubegrid_dir(&s.grid, (int32_t)k);
    double km0 = zb_dv_angle(d, nexus[0]) * RADIUS / 1000.0, km1 = zb_dv_angle(d, nexus[1]) * RADIUS / 1000.0;
    TEST_ASSERT_TRUE(km0 <= 60.0 || km1 <= 55.5);
  }
  TEST_ASSERT_TRUE(magical >= 2);
  zb_climate_destroy(c);
  zb_free(zb_test_alloc(), gf);
  zb_free(zb_test_alloc(), s.height);
  zb_cubegrid_free(&s.grid);
}

/* spec: climate-and-biomes: Biogeographic provinces */
static void test_provinces(void) {
  real r;
  real_make(&r, 4, 64);
  const uint8_t *map = zb_climate_province_map(r.climate);
  const uint16_t *f = zb_climate_flags(r.climate);
  int count;
  const zb_province *pv = zb_climate_provinces(r.climate, &count);
  TEST_ASSERT_TRUE(count >= 3);
  double area = 0.0;
  for (size_t c = 0; c < r.grid->cells; ++c) {
    if (f[c] & ZB_CELL_OCEAN) {
      TEST_ASSERT_EQUAL_UINT8(0, map[c]);
    } else {
      TEST_ASSERT_TRUE(map[c] >= 1 && map[c] <= count); /* every land cell belongs to one */
      area += (double)zb_cubegrid_area(r.grid, (int32_t)c);
    }
  }
  double sum = 0.0;
  bool island = false, mainland = false;
  for (int p = 0; p < count; ++p) {
    sum += (double)pv[p].area;
    TEST_ASSERT_TRUE(pv[p].isolation >= 0.0f && pv[p].isolation <= 1.0f);
    island = island || pv[p].isolation > 0.5f;
    mainland = mainland || pv[p].isolation < 0.5f;
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, zb_dv_len(pv[p].centre));
  }
  TEST_ASSERT_DOUBLE_WITHIN(area * 1e-4, area, sum);
  TEST_ASSERT_TRUE(island && mainland);
  /* Affinity: itself fully, its landmass more than a far one. */
  TEST_ASSERT_EQUAL_DOUBLE(1.0, zb_climate_province_affinity(r.climate, 0, 0));
  int same = -1, other = -1;
  for (int p = 1; p < count; ++p) {
    if (pv[p].landmass == pv[0].landmass && same < 0) {
      same = p;
    }
    if (pv[p].landmass != pv[0].landmass && other < 0) {
      other = p;
    }
  }
  if (same > 0 && other > 0) {
    TEST_ASSERT_TRUE(zb_climate_province_affinity(r.climate, 0, same) >
                     zb_climate_province_affinity(r.climate, 0, other));
  }
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_climate_province_affinity(r.climate, 0, count));
  /* Provinces keep to their crust lineage and those that met it recently (old terrane boundaries). */
  long agree = 0, total = 0;
  for (size_t c = 0; c < r.grid->cells; ++c) {
    if (map[c] && r.link.lineage[c] && pv[map[c] - 1].lineage) {
      total++;
      int a = r.link.lineage[c], b = pv[map[c] - 1].lineage;
      double met = zb_geology_lineage_contact(r.geo, a, b);
      agree += a == b || (met >= 0.0 && met < 50.0); /* recently joined lineages share provinces */
    }
  }
  char msg[64];
  snprintf(msg, sizeof msg, "%ld of %ld cells on their province's lineage", agree, total);
  TEST_ASSERT_TRUE_MESSAGE(agree * 10 > total * 8, msg);
  real_free(&r);
}

/* spec: climate-and-biomes: Deterministic climate */
static void test_determinism(void) {
  real a, b;
  real_make(&a, 6, 32);
  real_make(&b, 6, 32);
  TEST_ASSERT_EQUAL_UINT64(zb_climate_hash(a.climate), zb_climate_hash(b.climate));
  char hex[17];
  zb_hash_hex(zb_climate_hash(a.climate), hex);
  printf("climate golden hash: %s\n", hex);
  TEST_ASSERT_EQUAL_STRING("df3e9257fba03d62", hex); /* the same on every platform */
  real_free(&a);
  real_free(&b);
}

static void test_progress_and_errors(void) {
  synth s;
  memset(&s, 0, sizeof s);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_cubegrid_init(&s.grid, zb_test_alloc(), 16, RADIUS / 1000.0));
  s.height = zb_alloc(zb_test_alloc(), sizeof(float) * s.grid.cells);
  for (size_t k = 0; k < s.grid.cells; ++k) {
    s.height[k] = 10.0f; /* all land */
  }
  zb_climate_input in;
  memset(&in, 0, sizeof in);
  char err[128];
  TEST_ASSERT_NULL(zb_climate_create(zb_test_alloc(), &in, err, sizeof err));
  in.grid = &s.grid;
  in.height = s.height;
  in.radius = 10.0;
  in.axial_tilt = 23.0;
  TEST_ASSERT_NULL(zb_climate_create(zb_test_alloc(), &in, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("radius out of range", err);
  in.radius = RADIUS;
  in.axial_tilt = 90.0;
  TEST_ASSERT_NULL(zb_climate_create(zb_test_alloc(), &in, err, sizeof err));
  in.axial_tilt = 23.0;
  zb_climate *c = zb_climate_create(zb_test_alloc(), &in, err, sizeof err);
  TEST_ASSERT_NOT_NULL(c);
  TEST_ASSERT_NULL(zb_climate_biomes(c)); /* nothing before it is done */
  int pcount;
  TEST_ASSERT_NULL(zb_climate_provinces(c, &pcount));
  TEST_ASSERT_EQUAL_INT(ZB_BIOME_NONE, zb_climate_biome_at(c, at(0, 0)));
  zb_climate_stats st;
  zb_climate_get_stats(c, &st);
  TEST_ASSERT_EQUAL_INT(0, st.cells);
  double last = -1.0;
  int steps = 0;
  while (zb_climate_advance(c) != ZB_CLIMATE_PHASE_DONE) {
    TEST_ASSERT_TRUE(zb_climate_progress(c) > last);
    last = zb_climate_progress(c);
    steps++;
  }
  TEST_ASSERT_EQUAL_INT(ZB_MONTHS + 2, steps);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, zb_climate_progress(c));
  TEST_ASSERT_EQUAL_INT(ZB_CLIMATE_PHASE_DONE, zb_climate_advance(c));
  TEST_ASSERT_NOT_NULL(zb_climate_grid(c));
  zb_climate_get_stats(c, &st);
  TEST_ASSERT_EQUAL_INT((int)s.grid.cells, st.land_cells); /* a world without ocean */
  zb_climate_destroy(c);
  zb_climate_destroy(NULL);
  /* Out of memory at every allocation of creation. */
  for (long budget = 0; budget < 40; ++budget) {
    zb_failing_allocator fa;
    zb_climate *f = zb_climate_create(zb_failing_init(&fa, zb_test_alloc(), budget), &in, err, sizeof err);
    if (f) {
      zb_climate_run(f);
      zb_climate_destroy(f);
      break;
    }
    TEST_ASSERT_EQUAL_STRING("out of memory", err);
  }
  /* The geology link needs a finished world. */
  zb_geology_options o;
  zb_geology_default_options(&o, RADIUS);
  o.points = 600;
  o.steps = 10;
  zb_geology *g = zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err);
  zb_climate_link link;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, zb_climate_input_from_geology(&in, &link, zb_test_alloc(), g, 1));
  zb_climate_link_free(&link);
  zb_geology_destroy(g);
  zb_free(zb_test_alloc(), s.height);
  zb_cubegrid_free(&s.grid);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_insolation_and_seasons);
  RUN_TEST(test_continentality);
  RUN_TEST(test_lapse_rate);
  RUN_TEST(test_rain_shadow);
  RUN_TEST(test_rain_belts);
  RUN_TEST(test_classify);
  RUN_TEST(test_hydrology);
  RUN_TEST(test_world_biomes);
  RUN_TEST(test_magical_biomes);
  RUN_TEST(test_provinces);
  RUN_TEST(test_determinism);
  RUN_TEST(test_progress_and_errors);
  return UNITY_END();
}

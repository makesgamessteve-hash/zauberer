/* SPDX-License-Identifier: MIT */
#include "core/hash.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/dvec.h"
#include "world/geology_internal.h"

#include <stdio.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

#define RADIUS 2000000.0

static zb_geology_options small_options(int points, int steps) {
  zb_geology_options o;
  zb_geology_default_options(&o, RADIUS);
  o.points = points;
  o.steps = steps;
  o.macro_res = 16;
  o.erosion_iterations = 2;
  return o;
}

static zb_geology *make(const zb_geology_options *o, uint64_t seed) {
  char err[128];
  zb_geology *g = zb_geology_create(zb_test_alloc(), seed, o, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(g, err);
  return g;
}

static double brute_nearest_dot(const zb_lattice *l, zb_dvec3 d, int *index) {
  double best = -2.0;
  for (int i = 0; i < l->n; ++i) {
    double dot = zb_dv_dot(l->p[i], d);
    if (dot > best) {
      best = dot;
      *index = i;
    }
  }
  return best;
}

/* spec: deep-time-geology: Even lattice */
static void test_lattice(void) {
  zb_lattice l;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_lattice_init(&l, zb_test_alloc(), 10));
  zb_lattice_free(&l); /* safe on a failed init */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_lattice_init(&l, zb_test_alloc(), 3000));
  double ideal = sqrt(4.0 * ZB_GEO_PI / 3000.0);
  TEST_ASSERT_TRUE(l.spacing > 0.8 * ideal && l.spacing < 1.2 * ideal);
  TEST_ASSERT_EQUAL_DOUBLE(4.0 * ZB_GEO_PI / 3000.0, zb_lattice_cell_area(&l));
  for (int i = 0; i < l.n; ++i) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, zb_dv_len(l.p[i]));
    double nearest = zb_dv_angle(l.p[i], l.p[zb_lattice_nbr(&l, i, 0)]);
    TEST_ASSERT_TRUE(nearest > 0.5 * l.spacing && nearest < 1.5 * l.spacing);
    for (int k = 1; k < ZB_LATTICE_K; ++k) { /* nearest first */
      TEST_ASSERT_TRUE(zb_dv_angle(l.p[i], l.p[zb_lattice_nbr(&l, i, k)]) >= nearest - 1e-12);
    }
  }
  zb_rng r = zb_rng_make(9, 1);
  for (int q = 0; q < 4000; ++q) {
    double z = 2.0 * zb_rng_next_double(&r) - 1.0, phi = 6.283185307 * zb_rng_next_double(&r);
    zb_dvec3 d = zb_dv(sqrt(1 - z * z) * zb_dcos(phi), sqrt(1 - z * z) * zb_dsin(phi), z);
    int want = -1;
    double best = brute_nearest_dot(&l, d, &want);
    int hint = q % 3 == 0 ? -1 : (int)zb_rng_range(&r, 0, l.n - 1);
    int got = zb_lattice_nearest(&l, d, hint);
    TEST_ASSERT_EQUAL_DOUBLE(best, zb_dv_dot(l.p[got], d));
  }
  zb_lattice_free(&l);
}

/* spec: deep-time-geology: Rock record */
static void test_strata(void) {
  zb_stratum col[ZB_STRATA];
  memset(col, 0, sizeof col);
  zb_geo_add_stratum(col, ZB_ROCK_SANDSTONE, ZB_ENV_RIVER, 0.2, 400.0); /* too thin: ignored */
  TEST_ASSERT_EQUAL_UINT16(0, col[0].thickness);
  zb_geo_add_stratum(col, ZB_ROCK_SANDSTONE, ZB_ENV_RIVER, 100.0, 400.0);
  zb_geo_add_stratum(col, ZB_ROCK_SANDSTONE, ZB_ENV_RIVER, 50.0, 380.0); /* same rock soon after: merges */
  TEST_ASSERT_EQUAL_UINT16(150, col[0].thickness);
  TEST_ASSERT_EQUAL_UINT16(380, col[0].age);
  zb_geo_add_stratum(col, ZB_ROCK_LIMESTONE, ZB_ENV_SHALLOW_SEA, 70.0, 300.0);
  TEST_ASSERT_EQUAL_UINT8(ZB_ROCK_LIMESTONE, col[0].rock);
  TEST_ASSERT_EQUAL_UINT8(ZB_ROCK_SANDSTONE, col[1].rock);
  for (int k = 0; k < 10; ++k) { /* overflow: the oldest strata merge at the bottom */
    zb_geo_add_stratum(col, k % 2 ? ZB_ROCK_SHALE : ZB_ROCK_COAL, ZB_ENV_SWAMP, 10.0, 290.0 - k * 10.0);
  }
  int total = 0;
  for (int k = 0; k < ZB_STRATA; ++k) {
    total += col[k].thickness;
  }
  TEST_ASSERT_EQUAL_INT(150 + 70 + 100, total); /* nothing lost */
  TEST_ASSERT_TRUE(col[ZB_STRATA - 1].thickness > 0);
  for (int k = 1; k < ZB_STRATA; ++k) {
    TEST_ASSERT_TRUE(col[k].age >= col[k - 1].age); /* youngest on top */
  }
  zb_geo_add_stratum(col, ZB_ROCK_SALT, ZB_ENV_SALT_BASIN, 1e9, 10.0); /* clamped */
  TEST_ASSERT_EQUAL_UINT16(65535, col[0].thickness);
  TEST_ASSERT_EQUAL_DOUBLE(65535.0, zb_geo_erode_strata(col, 65535.0));
  TEST_ASSERT_EQUAL_DOUBLE(5.0, zb_geo_erode_strata(col, 5.0));
  double before = zb_geo_sediment_thickness(col);
  TEST_ASSERT_TRUE(before > 0.0);
  double removed = zb_geo_erode_strata(col, 1e7); /* everything */
  TEST_ASSERT_EQUAL_DOUBLE(before, removed);
  TEST_ASSERT_EQUAL_UINT16(0, col[0].thickness);
  /* Metamorphism. */
  zb_stratum m[ZB_STRATA];
  memset(m, 0, sizeof m);
  static const zb_rock in[] = {ZB_ROCK_GRANITE, ZB_ROCK_SANDSTONE, ZB_ROCK_LIMESTONE, ZB_ROCK_SHALE, ZB_ROCK_BASALT};
  static const zb_rock out[] = {ZB_ROCK_GNEISS, ZB_ROCK_QUARTZITE, ZB_ROCK_MARBLE, ZB_ROCK_SCHIST, ZB_ROCK_BASALT};
  for (int k = 0; k < 5; ++k) {
    zb_geo_add_stratum(m, in[k], ZB_ENV_RIVER, 10.0, 500.0 - k * 100.0);
  }
  zb_geo_metamorphose(m);
  for (int k = 0; k < 5; ++k) {
    TEST_ASSERT_EQUAL_UINT8(out[k], m[4 - k].rock);
  }
  TEST_ASSERT_EQUAL_UINT8(ZB_ENV_MOUNTAIN_ROOT, m[1].env);
  TEST_ASSERT_EQUAL_UINT8(ZB_ENV_RIVER, m[0].env); /* basalt stays */
}

/* spec: deep-time-geology: Fossils by age and setting */
static void test_fossils(void) {
  zb_stratum s = {ZB_ROCK_LIMESTONE, ZB_ENV_SHALLOW_SEA, 100, 120};
  TEST_ASSERT_EQUAL_INT(ZB_FOSSIL_SEA_CREATURES, zb_stratum_fossil(&s, 1));
  s.env = ZB_ENV_MOUNTAIN_ROOT;
  TEST_ASSERT_EQUAL_INT(ZB_FOSSIL_NONE, zb_stratum_fossil(&s, 1));
  zb_stratum beasts = {ZB_ROCK_SANDSTONE, ZB_ENV_RIVER, 100, 150};
  TEST_ASSERT_EQUAL_INT(ZB_FOSSIL_GIANT_BEASTS, zb_stratum_fossil(&beasts, 1));
  beasts.age = 30;
  TEST_ASSERT_EQUAL_INT(ZB_FOSSIL_NONE, zb_stratum_fossil(&beasts, 1));
  zb_stratum coal = {ZB_ROCK_COAL, ZB_ENV_SWAMP, 30, 100};
  TEST_ASSERT_EQUAL_INT(ZB_FOSSIL_AMBER, zb_stratum_fossil(&coal, 1));
  coal.age = 280;
  TEST_ASSERT_EQUAL_INT(ZB_FOSSIL_PLANTS, zb_stratum_fossil(&coal, 1));
  zb_stratum basalt = {ZB_ROCK_BASALT, ZB_ENV_VOLCANIC, 30, 400};
  TEST_ASSERT_EQUAL_INT(ZB_FOSSIL_NONE, zb_stratum_fossil(&basalt, 1));
  zb_stratum empty = {0};
  TEST_ASSERT_EQUAL_INT(ZB_FOSSIL_NONE, zb_stratum_fossil(&empty, 1));
  /* Old sediment glows sometimes: some salts give mana fossils, some do not. */
  zb_stratum old = {ZB_ROCK_SHALE, ZB_ENV_DEEP_SEA, 100, 420};
  int mana = 0;
  for (uint64_t salt = 0; salt < 200; ++salt) {
    mana += zb_stratum_fossil(&old, salt) == ZB_FOSSIL_MANA;
  }
  TEST_ASSERT_TRUE(mana > 20 && mana < 180);
  TEST_ASSERT_EQUAL_STRING("mana fossils", zb_fossil_name(ZB_FOSSIL_MANA));
  TEST_ASSERT_EQUAL_STRING("?", zb_fossil_name(ZB_FOSSIL_COUNT));
  TEST_ASSERT_EQUAL_STRING("marble", zb_rock_name(ZB_ROCK_MARBLE));
  TEST_ASSERT_EQUAL_STRING("?", zb_rock_name(ZB_ROCK_COUNT));
  TEST_ASSERT_EQUAL_STRING("salt basin", zb_rock_env_name(ZB_ENV_SALT_BASIN));
  TEST_ASSERT_EQUAL_STRING("?", zb_rock_env_name(ZB_ENV_COUNT));
  TEST_ASSERT_EQUAL_STRING("copper", zb_mineral_name(ZB_MIN_COPPER));
  TEST_ASSERT_EQUAL_STRING("mana crystal", zb_mineral_name(ZB_MIN_MANA_CRYSTAL));
  TEST_ASSERT_EQUAL_STRING("?", zb_mineral_name(3));
  TEST_ASSERT_EQUAL_STRING("pangaea", zb_geology_target_name(ZB_GEO_TARGET_PANGAEA));
  TEST_ASSERT_EQUAL_STRING("?", zb_geology_target_name(ZB_GEO_TARGET_COUNT));
}

/* ---- Hand-made plate set-ups ------------------------------------------------------------ */

static double rate(double cm_per_year) { return cm_per_year * 10.0 / ZB_GEO_REF_RADIUS_KM; }

/* Two plates split by the plane x = 0 that turn about the z axis in opposite
 * directions: they converge near +y and diverge near -y. */
static zb_geology *two_plates(bool cont0, bool cont1, double cm) {
  zb_geology_options o = small_options(4000, 40);
  o.max_retries = 0;
  zb_geology *g = make(&o, 3);
  g->min_plates = 0;
  g->rift_share = 10.0;
  g->rift_chance = 0.0;
  g->hotspot_count = 0;
  for (int i = 0; i < g->n; ++i) {
    int p = g->lat.p[i].x < 0.0 ? 0 : 1;
    bool cont = p == 0 ? cont0 : cont1;
    g->cur.plate[i] = (uint8_t)p;
    g->cur.continental[i] = cont;
    g->cur.thickness[i] = cont ? 35.0f : 7.0f;
    g->cur.born[i] = cont ? 2000.0f : 520.0f;
    g->cur.orogeny[i] = -1.0f;
    g->cur.glacial[i] = -1.0f;
    g->cur.flags[i] = 0;
    g->cur.basement[i] = cont ? ZB_ROCK_GRANITE : ZB_ROCK_BASALT;
    g->cur.lineage[i] = (uint8_t)(cont ? p + 1 : 0);
    zb_stratum *col = &g->cur.strata[(size_t)i * ZB_STRATA];
    memset(col, 0, sizeof(zb_stratum) * ZB_STRATA);
    zb_geo_add_stratum(col, cont ? ZB_ROCK_LIMESTONE : ZB_ROCK_PELAGIC_CLAY, ZB_ENV_SHALLOW_SEA, 200.0, 520.0);
  }
  memset(g->plates, 0, sizeof g->plates);
  for (int k = 0; k < ZB_GEO_LINEAGES * ZB_GEO_LINEAGES; ++k) {
    g->contact[k] = -1.0f;
  }
  g->plates[0] = (zb_geo_plate){true, zb_dv(0, 0, -1), rate(cm), 0};
  g->plates[1] = (zb_geo_plate){true, zb_dv(0, 0, 1), rate(cm), 0};
  zb_geo_update_elevation(g);
  return g;
}

static void steps(zb_geology *g, int count) {
  for (int s = 0; s < count; ++s) {
    zb_geo_step(g);
  }
}

static int count_flag(const zb_geology *g, uint16_t flag) {
  int c = 0;
  for (int i = 0; i < g->n; ++i) {
    c += (g->cur.flags[i] & flag) != 0;
  }
  return c;
}

static bool column_has(const zb_geology *g, int i, zb_rock rock) {
  for (int k = 0; k < ZB_STRATA; ++k) {
    const zb_stratum *s = &g->cur.strata[(size_t)i * ZB_STRATA + (size_t)k];
    if (s->thickness > 0 && s->rock == rock) {
      return true;
    }
  }
  return false;
}

/* spec: deep-time-geology: Plates drift */
static void test_drift(void) {
  zb_geology *g = two_plates(true, true, 5.0);
  g->plates[1].alive = false; /* one plate turns as a whole */
  for (int i = 0; i < g->n; ++i) {
    g->cur.plate[i] = 0;
  }
  int marked = zb_lattice_nearest(&g->lat, zb_dv(1, 0, 0), -1);
  g->cur.basement[marked] = ZB_ROCK_GABBRO;
  for (int k = 0; k < 6; ++k) { /* drift is not wiped by the perturbation of the pole */
    g->plates[0].axis = zb_dv(0, 0, 1);
    zb_geo_step(g);
  }
  int found = -1;
  for (int i = 0; i < g->n; ++i) {
    if (g->cur.basement[i] == ZB_ROCK_GABBRO) {
      found = i;
    }
  }
  TEST_ASSERT_TRUE(found >= 0);
  double moved = zb_dv_angle(g->lat.p[marked], g->lat.p[found]);
  double expected = 6.0 * g->dt * rate(5.0);
  TEST_ASSERT_TRUE(moved > 0.5 * expected && moved < 1.6 * expected);
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Continental collision */
/* spec: deep-time-geology: Lineages remember meetings */
static void test_collision(void) {
  zb_geology *g = two_plates(true, true, 6.0);
  float before = -1e9f;
  for (int i = 0; i < g->n; ++i) {
    before = g->elevation[i] > before ? g->elevation[i] : before;
  }
  steps(g, 1);
  int collided = 0, metamorphic = 0;
  float top = -1e9f;
  for (int i = 0; i < g->n; ++i) {
    if (g->cur.flags[i] & ZB_GEO_COLLISION) {
      collided++;
      TEST_ASSERT_TRUE(g->lat.p[i].y > 0.0); /* where the plates converge */
      metamorphic += column_has(g, i, ZB_ROCK_MARBLE) || g->cur.basement[i] == ZB_ROCK_GNEISS;
      TEST_ASSERT_TRUE(g->cur.orogeny[i] >= 0.0f);
    }
    top = g->elevation[i] > top ? g->elevation[i] : top;
  }
  TEST_ASSERT_TRUE(collided > 10);
  TEST_ASSERT_TRUE(metamorphic > collided / 2);
  /* The two continents' lineages remember when they met. */
  double met = zb_geology_lineage_contact(g, 1, 2);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, g->opt.span_myr - g->dt, met);
  TEST_ASSERT_EQUAL_DOUBLE(met, zb_geology_lineage_contact(g, 2, 1));
  TEST_ASSERT_EQUAL_DOUBLE(-1.0, zb_geology_lineage_contact(g, 1, 1));
  TEST_ASSERT_EQUAL_DOUBLE(-1.0, zb_geology_lineage_contact(g, 0, 2));
  TEST_ASSERT_EQUAL_DOUBLE(-1.0, zb_geology_lineage_contact(g, 1, ZB_GEO_LINEAGES));
  TEST_ASSERT_TRUE(top > before + 2000.0f);
  /* Sustained collision welds the plates; the seam is a suture. */
  steps(g, 4);
  int alive = 0;
  for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
    alive += g->plates[k].alive;
  }
  TEST_ASSERT_EQUAL_INT(1, alive);
  TEST_ASSERT_TRUE(count_flag(g, ZB_GEO_SUTURE) > 0);
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Subduction builds arcs */
static void test_subduction(void) {
  zb_geology *g = two_plates(false, true, 6.0);
  steps(g, 3);
  int arcs = 0, andesite = 0, trench = 0;
  for (int i = 0; i < g->n; ++i) {
    if (g->cur.flags[i] & ZB_GEO_ARC) {
      arcs++;
      TEST_ASSERT_EQUAL_UINT8(1, g->cur.continental[i]); /* on the overriding continent */
      andesite += column_has(g, i, ZB_ROCK_ANDESITE);
    }
    if (g->cur.flags[i] & ZB_GEO_TRENCH) {
      trench++;
      TEST_ASSERT_EQUAL_UINT8(0, g->cur.continental[i]);
      TEST_ASSERT_TRUE(g->elevation[i] < -5000.0f);
    }
  }
  TEST_ASSERT_TRUE(arcs > 10);
  TEST_ASSERT_TRUE(andesite > arcs / 2); /* high arcs erode their own lavas */
  TEST_ASSERT_TRUE(trench > 5);
  zb_geo_compute_minerals(g);
  for (int i = 0; i < g->n; ++i) {
    if (g->cur.flags[i] & ZB_GEO_ARC) {
      TEST_ASSERT_TRUE(g->minerals[i] & ZB_MIN_COPPER);
    } else {
      TEST_ASSERT_FALSE(g->minerals[i] & ZB_MIN_COPPER);
    }
  }
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Spreading ridges */
static void test_spreading(void) {
  zb_geology *g = two_plates(false, false, 5.0);
  steps(g, 3);
  int ridge = 0;
  double now = g->opt.span_myr - (double)g->step * g->dt;
  for (int i = 0; i < g->n; ++i) {
    if ((g->cur.flags[i] & ZB_GEO_RIDGE) && (double)g->cur.born[i] - now < 1.0) {
      ridge++;
      TEST_ASSERT_TRUE(g->lat.p[i].y < 0.0); /* where the plates diverge */
      TEST_ASSERT_FLOAT_WITHIN(800.0f, -2600.0f, g->elevation[i]);
    }
  }
  TEST_ASSERT_TRUE(ridge > 10);
  /* Older floor further from the ridge is deeper. */
  float young = 0.0f, old = 0.0f;
  int ny = 0, no = 0;
  for (int i = 0; i < g->n; ++i) {
    double age = (double)g->cur.born[i] - now;
    if (age < 5.0) {
      young += g->elevation[i];
      ny++;
    } else if (age > 20.0) {
      old += g->elevation[i];
      no++;
    }
  }
  TEST_ASSERT_TRUE(ny > 0 && no > 0 && young / (float)ny > old / (float)no);
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Hotspot tracks */
static void test_hotspot_track(void) {
  zb_geology *g = two_plates(false, false, 8.0);
  g->plates[1].alive = false;
  for (int i = 0; i < g->n; ++i) {
    g->cur.plate[i] = 0;
  }
  g->hotspot_count = 1;
  g->hotspots[0] = (zb_geo_hotspot){zb_dv(1, 0, 0), 1000.0f, -1.0f};
  steps(g, 12);
  int count = 0, basalt = 0;
  double spread = 0.0;
  for (int i = 0; i < g->n; ++i) {
    if (!(g->cur.flags[i] & ZB_GEO_HOTSPOT)) {
      continue;
    }
    count++;
    basalt += column_has(g, i, ZB_ROCK_BASALT);
    for (int j = 0; j < g->n; ++j) {
      if ((g->cur.flags[j] & ZB_GEO_HOTSPOT) && zb_dv_angle(g->lat.p[i], g->lat.p[j]) > spread) {
        spread = zb_dv_angle(g->lat.p[i], g->lat.p[j]);
      }
    }
  }
  TEST_ASSERT_TRUE(count >= 5);
  TEST_ASSERT_TRUE(basalt > 0);                    /* islands above the sea erode theirs */
  TEST_ASSERT_TRUE(spread > 4.0 * g->lat.spacing); /* a chain, not a pile */
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Rifting splits a continent */
static void test_rifting(void) {
  zb_geology *g = two_plates(true, true, 0.0);
  for (int i = 0; i < g->n; ++i) {
    g->cur.plate[i] = 0;
  }
  g->plates[1].alive = false;
  TEST_ASSERT_TRUE(zb_geo_rift_plate(g, 0, zb_dv(0, 0, 1)));
  int moved = 0, rift = 0;
  for (int i = 0; i < g->n; ++i) {
    if (g->cur.plate[i] != 0) {
      moved++;
      TEST_ASSERT_TRUE(g->lat.p[i].z > 0.0);
    }
    if (g->cur.flags[i] & ZB_GEO_RIFT) {
      rift++;
      TEST_ASSERT_TRUE(fabs(g->lat.p[i].z) < 0.15);
    }
  }
  TEST_ASSERT_TRUE(moved > g->n / 3 && moved < 2 * g->n / 3);
  TEST_ASSERT_TRUE(rift > 20);
  steps(g, 4); /* the halves drift apart: a young ocean opens in the rift */
  int ridge = 0;
  for (int i = 0; i < g->n; ++i) {
    ridge += (g->cur.flags[i] & ZB_GEO_RIDGE) && fabs(g->lat.p[i].z) < 0.3;
  }
  TEST_ASSERT_TRUE(ridge > 10);
  /* A plate of one point cannot split. */
  g->cur.plate[0] = 5;
  g->plates[5].alive = true;
  TEST_ASSERT_FALSE(zb_geo_rift_plate(g, 5, zb_dv(1, 0, 0)));
  /* No free plate id. */
  for (int k = 0; k < ZB_GEO_MAX_PLATES; ++k) {
    g->plates[k].alive = true;
  }
  TEST_ASSERT_FALSE(zb_geo_rift_plate(g, 0, zb_dv(0, 1, 0)));
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Erosion wears ranges down */
static void test_erosion(void) {
  zb_geology *g = two_plates(true, true, 0.0);
  g->plates[0].rate = g->plates[1].rate = 0.0;
  int peak = zb_lattice_nearest(&g->lat, zb_dv(0, 0, 1), -1);
  g->cur.thickness[peak] = 65.0f;
  for (int k = 0; k < 6; ++k) {
    g->cur.thickness[zb_lattice_nbr(&g->lat, peak, k)] = 60.0f;
  }
  zb_geo_update_elevation(g);
  float high = g->elevation[peak];
  steps(g, 10);
  TEST_ASSERT_TRUE(g->elevation[peak] < high - 300.0f);
  /* The eroded rock settled somewhere lower as new strata. */
  int deposits = 0;
  for (int i = 0; i < g->n; ++i) {
    const zb_stratum *top = &g->cur.strata[(size_t)i * ZB_STRATA];
    deposits += top->thickness > 0 && top->age < 520 &&
                (top->rock == ZB_ROCK_SANDSTONE || top->rock == ZB_ROCK_CONGLOMERATE || top->rock == ZB_ROCK_SHALE ||
                 top->rock == ZB_ROCK_LIMESTONE || top->rock == ZB_ROCK_RED_SANDSTONE || top->rock == ZB_ROCK_SALT ||
                 top->rock == ZB_ROCK_COAL || top->rock == ZB_ROCK_TILL);
  }
  TEST_ASSERT_TRUE(deposits > 0);
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Ice ages leave their mark */
static void test_glaciation(void) {
  zb_geology *g = two_plates(true, true, 0.0);
  g->plates[0].rate = g->plates[1].rate = 0.0;
  for (int i = 0; i < g->n; ++i) {
    g->cur.thickness[i] = 38.0f + (float)(i % 10) * 0.3f; /* land nearly everywhere */
  }
  g->opt.land_fraction = 0.95;
  g->temp_offset = -5.0; /* a deep ice age */
  steps(g, 1);
  TEST_ASSERT_TRUE(g->eras[0].ice_age);
  int high = 0;
  for (int i = 0; i < g->n; ++i) {
    double z = fabs(g->lat.p[i].z);
    if (g->cur.flags[i] & ZB_GEO_GLACIATED) {
      TEST_ASSERT_TRUE(z > 0.5); /* ice sheets grow towards the poles */
      TEST_ASSERT_TRUE(column_has(g, i, ZB_ROCK_TILL));
      high += z > 0.8;
    } else {
      TEST_ASSERT_TRUE(z < 0.82 || g->elevation[i] <= g->sea_level + 200.0f); /* high land is under ice */
    }
  }
  TEST_ASSERT_TRUE(high > 100);
  /* A warm era leaves no new ice. */
  g->temp_offset = 5.0;
  for (int i = 0; i < g->n; ++i) {
    g->cur.flags[i] &= (uint16_t)~ZB_GEO_GLACIATED;
  }
  steps(g, 1);
  TEST_ASSERT_FALSE(g->eras[1].ice_age);
  TEST_ASSERT_EQUAL_INT(0, count_flag(g, ZB_GEO_GLACIATED));
  zb_geology_destroy(g);
}

/* ---- Whole runs ---------------------------------------------------------------------------- */

/* spec: deep-time-geology: Sediments record their environment */
/* spec: deep-time-geology: Minerals by geological event */
static void test_full_run(void) {
  zb_geology_options o = small_options(5000, 100);
  zb_geology *g = make(&o, 11);
  zb_geology_run(g);
  zb_geology_points pts;
  zb_geology_get_points(g, &pts);
  TEST_ASSERT_EQUAL_INT(5000, pts.n);
  int coal = 0, salt = 0, till = 0, glaciated = 0;
  for (int i = 0; i < pts.n; ++i) {
    const zb_stratum *col = &pts.strata[(size_t)i * ZB_STRATA];
    for (int k = 0; k < ZB_STRATA; ++k) {
      if (k > 0 && col[k].thickness > 0) {
        TEST_ASSERT_TRUE(col[k].age >= col[k - 1].age);
      }
      if (col[k].thickness == 0) {
        continue;
      }
      switch (col[k].rock) {
        case ZB_ROCK_COAL:
          coal++;
          TEST_ASSERT_TRUE(col[k].age <= 360);
          TEST_ASSERT_TRUE(col[k].env == ZB_ENV_SWAMP || col[k].env == ZB_ENV_MOUNTAIN_ROOT);
          break;
        case ZB_ROCK_SALT:
        case ZB_ROCK_GYPSUM:
          salt++;
          TEST_ASSERT_TRUE(col[k].env == ZB_ENV_SALT_BASIN || col[k].env == ZB_ENV_MOUNTAIN_ROOT);
          break;
        case ZB_ROCK_RED_SANDSTONE: TEST_ASSERT_EQUAL_UINT8(ZB_ENV_DESERT, col[k].env); break;
        case ZB_ROCK_TILL:
          till++;
          TEST_ASSERT_EQUAL_UINT8(ZB_ENV_GLACIAL, col[k].env);
          break;
        case ZB_ROCK_ANDESITE: TEST_ASSERT_EQUAL_UINT8(ZB_ENV_VOLCANIC, col[k].env); break;
        case ZB_ROCK_CHALK: TEST_ASSERT_TRUE(col[k].age <= 145); break;
        default: break;
      }
    }
    glaciated += (pts.flags[i] & ZB_GEO_GLACIATED) != 0;
    uint32_t m = pts.minerals[i];
    if (m & ZB_MIN_COPPER) {
      TEST_ASSERT_TRUE(pts.flags[i] & ZB_GEO_ARC);
    }
    if (m & ZB_MIN_TIN) {
      TEST_ASSERT_TRUE(pts.flags[i] & ZB_GEO_COLLISION);
    }
    if (m & ZB_MIN_IRON) {
      TEST_ASSERT_TRUE(pts.flags[i] & ZB_GEO_CRATON);
    }
    if (m & ZB_MIN_SALT) {
      TEST_ASSERT_TRUE(column_has(g, i, ZB_ROCK_SALT));
    }
    if (m & ZB_MIN_COAL) {
      TEST_ASSERT_TRUE(column_has(g, i, ZB_ROCK_COAL));
    }
    if (m & ZB_MIN_SULFUR) {
      TEST_ASSERT_TRUE(pts.flags[i] & ZB_GEO_ACTIVE_VOLCANO);
    }
    if (m & ZB_MIN_MANA_CRYSTAL) {
      TEST_ASSERT_TRUE(pts.flags[i] & ZB_GEO_SUTURE);
    }
  }
  TEST_ASSERT_TRUE(coal > 0);
  TEST_ASSERT_TRUE(salt > 0);
  int cont = 0, traced = 0;
  for (int i = 0; i < pts.n; ++i) {
    if (pts.continental[i]) {
      cont++;
      traced += pts.lineage[i] != 0;
    } else if (pts.flags[i] & ZB_GEO_RIDGE) {
      TEST_ASSERT_EQUAL_UINT8(0, pts.lineage[i]); /* new ocean floor has no lineage */
    }
  }
  char msg[64];
  snprintf(msg, sizeof msg, "%d of %d continental points traced", traced, cont);
  TEST_ASSERT_TRUE_MESSAGE(traced > cont * 9 / 10, msg); /* continents remember where they came from */
  int count;
  const zb_geology_era *eras = zb_geology_eras(g, &count);
  TEST_ASSERT_EQUAL_INT(100, count);
  int ice_ages = 0;
  for (int s = 0; s < count; ++s) {
    ice_ages += eras[s].ice_age;
    TEST_ASSERT_TRUE(eras[s].plates >= 2);
    if (s > 0) {
      TEST_ASSERT_TRUE(eras[s].myr_ago < eras[s - 1].myr_ago);
    }
  }
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, eras[count - 1].myr_ago);
  TEST_ASSERT_TRUE(ice_ages > 0 && ice_ages < count);
  TEST_ASSERT_TRUE(till > 0 && glaciated > 0); /* the marks drift with their crust afterwards */
  int found[ZB_MIN_COUNT] = {0};
  for (int i = 0; i < pts.n; ++i) {
    for (int b = 0; b < ZB_MIN_COUNT; ++b) {
      found[b] += (int)((pts.minerals[i] >> (unsigned)b) & 1u);
    }
  }
  TEST_ASSERT_TRUE(found[0] > 0 && found[1] > 0 && found[4] > 0 && found[5] > 0 && found[9] > 0);
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Earth-like worlds */
static void test_earth_like_corpus(void) {
  for (uint64_t seed = 1; seed <= 4; ++seed) {
    zb_geology_options o = small_options(4000, 100);
    o.macro_res = 8;
    o.erosion_iterations = 0;
    zb_geology *g = make(&o, seed);
    zb_geology_run(g);
    const zb_geology_metrics *m = zb_geology_get_metrics(g);
    char msg[256];
    snprintf(msg, sizeof msg,
             "seed %u: land %.3f continents %d mountains %d young %d old %d ice %.3f bands %u cont %.2f",
             (unsigned)seed, m->land_fraction, m->continents, m->continents_with_mountains, m->young_ranges,
             m->old_ranges, m->ice_fraction, m->climate_bands, m->continental_land);
    TEST_ASSERT_TRUE_MESSAGE(m->passed, msg);
    TEST_ASSERT_FLOAT_WITHIN(0.02, 0.35, m->land_fraction); /* ties at the waterline */
    TEST_ASSERT_TRUE(m->continents >= 3 && m->continents <= 8);
    TEST_ASSERT_TRUE(m->attempts >= 1 && m->attempts <= 4);
    zb_geology_destroy(g);
  }
}

/* spec: deep-time-geology: Other kinds of world */
static void test_targets(void) {
  for (int t = ZB_GEO_TARGET_PANGAEA; t < ZB_GEO_TARGET_COUNT; ++t) {
    zb_geology_options o = small_options(4000, 100);
    o.macro_res = 8;
    o.erosion_iterations = 0;
    o.target = (zb_geology_target)t;
    if (t == ZB_GEO_TARGET_ARCHIPELAGO) {
      o.land_fraction = 0.2;
    }
    zb_geology *g = make(&o, 2);
    zb_geology_run(g);
    const zb_geology_metrics *m = zb_geology_get_metrics(g);
    char msg[160];
    snprintf(msg, sizeof msg, "%s: continents %d islands %d largest %.3f ice %.3f score %.2f",
             zb_geology_target_name((zb_geology_target)t), m->continents, m->islands, m->largest_landmass,
             m->ice_fraction, m->score);
    TEST_ASSERT_TRUE_MESSAGE(m->passed, msg);
    switch (t) {
      case ZB_GEO_TARGET_PANGAEA: TEST_ASSERT_TRUE(m->continents <= 2); break;
      case ZB_GEO_TARGET_ARCHIPELAGO: TEST_ASSERT_TRUE(m->largest_landmass < 0.05); break;
      case ZB_GEO_TARGET_ICE_WORLD: TEST_ASSERT_TRUE(m->ice_fraction >= 0.15); break;
      default: TEST_ASSERT_TRUE(m->ice_fraction < 0.02); break;
    }
    zb_geology_destroy(g);
  }
}

/* spec: deep-time-geology: Steering re-runs the final phase */
static void test_steering(void) {
  zb_geology_options o = small_options(3000, 40);
  o.land_fraction = 0.9; /* never Earth-like */
  o.max_retries = 2;
  o.macro_res = 8;
  o.erosion_iterations = 0;
  zb_geology *g = make(&o, 5);
  zb_geology_progress p;
  int restarts = 0, last = -1;
  while (zb_geology_advance(g) != ZB_GEO_PHASE_DONE) {
    zb_geology_get_progress(g, &p);
    restarts += p.phase == ZB_GEO_PHASE_TECTONICS && p.step < last;
    last = p.step;
  }
  const zb_geology_metrics *m = zb_geology_get_metrics(g);
  TEST_ASSERT_FALSE(m->passed);
  TEST_ASSERT_EQUAL_INT(3, m->attempts);
  TEST_ASSERT_EQUAL_INT(2, restarts); /* back to 75 % of the span twice */
  TEST_ASSERT_TRUE(m->score > 0.0);
  int count;
  zb_geology_eras(g, &count);
  TEST_ASSERT_EQUAL_INT(40, count);
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Macro grid */
static void test_macro_grid(void) {
  zb_geology_options o = small_options(4000, 60);
  o.macro_res = 40;
  o.erosion_iterations = 0;
  zb_geology *raw = make(&o, 8);
  zb_geology_run(raw);
  o.erosion_iterations = 8;
  zb_geology *g = make(&o, 8);
  TEST_ASSERT_NULL(zb_geology_get_macro(g)); /* not before it is done */
  zb_geology_run(g);
  const zb_geology_macro *m = zb_geology_get_macro(g), *r = zb_geology_get_macro(raw);
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_INT(40, m->res);
  size_t cells = (size_t)6 * 40 * 40;
  double area = 0.0, land = 0.0, relief_raw = 0.0, relief = 0.0;
  for (size_t c = 0; c < cells; ++c) {
    double a = (double)m->area[c % 1600];
    area += a;
    land += m->height[c] > 0.0f ? a : 0.0;
    if (m->height[c] > 0.0f && r->height[c] > 0.0f) {
      relief += (double)m->height[c];
      relief_raw += (double)r->height[c];
    }
    TEST_ASSERT_TRUE(m->point[c] >= 0 && m->point[c] < 4000);
    TEST_ASSERT_TRUE(m->lake[c] >= 0.0f);
    if (m->lake[c] > 0.0f) { /* lakes fill inland basins, some below sea level */
      TEST_ASSERT_TRUE(m->receiver[c] >= 0);
    }
  }
  double sphere = 4.0 * ZB_GEO_PI * 2000.0 * 2000.0;
  TEST_ASSERT_DOUBLE_WITHIN(sphere * 1e-3, sphere, area);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.35, land / area);
  TEST_ASSERT_TRUE(relief < relief_raw); /* erosion lowers the land */
  /* Every land cell drains to the sea; drainage grows downstream. */
  float biggest = 0.0f;
  for (size_t c = 0; c < cells; ++c) {
    if (m->height[c] <= 0.0f) {
      continue;
    }
    int32_t at = (int32_t)c;
    int guard = 0;
    while (m->height[at] > 0.0f && guard++ < (int)cells) {
      int32_t next = m->receiver[at];
      TEST_ASSERT_TRUE(next >= 0);
      TEST_ASSERT_TRUE(m->drainage[next] >= m->drainage[at]);
      at = next;
    }
    TEST_ASSERT_TRUE(m->height[at] <= 0.0f);
    biggest = m->drainage[c] > biggest ? m->drainage[c] : biggest;
  }
  TEST_ASSERT_TRUE(biggest > 50.0f * m->area[0]); /* rivers gather large basins */
  /* Sampling: at a cell centre, the cell's height. */
  for (int32_t c = 0; c < (int32_t)cells; c += 97) {
    int y = (c / 40) % 40, x = c % 40;
    if (x == 0 || y == 0 || x == 39 || y == 39) {
      continue;
    }
    TEST_ASSERT_DOUBLE_WITHIN(1.0, (double)m->height[c], zb_geology_height(g, zb_geo_macro_dir(g, c)));
  }
  /* Neighbours across cube edges are symmetric (except at corners). */
  int asymmetric = 0;
  for (int32_t c = 0; c < (int32_t)cells; ++c) {
    for (int k = 0; k < 4; ++k) {
      int32_t nb = zb_geo_macro_nbr(g, c, k);
      TEST_ASSERT_TRUE(nb >= 0);
      bool back = false;
      for (int j = 0; j < 4; ++j) {
        back = back || zb_geo_macro_nbr(g, nb, j) == c;
      }
      asymmetric += !back;
    }
  }
  TEST_ASSERT_TRUE(asymmetric <= 24); /* only next to the eight cube corners */
  TEST_ASSERT_EQUAL_INT(zb_geology_point_at(g, g->lat.p[17]), 17);
  zb_geology_destroy(raw);
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Ley network */
static void test_ley_network(void) {
  zb_geology_options o = small_options(5000, 100);
  o.macro_res = 8;
  o.erosion_iterations = 0;
  zb_geology *g = make(&o, 4);
  zb_geology_run(g);
  zb_ley_network ley;
  zb_geology_get_ley(g, &ley);
  TEST_ASSERT_TRUE(ley.nodes >= 5);
  TEST_ASSERT_TRUE(ley.edges >= ley.nodes - 1);
  for (int a = 0; a < ley.nodes; ++a) {
    TEST_ASSERT_TRUE(ley.node[a].strength > 0.0f && ley.node[a].strength <= 1.0f);
    for (int b = a + 1; b < ley.nodes; ++b) {
      TEST_ASSERT_TRUE(zb_dv_angle(g->lat.p[ley.node[a].point], g->lat.p[ley.node[b].point]) > 0.3);
    }
  }
  /* The tree reaches every node; paths are chains of neighbours. */
  int parent[64];
  for (int k = 0; k < ley.nodes; ++k) {
    parent[k] = k;
  }
  int loops = 0, on_structure = 0, path_points = 0;
  for (int e = 0; e < ley.edges; ++e) {
    const zb_ley_edge *ed = &ley.edge[e];
    const int32_t *path = ley.path + ed->path;
    TEST_ASSERT_EQUAL_INT(ley.node[ed->a].point, path[0]);
    TEST_ASSERT_EQUAL_INT(ley.node[ed->b].point, path[ed->length - 1]);
    for (int q = 1; q < ed->length; ++q) {
      bool adjacent = false;
      for (int k = 0; k < 6; ++k) {
        adjacent = adjacent || zb_lattice_nbr(&g->lat, path[q - 1], k) == path[q];
      }
      TEST_ASSERT_TRUE(adjacent);
    }
    for (int q = 0; q < ed->length; ++q) {
      on_structure += (g->cur.flags[path[q]] & (ZB_GEO_SUTURE | ZB_GEO_RIFT | ZB_GEO_HOTSPOT | ZB_GEO_COLLISION |
                                                ZB_GEO_ARC | ZB_GEO_BOUNDARY)) != 0;
      path_points++;
    }
    loops += ed->loop;
    int ra = ed->a, rb = ed->b;
    while (parent[ra] != ra) {
      ra = parent[ra];
    }
    while (parent[rb] != rb) {
      rb = parent[rb];
    }
    parent[ra] = rb;
  }
  int roots = 0;
  for (int k = 0; k < ley.nodes; ++k) {
    roots += parent[k] == k;
  }
  TEST_ASSERT_TRUE(roots >= 1 && roots <= 2); /* nearest-neighbour candidates may leave one island */
  TEST_ASSERT_TRUE(loops >= 1);
  int anywhere =
      count_flag(g, ZB_GEO_SUTURE | ZB_GEO_RIFT | ZB_GEO_HOTSPOT | ZB_GEO_COLLISION | ZB_GEO_ARC | ZB_GEO_BOUNDARY);
  /* Ley lines follow old structures more than the planet at large. */
  TEST_ASSERT_TRUE((double)on_structure / path_points > (double)anywhere / g->n);
  zb_geology_destroy(g);
}

/* spec: deep-time-geology: Deterministic everywhere */
static void test_determinism(void) {
  zb_geology_options o = small_options(3000, 30);
  zb_geology *a = make(&o, 42), *b = make(&o, 42), *c = make(&o, 43);
  zb_geology_run(a);
  zb_geology_run(b);
  zb_geology_run(c);
  char hex[17];
  zb_hash_hex(zb_geology_hash(a), hex);
  printf("geology golden hash: %s\n", hex);
  TEST_ASSERT_EQUAL_UINT64(zb_geology_hash(a), zb_geology_hash(b));
  TEST_ASSERT_NOT_EQUAL_UINT64(zb_geology_hash(a), zb_geology_hash(c));
  TEST_ASSERT_EQUAL_STRING("9edd690a29818df7", hex); /* the same on every platform */
  zb_geology_destroy(a);
  zb_geology_destroy(b);
  zb_geology_destroy(c);
}

/* spec: deep-time-geology: Progress and cancellation */
static void test_progress(void) {
  zb_geology_options o = small_options(2000, 20);
  zb_geology *g = make(&o, 1);
  zb_geology_progress p;
  zb_geology_get_progress(g, &p);
  TEST_ASSERT_EQUAL_INT(ZB_GEO_PHASE_TECTONICS, p.phase);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, p.fraction);
  TEST_ASSERT_EQUAL_DOUBLE(500.0, p.myr_ago);
  double last = 0.0;
  bool saw[ZB_GEO_PHASE_DONE + 1] = {false};
  while (zb_geology_advance(g) != ZB_GEO_PHASE_DONE) {
    zb_geology_get_progress(g, &p);
    saw[p.phase] = true;
    if (p.attempt == 0) {
      TEST_ASSERT_TRUE(p.fraction >= last);
      last = p.fraction;
    }
  }
  zb_geology_get_progress(g, &p);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, p.fraction);
  TEST_ASSERT_TRUE(saw[ZB_GEO_PHASE_MACRO] || saw[ZB_GEO_PHASE_EROSION]);
  TEST_ASSERT_EQUAL_INT(ZB_GEO_PHASE_DONE, zb_geology_advance(g)); /* stays done */
  zb_geology_destroy(g);
  /* Cancelling: destroying a half-run world frees everything. */
  g = make(&o, 1);
  for (int k = 0; k < 7; ++k) {
    zb_geology_advance(g);
  }
  zb_geology_destroy(g);
  zb_geology_destroy(NULL);
}

static void test_options(void) {
  zb_geology_options o;
  char err[128];
  zb_geology_default_options(&o, RADIUS);
  o.radius = 10.0;
  TEST_ASSERT_NULL(zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING("radius out of range", err);
  zb_geology_default_options(&o, RADIUS);
  o.points = 100;
  TEST_ASSERT_NULL(zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err));
  zb_geology_default_options(&o, RADIUS);
  o.steps = 5;
  TEST_ASSERT_NULL(zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err));
  zb_geology_default_options(&o, RADIUS);
  o.span_myr = 10.0;
  TEST_ASSERT_NULL(zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err));
  zb_geology_default_options(&o, RADIUS);
  o.plates = 1;
  TEST_ASSERT_NULL(zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err));
  zb_geology_default_options(&o, RADIUS);
  o.land_fraction = 0.0;
  TEST_ASSERT_NULL(zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err));
  zb_geology_default_options(&o, RADIUS);
  o.target = ZB_GEO_TARGET_COUNT;
  TEST_ASSERT_NULL(zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err));
  zb_geology_default_options(&o, RADIUS);
  o.max_retries = 11;
  TEST_ASSERT_NULL(zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err));
  zb_geology_default_options(&o, RADIUS);
  o.macro_res = 4;
  TEST_ASSERT_NULL(zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err));
  zb_geology_default_options(&o, RADIUS);
  o.erosion_iterations = -1;
  TEST_ASSERT_NULL(zb_geology_create(zb_test_alloc(), 1, &o, err, sizeof err));
  /* The macro resolution follows the radius: about 6 km cells, at least 16. */
  zb_geology_default_options(&o, RADIUS);
  o.points = 600;
  o.steps = 10;
  zb_geology *g = make(&o, 1);
  TEST_ASSERT_EQUAL_INT(524, g->opt.macro_res);
  zb_geology_destroy(g);
  o.radius = 50000.0;
  o.plates = 4;
  g = make(&o, 1);
  TEST_ASSERT_EQUAL_INT(16, g->opt.macro_res);
  TEST_ASSERT_EQUAL_INT(4, g->opt.plates);
  zb_geology_destroy(g);
  /* Out of memory at every allocation of creation. */
  for (long budget = 0; budget < 40; ++budget) {
    zb_failing_allocator fa;
    zb_geology *f = zb_geology_create(zb_failing_init(&fa, zb_test_alloc(), budget), 1, &o, err, sizeof err);
    if (f) {
      zb_geology_destroy(f);
      break;
    }
    TEST_ASSERT_EQUAL_STRING("out of memory", err);
  }
}

/* Out of memory later on: macro allocation fails, the lattice result stays. */
static void test_out_of_memory_later(void) {
  zb_geology_options o = small_options(2000, 20);
  o.macro_res = 32;
  for (long extra = 0; extra < 60; extra += 3) {
    zb_failing_allocator fa;
    char err[64];
    zb_geology *g = zb_geology_create(zb_failing_init(&fa, zb_test_alloc(), -1), 1, &o, err, sizeof err);
    TEST_ASSERT_NOT_NULL(g);
    while (g->phase == ZB_GEO_PHASE_TECTONICS) {
      zb_geology_advance(g);
    }
    fa.budget = fa.calls + extra; /* fail somewhere in the macro phases */
    zb_geology_run(g);
    TEST_ASSERT_EQUAL_INT(ZB_GEO_PHASE_DONE, g->phase);
    zb_geology_destroy(g);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_lattice);
  RUN_TEST(test_strata);
  RUN_TEST(test_fossils);
  RUN_TEST(test_drift);
  RUN_TEST(test_collision);
  RUN_TEST(test_subduction);
  RUN_TEST(test_spreading);
  RUN_TEST(test_hotspot_track);
  RUN_TEST(test_rifting);
  RUN_TEST(test_erosion);
  RUN_TEST(test_glaciation);
  RUN_TEST(test_full_run);
  RUN_TEST(test_earth_like_corpus);
  RUN_TEST(test_targets);
  RUN_TEST(test_steering);
  RUN_TEST(test_macro_grid);
  RUN_TEST(test_ley_network);
  RUN_TEST(test_determinism);
  RUN_TEST(test_progress);
  RUN_TEST(test_options);
  RUN_TEST(test_out_of_memory_later);
  return UNITY_END();
}

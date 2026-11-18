/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "sim/tier.h"
#include "support/test_support.h"
#include "unity.h"
#include "world/delta.h"
#include "world/dvec.h"
#include "world/ley.h"
#include "world/stream.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static zb_flora g_flora;
static zb_materials *g_materials;

static char *read_asset(const char *rel, size_t *len) {
  char path[512];
  snprintf(path, sizeof path, "%s/assets/base/%s", zb_test_source_dir(), rel);
  char *text = zb_test_read_file(path, len);
  TEST_ASSERT_NOT_NULL_MESSAGE(text, path);
  return text;
}

static void load_data(void) {
  if (g_materials) {
    return;
  }
  size_t sl, al, ml;
  char *species = read_asset("data/flora.lua", &sl), *atlas = read_asset("sprites/flora.lua", &al);
  char err[160];
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      ZB_OK, zb_flora_parse(&g_flora, zb_heap_allocator(), species, sl, atlas, al, err, sizeof err), err);
  free(species);
  free(atlas);
  char *text = read_asset("data/materials.lua", &ml);
  zb_luon_doc doc;
  zb_luon_error le;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_luon_parse(&doc, zb_heap_allocator(), text, ml, &le));
  free(text);
  zb_registry r;
  zb_registry_init(&r, zb_heap_allocator(), "materials");
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_registry_merge(&r, "base", &doc.root));
  zb_luon_free(&doc);
  g_materials = malloc(sizeof *g_materials);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_materials_build(g_materials, &r, err, sizeof err));
}

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

typedef struct scene {
  zb_world *world;
  zb_tier *tier;
  zb_mana *mana;
  zb_planet_pos centre;
} scene;

typedef enum ground { HALVES, NATURAL } ground;

/* West half bare rock, east half soil with the chunk's own plants. */
static void reshape(zb_chunk_terrain *ct) {
  for (int y = 0; y < ZB_HEIGHT_RES; ++y) {
    for (int x = 0; x < ZB_HEIGHT_RES; ++x) {
      ct->height[y * ZB_HEIGHT_RES + x] = 50.0f;
    }
  }
  for (int y = 0; y < ZB_SURFACE_RES; ++y) {
    for (int x = 0; x < ZB_SURFACE_RES; ++x) {
      ct->material[y * ZB_SURFACE_RES + x] = x < 128 ? ZB_MAT_ROCK : ZB_MAT_SOIL;
    }
  }
  for (uint32_t k = 0; k < ct->plant_count; ++k) {
    if (ct->plants[k].x < 32768u) {
      ct->plants[k].layer = ZB_PLANT_REMOVED;
    }
  }
}

static const zb_weather CALM = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 15.0};

/* A world at dir (default: a temperate forest) with the focus chunk loaded
 * with its plants, and the active tier (one chunk) over it. */
static void scene_open_at(scene *s, ground g, const zb_dvec3 *at) {
  load_data();
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  d.profile = ZB_STREAM_MOBILE;
  d.clock_ns = frozen_clock;
  d.flora = &g_flora;
  char err[128];
  s->world = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(s->world, err);
  const zb_planet *p = zb_world_planet(s->world);
  double lat = 46.0 * ZB_DPI / 180.0, lon = 0.3;
  zb_dvec3 dir = at ? *at : zb_dv(zb_dcos(lat) * zb_dcos(lon), zb_dcos(lat) * zb_dsin(lon), zb_dsin(lat));
  zb_stream_focus f;
  memset(&f, 0, sizeof f);
  f.pos = zb_planet_from_cart(p, zb_dv_scale(dir, p->radius));
  if (!at) {
    f.pos.lx = f.pos.ly = 128.0f;
  }
  s->centre = f.pos;
  zb_world_set_focus(s->world, 0, &f);
  for (int i = 0; i < 3000; ++i) {
    zb_world_update(s->world);
    const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(s->world), f.pos.chunk);
    if (ct && ct->stage >= ZB_STAGE_ECOLOGY) {
      break;
    }
  }
  zb_chunk_terrain *ct = zb_stream_edit(zb_world_stream(s->world), f.pos.chunk);
  TEST_ASSERT_TRUE(ct && ct->stage >= ZB_STAGE_ECOLOGY);
  if (g == HALVES) {
    reshape(ct);
  }
  zb_tier_config tc = {zb_test_alloc(), s->world, g_materials, 1.0, 7, NULL, NULL};
  s->tier = zb_tier_create(&tc);
  TEST_ASSERT_NOT_NULL(s->tier);
  zb_weather_control *wc = zb_tier_weather(s->tier);
  wc->overridden = true;
  wc->now = CALM;
  zb_tier_apply_weather(s->tier, &CALM);
  s->mana = zb_tier_mana(s->tier);
  zb_tier_update(s->tier, 0.0);
}

static void scene_open(scene *s, ground g) { scene_open_at(s, g, NULL); }

static void scene_close(scene *s) {
  zb_tier_destroy(s->tier);
  zb_world_destroy(s->world);
}

static void run(scene *s, double seconds) {
  for (double t = 0.0; t < seconds - 1e-9; t += 0.25) {
    zb_tier_update(s->tier, 0.25);
  }
}

static zb_planet_pos at(const scene *s, float x, float y) {
  zb_planet_pos p = s->centre;
  p.lx = x;
  p.ly = y;
  return p;
}

static zb_mana_sample sample(const scene *s, float x, float y) {
  zb_mana_sample m;
  zb_planet_pos p = at(s, x, y);
  zb_mana_sample_at(s->mana, &p, &m);
  TEST_ASSERT_TRUE(m.loaded);
  return m;
}

/* The game time of the next local noon (want_day) or midnight at the scene. */
static double time_of(scene *s, bool want_day) {
  const zb_planet *p = zb_world_planet(s->world);
  zb_dvec3 dir = zb_dv_norm(zb_planet_to_cart(p, &s->centre));
  double t0 = zb_world_time(s->world), best = t0, best_v = -1.0;
  for (double t = t0; t < t0 + 86400.0; t += 900.0) {
    zb_sky_place sky;
    zb_calendar_sky(zb_world_calendar(s->world), t, dir, &sky);
    double v = want_day ? sky.daylight : 1.0 - sky.daylight;
    if (v > best_v) {
      best_v = v;
      best = t;
    }
  }
  return best;
}

/* spec: mana: Ley lines and nexuses */
static void test_ley(void) {
  load_data();
  zb_terrain t;
  zb_planet p;
  TEST_ASSERT_TRUE(zb_planet_init(&p, ZB_PLANET_DEFAULT_RADIUS, NULL, 0));
  zb_terrain_init(&t, &p, 5, NULL);
  static zb_ley a, b;
  zb_ley_build(&a, &t, p.radius, 5);
  zb_ley_build(&b, &t, p.radius, 5);
  TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof a); /* the same world, the same network */
  TEST_ASSERT_TRUE(a.lines >= ZB_LEY_NEXUSES && a.lines <= ZB_LEY_MAX_LINES);
  /* Nexuses sit on high ground: higher than random places on average. */
  double nexus_h = 0.0, random_h = 0.0;
  for (int k = 0; k < ZB_LEY_NEXUSES; ++k) {
    nexus_h += zb_terrain_macro_height(&t, a.nexus[k].dir);
    double z = -1.0 + (k + 0.5) * 2.0 / ZB_LEY_NEXUSES, r = sqrt(1.0 - z * z), phi = k * 2.399963;
    random_h += zb_terrain_macro_height(&t, zb_dv(r * zb_dcos(phi), r * zb_dsin(phi), z));
    TEST_ASSERT_TRUE(a.nexus[k].colors[0] != a.nexus[k].colors[1] && a.nexus[k].colors[1] < 8);
    TEST_ASSERT_TRUE(a.nexus[k].power >= 0.5f && a.nexus[k].power <= 1.0f);
  }
  TEST_ASSERT_TRUE(nexus_h > random_h);
  /* At a nexus and halfway along a line: on the network. */
  zb_ley_sample s;
  zb_ley_sample_at(&a, a.nexus[3].dir, &s);
  TEST_ASSERT_EQUAL_INT(3, s.nexus);
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 0.0, s.nexus_distance);
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 0.0, s.line_distance);
  zb_dvec3 mid = zb_dv_norm(zb_dv_add(a.nexus[a.line[0].a].dir, a.nexus[a.line[0].b].dir));
  zb_ley_sample_at(&a, mid, &s);
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 0.0, s.line_distance);
  TEST_ASSERT_TRUE(s.nexus_distance > 1000.0);
  int ca, cb;
  zb_ley_line_colors(&a, 0, &ca, &cb);
  TEST_ASSERT_EQUAL_INT(a.nexus[a.line[0].a].colors[0], ca);
  TEST_ASSERT_EQUAL_INT(a.nexus[a.line[0].b].colors[0], cb);
  /* Another seed, another network. */
  zb_ley_build(&b, &t, p.radius, 6);
  TEST_ASSERT_FALSE(zb_dv_dot(a.nexus[0].dir, b.nexus[0].dir) > 0.999999);
}

/* spec: mana: The world shapes the mana */
static void test_sources(void) {
  scene s;
  scene_open(&s, HALVES);
  /* Bare rock in the west, plants on soil in the east. */
  zb_mana_sample rock = sample(&s, 48.0f, 128.0f), green = sample(&s, 208.0f, 128.0f);
  TEST_ASSERT_TRUE_MESSAGE(rock.cap[ZB_MANA_EARTH] > 1.5 * green.cap[ZB_MANA_EARTH], "rock is Earth");
  TEST_ASSERT_TRUE_MESSAGE(green.cap[ZB_MANA_LIFE] > 2.0 * rock.cap[ZB_MANA_LIFE], "plants are Life");
  for (int k = 0; k < ZB_MANA_COLORS; ++k) {
    TEST_ASSERT_TRUE(rock.cap[k] >= 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, rock.cap[k], rock.stored[k]); /* found full */
  }
  /* Day and night: Light by day, Void by night. */
  zb_world_set_time(s.world, time_of(&s, true));
  run(&s, 3.0);
  zb_mana_sample day = sample(&s, 208.0f, 128.0f);
  zb_world_set_time(s.world, time_of(&s, false));
  run(&s, 3.0);
  zb_mana_sample night = sample(&s, 208.0f, 128.0f);
  TEST_ASSERT_TRUE(day.cap[ZB_MANA_LIGHT] > 2.0 * night.cap[ZB_MANA_LIGHT]);
  TEST_ASSERT_TRUE(night.cap[ZB_MANA_VOID] > day.cap[ZB_MANA_VOID] + 5.0);
  /* A thunderstorm feeds Storm, rain feeds Water. */
  zb_weather storm = CALM;
  storm.storm = 1.0;
  storm.rain = 40.0;
  storm.cloud = 1.0;
  zb_tier_weather(s.tier)->now = storm;
  zb_tier_apply_weather(s.tier, &storm);
  run(&s, 3.0);
  zb_mana_sample wet = sample(&s, 208.0f, 128.0f);
  TEST_ASSERT_TRUE(wet.cap[ZB_MANA_STORM] > night.cap[ZB_MANA_STORM] + 30.0);
  TEST_ASSERT_TRUE(wet.cap[ZB_MANA_WATER] > night.cap[ZB_MANA_WATER] + 30.0);
  zb_tier_weather(s.tier)->now = CALM;
  zb_tier_apply_weather(s.tier, &CALM);
  /* Fire on the plants: a Fire spike. */
  zb_planet_pos blaze = at(&s, 208.0f, 128.0f);
  double before = sample(&s, 208.0f, 128.0f).cap[ZB_MANA_FIRE];
  TEST_ASSERT_TRUE(zb_fire_ignite(zb_tier_fire(s.tier), &blaze, 14.0) > 0);
  run(&s, 3.0);
  TEST_ASSERT_TRUE_MESSAGE(sample(&s, 208.0f, 128.0f).cap[ZB_MANA_FIRE] > before + 20.0, "burning is Fire");
  /* A pond: Water. */
  zb_planet_pos pond = at(&s, 48.0f, 208.0f);
  double dry = sample(&s, 48.0f, 208.0f).cap[ZB_MANA_WATER];
  zb_surface_water_add(zb_tier_water(s.tier), &pond, 20.0, 600.0);
  run(&s, 3.0);
  TEST_ASSERT_TRUE_MESSAGE(sample(&s, 48.0f, 208.0f).cap[ZB_MANA_WATER] > dry + 15.0, "water is Water");
  /* New ground: a mound of earth raises Earth and Storm where it stands. */
  double flat = sample(&s, 208.0f, 48.0f).cap[ZB_MANA_EARTH];
  zb_planet_pos hill = at(&s, 208.0f, 48.0f);
  zb_chunk changed[9];
  int n = zb_world_deform(s.world, &hill, 30.0, 25.0, changed, 9);
  zb_tier_ground_changed(s.tier, changed, n);
  TEST_ASSERT_TRUE(sample(&s, 208.0f, 48.0f).cap[ZB_MANA_EARTH] > flat);
  zb_tier_stats ts;
  zb_tier_get_stats(s.tier, &ts);
  TEST_ASSERT_TRUE(ts.ms[ZB_TIER_MANA] >= 0.0);
  scene_close(&s);
}

/* spec: mana: Mana fills, spreads and flows along ley lines */
static void test_refill(void) {
  scene s;
  scene_open(&s, HALVES);
  zb_planet_pos p = at(&s, 48.0f, 48.0f);
  zb_mana_sample full = sample(&s, 48.0f, 48.0f);
  double cap = full.cap[ZB_MANA_EARTH];
  TEST_ASSERT_TRUE(cap > 10.0);
  double got = zb_mana_draw(s.mana, &p, ZB_MANA_EARTH, cap * 0.9, 0.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, cap * 0.9, got);
  TEST_ASSERT_DOUBLE_WITHIN(0.01 * cap, cap * 0.1, sample(&s, 48.0f, 48.0f).stored[ZB_MANA_EARTH]);
  /* It fills again over minutes, and its neighbours give a little. */
  zb_mana_sample beside0 = sample(&s, 80.0f, 48.0f);
  run(&s, 300.0);
  double back = sample(&s, 48.0f, 48.0f).stored[ZB_MANA_EARTH];
  TEST_ASSERT_TRUE_MESSAGE(back > 0.6 * cap && back < cap, "most of it is back");
  TEST_ASSERT_TRUE(sample(&s, 80.0f, 48.0f).stored[ZB_MANA_EARTH] <= beside0.stored[ZB_MANA_EARTH] + 1e-3);
  /* Released mana adds to the cell and settles back to its cap. */
  zb_mana_release(s.mana, &p, ZB_MANA_FIRE, 50.0);
  zb_mana_sample over = sample(&s, 48.0f, 48.0f);
  TEST_ASSERT_TRUE(over.stored[ZB_MANA_FIRE] > over.cap[ZB_MANA_FIRE] + 40.0);
  run(&s, 900.0);
  zb_mana_sample settled = sample(&s, 48.0f, 48.0f);
  TEST_ASSERT_TRUE(settled.stored[ZB_MANA_FIRE] < settled.cap[ZB_MANA_FIRE] + 5.0);
  scene_close(&s);
  /* At a nexus: its colours and Void are rich, and the ley lines carry mana
   * faster than plain ground. */
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = zb_test_alloc();
  d.seed = 1;
  char err[128];
  zb_world *w = zb_world_create(&d, err, sizeof err);
  TEST_ASSERT_NOT_NULL(w);
  zb_ley ley = *zb_world_ley(w);
  int k = 0;
  while (k < ZB_LEY_NEXUSES - 1 && zb_terrain_macro_height(zb_world_terrain(w), ley.nexus[k].dir) < 5.0) {
    k++; /* a nexus on land */
  }
  zb_world_destroy(w);
  scene_open_at(&s, NATURAL, &ley.nexus[k].dir);
  zb_mana_sample nexus = sample(&s, s.centre.lx, s.centre.ly);
  TEST_ASSERT_TRUE(nexus.nexus_distance < 30.0 && nexus.ley_distance < 30.0);
  TEST_ASSERT_TRUE_MESSAGE(nexus.cap[ley.nexus[k].colors[0]] > 30.0, "the nexus colour is rich");
  TEST_ASSERT_TRUE(nexus.cap[ZB_MANA_VOID] > 30.0);
  zb_mana_stats st;
  zb_mana_get_stats(s.mana, &st);
  TEST_ASSERT_TRUE(st.chunks >= 1);
  scene_close(&s);
}

/* spec: mana: Drawing depletes and overdrawing blights */
static void test_blight(void) {
  scene s;
  scene_open(&s, HALVES);
  zb_planet_pos p = at(&s, 208.0f, 208.0f);
  double life = sample(&s, 208.0f, 208.0f).cap[ZB_MANA_LIFE];
  TEST_ASSERT_TRUE(life > 5.0);
  /* A modest draw from around a place takes from several cells. */
  double got = zb_mana_draw(s.mana, &p, ZB_MANA_LIFE, 5.0, 40.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, got);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_mana_draw(s.mana, &p, ZB_MANA_LIFE, -1.0, 0.0));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_mana_draw(s.mana, &p, ZB_MANA_COLORS, 1.0, 0.0));
  zb_planet_pos away = p;
  away.chunk.x += 40;
  TEST_ASSERT_EQUAL_DOUBLE(0.0, zb_mana_draw(s.mana, &away, ZB_MANA_LIFE, 1.0, 0.0));
  zb_mana_sample none;
  zb_mana_sample_at(s.mana, &away, &none);
  TEST_ASSERT_FALSE(none.loaded);
  /* Draining one cell hard, again and again: blight. */
  zb_mana_stats st;
  zb_mana_draw(s.mana, &p, ZB_MANA_LIFE, life * 0.5, 0.0);
  zb_mana_get_stats(s.mana, &st);
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, st.blighted, "half a cell is fine");
  for (int k = 0; k < 4; ++k) {
    zb_mana_draw(s.mana, &p, ZB_MANA_LIFE, 1000.0, 0.0); /* far more than there is */
    run(&s, 2.0);
  }
  zb_mana_get_stats(s.mana, &st);
  TEST_ASSERT_EQUAL_INT(1, st.blighted);
  TEST_ASSERT_TRUE(st.drawn > 60.0);
  zb_mana_sample b = sample(&s, 208.0f, 208.0f);
  TEST_ASSERT_TRUE(b.blighted);
  TEST_ASSERT_TRUE_MESSAGE(b.cap[ZB_MANA_LIFE] < 0.5 * life, "little Life");
  TEST_ASSERT_TRUE_MESSAGE(b.cap[ZB_MANA_DECAY] > 30.0, "much Decay");
  size_t n;
  const zb_edit *e = zb_deltas_get(zb_world_deltas(s.world), s.centre.chunk, &n);
  TEST_ASSERT_TRUE(n >= 1 && e[n - 1].kind == ZB_EDIT_BLIGHT);
  /* The blight is kept with the world: a new field finds it, two months later
   * it is gone. */
  zb_mana_set_world(s.mana, s.world);
  run(&s, 1.0);
  TEST_ASSERT_TRUE(sample(&s, 208.0f, 208.0f).blighted);
  zb_world_set_time(s.world, zb_world_time(s.world) + ZB_BLIGHT_SECONDS + 86400.0);
  run(&s, 3.0);
  TEST_ASSERT_FALSE(sample(&s, 208.0f, 208.0f).blighted);
  scene_close(&s);
}

/* spec: mana: Deterministic mana */
static void test_deterministic(void) {
  uint64_t h[2];
  for (int k = 0; k < 2; ++k) {
    scene s;
    scene_open(&s, HALVES);
    zb_planet_pos p = at(&s, 100.0f, 100.0f);
    zb_mana_draw(s.mana, &p, ZB_MANA_EARTH, 30.0, 60.0);
    run(&s, 20.0);
    h[k] = zb_mana_hash(s.mana);
    scene_close(&s);
  }
  TEST_ASSERT_EQUAL_UINT64(h[0], h[1]);
  zb_mana_destroy(NULL);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ley);
  RUN_TEST(test_sources);
  RUN_TEST(test_refill);
  RUN_TEST(test_blight);
  RUN_TEST(test_deterministic);
  int failures = UNITY_END();
  if (g_materials) {
    zb_materials_free(g_materials);
    free(g_materials);
  }
  return failures;
}

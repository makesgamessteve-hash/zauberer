/* SPDX-License-Identifier: MIT */
#include "core/dmath.h"
#include "sim/creatures.h"
#include "sim/tier.h"
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
static zb_flora g_flora;

static void load_materials(void) {
  if (g_materials) {
    return;
  }
  char path[512];
  snprintf(path, sizeof path, "%s/assets/base/data/flora.lua", zb_test_source_dir());
  size_t sl, al;
  char *species = zb_test_read_file(path, &sl);
  snprintf(path, sizeof path, "%s/assets/base/sprites/flora.lua", zb_test_source_dir());
  char *atlas = zb_test_read_file(path, &al);
  TEST_ASSERT_TRUE(species && atlas);
  char ferr[160];
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      ZB_OK, zb_flora_parse(&g_flora, zb_heap_allocator(), species, sl, atlas, al, ferr, sizeof ferr), ferr);
  free(species);
  free(atlas);
  snprintf(path, sizeof path, "%s/assets/base/data/materials.lua", zb_test_source_dir());
  size_t len;
  char *text = zb_test_read_file(path, &len);
  TEST_ASSERT_NOT_NULL(text);
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

static uint64_t frozen_clock(void *ctx) {
  (void)ctx;
  return 0;
}

typedef struct scene {
  zb_world *world;
  zb_tier *tier;
  zb_creatures *cr;
  zb_planet_pos centre;
  int wolf, deer, bird;
} scene;

typedef enum ground { MEADOW, POND } ground;

static void reshape(zb_chunk_terrain *ct, ground g) {
  for (int y = 0; y < ZB_HEIGHT_RES; ++y) {
    for (int x = 0; x < ZB_HEIGHT_RES; ++x) {
      double dx = x - 200.0, dy = y - 128.0;
      ct->height[y * ZB_HEIGHT_RES + x] = g == POND && dx * dx + dy * dy < 30.0 * 30.0 ? 45.0f : 50.0f;
    }
  }
  memset(ct->material, ZB_MAT_SOIL, ZB_SURFACE_RES * ZB_SURFACE_RES);
  ct->plant_count = 0; /* open ground: grass to burn, no trees */
}

static zb_animal animal(const char *id, double length, double walk, double run, zb_diet diet) {
  zb_animal a;
  memset(&a, 0, sizeof a);
  snprintf(a.id, sizeof a.id, "%s", id);
  a.length = length;
  a.mass = 40.0;
  a.walk = walk;
  a.run = run;
  a.diet = diet;
  a.sight = 80.0;
  a.hearing = 120.0;
  a.smell = 200.0;
  a.health = 50.0;
  a.mana_color = -1;
  return a;
}

static void scene_open(scene *s, ground g) {
  load_materials();
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
    if (ct && ct->stage >= ZB_STAGE_ECOLOGY) {
      break;
    }
  }
  reshape(zb_stream_edit(zb_world_stream(s->world), f.pos.chunk), g);
  zb_tier_config tc = {zb_test_alloc(), s->world, g_materials, 1.0, 3, NULL, NULL};
  s->tier = zb_tier_create(&tc);
  TEST_ASSERT_NOT_NULL(s->tier);
  zb_weather calm = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 15.0};
  zb_tier_weather(s->tier)->overridden = true;
  zb_tier_weather(s->tier)->now = calm;
  zb_tier_apply_weather(s->tier, &calm);
  zb_tier_update(s->tier, 0.0);
  s->cr = zb_creatures_create(zb_test_alloc(), s->world, 9);
  TEST_ASSERT_NOT_NULL(s->cr);
  zb_creature_sources src = {zb_tier_fire(s->tier), zb_tier_water(s->tier), zb_tier_air(s->tier),
                             zb_tier_bodies(s->tier)};
  zb_creatures_set_sources(s->cr, &src);
  zb_animal wolf = animal("wolf", 1.4, 1.5, 9.0, ZB_DIET_CARNIVORE);
  wolf.pack = true;
  wolf.aggression = 0.9;
  wolf.fear_of_fire = 1.0;
  wolf.fear_of_player = 0.1;
  zb_animal deer = animal("deer", 1.8, 1.5, 11.0, ZB_DIET_HERBIVORE);
  deer.fear_of_fire = 1.0;
  deer.fear_of_player = 0.9;
  zb_animal bird = animal("storm_bird", 1.2, 6.0, 18.0, ZB_DIET_CARNIVORE);
  bird.flies = true;
  bird.cruise = 20.0;
  bird.magical = true;
  bird.mana_color = ZB_MANA_STORM;
  bird.motes = 30.0;
  bird.resist[ZB_HARM_SMOKE] = 0.5;
  bird.smell = 0.0; /* birds hunt by sight */
  s->wolf = zb_creatures_define(s->cr, &wolf);
  s->deer = zb_creatures_define(s->cr, &deer);
  s->bird = zb_creatures_define(s->cr, &bird);
  TEST_ASSERT_TRUE(s->wolf == 0 && s->deer == 1 && s->bird == 2);
}

static void scene_close(scene *s) {
  zb_creatures_destroy(s->cr);
  zb_tier_destroy(s->tier);
  zb_world_destroy(s->world);
}

static void run(scene *s, double seconds) {
  for (double t = 0.0; t < seconds - 1e-9; t += 1.0 / 60.0) {
    zb_tier_update(s->tier, 1.0 / 60.0);
    zb_creatures_update(s->cr, 1.0 / 60.0);
  }
}

static zb_planet_pos at(const scene *s, float x, float y) {
  zb_planet_pos p = s->centre;
  p.lx = x;
  p.ly = y;
  p.alt = 50.0f;
  return p;
}

static zb_creature_state state(const scene *s, uint32_t id) {
  zb_creature_state st;
  TEST_ASSERT_TRUE(zb_creatures_get(s->cr, id, &st));
  return st;
}

static double apart(const scene *s, const zb_planet_pos *a, const zb_planet_pos *b) {
  return zb_planet_distance(zb_world_planet(s->world), a, b);
}

/* spec: creatures: Species are data */
static void test_species(void) {
  scene s;
  scene_open(&s, MEADOW);
  TEST_ASSERT_EQUAL_INT(1, zb_creatures_find_species(s.cr, "deer"));
  TEST_ASSERT_EQUAL_INT(-1, zb_creatures_find_species(s.cr, "unicorn"));
  TEST_ASSERT_EQUAL_STRING("storm_bird", zb_creatures_species(s.cr, 2)->id);
  TEST_ASSERT_NULL(zb_creatures_species(s.cr, 9));
  zb_animal bad = animal("slug", 0.0, 1.0, 1.0, ZB_DIET_HERBIVORE);
  TEST_ASSERT_EQUAL_INT(-1, zb_creatures_define(s.cr, &bad));
  bad = animal("snail", 0.1, 2.0, 1.0, ZB_DIET_HERBIVORE); /* runs slower than it walks */
  TEST_ASSERT_EQUAL_INT(-1, zb_creatures_define(s.cr, &bad));
  zb_animal faster = animal("deer", 1.8, 2.0, 14.0, ZB_DIET_HERBIVORE);
  TEST_ASSERT_EQUAL_INT(1, zb_creatures_define(s.cr, &faster)); /* replaces the old one */
  TEST_ASSERT_EQUAL_DOUBLE(14.0, zb_creatures_species(s.cr, 1)->run);
  for (int k = 0; k < ZB_SPECIES_MAX; ++k) {
    zb_animal extra = animal("x", 1.0, 1.0, 1.0, ZB_DIET_OMNIVORE);
    snprintf(extra.id, sizeof extra.id, "kind%d", k);
    zb_creatures_define(s.cr, &extra);
  }
  zb_animal one_more = animal("too_many", 1.0, 1.0, 1.0, ZB_DIET_OMNIVORE);
  TEST_ASSERT_EQUAL_INT(-1, zb_creatures_define(s.cr, &one_more));
  zb_planet_pos p = at(&s, 128.0f, 128.0f);
  TEST_ASSERT_EQUAL_UINT32(0, zb_creatures_spawn(s.cr, 99, &p));
  TEST_ASSERT_EQUAL_STRING("investigate", zb_action_name(ZB_ACT_INVESTIGATE));
  TEST_ASSERT_EQUAL_STRING("?", zb_action_name(ZB_ACTIONS));
  TEST_ASSERT_EQUAL_STRING("smoke", zb_harm_name(ZB_HARM_SMOKE));
  TEST_ASSERT_EQUAL_STRING("?", zb_harm_name(ZB_HARMS));
  scene_close(&s);
}

/* spec: creatures: On the ground, in the air, out of the water */
static void test_moving(void) {
  scene s;
  scene_open(&s, POND);
  zb_planet_pos p = at(&s, 128.0f, 128.0f);
  uint32_t deer = zb_creatures_spawn(s.cr, s.deer, &p);
  uint32_t bird = zb_creatures_spawn(s.cr, s.bird, &p);
  zb_planet_pos shore = at(&s, 168.0f, 128.0f); /* the pond's edge is at x = 170 */
  uint32_t wader = zb_creatures_spawn(s.cr, s.deer, &shore);
  zb_planet_pos middle = at(&s, 200.0f, 128.0f);
  zb_surface_water_add(zb_tier_water(s.tier), &middle, 25.0, 9000.0); /* about 3 m deep */
  run(&s, 2.0);
  int wet = 0;
  for (int k = 0; k < 120; ++k) {
    run(&s, 0.5);
    zb_creature_state d = state(&s, deer), b = state(&s, bird), w = state(&s, wader);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, zb_world_ground_height(s.world, &d.pos), d.pos.alt);
    TEST_ASSERT_TRUE(b.pos.alt > zb_world_ground_height(s.world, &b.pos) + 10.0);
    zb_water_cell wc;
    zb_surface_water_cell_at(zb_tier_water(s.tier), &w.pos, &wc);
    wet += wc.depth > 0.5 ? 1 : 0;
  }
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, wet, "the wader kept out of the pond");
  zb_creature_state moved = state(&s, deer);
  TEST_ASSERT_TRUE(apart(&s, &moved.pos, &p) > 1.0); /* it wandered */
  /* On the frozen pond it stands on the ice. */
  zb_surface_water_freeze(zb_tier_water(s.tier), &middle, 29.0, 600.0);
  uint32_t skater = zb_creatures_spawn(s.cr, s.deer, &middle);
  zb_water_cell ice;
  zb_surface_water_cell_at(zb_tier_water(s.tier), &middle, &ice);
  TEST_ASSERT_TRUE(ice.frozen);
  TEST_ASSERT_DOUBLE_WITHIN(0.05, ice.level, state(&s, skater).pos.alt);
  /* Far outside the active tier a creature waits. */
  zb_planet_pos far = p;
  far.chunk.x += 40;
  uint32_t lost = zb_creatures_spawn(s.cr, s.deer, &far);
  run(&s, 1.0);
  zb_creature_state l = state(&s, lost);
  TEST_ASSERT_TRUE(l.frozen && l.speed == 0.0);
  zb_creatures_stats st;
  zb_creatures_get_stats(s.cr, &st);
  TEST_ASSERT_EQUAL_INT(1, st.frozen);
  TEST_ASSERT_EQUAL_INT(4, st.alive);
  TEST_ASSERT_TRUE(zb_creatures_remove(s.cr, lost));
  TEST_ASSERT_FALSE(zb_creatures_remove(s.cr, lost));
  zb_creature_state none;
  TEST_ASSERT_FALSE(zb_creatures_get(s.cr, lost, &none));
  scene_close(&s);
}

/* spec: creatures: Fear of fire, hunters and the player */
static void test_fear_and_hunting(void) {
  scene s;
  scene_open(&s, MEADOW);
  /* A deer near a fire runs from it. */
  zb_planet_pos grass = at(&s, 60.0f, 60.0f), near = at(&s, 66.0f, 60.0f);
  uint32_t deer = zb_creatures_spawn(s.cr, s.deer, &near);
  TEST_ASSERT_TRUE(zb_fire_ignite(zb_tier_fire(s.tier), &grass, 3.0) > 0);
  run(&s, 1.0);
  zb_creature_state d = state(&s, deer);
  TEST_ASSERT_EQUAL_STRING("flee", zb_action_name(d.action));
  TEST_ASSERT_TRUE(d.fear > 0.5);
  run(&s, 3.0);
  zb_creature_state fled = state(&s, deer);
  TEST_ASSERT_TRUE_MESSAGE(apart(&s, &fled.pos, &grass) > 15.0, "away from the fire");
  zb_creature_event ev[16];
  int n = zb_creatures_events(s.cr, ev, 16);
  TEST_ASSERT_TRUE(n >= 1 && ev[0].kind == ZB_CREATURE_FLED && ev[0].id == deer);
  TEST_ASSERT_EQUAL_INT(0, zb_creatures_events(s.cr, ev, 16));
  zb_fire_cool(zb_tier_fire(s.tier), &grass, 30.0, 1e12);
  /* A hungry wolf hunts a mage on foot and attacks; a mage high on the carpet
   * is less tempting. */
  zb_planet_pos mage = at(&s, 200.0f, 200.0f), den = at(&s, 200.0f, 170.0f);
  uint32_t wolf = zb_creatures_spawn(s.cr, s.wolf, &den);
  zb_creatures_set_player(s.cr, &mage, true);
  run(&s, 1.0);
  TEST_ASSERT_EQUAL_STRING("hunt", zb_action_name(state(&s, wolf).action));
  TEST_ASSERT_TRUE(state(&s, wolf).sees_player > 0.0);
  int attacks = 0;
  for (int k = 0; k < 20 && attacks == 0; ++k) {
    run(&s, 0.5);
    n = zb_creatures_events(s.cr, ev, 16);
    for (int i = 0; i < n; ++i) {
      attacks += ev[i].kind == ZB_CREATURE_ATTACKED ? 1 : 0;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(attacks > 0, "the wolf reached the mage");
  zb_planet_pos high = mage;
  high.alt = 50.0f + 60.0f;
  zb_creatures_set_player(s.cr, &high, false);
  run(&s, 1.0);
  TEST_ASSERT_TRUE(strcmp(zb_action_name(state(&s, wolf).action), "hunt") != 0 || state(&s, wolf).sees_player < 0.0);
  /* A threat and a loud noise scare a deer; a light at night looks like fire. */
  zb_creatures_set_player(s.cr, NULL, false);
  zb_planet_pos field = at(&s, 128.0f, 40.0f);
  uint32_t calm = zb_creatures_spawn(s.cr, s.deer, &field);
  run(&s, 1.0);
  zb_creatures_stimulus(s.cr, ZB_STIMULUS_NOISE, &field, 1.0, 2.0);
  run(&s, 0.5);
  TEST_ASSERT_EQUAL_STRING("flee", zb_action_name(state(&s, calm).action));
  for (int k = 0; k < 70; ++k) {
    zb_creatures_stimulus(s.cr, ZB_STIMULUS_THREAT, &field, 1.0, 1.0); /* old ones make room */
  }
  run(&s, 0.5);
  TEST_ASSERT_TRUE(state(&s, calm).fear > 0.8);
  scene_close(&s);
}

/* spec: carpet-cloak: Seen or not */
static void test_seen_or_not(void) {
  scene s;
  scene_open(&s, MEADOW);
  /* A wary kind of deer keeps 60 m from the mage: seen plainly at 40 m it
   * flees; camouflaged at the same distance it is not seen and stays. */
  zb_animal wary = *zb_creatures_species(s.cr, s.deer);
  snprintf(wary.id, sizeof wary.id, "%s", "wary_deer");
  wary.flight = 60.0;
  wary.sight = 200.0;
  int kind = zb_creatures_define(s.cr, &wary);
  TEST_ASSERT_TRUE(kind >= 0);
  zb_planet_pos mage = at(&s, 128.0f, 128.0f), grazing = at(&s, 128.0f, 168.0f);
  zb_creatures_set_player(s.cr, &mage, true);
  zb_creatures_set_player_presence(s.cr, 0.05, 1.0);
  uint32_t deer = zb_creatures_spawn(s.cr, kind, &grazing);
  run(&s, 1.0);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, deer).sees_player < 0.0, "camouflaged: not seen");
  TEST_ASSERT_TRUE(strcmp(zb_action_name(state(&s, deer).action), "flee") != 0);
  zb_creatures_set_player_presence(s.cr, 1.0, 1.0);
  run(&s, 1.0);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, deer).sees_player > 0.0, "plain: seen");
  TEST_ASSERT_EQUAL_STRING("flee", zb_action_name(state(&s, deer).action));
  /* The plain deer, at 40 m, is outside its 25 m and stays calm. */
  zb_planet_pos field = at(&s, 40.0f, 40.0f), near = at(&s, 40.0f, 80.0f);
  zb_creatures_set_player(s.cr, &field, true);
  uint32_t calm = zb_creatures_spawn(s.cr, s.deer, &near);
  run(&s, 1.0);
  TEST_ASSERT_TRUE(state(&s, calm).fear < 0.1);
  /* Presence: a vivid mage frightens more at the same distance; limits hold. */
  zb_planet_pos close = at(&s, 40.0f, 55.0f);
  uint32_t shy = zb_creatures_spawn(s.cr, s.deer, &close);
  run(&s, 0.2);
  double plain = state(&s, shy).fear;
  zb_creatures_remove(s.cr, shy);
  shy = zb_creatures_spawn(s.cr, s.deer, &close);
  zb_creatures_set_player_presence(s.cr, 5.0, 9.0); /* clamped to 2 and 3 */
  run(&s, 0.2);
  TEST_ASSERT_TRUE(state(&s, shy).fear > plain);
  zb_creatures_set_player_presence(s.cr, -1.0, -1.0); /* clamped to 0 */
  run(&s, 0.5);
  TEST_ASSERT_TRUE(state(&s, shy).sees_player < 0.0 || state(&s, shy).sees_player <= 120.0 * 0.25);
  scene_close(&s);
}

/* spec: creatures: Scent on the wind */
static void test_scent(void) {
  scene s;
  scene_open(&s, MEADOW);
  zb_planet_pos den = at(&s, 128.0f, 128.0f), source = at(&s, 100.0f, 128.0f);
  uint32_t wolf = zb_creatures_spawn(s.cr, s.wolf, &den);
  for (int k = 0; k < 40; ++k) {
    zb_air_emit(zb_tier_air(s.tier), &source, ZB_GAS_SCENT, 1.0);
    zb_air_emit(zb_tier_air(s.tier), &den, ZB_GAS_SCENT, 0.5);
    run(&s, 0.25);
  }
  TEST_ASSERT_EQUAL_STRING("investigate", zb_action_name(state(&s, wolf).action));
  scene_close(&s);
}

/* spec: creatures: Harm and death */
static void test_harm(void) {
  scene s;
  scene_open(&s, MEADOW);
  zb_planet_pos p = at(&s, 128.0f, 128.0f), q = at(&s, 40.0f, 200.0f);
  uint32_t deer = zb_creatures_spawn(s.cr, s.deer, &p);
  uint32_t bird = zb_creatures_spawn(s.cr, s.bird, &q);
  TEST_ASSERT_TRUE(zb_creatures_hurt(s.cr, deer, ZB_HARM_MAGIC, 20.0));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 30.0, state(&s, deer).health);
  TEST_ASSERT_TRUE(state(&s, deer).fear >= 0.8); /* pain */
  TEST_ASSERT_TRUE(zb_creatures_hurt(s.cr, deer, ZB_HARM_MAGIC, -100.0));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 50.0, state(&s, deer).health); /* healed, to its full health */
  zb_creatures_hurt(s.cr, bird, ZB_HARM_SMOKE, 20.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 40.0, state(&s, bird).health); /* resists half the smoke */
  TEST_ASSERT_FALSE(zb_creatures_hurt(s.cr, 999, ZB_HARM_MAGIC, 1.0));
  TEST_ASSERT_FALSE(zb_creatures_hurt(s.cr, deer, ZB_HARMS, 1.0));
  /* A storm bird struck down: it dies, and the death is told with the cause. */
  TEST_ASSERT_FALSE(zb_creatures_hurt(s.cr, bird, ZB_HARM_IMPACT, 500.0));
  zb_creature_event ev[8];
  int n = zb_creatures_events(s.cr, ev, 8);
  TEST_ASSERT_TRUE(n >= 1);
  TEST_ASSERT_TRUE(ev[n - 1].kind == ZB_CREATURE_DIED && ev[n - 1].id == bird && ev[n - 1].cause == ZB_HARM_IMPACT);
  TEST_ASSERT_TRUE(zb_creatures_species(s.cr, ev[n - 1].species)->magical);
  zb_creatures_hurt(s.cr, bird, ZB_HARM_MAGIC, 5.0); /* the dead feel nothing */
  TEST_ASSERT_FALSE(state(&s, bird).alive);
  run(&s, 3.0); /* and fall out of the sky */
  zb_creature_state fallen = state(&s, bird);
  TEST_ASSERT_DOUBLE_WITHIN(0.05, zb_world_ground_height(s.world, &fallen.pos), fallen.pos.alt);
  /* A deer in the flames burns. */
  zb_creature_state where = state(&s, deer);
  zb_fire_ignite(zb_tier_fire(s.tier), &where.pos, 8.0);
  for (int k = 0; k < 40 && state(&s, deer).alive; ++k) {
    run(&s, 0.25);
  }
  TEST_ASSERT_TRUE(state(&s, deer).health < 50.0);
  /* A boulder rolling into a creature hurts it. */
  zb_planet_pos field = at(&s, 200.0f, 60.0f);
  uint32_t wolf = zb_creatures_spawn(s.cr, s.wolf, &field);
  int rock = zb_materials_find(g_materials, "rock");
  zb_body_desc b;
  memset(&b, 0, sizeof b);
  b.shape = ZB_SHAPE_SPHERE;
  b.material = rock;
  b.radius = 0.5;
  b.pos = state(&s, wolf).pos;
  b.pos.alt += 0.6f;
  b.velocity[2] = -12.0;
  zb_bodies_add(zb_tier_bodies(s.tier), &b);
  double before = state(&s, wolf).health;
  run(&s, 0.5);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, wolf).health < before, "struck by the boulder");
  zb_creatures_stats st;
  zb_creatures_get_stats(s.cr, &st);
  TEST_ASSERT_TRUE(st.dead >= 1);
  uint32_t ids[8];
  TEST_ASSERT_TRUE(zb_creatures_near(s.cr, &field, 10.0, ids, 8) >= 1);
  scene_close(&s);
}

/* Metres between two creatures. */
static double between(const scene *s, uint32_t a, uint32_t b) {
  zb_creature_state x = state(s, a), y = state(s, b);
  return apart(s, &x.pos, &y.pos);
}

static uint32_t spawn_at(scene *s, int species, float x, float y) {
  zb_planet_pos p = at(s, x, y);
  uint32_t id = zb_creatures_spawn(s->cr, species, &p);
  TEST_ASSERT_TRUE(id > 0);
  return id;
}

/* The scene's wolf hunting deer (and a slower deer to catch). */
static void wolf_hunts_deer(scene *s, double deer_run) {
  zb_animal wolf = *zb_creatures_species(s->cr, s->wolf);
  snprintf(wolf.eats[0], sizeof wolf.eats[0], "%s", "deer");
  wolf.eats_count = 1;
  TEST_ASSERT_EQUAL_INT(s->wolf, zb_creatures_define(s->cr, &wolf));
  zb_animal deer = *zb_creatures_species(s->cr, s->deer);
  deer.run = deer_run;
  TEST_ASSERT_EQUAL_INT(s->deer, zb_creatures_define(s->cr, &deer));
}

/* spec: fauna-behaviour: Hunting prey */
static void test_hunting(void) {
  scene s;
  scene_open(&s, MEADOW);
  wolf_hunts_deer(&s, 4.0);
  uint32_t wolf = spawn_at(&s, s.wolf, 90.0f, 128.0f), deer = spawn_at(&s, s.deer, 140.0f, 128.0f);
  TEST_ASSERT_TRUE(zb_creatures_set_needs(s.cr, wolf, 0.9, -1.0, -1.0));
  /* The wolf stalks, the deer takes fright, the wolf gives chase and kills. */
  bool stalked = false, chased = false, alerted = false;
  zb_creature_event died = {0};
  for (int k = 0; k < 400 && !died.id; ++k) {
    run(&s, 0.1);
    zb_creature_state w = state(&s, wolf), d = state(&s, deer);
    stalked |= w.action == ZB_ACT_HUNT && w.target == deer && w.speed > 0.5 && w.speed < 2.0;
    chased |= w.action == ZB_ACT_HUNT && w.speed > 5.0;
    alerted |= d.action == ZB_ACT_ALERT || d.action == ZB_ACT_FLEE;
    zb_creature_event ev[16];
    int n = zb_creatures_events(s.cr, ev, 16);
    for (int i = 0; i < n; ++i) {
      died = ev[i].kind == ZB_CREATURE_DIED && ev[i].id == deer ? ev[i] : died;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(stalked, "stalked");
  TEST_ASSERT_TRUE_MESSAGE(chased, "chased");
  TEST_ASSERT_TRUE_MESSAGE(alerted, "the deer saw it coming");
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(deer, died.id, "the deer was killed");
  TEST_ASSERT_EQUAL_UINT32(wolf, died.by);
  TEST_ASSERT_EQUAL_INT(ZB_HARM_IMPACT, died.cause);
  /* Then it eats. */
  double hunger = state(&s, wolf).hunger;
  run(&s, 6.0);
  zb_creature_state w = state(&s, wolf);
  TEST_ASSERT_EQUAL_STRING("feed", zb_action_name(w.action));
  TEST_ASSERT_TRUE(w.hunger < hunger && state(&s, deer).eaten > 0.0);
  /* Spent, a hunter gives up and leaves prey alone for a while. */
  TEST_ASSERT_TRUE(zb_creatures_remove(s.cr, deer)); /* no carcass to go back to */
  TEST_ASSERT_TRUE(zb_creatures_remove(s.cr, wolf));
  wolf_hunts_deer(&s, 11.0); /* this one is too fast to catch */
  uint32_t fresh = spawn_at(&s, s.deer, 130.0f, 60.0f), tired = spawn_at(&s, s.wolf, 120.0f, 60.0f);
  zb_creatures_set_needs(s.cr, tired, 1.0, -1.0, 0.32);
  bool gave_up = false;
  for (int k = 0; k < 100 && !gave_up; ++k) {
    run(&s, 0.1);
    zb_creature_state t = state(&s, tired);
    gave_up = t.energy < 0.2 && t.target == 0;
  }
  TEST_ASSERT_TRUE_MESSAGE(gave_up, "the spent wolf gave up");
  zb_creatures_set_needs(s.cr, tired, -1.0, -1.0, 1.0);
  run(&s, 1.0);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, tired).target != fresh, "and leaves it alone for a while");
  scene_close(&s);
}

/* spec: fauna-behaviour: Thirst and the time of day */
static void test_thirst_and_day(void) {
  scene s;
  scene_open(&s, POND);
  zb_planet_pos middle = at(&s, 200.0f, 128.0f);
  zb_surface_water_add(zb_tier_water(s.tier), &middle, 25.0, 9000.0);
  run(&s, 2.0);
  uint32_t deer = spawn_at(&s, s.deer, 140.0f, 128.0f);
  TEST_ASSERT_TRUE(zb_creatures_set_needs(s.cr, deer, 0.0, 0.95, 1.0));
  TEST_ASSERT_FALSE(zb_creatures_set_needs(s.cr, 999, 0.0, 0.0, 0.0));
  bool drank = false;
  for (int k = 0; k < 40 && !drank; ++k) {
    run(&s, 1.0);
    drank = state(&s, deer).thirst < 0.4;
  }
  TEST_ASSERT_TRUE_MESSAGE(drank, "it went to the water and drank");
  zb_water_cell wc;
  zb_creature_state at_water = state(&s, deer);
  zb_surface_water_cell_at(zb_tier_water(s.tier), &at_water.pos, &wc);
  TEST_ASSERT_TRUE_MESSAGE(wc.depth < 0.5, "from the shore");
  /* A night animal rests in the day and is about at night. */
  zb_animal owl = *zb_creatures_species(s.cr, s.deer);
  snprintf(owl.id, sizeof owl.id, "%s", "night_deer");
  owl.activity = ZB_ACTIVE_NIGHT;
  int night = zb_creatures_define(s.cr, &owl);
  uint32_t sleeper = spawn_at(&s, night, 100.0f, 60.0f);
  zb_creatures_set_needs(s.cr, sleeper, 0.2, 0.0, 1.0);
  double t0 = zb_world_time(s.world), day = 86400.0;
  double noon = t0 - fmod(t0, day) + 0.5 * day, midnight = noon + 0.5 * day;
  zb_world_set_time(s.world, noon);
  run(&s, 1.0);
  TEST_ASSERT_EQUAL_STRING("rest", zb_action_name(state(&s, sleeper).action));
  zb_world_set_time(s.world, midnight);
  run(&s, 1.0);
  TEST_ASSERT_TRUE_MESSAGE(state(&s, sleeper).action != ZB_ACT_REST, "awake at night");
  scene_close(&s);
}

/* spec: fauna-behaviour: Herds and stampedes */
static void test_herd(void) {
  scene s;
  scene_open(&s, MEADOW);
  zb_animal deer = *zb_creatures_species(s.cr, s.deer);
  deer.pack = true;
  zb_creatures_define(s.cr, &deer);
  uint32_t herd[6];
  for (int k = 0; k < 6; ++k) {
    herd[k] = spawn_at(&s, s.deer, 128.0f + 0.3f * (float)k, 128.0f);
    zb_creatures_set_needs(s.cr, herd[k], 0.0, 0.0, 1.0); /* not hungry: they move about */
  }
  run(&s, 12.0);
  double closest = 1e9, farthest = 0.0;
  for (int i = 0; i < 6; ++i) {
    for (int j = i + 1; j < 6; ++j) {
      zb_creature_state a = state(&s, herd[i]), b = state(&s, herd[j]);
      double d = apart(&s, &a.pos, &b.pos);
      closest = d < closest ? d : closest;
      farthest = d > farthest ? d : farthest;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(closest > 1.0, "they keep apart");
  TEST_ASSERT_TRUE_MESSAGE(farthest < 40.0, "and together");
  /* One is hurt: the whole herd runs. */
  zb_creatures_hurt(s.cr, herd[0], ZB_HARM_MAGIC, 5.0);
  run(&s, 0.6);
  int fleeing = 0;
  for (int k = 0; k < 6; ++k) {
    fleeing += state(&s, herd[k]).action == ZB_ACT_FLEE ? 1 : 0;
  }
  TEST_ASSERT_EQUAL_INT_MESSAGE(6, fleeing, "a stampede");
  scene_close(&s);
}

/* spec: fauna-behaviour: Following a scent upwind */
static void test_scent_trail(void) {
  scene s;
  scene_open(&s, MEADOW);
  wolf_hunts_deer(&s, 4.0);
  uint32_t deer = spawn_at(&s, s.deer, 220.0f, 128.0f), wolf = spawn_at(&s, s.wolf, 40.0f, 128.0f);
  /* A breeze from the deer to the wolf. */
  const zb_planet *pl = zb_world_planet(s.world);
  zb_planet_pos from = state(&s, deer).pos, to = state(&s, wolf).pos;
  zb_dvec3 a = zb_planet_to_cart(pl, &from), b = zb_planet_to_cart(pl, &to), e, n, u;
  zb_planet_frame(zb_dv_norm(a), &e, &n, &u);
  zb_dvec3 d = zb_dv_sub(b, a);
  double east = zb_dv_dot(d, e), north = zb_dv_dot(d, n), len = sqrt(east * east + north * north);
  zb_weather breeze = {0.0, 0.0, 0.0, 0.0, 3.0 * east / len, 3.0 * north / len, 15.0};
  zb_tier_weather(s.tier)->now = breeze;
  zb_tier_apply_weather(s.tier, &breeze);
  zb_creatures_set_needs(s.cr, deer, 0.0, 0.0, 1.0);
  run(&s, 10.0); /* the deer's scent drifts downwind */
  zb_creatures_set_needs(s.cr, wolf, 1.0, 0.0, 1.0);
  double start = between(&s, wolf, deer);
  bool tracked = false;
  for (int k = 0; k < 80; ++k) {
    run(&s, 0.5);
    tracked |= state(&s, wolf).action == ZB_ACT_INVESTIGATE;
  }
  double end = between(&s, wolf, deer);
  TEST_ASSERT_TRUE_MESSAGE(tracked, "the wolf picked up the scent");
  TEST_ASSERT_TRUE_MESSAGE(end < start - 25.0, "and followed it upwind");
  scene_close(&s);
}

static int counted;
static void count_each(void *ctx, const zb_creature_state *st, double phase) {
  (void)ctx;
  TEST_ASSERT_TRUE(st->id > 0 && phase >= 0.0);
  counted++;
}

/* spec: creatures: Deterministic creatures */
static void test_deterministic(void) {
  uint64_t h[2];
  for (int k = 0; k < 2; ++k) {
    scene s;
    scene_open(&s, MEADOW);
    for (int i = 0; i < 12; ++i) {
      zb_planet_pos p = at(&s, 40.0f + 15.0f * (float)i, 100.0f + 5.0f * (float)(i % 3));
      zb_creatures_spawn(s.cr, i % 3, &p);
    }
    zb_planet_pos mage = at(&s, 128.0f, 140.0f);
    zb_creatures_set_player(s.cr, &mage, true);
    run(&s, 5.0);
    h[k] = zb_creatures_hash(s.cr);
    counted = 0;
    zb_creatures_each(s.cr, count_each, NULL);
    TEST_ASSERT_EQUAL_INT(12, counted);
    zb_creatures_set_world(s.cr, s.world);
    zb_creatures_stats st;
    zb_creatures_get_stats(s.cr, &st);
    TEST_ASSERT_EQUAL_INT(0, st.alive);
    scene_close(&s);
  }
  TEST_ASSERT_EQUAL_UINT64(h[0], h[1]);
  zb_creatures_destroy(NULL);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_species);
  RUN_TEST(test_moving);
  RUN_TEST(test_fear_and_hunting);
  RUN_TEST(test_scent);
  RUN_TEST(test_harm);
  RUN_TEST(test_deterministic);
  RUN_TEST(test_hunting);
  RUN_TEST(test_thirst_and_day);
  RUN_TEST(test_herd);
  RUN_TEST(test_scent_trail);
  RUN_TEST(test_seen_or_not);
  int failures = UNITY_END();
  if (g_materials) {
    zb_materials_free(g_materials);
    free(g_materials);
  }
  return failures;
}

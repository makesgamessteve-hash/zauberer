/* SPDX-License-Identifier: MIT */
#include "core/log.h"
#include "sim/material.h"
#include "support/test_support.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static char g_log[8192];

static void capture(zb_log_level level, const char *message, void *user) {
  (void)user;
  if (level >= ZB_LOG_WARN && strlen(g_log) + strlen(message) + 2 < sizeof g_log) {
    strcat(g_log, message);
    strcat(g_log, "\n");
  }
}

static void merge_text(zb_registry *r, const char *package, const char *text) {
  zb_luon_doc doc;
  zb_luon_error e;
  zb_result res = zb_luon_parse(&doc, zb_test_alloc(), text, strlen(text), &e);
  TEST_ASSERT_EQUAL_INT_MESSAGE(ZB_OK, res, e.message);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_registry_merge(r, package, &doc.root));
  zb_luon_free(&doc);
}

/* The base game's data file. */
static void merge_base(zb_registry *r) {
  char path[512];
  snprintf(path, sizeof path, "%s/assets/base/data/materials.lua", zb_test_source_dir());
  size_t len;
  char *text = zb_test_read_file(path, &len);
  TEST_ASSERT_NOT_NULL(text);
  merge_text(r, "base", text);
  free(text);
}

static zb_materials *build(zb_registry *r, zb_result want) {
  zb_materials *m = malloc(sizeof *m);
  char err[160] = "";
  TEST_ASSERT_EQUAL_INT_MESSAGE(want, zb_materials_build(m, r, err, sizeof err), err);
  return m;
}

static void destroy(zb_materials *m) {
  zb_materials_free(m);
  free(m);
}

static const zb_material_info *get(const zb_materials *m, const char *ref) {
  int i = zb_materials_find(m, ref);
  TEST_ASSERT_TRUE_MESSAGE(i >= 0, ref);
  return &m->info[i];
}

/* spec: materials: The base materials */
static void test_base(void) {
  g_log[0] = '\0';
  zb_log_add_sink(capture, NULL);
  zb_registry r;
  zb_registry_init(&r, zb_test_alloc(), "materials");
  merge_base(&r);
  zb_materials *m = build(&r, ZB_OK);
  TEST_ASSERT_EQUAL_STRING("", g_log); /* every entry is valid */
  TEST_ASSERT_TRUE(m->count >= 30);
  /* Every phase is there, and the entries the engine relies on. */
  int phases[ZB_PHASE_COUNT] = {0};
  for (int i = 0; i < m->count; ++i) {
    phases[m->info[i].phase]++;
  }
  for (int p = 0; p < ZB_PHASE_COUNT; ++p) {
    TEST_ASSERT_TRUE_MESSAGE(phases[p] >= 3, zb_phase_name((zb_phase)p));
  }
  TEST_ASSERT_EQUAL_STRING("base:air", m->info[m->air].key);
  TEST_ASSERT_EQUAL_STRING("base:water", m->info[m->water].key);
  TEST_ASSERT_EQUAL_INT(-1, m->surface[ZB_MAT_NONE]);
  for (int s = 1; s < ZB_MAT_COUNT; ++s) {
    TEST_ASSERT_TRUE(m->surface[s] >= 0);
  }
  TEST_ASSERT_EQUAL_STRING("Granite", m->info[m->surface[ZB_MAT_ROCK]].name);
  /* Water: real values, freezing and boiling with latent heat. */
  const zb_material_info *water = get(m, "water");
  TEST_ASSERT_EQUAL_FLOAT(1000.0f, water->density);
  TEST_ASSERT_EQUAL_FLOAT(4186.0f, water->specific_heat);
  TEST_ASSERT_EQUAL_INT(zb_materials_find(m, "ice"), water->transition[ZB_FREEZES].to);
  TEST_ASSERT_EQUAL_FLOAT(334000.0f, water->transition[ZB_FREEZES].latent);
  TEST_ASSERT_EQUAL_INT(zb_materials_find(m, "steam"), water->transition[ZB_BOILS].to);
  TEST_ASSERT_EQUAL_INT(-1, water->transition[ZB_MELTS].to);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, water->affinity[ZB_MANA_WATER]);
  /* Wood burns into charcoal, ash, smoke and carbon dioxide. */
  const zb_material_info *wood = get(m, "wood");
  TEST_ASSERT_TRUE(wood->flammability > 0.0f && wood->ignition == 300.0f && wood->heat > 1e7f);
  int found = 0;
  for (int k = 0; k < ZB_MATERIAL_PRODUCTS; ++k) {
    found += wood->product[k] == zb_materials_find(m, "charcoal") || wood->product[k] == zb_materials_find(m, "ash");
  }
  TEST_ASSERT_EQUAL_INT(2, found);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, get(m, "rock")->flammability);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, get(m, "air")->friction); /* gases have no grip */
  TEST_ASSERT_EQUAL_FLOAT(0.6f, get(m, "soil")->friction);
  TEST_ASSERT_EQUAL_FLOAT(0.7f, get(m, "rock")->friction);
  TEST_ASSERT_EQUAL_FLOAT(0.95f, get(m, "copper")->mana_conductivity);
  TEST_ASSERT_EQUAL_STRING("decay", zb_mana_color_name(ZB_MANA_DECAY));
  TEST_ASSERT_EQUAL_STRING("?", zb_mana_color_name((zb_mana_color)8));
  TEST_ASSERT_EQUAL_STRING("powder", zb_phase_name(ZB_PHASE_POWDER));
  TEST_ASSERT_EQUAL_STRING("?", zb_phase_name(ZB_PHASE_COUNT));
  zb_log_remove_sink(capture, NULL);
  destroy(m);
}

/* spec: materials: Phase changes by temperature */
static void test_phases(void) {
  zb_registry r;
  zb_registry_init(&r, zb_test_alloc(), "materials");
  merge_base(&r);
  zb_materials *m = build(&r, ZB_OK);
  int ice = zb_materials_find(m, "ice"), water = zb_materials_find(m, "water"), steam = zb_materials_find(m, "steam");
  TEST_ASSERT_EQUAL_INT(ice, zb_material_at_temperature(m, water, -5.0f));
  TEST_ASSERT_EQUAL_INT(water, zb_material_at_temperature(m, water, 20.0f));
  TEST_ASSERT_EQUAL_INT(steam, zb_material_at_temperature(m, ice, 150.0f)); /* melts, then boils */
  TEST_ASSERT_EQUAL_INT(ice, zb_material_at_temperature(m, steam, -20.0f)); /* condenses, then freezes */
  int rock = zb_materials_find(m, "rock"), lava = zb_materials_find(m, "lava"), basalt = zb_materials_find(m, "basalt");
  TEST_ASSERT_EQUAL_INT(lava, zb_material_at_temperature(m, rock, 1300.0f));
  TEST_ASSERT_EQUAL_INT(basalt, zb_material_at_temperature(m, lava, 900.0f)); /* lava cools into basalt */
  int molten = zb_material_at_temperature(m, zb_materials_find(m, "sand"), 1800.0f);
  TEST_ASSERT_EQUAL_INT(zb_materials_find(m, "molten_glass"), molten);                                /* sand melts */
  TEST_ASSERT_EQUAL_INT(zb_materials_find(m, "glass"), zb_material_at_temperature(m, molten, 20.0f)); /* into glass */
  TEST_ASSERT_EQUAL_INT(-1, zb_material_at_temperature(m, -1, 20.0f));
  destroy(m);
}

/* spec: materials: Broken entries are skipped */
static void test_invalid(void) {
  static const char *const cases[][2] = {
      {"{ id = 'x', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1, color = { 0, 0, 0 } }",
       "base:x left out: name must be a string"},
      {"{ id = 'x', name = 'X', phase = 'plasma', density = 1, specific_heat = 1, conductivity = 1 }",
       "phase must be solid, powder, liquid or gas"},
      {"{ id = 'x', name = 'X', phase = 'solid', specific_heat = 1, conductivity = 1 }", "density is missing"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 'heavy', specific_heat = 1, conductivity = 1 }",
       "density must be a number"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = -1, specific_heat = 1, conductivity = 1 }",
       "density must be 0.01 .. 25000"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1, color = 3 }",
       "color must be { r, g, b } with 0 .. 1"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 2, 0 } }",
       "color must be { r, g, b } with 0 .. 1"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, mana = 1 }",
       "mana must be a table"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, mana = { pink = 1 } }",
       "mana takes conductivity and the eight colours"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, mana = { conductivity = 2 } }",
       "mana.conductivity must be 0 .. 1"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, mana = { fire = 3 } }",
       "mana affinities must be 0 .. 1"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, burns = true }",
       "burns must be a table"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, burns = { flammability = 1, heat = 1, rate = 1 } }",
       "burns: ignition is missing"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, burns = { flammability = 1, ignition = 1, heat = 1, rate = 1, products = 4 } }",
       "burns.products must map materials"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, burns = { flammability = 1, ignition = 1, heat = 1, rate = 1,"
       " products = { ash = 1, smoke = 1, water = 1, air = 1, steam = 1 } } }",
       "burns.products has more than 4 materials"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, burns = { flammability = 1, ignition = 1, heat = 1, rate = 1,"
       " products = { ash = -1 } } }",
       "burns.products shares must be numbers 0 .. 10"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, burns = { flammability = 1, ignition = 1, heat = 1, rate = 1,"
       " products = { unobtainium = 1 } } }",
       "burns.products names no material (unobtainium)"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, melts = 5 }",
       "melts must be a table { at, to, latent }"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, melts = { to = 'water' } }",
       "melts: at is missing"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, melts = { at = 5, to = 7 } }",
       "melts: to must name a material"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, melts = { at = 5, to = 'steam' } }",
       "melts cannot turn a solid into a gas"},
      {"{ id = 'x', name = 'X', phase = 'gas', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, boils = { at = 5, to = 'steam' } }",
       "boils cannot turn a gas into a gas"},
      {"{ id = 'x', name = 'X', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, freezes = { at = 5, to = 'ice' } }",
       "freezes cannot turn a solid into a solid"},
      {"{ id = 'x', name = 'X', phase = 'liquid', density = 1, specific_heat = 1, conductivity = 1,"
       " color = { 0, 0, 0 }, condenses = { at = 5, to = 'water' } }",
       "condenses cannot turn a liquid into a liquid"},
  };
  for (size_t c = 0; c < sizeof cases / sizeof cases[0]; ++c) {
    g_log[0] = '\0';
    zb_log_add_sink(capture, NULL);
    zb_registry r;
    zb_registry_init(&r, zb_test_alloc(), "materials");
    merge_base(&r);
    char text[1024];
    snprintf(text, sizeof text, "return { %s }", cases[c][0]);
    merge_text(&r, "base", text);
    zb_materials *m = build(&r, ZB_OK);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(g_log, cases[c][1]), g_log);
    TEST_ASSERT_EQUAL_INT(-1, zb_materials_find(m, "base:x"));
    zb_log_remove_sink(capture, NULL);
    destroy(m);
  }
  /* A material that only a dropped one leads to is dropped as well. */
  g_log[0] = '\0';
  zb_log_add_sink(capture, NULL);
  zb_registry r;
  zb_registry_init(&r, zb_test_alloc(), "materials");
  merge_base(&r);
  merge_text(&r, "mod",
             "return { { id = 'broken', name = 'B', phase = 'liquid' },"
             "  { id = 'chain', name = 'C', phase = 'solid', density = 1, specific_heat = 1, conductivity = 1,"
             "    color = { 0, 0, 0 }, melts = { at = 1, to = 'broken' } } }");
  zb_materials *m = build(&r, ZB_OK);
  TEST_ASSERT_NOT_NULL(strstr(g_log, "mod:broken left out: density is missing"));
  TEST_ASSERT_NOT_NULL(strstr(g_log, "mod:chain left out: melts: to names no material (broken)"));
  zb_log_remove_sink(capture, NULL);
  destroy(m);
}

/* spec: materials: The engine's materials are required */
static void test_required(void) {
  /* Removed, and the one reported: without ice, water (which freezes to ice) goes too. */
  static const char *const needed[][2] = {{"air", "air"},   {"water", "water"},   {"soil", "soil"}, {"sand", "sand"},
                                          {"rock", "rock"}, {"gravel", "gravel"}, {"snow", "snow"}, {"ice", "water"},
                                          {"mud", "mud"},   {"ash", "ash"}};
  for (size_t n = 0; n < sizeof needed / sizeof needed[0]; ++n) {
    zb_registry r;
    zb_registry_init(&r, zb_test_alloc(), "materials");
    merge_base(&r);
    zb_registry_remove(&r, (size_t)zb_registry_index(&r, needed[n][0]));
    zb_materials *m = malloc(sizeof *m);
    char err[160], want[160];
    zb_log_set_stderr(false);
    zb_result res = zb_materials_build(m, &r, err, sizeof err);
    zb_log_set_stderr(true);
    TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, res);
    snprintf(want, sizeof want, "materials: no usable entry for %s", needed[n][1]);
    TEST_ASSERT_EQUAL_STRING(want, err);
    destroy(m);
  }
  /* Too many entries: the last ones are left out. */
  zb_registry r;
  zb_registry_init(&r, zb_test_alloc(), "materials");
  merge_base(&r);
  for (int i = 0; i < ZB_MATERIALS_MAX; ++i) {
    char text[256];
    snprintf(text, sizeof text,
             "return { { id = 'extra%d', name = 'E', phase = 'gas', density = 1, specific_heat = 1, "
             "conductivity = 1, color = { 0, 0, 0 } } }",
             i);
    merge_text(&r, "mod", text);
  }
  zb_log_set_stderr(false);
  zb_materials *m = build(&r, ZB_OK);
  zb_log_set_stderr(true);
  TEST_ASSERT_EQUAL_INT(ZB_MATERIALS_MAX, m->count);
  destroy(m);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_base);
  RUN_TEST(test_phases);
  RUN_TEST(test_invalid);
  RUN_TEST(test_required);
  return UNITY_END();
}

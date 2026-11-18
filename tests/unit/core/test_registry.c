/* SPDX-License-Identifier: MIT */
#include "core/log.h"
#include "core/registry.h"
#include "support/test_support.h"
#include "unity.h"

#include <string.h>

ZB_TEST_LEAK_CHECKED

static char g_log[4096];

static void capture(zb_log_level level, const char *message, void *user) {
  (void)user;
  if (level >= ZB_LOG_INFO && strlen(g_log) + strlen(message) + 2 < sizeof g_log) {
    strcat(g_log, message);
    strcat(g_log, "\n");
  }
}

static zb_result merge_text(zb_registry *r, const char *package, const char *text) {
  zb_luon_doc doc;
  zb_luon_error e;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_luon_parse(&doc, zb_test_alloc(), text, strlen(text), &e));
  zb_result res = zb_registry_merge(r, package, &doc.root);
  zb_luon_free(&doc);
  return res;
}

static double field(const zb_registry_entry *e, const char *name) {
  return zb_value_num(zb_table_get(&e->value, name), -1.0);
}

/* spec: materials: Registries merge across packages */
static void test_merge(void) {
  g_log[0] = '\0';
  zb_log_add_sink(capture, NULL);
  zb_registry r;
  zb_registry_init(&r, zb_test_alloc(), "materials");
  TEST_ASSERT_EQUAL_INT(ZB_OK, merge_text(&r, "base",
                                          "return { { id = 'sand', density = 1600, hardness = 7 },"
                                          "  { id = 'clay', density = 1700, tags = { 'soft', 'wet' } } }"));
  TEST_ASSERT_EQUAL_size_t(2, r.count);
  const zb_registry_entry *sand = zb_registry_find(&r, "base:sand");
  TEST_ASSERT_NOT_NULL(sand);
  TEST_ASSERT_EQUAL_STRING("sand", sand->id);
  TEST_ASSERT_EQUAL_STRING("base", sand->package);
  TEST_ASSERT_NULL(sand->changed);
  TEST_ASSERT_NULL(zb_table_get(&sand->value, "id")); /* the id is the key, not a field */
  TEST_ASSERT_EQUAL_PTR(sand, zb_registry_find(&r, "sand"));
  /* A mod adds, patches and overrides; the values outlive the documents. */
  TEST_ASSERT_EQUAL_INT(ZB_OK, merge_text(&r, "mod",
                                          "return { { id = 'sand', density = 1500 },"
                                          "  { patch = 'base:sand', hardness = 6, color = { 1, 0, 0 } },"
                                          "  { override = 'clay', density = 1800 } }"));
  TEST_ASSERT_EQUAL_size_t(3, r.count);
  TEST_ASSERT_NULL(zb_registry_find(&r, "sand")); /* two packages define it now */
  TEST_ASSERT_EQUAL_INT(-1, zb_registry_index(&r, "sand"));
  sand = zb_registry_find(&r, "base:sand");
  TEST_ASSERT_EQUAL_DOUBLE(1600.0, field(sand, "density")); /* kept */
  TEST_ASSERT_EQUAL_DOUBLE(6.0, field(sand, "hardness"));   /* patched */
  TEST_ASSERT_EQUAL_size_t(3, zb_table_len(zb_table_get(&sand->value, "color")));
  TEST_ASSERT_NULL(zb_table_get(&sand->value, "patch"));
  TEST_ASSERT_EQUAL_STRING("mod", sand->changed);
  const zb_registry_entry *clay = zb_registry_find(&r, "base:clay");
  TEST_ASSERT_EQUAL_DOUBLE(1800.0, field(clay, "density"));
  TEST_ASSERT_NULL(zb_table_get(&clay->value, "tags")); /* replaced completely */
  TEST_ASSERT_NULL(zb_table_get(&clay->value, "override"));
  TEST_ASSERT_EQUAL_DOUBLE(1500.0, field(zb_registry_find(&r, "mod:sand"), "density"));
  TEST_ASSERT_NOT_NULL(strstr(g_log, "materials: base:sand patched by mod (hardness, color)"));
  TEST_ASSERT_NOT_NULL(strstr(g_log, "materials: base:clay overridden by mod"));
  TEST_ASSERT_EQUAL_INT(0, r.problems);
  zb_registry_remove(&r, 0);
  TEST_ASSERT_EQUAL_STRING("base:clay", r.entry[0].key);
  zb_log_remove_sink(capture, NULL);
  zb_registry_free(&r);
}

/* spec: materials: Broken entries are skipped */
static void test_problems(void) {
  g_log[0] = '\0';
  zb_log_add_sink(capture, NULL);
  zb_registry r;
  zb_registry_init(&r, zb_test_alloc(), "materials");
  TEST_ASSERT_EQUAL_INT(ZB_OK, merge_text(&r, "bad", "return 5"));
  TEST_ASSERT_EQUAL_INT(ZB_OK, merge_text(&r, "bad",
                                          "return { 7, { name = 'no id' }, { id = 'a', patch = 'b' },"
                                          "  { id = 3 }, { id = 'Bad-Id' }, { id = 'ok' }, { id = 'ok' },"
                                          "  { patch = 'missing' }, { override = 'nothing:here' } }"));
  TEST_ASSERT_EQUAL_size_t(1, r.count);
  TEST_ASSERT_EQUAL_INT(9, r.problems);
  TEST_ASSERT_NOT_NULL(strstr(g_log, "materials of bad, entry 0: the data is not a list of entries"));
  TEST_ASSERT_NOT_NULL(strstr(g_log, "materials of bad, entry 1: not a table"));
  TEST_ASSERT_NOT_NULL(strstr(g_log, "materials of bad, entry 2: needs exactly one of id, override or patch"));
  TEST_ASSERT_NOT_NULL(strstr(g_log, "entry 5: ids use a-z, 0-9 and _ and are short: Bad-Id"));
  TEST_ASSERT_NOT_NULL(strstr(g_log, "entry 7: defined twice: bad:ok"));
  TEST_ASSERT_NOT_NULL(strstr(g_log, "entry 8: cannot patch an entry there is not (or two packages define): missing"));
  TEST_ASSERT_NOT_NULL(strstr(g_log, "entry 9: cannot override an entry"));
  zb_log_remove_sink(capture, NULL);
  zb_registry_free(&r);
}

/* spec: materials: Broken entries are skipped */
static void test_out_of_memory(void) {
  const char *text =
      "return { { id = 'a', x = { 1, 2, 'three' } }, { id = 'b' }, { patch = 'a', y = 'z' },"
      "  { override = 'b', w = 1 } }";
  zb_luon_doc doc;
  zb_luon_error e;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_luon_parse(&doc, zb_test_alloc(), text, strlen(text), &e));
  bool ok = false;
  for (long budget = 0; budget < 40 && !ok; ++budget) {
    zb_failing_allocator fa;
    zb_registry r;
    zb_registry_init(&r, zb_failing_init(&fa, zb_test_alloc(), budget), "x");
    zb_result res = zb_registry_merge(&r, "p", &doc.root);
    TEST_ASSERT_TRUE(res == ZB_OK || res == ZB_ERR_NOMEM);
    ok = res == ZB_OK;
    zb_registry_free(&r);
  }
  TEST_ASSERT_TRUE(ok);
  zb_luon_free(&doc);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_merge);
  RUN_TEST(test_problems);
  RUN_TEST(test_out_of_memory);
  return UNITY_END();
}

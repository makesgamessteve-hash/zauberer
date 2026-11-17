/* SPDX-License-Identifier: MIT */
#include "core/luon.h"
#include "support/test_support.h"
#include "unity.h"

#include <string.h>

ZB_TEST_LEAK_CHECKED

static zb_luon_doc parse_ok(const char *text) {
  zb_luon_doc doc;
  zb_luon_error err;
  zb_result r = zb_luon_parse(&doc, zb_test_alloc(), text, strlen(text), &err);
  if (r != ZB_OK) {
    TEST_FAIL_MESSAGE(err.message);
  }
  return doc;
}

static zb_luon_error parse_err(const char *text) {
  zb_luon_doc doc;
  zb_luon_error err;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_PARSE, zb_luon_parse(&doc, zb_test_alloc(), text, strlen(text), &err));
  TEST_ASSERT_EQUAL_INT(ZB_VAL_NIL, doc.root.type);
  return err;
}

static void test_manifest_shape(void) {
  zb_luon_doc doc = parse_ok(
      "-- package manifest\n"
      "return {\n"
      "  id = \"base\", version = '0.1.0', api = \"1.0\",\n"
      "  license = { code = \"MIT\", assets = \"CC-BY-4.0\" },\n"
      "  requires = { },\n"
      "  [\"weird key\"] = true; [42] = false,\n"
      "  list = { 1, 2.5, -3, 0x10, 1e3, .5 },\n"
      "}\n");
  const zb_value *root = &doc.root;
  TEST_ASSERT_TRUE(zb_value_is(root, ZB_VAL_TABLE));
  TEST_ASSERT_EQUAL_STRING("base", zb_value_str(zb_table_get(root, "id"), NULL));
  TEST_ASSERT_EQUAL_STRING("0.1.0", zb_value_str(zb_table_get(root, "version"), NULL));
  const zb_value *lic = zb_table_get(root, "license");
  TEST_ASSERT_EQUAL_STRING("CC-BY-4.0", zb_value_str(zb_table_get(lic, "assets"), NULL));
  TEST_ASSERT_EQUAL_size_t(0, zb_table_count(zb_table_get(root, "requires")));
  TEST_ASSERT_TRUE(zb_value_bool(zb_table_get(root, "weird key"), false));
  const zb_value *list = zb_table_get(root, "list");
  TEST_ASSERT_EQUAL_size_t(6, zb_table_len(list));
  TEST_ASSERT_EQUAL_DOUBLE(2.5, zb_value_num(zb_table_index(list, 2), 0));
  TEST_ASSERT_EQUAL_DOUBLE(-3, zb_value_num(zb_table_index(list, 3), 0));
  TEST_ASSERT_EQUAL_DOUBLE(16, zb_value_num(zb_table_index(list, 4), 0));
  TEST_ASSERT_EQUAL_DOUBLE(1000, zb_value_num(zb_table_index(list, 5), 0));
  TEST_ASSERT_EQUAL_DOUBLE(0.5, zb_value_num(zb_table_index(list, 6), 0));
  TEST_ASSERT_NULL(zb_table_index(list, 7));
  TEST_ASSERT_FALSE(zb_value_bool(zb_table_index(root, 42), true));
  TEST_ASSERT_NULL(zb_table_get(root, "missing"));
  TEST_ASSERT_NULL(zb_table_get(zb_table_get(root, "id"), "x")); /* not a table */
  TEST_ASSERT_NULL(zb_table_index(NULL, 1));
  TEST_ASSERT_EQUAL_size_t(0, zb_table_count(NULL));
  TEST_ASSERT_NULL(zb_table_entry_at(root, 999));
  TEST_ASSERT_NOT_NULL(zb_table_entry_at(root, 0));
  TEST_ASSERT_EQUAL_STRING("dflt", zb_value_str(zb_table_get(root, "nope"), "dflt"));
  TEST_ASSERT_EQUAL_DOUBLE(7, zb_value_num(NULL, 7));
  zb_luon_free(&doc);
}

static void test_scalars_and_keywords(void) {
  zb_luon_doc d = parse_ok("nil");
  TEST_ASSERT_EQUAL_INT(ZB_VAL_NIL, d.root.type);
  zb_luon_free(&d);
  d = parse_ok("  true ; ");
  TEST_ASSERT_TRUE(d.root.as.b);
  zb_luon_free(&d);
  d = parse_ok("\xEF\xBB\xBF" "false");
  TEST_ASSERT_FALSE(d.root.as.b);
  zb_luon_free(&d);
  d = parse_ok("- 12");
  TEST_ASSERT_EQUAL_DOUBLE(-12, d.root.as.num);
  zb_luon_free(&d);
  d = parse_ok("{ nilly = 1, truex = 2 }");
  TEST_ASSERT_EQUAL_DOUBLE(1, zb_value_num(zb_table_get(&d.root, "nilly"), 0));
  zb_luon_free(&d);
}

static void test_strings_and_escapes(void) {
  zb_luon_doc d = parse_ok("{ a = \"tab\\tnl\\nq\\\"s\\'bs\\\\\", b = '\\x41\\65\\u{263A}\\u{10348}\\u{7FF}', "
                           "c = [[\nlong\nstring]], d = [==[with ]] inside]==], e = \"\\a\\b\\f\\v\\r\", "
                           "f = \"line\\\ncont\", g = \"a\\z   \n  b\" }");
  TEST_ASSERT_EQUAL_STRING("tab\tnl\nq\"s'bs\\", zb_value_str(zb_table_get(&d.root, "a"), NULL));
  TEST_ASSERT_EQUAL_STRING("AA\xE2\x98\xBA\xF0\x90\x8D\x88\xDF\xBF", zb_value_str(zb_table_get(&d.root, "b"), NULL));
  TEST_ASSERT_EQUAL_STRING("long\nstring", zb_value_str(zb_table_get(&d.root, "c"), NULL));
  TEST_ASSERT_EQUAL_STRING("with ]] inside", zb_value_str(zb_table_get(&d.root, "d"), NULL));
  TEST_ASSERT_EQUAL_STRING("\a\b\f\v\r", zb_value_str(zb_table_get(&d.root, "e"), NULL));
  TEST_ASSERT_EQUAL_STRING("line\ncont", zb_value_str(zb_table_get(&d.root, "f"), NULL));
  TEST_ASSERT_EQUAL_STRING("ab", zb_value_str(zb_table_get(&d.root, "g"), NULL));
  zb_luon_free(&d);
  d = parse_ok("\"\"");
  TEST_ASSERT_EQUAL_size_t(0, d.root.as.str.len);
  zb_luon_free(&d);
}

static void test_comments(void) {
  zb_luon_doc d = parse_ok("--[[ block\ncomment ]] --[==[ another ]==]\n{ x = 1 -- trailing\n }");
  TEST_ASSERT_EQUAL_DOUBLE(1, zb_value_num(zb_table_get(&d.root, "x"), 0));
  zb_luon_free(&d);
}

static void test_errors_report_position(void) {
  zb_luon_error e = parse_err("{\n  id = \"base\",\n  version = ,\n}");
  TEST_ASSERT_EQUAL_INT(3, e.line);
  TEST_ASSERT_TRUE(e.column > 1);
  TEST_ASSERT_NOT_NULL(strstr(e.message, "unexpected"));
  e = parse_err("{ x = 1 y = 2 }");
  TEST_ASSERT_NOT_NULL(strstr(e.message, "'}' expected"));
  e = parse_err("\"unterminated");
  TEST_ASSERT_NOT_NULL(strstr(e.message, "unfinished string"));
  e = parse_err("{ [nil] = 1 }");
  TEST_ASSERT_NOT_NULL(strstr(e.message, "nil"));
  e = parse_err("{ [1 = 1 }");
  TEST_ASSERT_NOT_NULL(strstr(e.message, "']' expected"));
  e = parse_err("{ [1] 1 }");
  TEST_ASSERT_NOT_NULL(strstr(e.message, "'=' expected"));
  e = parse_err("{} extra");
  TEST_ASSERT_NOT_NULL(strstr(e.message, "trailing"));
  e = parse_err("");
  TEST_ASSERT_NOT_NULL(strstr(e.message, "end of input"));
  e = parse_err("os.execute('x')");
  e = parse_err("12abc");
  TEST_ASSERT_NOT_NULL(strstr(e.message, "malformed number"));
  e = parse_err("0x");
  e = parse_err("1.2.3");
  e = parse_err("- x");
  e = parse_err("--[[ never closed");
  e = parse_err("[[ never closed");
  e = parse_err("'\\q'");
  e = parse_err("'\\xZZ'");
  e = parse_err("'\\u263A'");
  e = parse_err("'\\u{FFFFFFF}'");
  e = parse_err("'\\999'");
  e = parse_err("'line\nbreak'");
  e = parse_err("{ x = { y = } }");
  e = parse_err("{ x = 1, [2] = }");
  e = parse_err("{ x = }");
  char deep[200] = {0};
  for (int i = 0; i < ZB_LUON_MAX_DEPTH + 1; ++i) {
    deep[i] = '{';
  }
  e = parse_err(deep);
  TEST_ASSERT_NOT_NULL(strstr(e.message, "nested"));
  char huge[100];
  memset(huge, '1', sizeof huge - 1);
  huge[sizeof huge - 1] = '\0';
  parse_err(huge);
  zb_luon_doc doc;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_PARSE, zb_luon_parse(&doc, zb_test_alloc(), "{", 1, NULL));
}

static void test_out_of_memory(void) {
  for (long budget = 0; budget < 12; ++budget) {
    zb_failing_allocator f;
    zb_luon_doc doc;
    zb_luon_error err;
    zb_result r = zb_luon_parse(&doc, zb_failing_init(&f, zb_test_alloc(), budget),
                                "{ a = { 'x', 'y' }, b = \"str\" }", 31, &err);
    if (r == ZB_OK) {
      zb_luon_free(&doc);
    } else {
      TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, r);
    }
  }
}

static void test_type_names(void) {
  TEST_ASSERT_EQUAL_STRING("table", zb_value_type_name(ZB_VAL_TABLE));
  TEST_ASSERT_EQUAL_STRING("nil", zb_value_type_name(ZB_VAL_NIL));
  TEST_ASSERT_EQUAL_STRING("boolean", zb_value_type_name(ZB_VAL_BOOL));
  TEST_ASSERT_EQUAL_STRING("number", zb_value_type_name(ZB_VAL_NUMBER));
  TEST_ASSERT_EQUAL_STRING("string", zb_value_type_name(ZB_VAL_STRING));
  TEST_ASSERT_EQUAL_STRING("?", zb_value_type_name((zb_value_type)99));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_manifest_shape);
  RUN_TEST(test_scalars_and_keywords);
  RUN_TEST(test_strings_and_escapes);
  RUN_TEST(test_comments);
  RUN_TEST(test_errors_report_position);
  RUN_TEST(test_out_of_memory);
  RUN_TEST(test_type_names);
  return UNITY_END();
}

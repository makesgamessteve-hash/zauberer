/* SPDX-License-Identifier: MIT */
/* Lua unit tests (--test) and scripted scenarios (--scenario). */
#include "script/script_internal.h"

#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REG_CASES "zb.cases"
#define REG_CAPTURE "zb.capture"

/* ---- expectations (shared by tests and scenarios) ------------------------------ */

static void value_repr(lua_State *L, int idx, char *buf, size_t len) {
  if (lua_type(L, idx) == LUA_TSTRING) {
    snprintf(buf, len, "\"%s\"", lua_tostring(L, idx));
  } else {
    snprintf(buf, len, "%s", luaL_tolstring(L, idx, NULL));
    lua_pop(L, 1);
  }
}

/* In tests a failed expectation raises an error; in scenarios it is counted
 * and execution continues. */
static int fail(lua_State *L, const char *fmt, ...) ZB_PRINTF(2, 3);

static int fail(lua_State *L, const char *fmt, ...) {
  zb_script *s = zb_script_from(L);
  char msg[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  if (s->scenario_started && L == s->co) {
    lua_Debug ar;
    char where[256] = "";
    if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
      snprintf(where, sizeof where, "%s:%d: ", ar.short_src, ar.currentline);
    }
    s->scenario_failures++;
    uint64_t tick = s->cfg.host.tick_count ? s->cfg.host.tick_count(s->cfg.host.ctx) : 0;
    zb_error("scenario '%s' FAILED at tick %llu: %s%s", s->scenario_name, (unsigned long long)tick, where, msg);
    lua_pushboolean(L, 0);
    return 1;
  }
  luaL_where(L, 1);
  lua_pushstring(L, msg);
  lua_concat(L, 2);
  return lua_error(L);
}

/* @lua test.expect(value: any, message?: string): boolean
 * Fails unless value is truthy. Also available as expect() in scenarios, where failures are counted and the
 * scenario continues.
 */
static int t_expect(lua_State *L) {
  if (!lua_toboolean(L, 1)) {
    return fail(L, "expectation failed: %s", luaL_optstring(L, 2, "(no message)"));
  }
  lua_pushboolean(L, 1);
  return 1;
}

/* @lua test.expect_eq(actual: any, expected: any, message?: string): boolean
 * Fails unless actual == expected. Also expect_eq() in scenarios.
 */
static int t_expect_eq(lua_State *L) {
  if (!lua_compare(L, 1, 2, LUA_OPEQ)) {
    char a[200], b[200];
    value_repr(L, 1, a, sizeof a);
    value_repr(L, 2, b, sizeof b);
    return fail(L, "%s: expected %s, got %s", luaL_optstring(L, 3, "values differ"), b, a);
  }
  lua_pushboolean(L, 1);
  return 1;
}

/* @lua test.expect_near(actual: number, expected: number, eps?: number, message?: string): boolean
 * Fails unless |actual - expected| <= eps (default 1e-6). Also expect_near() in scenarios.
 */
static int t_expect_near(lua_State *L) {
  double a = luaL_checknumber(L, 1), b = luaL_checknumber(L, 2), eps = luaL_optnumber(L, 3, 1e-6);
  if (!(a >= b - eps && a <= b + eps)) {
    return fail(L, "%s: expected %g (+/- %g), got %g", luaL_optstring(L, 4, "values differ"), b, eps, a);
  }
  lua_pushboolean(L, 1);
  return 1;
}

/* @lua test.expect_error(fn: function, substring?: string): boolean
 * Fails unless fn raises an error (containing substring). Also expect_error() in scenarios.
 */
static int t_expect_error(lua_State *L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  const char *pattern = luaL_optstring(L, 2, NULL);
  lua_pushvalue(L, 1);
  if (lua_pcall(L, 0, 0, 0) == LUA_OK) {
    return fail(L, "expected an error, but the function succeeded");
  }
  const char *msg = lua_tostring(L, -1);
  if (pattern && (!msg || !strstr(msg, pattern))) {
    return fail(L, "error '%s' does not contain '%s'", msg ? msg : "?", pattern);
  }
  lua_pushboolean(L, 1);
  return 1;
}

/* @lua test.case(name: string, fn: function)
 * Lua tests (--test): registers a test case.
 */
static int t_case(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_getfield(L, LUA_REGISTRYINDEX, REG_CASES);
  lua_newtable(L);
  lua_pushstring(L, name);
  lua_setfield(L, -2, "name");
  lua_pushvalue(L, 2);
  lua_setfield(L, -2, "fn");
  lua_getfield(L, LUA_REGISTRYINDEX, "zb.current_file");
  lua_setfield(L, -2, "file");
  lua_rawseti(L, -2, (lua_Integer)luaL_len(L, -2) + 1);
  return 0;
}

typedef struct file_list {
  const zb_allocator *a;
  zb_array names;
} file_list;

/* *_test.lua or test_*.lua; other files in a test directory are helpers. */
static bool is_test_file(const char *name) {
  size_t n = strlen(name);
  if (n <= 4 || strcmp(name + n - 4, ".lua") != 0) {
    return false;
  }
  return strncmp(name, "test_", 5) == 0 || (n > 9 && strcmp(name + n - 9, "_test.lua") == 0);
}

static bool collect(const char *name, bool is_dir, void *user) {
  file_list *fl = user;
  if (!is_dir && is_test_file(name)) {
    char *copy = zb_strdup(fl->a, name);
    if (copy && !zb_array_push(&fl->names, &copy)) {
      zb_free(fl->a, copy);
    }
  }
  return true;
}

static int cmp_names(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static bool load_test_file(zb_script *s, const char *path) {
  lua_State *L = s->L;
  const zb_fs_api *fs = s->cfg.fs;
  char *text = NULL;
  size_t len = 0;
  if (fs->read(fs->ctx, path, s->cfg.a, &text, &len) != ZB_OK) {
    zb_script_report(s, "test", "cannot read test file");
    return false;
  }
  lua_pushstring(L, path);
  lua_setfield(L, LUA_REGISTRYINDEX, "zb.current_file");
  char name[1100];
  snprintf(name, sizeof name, "@%s", path);
  int status = luaL_loadbufferx(L, text, len, name, "t");
  zb_free(s->cfg.a, text);
  if (status != LUA_OK) {
    zb_script_report(s, "test", lua_tostring(L, -1));
    lua_pop(L, 1);
    return false;
  }
  zb_script_new_env(s, -1);
  lua_setupvalue(L, -2, 1);
  return zb_script_pcall(s, 0, 0, "test", false);
}

static void xml_escape(FILE *f, const char *s) {
  for (; *s; ++s) {
    switch (*s) {
      case '<': fputs("&lt;", f); break;
      case '>': fputs("&gt;", f); break;
      case '&': fputs("&amp;", f); break;
      case '"': fputs("&quot;", f); break;
      default: fputc(*s, f); break;
    }
  }
}

int zb_script_run_tests(zb_script *s, const char *path, const char *junit_path) {
  lua_State *L = s->L;
  const zb_fs_api *fs = s->cfg.fs;
  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, REG_CASES);
  zb_fs_stat st;
  if (fs->stat(fs->ctx, path, &st) != ZB_OK) {
    zb_error("test path '%s' not found", path);
    return -1;
  }
  int load_failures = 0;
  if (st.is_dir) {
    file_list fl = {s->cfg.a, {0}};
    zb_array_init(&fl.names, s->cfg.a, sizeof(char *));
    fs->list(fs->ctx, path, collect, &fl);
    if (fl.names.len > 1) {
      qsort(fl.names.data, fl.names.len, sizeof(char *), cmp_names);
    }
    for (size_t i = 0; i < fl.names.len; ++i) {
      char *n = *(char **)zb_array_at(&fl.names, i);
      char full[1200];
      zb_path_join(full, sizeof full, path, n);
      load_failures += load_test_file(s, full) ? 0 : 1;
      zb_free(s->cfg.a, n);
    }
    zb_array_free(&fl.names);
  } else {
    load_failures += load_test_file(s, path) ? 0 : 1;
  }
  lua_getfield(L, LUA_REGISTRYINDEX, REG_CASES);
  int cases = lua_gettop(L);
  lua_Integer n = (lua_Integer)luaL_len(L, cases);
  int failures = load_failures;
  FILE *xml = NULL;
  if (junit_path) {
    xml = fopen(junit_path, "w");
    if (xml) {
      fprintf(xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuite name=\"lua\" tests=\"%lld\">\n",
              (long long)n);
    }
  }
  for (lua_Integer i = 1; i <= n; ++i) {
    lua_rawgeti(L, cases, i);
    lua_getfield(L, -1, "name");
    lua_getfield(L, -2, "file");
    const char *name = lua_tostring(L, -2);
    const char *file = lua_tostring(L, -1);
    lua_getfield(L, -3, "fn");
    size_t errors_before = s->error_count;
    bool ok = zb_script_pcall(s, 0, 0, "test", true);
    if (ok) {
      zb_info("PASS %s: %s", file, name);
    } else {
      failures++;
      zb_error("FAIL %s: %s", file, name);
    }
    if (xml) {
      fputs("  <testcase classname=\"", xml);
      xml_escape(xml, file);
      fputs("\" name=\"", xml);
      xml_escape(xml, name);
      fputs("\">", xml);
      if (!ok) {
        fputs("<failure message=\"", xml);
        xml_escape(xml,
                   s->error_count > errors_before ? s->errors[(s->error_count - 1) % ZB_SCRIPT_MAX_ERRORS] : "failed");
        fputs("\"/>", xml);
      }
      fputs("</testcase>\n", xml);
    }
    lua_pop(L, 3);
  }
  if (xml) {
    fputs("</testsuite>\n", xml);
    fclose(xml);
  }
  lua_pop(L, 1);
  zb_info("%lld Lua test(s), %d failure(s)", (long long)n, failures);
  return failures;
}

/* @lua scenario(info: { name: string })
 * Scenarios (--scenario): names the scenario in reports.
 */
static int sc_scenario(lua_State *L) {
  zb_script *s = zb_script_from(L);
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_getfield(L, 1, "name");
  if (lua_isstring(L, -1)) {
    snprintf(s->scenario_name, sizeof s->scenario_name, "%s", lua_tostring(L, -1));
  }
  lua_pop(L, 1);
  return 0;
}

/* @lua step(ticks?: integer)
 * Scenarios: runs ticks simulation ticks (default 1) before continuing.
 */
static int sc_step(lua_State *L) {
  lua_Integer n = luaL_optinteger(L, 1, 1);
  luaL_argcheck(L, n >= 0, 1, "tick count must not be negative");
  lua_pushinteger(L, n);
  return lua_yield(L, 1);
}

static zb_result capture_into_registry(lua_State *L, bool internal) {
  zb_script *s = zb_script_from(L);
  zb_image img;
  zb_result r = zb_render_capture(s->cfg.render, internal, s->cfg.a, &img);
  if (r != ZB_OK) {
    return r;
  }
  lua_newtable(L);
  lua_pushlstring(L, (const char *)img.rgba, (size_t)img.width * (size_t)img.height * 4u);
  lua_setfield(L, -2, "rgba");
  lua_pushinteger(L, img.width);
  lua_setfield(L, -2, "width");
  lua_pushinteger(L, img.height);
  lua_setfield(L, -2, "height");
  zb_image_free(s->cfg.a, &img);
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_REGISTRYINDEX, REG_CAPTURE);
  return ZB_OK;
}

/* @lua render.capture(target?: "internal"|"window"): { width: integer, height: integer }?, string?
 * Test API: reads back the last frame.
 */
static int sc_capture(lua_State *L) {
  const char *target = luaL_optstring(L, 1, "internal");
  zb_result r = capture_into_registry(L, strcmp(target, "window") != 0);
  if (r != ZB_OK) {
    lua_pushnil(L);
    lua_pushstring(L, zb_result_str(r));
    return 2;
  }
  lua_newtable(L);
  lua_getfield(L, -2, "width");
  lua_setfield(L, -2, "width");
  lua_getfield(L, -2, "height");
  lua_setfield(L, -2, "height");
  return 1;
}

/* @lua render.pixel(x: integer, y: integer): integer, integer, integer, integer
 * Test API: RGBA of a pixel of the last capture (origin top-left).
 */
static int sc_pixel(lua_State *L) {
  lua_Integer x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
  if (lua_getfield(L, LUA_REGISTRYINDEX, REG_CAPTURE) != LUA_TTABLE) {
    return luaL_error(L, "no capture yet (call render.capture first)");
  }
  lua_getfield(L, -1, "width");
  lua_getfield(L, -2, "height");
  lua_getfield(L, -3, "rgba");
  lua_Integer w = lua_tointeger(L, -3), h = lua_tointeger(L, -2);
  luaL_argcheck(L, x >= 0 && x < w && y >= 0 && y < h, 1, "pixel outside the capture");
  const unsigned char *px = (const unsigned char *)lua_tostring(L, -1) + ((size_t)y * (size_t)w + (size_t)x) * 4u;
  for (int i = 0; i < 4; ++i) {
    lua_pushinteger(L, px[i]);
  }
  return 4;
}

/* @lua render.match_golden(name: string, target?: "internal"|"window"): boolean, string
 * Test API: compares the frame with tests/golden/<name>.png (channel delta <= 8, <= 0.5 % pixels);
 * ZB_UPDATE_GOLDEN=1 creates missing images.
 */
static int sc_match_golden(lua_State *L) {
  zb_script *s = zb_script_from(L);
  const char *name = luaL_checkstring(L, 1);
  const char *target = luaL_optstring(L, 2, "internal");
  zb_image actual;
  zb_result r = zb_render_capture(s->cfg.render, strcmp(target, "window") != 0, s->cfg.a, &actual);
  if (r != ZB_OK) {
    lua_pushboolean(L, 0);
    lua_pushfstring(L, "capture failed: %s", zb_result_str(r));
    return 2;
  }
  const zb_fs_api *fs = s->cfg.fs;
  unsigned char *png = NULL;
  size_t png_len = 0;
  zb_image_encode_png(s->cfg.a, &actual, &png, &png_len);
  char path[1200];
  if (s->out_dir[0] && png) {
    snprintf(path, sizeof path, "%s/%s.png", s->out_dir, name);
    fs->write(fs->ctx, path, png, png_len);
  }
  snprintf(path, sizeof path, "%s/%s.png", s->golden_dir[0] ? s->golden_dir : ".", name);
  char *golden_png = NULL;
  size_t golden_len = 0;
  bool ok = false;
  char info[1400];
  if (fs->read(fs->ctx, path, s->cfg.a, &golden_png, &golden_len) != ZB_OK) {
    if (getenv("ZB_UPDATE_GOLDEN") && png && fs->write(fs->ctx, path, png, png_len) == ZB_OK) {
      ok = true;
      snprintf(info, sizeof info, "golden image created: %s", path);
    } else {
      snprintf(info, sizeof info, "golden image missing: %s (set ZB_UPDATE_GOLDEN=1 to create it)", path);
    }
  } else {
    zb_image golden;
    if (zb_image_decode_png(s->cfg.a, golden_png, golden_len, &golden) == ZB_OK) {
      zb_image_diff diff;
      ok = zb_image_compare(&actual, &golden, 8, 0.005, &diff);
      snprintf(info, sizeof info, "%s: %.3f%% pixels differ (max delta %d)%s", name, diff.fraction_over * 100.0,
               diff.max_delta, diff.same_size ? "" : ", sizes differ");
      zb_image_free(s->cfg.a, &golden);
    } else {
      snprintf(info, sizeof info, "golden image %s cannot be decoded", path);
    }
    zb_free(s->cfg.a, golden_png);
  }
  zb_free(s->cfg.a, png);
  zb_image_free(s->cfg.a, &actual);
  lua_pushboolean(L, ok);
  lua_pushstring(L, info);
  return 2;
}

void zb_script_open_test_api(zb_script *s) {
  lua_State *L = s->L;
  static const luaL_Reg test_fns[] = {{"case", t_case},
                                      {"expect", t_expect},
                                      {"expect_eq", t_expect_eq},
                                      {"expect_near", t_expect_near},
                                      {"expect_error", t_expect_error},
                                      {NULL, NULL}};
  luaL_newlib(L, test_fns);
  lua_setglobal(L, "test");
  lua_getglobal(L, "render");
  lua_pushcfunction(L, sc_capture);
  lua_setfield(L, -2, "capture");
  lua_pushcfunction(L, sc_pixel);
  lua_setfield(L, -2, "pixel");
  lua_pushcfunction(L, sc_match_golden);
  lua_setfield(L, -2, "match_golden");
  lua_pop(L, 1);
}

zb_result zb_script_scenario_start(zb_script *s, const char *path) {
  lua_State *L = s->L;
  const zb_fs_api *fs = s->cfg.fs;
  char *text = NULL;
  size_t len = 0;
  if (fs->read(fs->ctx, path, s->cfg.a, &text, &len) != ZB_OK) {
    zb_error("scenario '%s' not found", path);
    return ZB_ERR_NOT_FOUND;
  }
  snprintf(s->scenario_name, sizeof s->scenario_name, "%s", path);
  s->co = lua_newthread(L);
  s->co_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  char name[1100];
  snprintf(name, sizeof name, "@%s", path);
  int status = luaL_loadbufferx(s->co, text, len, name, "t");
  zb_free(s->cfg.a, text);
  if (status != LUA_OK) {
    zb_script_report(s, "scenario", lua_tostring(s->co, -1));
    luaL_unref(L, LUA_REGISTRYINDEX, s->co_ref);
    s->co_ref = LUA_NOREF;
    s->co = NULL;
    s->scenario_failures++;
    return ZB_ERR_PARSE;
  }
  zb_script_new_env(s, -1);
  int env = lua_gettop(L);
  static const luaL_Reg fns[] = {{"scenario", sc_scenario},
                                 {"step", sc_step},
                                 {"expect", t_expect},
                                 {"expect_eq", t_expect_eq},
                                 {"expect_near", t_expect_near},
                                 {"expect_error", t_expect_error},
                                 {NULL, NULL}};
  for (const luaL_Reg *f = fns; f->name; ++f) {
    lua_pushcfunction(L, f->func);
    lua_setfield(L, env, f->name);
  }
  lua_xmove(L, s->co, 1);
  lua_setupvalue(s->co, -2, 1);
  s->scenario_started = true;
  return ZB_OK;
}

bool zb_script_scenario_resume(zb_script *s, uint64_t *ticks) {
  *ticks = 0;
  if (!s->co) {
    return false;
  }
  int nres = 0;
  bool prev = s->budget_active;
  s->budget_active = false;
  int status = lua_resume(s->co, s->L, 0, &nres);
  s->budget_active = prev;
  if (status == LUA_YIELD) {
    *ticks = nres > 0 ? (uint64_t)lua_tointeger(s->co, -1) : 1;
    lua_pop(s->co, nres);
    return true;
  }
  if (status != LUA_OK) {
    luaL_traceback(s->L, s->co, lua_tostring(s->co, -1), 0);
    zb_script_report(s, "scenario", lua_tostring(s->L, -1));
    lua_pop(s->L, 1);
    s->scenario_failures++;
  }
  luaL_unref(s->L, LUA_REGISTRYINDEX, s->co_ref);
  s->co_ref = LUA_NOREF;
  s->co = NULL;
  return false;
}

int zb_script_scenario_failures(const zb_script *s) { return s->scenario_failures; }

const char *zb_script_scenario_name(const zb_script *s) { return s->scenario_name; }

void zb_script_set_golden_dirs(zb_script *s, const char *golden_dir, const char *out_dir) {
  snprintf(s->golden_dir, sizeof s->golden_dir, "%s", golden_dir ? golden_dir : "");
  snprintf(s->out_dir, sizeof s->out_dir, "%s", out_dir ? out_dir : "");
}

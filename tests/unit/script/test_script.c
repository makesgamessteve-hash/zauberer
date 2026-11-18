/* SPDX-License-Identifier: MIT */
#include "core/log.h"
#include "core/memfs.h"
#include "net/session.h"
#include "net/socket.h"
#include "script/script_internal.h"
#include "support/test_support.h"
#include "support/test_threads.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- fixture ------------------------------------------------------------------ */

typedef struct fake_host {
  int quit_code;
  bool quit_called;
  bool paused;
  double time_scale;
  bool fullscreen;
  uint64_t ticks;
  double time;
  zb_result save_result;
  int saves;
} fake_host;

typedef struct fake_backend {
  int frames;
  uint32_t next;
  unsigned char shade; /* first channel of every captured pixel */
} fake_backend;

typedef struct fixture {
  zb_memfs *fs;
  zb_vfs *vfs;
  zb_input *input;
  zb_render *render;
  zb_audio *audio;
  zb_script *script;
  fake_host host;
  fake_backend backend;
  zb_script_config cfg;
} fixture;

static fixture F;
static int g_errors;
static int g_warnings;
static char g_last_error[1024];

static void sink(zb_log_level level, const char *msg, void *user) {
  (void)user;
  if (level == ZB_LOG_ERROR) {
    g_errors++;
    snprintf(g_last_error, sizeof g_last_error, "%s", msg);
  } else if (level == ZB_LOG_WARN) {
    g_warnings++;
  }
}

void setUp(void) {
  zb_test_begin();
  zb_log_reset();
  zb_log_set_stderr(false);
  zb_log_add_sink(sink, NULL);
  g_errors = 0;
  g_warnings = 0;
  g_last_error[0] = '\0';
  memset(&F, 0, sizeof F);
  F.fs = zb_memfs_create(zb_test_alloc());
}

static void stop(void) {
  zb_script_destroy(F.script);
  F.script = NULL;
  zb_audio_destroy(F.audio);
  F.audio = NULL;
  zb_render_destroy(F.render);
  F.render = NULL;
  zb_input_destroy(F.input);
  F.input = NULL;
  zb_vfs_destroy(F.vfs);
  F.vfs = NULL;
}

void tearDown(void) {
  stop();
  zb_memfs_destroy(F.fs);
  zb_log_reset();
  zb_test_end();
}

static void put(const char *path, const char *text) { zb_memfs_put(F.fs, path, text, strlen(text)); }

static void package(const char *root, const char *id, const char *extra, const char *main_lua) {
  char path[256], text[1024];
  snprintf(path, sizeof path, "%s/package.lua", root);
  snprintf(text, sizeof text,
           "return { id = \"%s\", version = \"1.0.0\", api = \"1.0\", license = { code = \"MIT\", assets = "
           "\"CC-BY-4.0\" }, %s }",
           id, extra ? extra : "");
  put(path, text);
  if (main_lua) {
    snprintf(path, sizeof path, "%s/scripts/main.lua", root);
    put(path, main_lua);
  }
}

/* ---- fake render backend (supports capture) ---------------------------------- */

static void fb_destroy(void *ctx) { (void)ctx; }
static uint32_t fb_texture(void *ctx, const zb_image *img) {
  (void)img;
  return ++((fake_backend *)ctx)->next;
}
static void fb_destroy_handle(void *ctx, uint32_t h) {
  (void)ctx;
  (void)h;
}
static uint32_t fb_mesh(void *ctx, const zb_mesh_data *m) {
  (void)m;
  return ++((fake_backend *)ctx)->next;
}
static void fb_execute(void *ctx, const zb_frame *frame) {
  (void)frame;
  ((fake_backend *)ctx)->frames++;
}
static zb_result fb_capture(void *ctx, bool internal, const zb_allocator *a, zb_image *out) {
  fake_backend *fb = ctx;
  out->width = internal ? 4 : 8;
  out->height = 2;
  out->rgba = zb_calloc(a, (size_t)out->width * 2u * 4u);
  if (!out->rgba) {
    return ZB_ERR_NOMEM;
  }
  for (int i = 0; i < out->width * 2; ++i) {
    out->rgba[i * 4] = fb->shade;
    out->rgba[i * 4 + 1] = (unsigned char)i;
    out->rgba[i * 4 + 2] = 7;
    out->rgba[i * 4 + 3] = 255;
  }
  return ZB_OK;
}

static zb_render_backend g_fake_vtable;

static zb_render_backend *fake_backend_create(void) {
  zb_render_backend *b = &g_fake_vtable;
  memset(b, 0, sizeof(*b));
  b->ctx = &F.backend;
  b->name = "fake";
  b->destroy = fb_destroy;
  b->create_texture = fb_texture;
  b->destroy_texture = fb_destroy_handle;
  b->create_mesh = fb_mesh;
  b->destroy_mesh = fb_destroy_handle;
  b->execute = fb_execute;
  b->capture = fb_capture;
  return b;
}

/* ---- fake host ----------------------------------------------------------------- */

static void h_quit(void *ctx, int code) {
  fake_host *h = ctx;
  h->quit_called = true;
  h->quit_code = code;
}
static void h_info(void *ctx, zb_script_info *out) {
  (void)ctx;
  out->platform = "testos";
  out->build = "development";
  out->input_mode = "touch";
  out->drawable_w = 1280;
  out->drawable_h = 720;
  out->seed = 99;
}
static double h_time(void *ctx) { return ((fake_host *)ctx)->time; }
static uint64_t h_ticks(void *ctx) { return ((fake_host *)ctx)->ticks; }
static void h_pause(void *ctx, bool paused) { ((fake_host *)ctx)->paused = paused; }
static void h_scale(void *ctx, double s) { ((fake_host *)ctx)->time_scale = s; }
static void h_fullscreen(void *ctx, bool on) { ((fake_host *)ctx)->fullscreen = on; }
static int g_user_data_changes;
static void h_user_data(void *ctx) {
  (void)ctx;
  g_user_data_changes++;
}

static zb_result h_save(void *ctx) {
  fake_host *h = ctx;
  h->saves++;
  return h->save_result;
}

static void configure(void) {
  memset(&F.cfg, 0, sizeof F.cfg);
  F.cfg.a = zb_test_alloc();
  F.cfg.fs = zb_memfs_api(F.fs);
  F.cfg.user_dir = "user/";
  F.cfg.seed = 42;
  F.cfg.test_api = true;
  F.cfg.host.ctx = &F.host;
  F.cfg.host.quit = h_quit;
  F.cfg.host.get_info = h_info;
  F.cfg.host.sim_time = h_time;
  F.cfg.host.tick_count = h_ticks;
  F.cfg.host.set_paused = h_pause;
  F.cfg.host.set_time_scale = h_scale;
  F.cfg.host.set_fullscreen = h_fullscreen;
  F.cfg.host.save_bindings = h_save;
  F.cfg.host.user_data_changed = h_user_data;
}

/* Mounts the packages and creates the script host (configure() first). */
static void start_with(const zb_script_config *cfg) {
  F.vfs = zb_vfs_create(zb_test_alloc(), zb_memfs_api(F.fs), NULL);
  zb_vfs_add_root(F.vfs, "assets");
  zb_vfs_add_root(F.vfs, "mods");
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_mount(F.vfs, 1, 0));
  F.input = zb_input_create(zb_test_alloc());
  F.render = zb_render_create(zb_test_alloc(), F.vfs, fake_backend_create());
  F.audio = zb_audio_create(zb_test_alloc(), F.vfs, NULL);
  zb_script_config c = *cfg;
  c.vfs = F.vfs;
  c.input = F.input;
  c.render = F.render;
  c.audio = F.audio;
  char err[256] = "";
  F.script = zb_script_create(&c, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(F.script, err);
}

static void start(void) {
  configure();
  start_with(&F.cfg);
}

static void run(void) {
  zb_script_run_packages(F.script);
  zb_script_init_hooks(F.script);
}

static void settle(void) {
  for (int i = 0; i < 3; ++i) {
    zb_vfs_poll(F.vfs);
  }
}

static lua_State *LS(void) { return F.script->L; }

static const char *gstr(const char *name) {
  lua_getglobal(LS(), name);
  const char *s = lua_tostring(LS(), -1);
  lua_pop(LS(), 1); /* strings in globals stay alive */
  return s ? s : "(nil)";
}

static double gnum(const char *name) {
  lua_getglobal(LS(), name);
  double v = lua_tonumber(LS(), -1);
  lua_pop(LS(), 1);
  return v;
}

static bool gbool(const char *name) {
  lua_getglobal(LS(), name);
  bool v = lua_toboolean(LS(), -1);
  lua_pop(LS(), 1);
  return v;
}

/* Runs a chunk in the global environment (assertions of the test itself). */
static void exec(const char *code) {
  if (luaL_dostring(LS(), code) != LUA_OK) {
    TEST_FAIL_MESSAGE(lua_tostring(LS(), -1));
  }
}

static bool error_contains(const char *needle) {
  for (size_t i = 0; i < zb_script_error_count(F.script) && i < ZB_SCRIPT_MAX_ERRORS; ++i) {
    if (strstr(zb_script_error(F.script, i), needle)) {
      return true;
    }
  }
  return false;
}

/* ---- packages, require and hooks ------------------------------------------------- */

/* spec: lua-scripting: Entry scripts run in order */
/* spec: lua-scripting: Require a package module */
static void test_entry_scripts_and_require(void) {
  package("assets/base", "base", NULL, "_G.order = (_G.order or '') .. 'base;'\n");
  package("mods/zeta", "zeta", "requires = { base = '>=1.0' }",
          "_G.order = _G.order .. 'zeta;'\n"
          "local m = require('util.math')\n"
          "local again = require('util.math')\n"
          "_G.sum = m.add(2, 3)\n"
          "_G.same = (m == again)\n"
          "_G.flag = require('util.flag')\n"
          "_G.pkg = package_id\n");
  put("mods/zeta/scripts/util/math.lua", "local M = {}\nfunction M.add(a, b) return a + b end\nreturn M\n");
  put("mods/zeta/scripts/util/flag.lua", "_G.flag_loaded = true\n");
  package("mods/alpha", "alpha", "requires = { zeta = '>=1.0' }",
          "_G.order = _G.order .. 'alpha;'\n"
          "local ok, err = pcall(require, 'nope.module')\n"
          "_G.missing = err\n"
          "local ok2, err2 = pcall(require, 'broken')\n"
          "_G.broken = err2\n");
  put("mods/alpha/scripts/broken.lua", "return {");
  package("mods/data", "data", NULL, NULL); /* data-only package */
  start();
  TEST_ASSERT_EQUAL_size_t(0, zb_script_run_packages(F.script));
  TEST_ASSERT_EQUAL_STRING("base;zeta;alpha;", gstr("order"));
  TEST_ASSERT_EQUAL_DOUBLE(5.0, gnum("sum"));
  TEST_ASSERT_TRUE(gbool("same"));
  TEST_ASSERT_TRUE(gbool("flag"));
  TEST_ASSERT_TRUE(gbool("flag_loaded"));
  TEST_ASSERT_EQUAL_STRING("zeta", gstr("pkg"));
  TEST_ASSERT_EQUAL_STRING("(nil)", gstr("package_id")); /* it lives in the package environment */
  TEST_ASSERT_NOT_NULL(strstr(gstr("missing"), "module 'nope.module' not found"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("broken"), "broken.lua"));
}

static void test_failing_entry_script(void) {
  package("assets/base", "base", NULL, "error('boom at load')\n");
  package("mods/ok", "ok", NULL, "_G.ok_ran = true\n");
  package("mods/syntax", "syntax", NULL, "this is not lua");
  start();
  TEST_ASSERT_EQUAL_size_t(2, zb_script_run_packages(F.script));
  TEST_ASSERT_TRUE(gbool("ok_ran"));
  TEST_ASSERT_TRUE(error_contains("[base]"));
  TEST_ASSERT_TRUE(error_contains("boom at load"));
  TEST_ASSERT_TRUE(error_contains("[syntax]"));
}

/* spec: lua-scripting: Tick hook runs per tick */
static void test_hooks(void) {
  package("assets/base", "base", NULL,
          "local n = 0\n"
          "engine.on('init', function() _G.inited = true end)\n"
          "engine.on('tick', function(dt) n = n + 1; _G.ticks = n; _G.dt = dt end)\n"
          "engine.on('frame', function(alpha) _G.alpha = alpha end)\n"
          "engine.on('event', function(name, e) _G.ev = name .. ':' .. e.a .. ':' .. e.b .. ':' .. "
          "tostring(e.width) .. ':' .. tostring(e.value) end)\n"
          "engine.on('shutdown', function() _G.down = (_G.down or '') .. 'base;' end)\n"
          "_G.bad_event = select(2, pcall(engine.on, 'teleport', function() end))\n"
          "_G.bad_fn = select(2, pcall(engine.on, 'tick', 5))\n");
  package("mods/b", "b", "requires = { base = '>=1.0' }",
          "engine.on('shutdown', function() _G.down = (_G.down or '') .. 'b;' end)\n");
  start();
  run();
  TEST_ASSERT_TRUE(gbool("inited"));
  for (int i = 0; i < 3; ++i) {
    zb_script_tick(F.script, 1.0 / 60.0);
  }
  TEST_ASSERT_EQUAL_DOUBLE(3.0, gnum("ticks"));
  TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0 / 60.0, gnum("dt"));
  zb_script_frame(F.script, 0.25);
  TEST_ASSERT_EQUAL_DOUBLE(0.25, gnum("alpha"));
  zb_script_event(F.script, "resize", 800, 600, NULL);
  TEST_ASSERT_EQUAL_STRING("resize:800:600:800:nil", gstr("ev"));
  zb_script_event(F.script, "input_mode", 0, 0, "touch");
  TEST_ASSERT_EQUAL_STRING("input_mode:0:0:nil:touch", gstr("ev"));
  zb_script_shutdown(F.script);
  TEST_ASSERT_EQUAL_STRING("b;base;", gstr("down"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("bad_event"), "unknown event"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("bad_fn"), "function expected"));
}

/* spec: lua-scripting: Failing tick hook */
static void test_failing_tick_hook(void) {
  package("assets/base", "base", NULL, "engine.on('tick', function() _G.base_ticks = (_G.base_ticks or 0) + 1 end)\n");
  package("mods/flaky", "flaky", NULL,
          "local n = 0\n"
          "engine.on('tick', function() n = n + 1; _G.flaky_ticks = n; if n == 2 then error('flaky broke') end end)\n");
  start();
  run();
  for (int i = 0; i < 5; ++i) {
    zb_script_tick(F.script, 1.0 / 60.0);
  }
  TEST_ASSERT_EQUAL_DOUBLE(5.0, gnum("base_ticks"));
  TEST_ASSERT_EQUAL_DOUBLE(2.0, gnum("flaky_ticks")); /* disabled after the error */
  TEST_ASSERT_EQUAL_size_t(1, zb_script_error_count(F.script));
  TEST_ASSERT_NOT_NULL(strstr(zb_script_error(F.script, 0), "[flaky]"));
  TEST_ASSERT_NOT_NULL(strstr(zb_script_error(F.script, 0), "flaky broke"));
  TEST_ASSERT_TRUE(g_warnings >= 1);
}

/* spec: lua-scripting: Infinite loop in a hook */
static void test_instruction_budget(void) {
  package("assets/base", "base", NULL,
          "engine.on('tick', function() _G.entered = (_G.entered or 0) + 1; while true do end end)\n"
          "engine.on('frame', function() local x = 0; for i = 1, 1000 do x = x + i end; _G.sum = x end)\n");
  configure();
  F.cfg.instruction_budget = 200000;
  start_with(&F.cfg);
  run();
  zb_script_tick(F.script, 1.0 / 60.0);
  zb_script_tick(F.script, 1.0 / 60.0);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, gnum("entered"));
  TEST_ASSERT_TRUE_MESSAGE(error_contains("instruction budget of 200000 exceeded"),
                           zb_script_error_count(F.script) ? zb_script_error(F.script, 0) : "no errors");
  zb_script_frame(F.script, 0.0);
  TEST_ASSERT_EQUAL_DOUBLE(500500.0, gnum("sum"));
}

/* spec: lua-scripting: Edit a script while running */
static void test_hot_reload(void) {
  package("assets/base", "base", NULL,
          "local state = { reloads = 0 }\n"
          "engine.preserve(state)\n"
          "engine.on('tick', function() _G.version = 1; _G.helper = require('helper').value end)\n");
  put("assets/base/scripts/helper.lua", "return { value = 'h1' }\n");
  zb_memfs_touch(F.fs, "assets/base/scripts/main.lua", 100);
  zb_memfs_touch(F.fs, "assets/base/scripts/helper.lua", 100);
  configure();
  F.cfg.dev = true;
  start_with(&F.cfg);
  run();
  zb_script_tick(F.script, 1.0 / 60.0);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, gnum("version"));
  TEST_ASSERT_EQUAL_STRING("h1", gstr("helper"));
  TEST_ASSERT_EQUAL_size_t(0, zb_script_poll_reload(F.script, 1000000000ull)); /* nothing changed */
  put("assets/base/scripts/main.lua",
      "engine.on('reload', function(old) old.reloads = old.reloads + 1; _G.reloads = old.reloads;"
      " engine.preserve(old) end)\n"
      "engine.on('tick', function() _G.version = 2; _G.helper = require('helper').value end)\n");
  zb_memfs_touch(F.fs, "assets/base/scripts/main.lua", 200);
  TEST_ASSERT_EQUAL_size_t(0, zb_script_poll_reload(F.script, 1100000000ull)); /* within 250 ms */
  TEST_ASSERT_EQUAL_size_t(1, zb_script_poll_reload(F.script, 1300000000ull));
  zb_script_tick(F.script, 1.0 / 60.0);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, gnum("version"));
  TEST_ASSERT_EQUAL_DOUBLE(1.0, gnum("reloads"));
  /* editing a required module reloads the package too */
  put("assets/base/scripts/helper.lua", "return { value = 'h2' }\n");
  zb_memfs_touch(F.fs, "assets/base/scripts/helper.lua", 300);
  TEST_ASSERT_EQUAL_size_t(1, zb_script_poll_reload(F.script, 2000000000ull));
  zb_script_tick(F.script, 1.0 / 60.0);
  TEST_ASSERT_EQUAL_STRING("h2", gstr("helper"));
  TEST_ASSERT_EQUAL_DOUBLE(2.0, gnum("reloads"));
  /* a reload with an error keeps the game running and reports it */
  put("assets/base/scripts/main.lua", "error('typo')\n");
  zb_memfs_touch(F.fs, "assets/base/scripts/main.lua", 400);
  TEST_ASSERT_EQUAL_size_t(1, zb_script_poll_reload(F.script, 3000000000ull));
  TEST_ASSERT_TRUE(error_contains("typo"));
}

static void test_reload_disabled_in_release(void) {
  package("assets/base", "base", NULL, "_G.x = 1\n");
  start();
  run();
  zb_memfs_touch(F.fs, "assets/base/scripts/main.lua", 999);
  TEST_ASSERT_EQUAL_size_t(0, zb_script_poll_reload(F.script, 5000000000ull));
}

/* ---- sandbox ------------------------------------------------------------------------ */

/* spec: lua-scripting: Forbidden call */
static void test_forbidden_calls(void) {
  package("assets/base", "base", NULL,
          "local function msg(f, ...) local ok, e = pcall(f, ...); return ok and 'ok' or e end\n"
          "_G.io_open = msg(io.open, '/etc/passwd')\n"
          "_G.os_execute = msg(os.execute, 'ls')\n"
          "_G.os_time = msg(os.time)\n"
          "_G.dofile = msg(dofile, 'x.lua')\n"
          "_G.loadfile = msg(loadfile, 'x.lua')\n"
          "_G.dump = msg(string.dump, function() end)\n"
          "_G.loadlib = msg(package.loadlib, 'x', 'y')\n"
          "print('hello', 42, nil, true)\n");
  start();
  run();
  TEST_ASSERT_NOT_NULL(strstr(gstr("io_open"), "io.open is unavailable in the Zauberer sandbox"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("os_execute"), "os.execute is unavailable"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("os_time"), "use engine.time"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("dofile"), "dofile is unavailable"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("loadfile"), "loadfile is unavailable"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("dump"), "string.dump is unavailable"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("loadlib"), "package.loadlib is unavailable"));
}

/* spec: lua-scripting: Binary chunk rejected */
static void test_binary_chunk_rejected(void) {
  package("assets/base", "base", NULL,
          "local f, err = load('\\27Lua\\84\\0')\n"
          "_G.binary = tostring(f) .. ' ' .. tostring(err)\n"
          "_G.with_env = load('return x * 2', 'chunk', 'b', { x = 21 })()\n"
          "local g, serr = load('return +')\n"
          "_G.syntax = serr\n"
          "_G.plain = load('return 7')()\n");
  start();
  run();
  TEST_ASSERT_NOT_NULL(strstr(gstr("binary"), "nil"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("binary"), "binary"));
  TEST_ASSERT_EQUAL_DOUBLE(42.0, gnum("with_env"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("syntax"), "unexpected symbol"));
  TEST_ASSERT_EQUAL_DOUBLE(7.0, gnum("plain"));
}

/* spec: lua-scripting: Repeatable randomness */
static void test_repeatable_randomness(void) {
  const char *code =
      "local t = {}\n"
      "for i = 1, 5 do t[#t + 1] = string.format('%.6f', math.random()) end\n"
      "t[#t + 1] = math.random(10)\n"
      "t[#t + 1] = math.random(5, 7)\n"
      "_G.seq = table.concat(t, ',')\n"
      "math.randomseed()\n"
      "_G.after_reset = math.random(1000000)\n"
      "math.randomseed(7)\n"
      "_G.seeded = math.random(1000000)\n"
      "_G.empty = select(2, pcall(math.random, 5, 1))\n";
  package("assets/base", "base", NULL, code);
  start();
  run();
  char first[256];
  snprintf(first, sizeof first, "%s", gstr("seq"));
  double reset = gnum("after_reset");
  double seeded = gnum("seeded");
  TEST_ASSERT_NOT_NULL(strstr(gstr("empty"), "interval is empty"));
  stop();
  start();
  run();
  TEST_ASSERT_EQUAL_STRING(first, gstr("seq"));
  TEST_ASSERT_EQUAL_DOUBLE(reset, gnum("after_reset"));
  TEST_ASSERT_EQUAL_DOUBLE(seeded, gnum("seeded"));
  stop();
  configure();
  F.cfg.seed = 43;
  start_with(&F.cfg);
  run();
  TEST_ASSERT_FALSE(strcmp(first, gstr("seq")) == 0);
}

/* ---- engine API ----------------------------------------------------------------------- */

/* spec: lua-scripting: Query platform */
static void test_engine_info(void) {
  package("assets/base", "base", NULL,
          "local i = engine.info()\n"
          "_G.info = i.platform .. '|' .. i.build .. '|' .. i.input_mode .. '|' .. i.drawable_size[1] .. 'x' .. "
          "i.drawable_size[2] .. '|' .. i.seed .. '|' .. i.version .. '|' .. i.api\n"
          "_G.ver = engine.version .. '/' .. engine.api_version\n");
  start();
  run();
  TEST_ASSERT_EQUAL_STRING("testos|development|touch|1280x720|99|0.1.0|1.0", gstr("info"));
  TEST_ASSERT_EQUAL_STRING("0.1.0/1.0", gstr("ver"));
}

static void test_engine_info_without_host(void) {
  package("assets/base", "base", NULL,
          "local i = engine.info()\n"
          "_G.info = i.platform .. '|' .. i.build .. '|' .. i.input_mode\n"
          "_G.t = engine.time() + engine.tick()\n"
          "engine.quit(); engine.pause(true); engine.set_fullscreen(true)\n"
          "_G.scale_err = select(2, pcall(engine.set_time_scale, 2))\n"
          "_G.hash = engine.hash_state()\n");
  configure();
  memset(&F.cfg.host, 0, sizeof F.cfg.host);
  F.cfg.test_api = false;
  start_with(&F.cfg);
  run();
  TEST_ASSERT_EQUAL_STRING("unknown|release|desktop", gstr("info"));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, gnum("t"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("scale_err"), "only available in development builds"));
  TEST_ASSERT_EQUAL_INT(16, (int)strlen(gstr("hash")));
}

static void test_engine_controls(void) {
  package("assets/base", "base", NULL, NULL);
  start();
  run();
  F.host.time = 2.5;
  F.host.ticks = 150;
  exec(
      "t = engine.time(); n = engine.tick()\n"
      "engine.pause(true); engine.set_time_scale(0.5); engine.set_fullscreen(true); engine.quit(3)\n"
      "engine.log('error', 'e'); engine.log('warn', 'w'); engine.log('debug', 'd'); engine.log('info', 'i')\n");
  TEST_ASSERT_EQUAL_DOUBLE(2.5, gnum("t"));
  TEST_ASSERT_EQUAL_DOUBLE(150.0, gnum("n"));
  TEST_ASSERT_TRUE(F.host.paused);
  TEST_ASSERT_EQUAL_DOUBLE(0.5, F.host.time_scale);
  TEST_ASSERT_TRUE(F.host.fullscreen);
  TEST_ASSERT_TRUE(F.host.quit_called);
  TEST_ASSERT_EQUAL_INT(3, F.host.quit_code);
  TEST_ASSERT_EQUAL_INT(1, g_errors);
  TEST_ASSERT_EQUAL_INT(1, g_warnings);
}

static void test_hash_state(void) {
  package("assets/base", "base", NULL, NULL);
  start();
  run();
  exec(
      "a = engine.hash_state({ x = 1, y = { 1, 2, 3 }, [true] = 'b', [2.5] = false })\n"
      "local t = {}; t[2.5] = false; t[true] = 'b'; t.y = { 1, 2, 3 }; t.x = 1\n"
      "b = engine.hash_state(t)\n"
      "c = engine.hash_state({ x = 1, y = { 1, 2, 4 }, [true] = 'b', [2.5] = false })\n"
      "d = engine.hash_state({ x = 1.0 })\n"
      "e = engine.hash_state({ x = 1 })\n"
      "local big = {}; for i = 1, 100 do big['k' .. i] = i end\n"
      "f = engine.hash_state(big)\n"
      "local deep = {}; local cur = deep; for i = 1, 30 do cur.next = {}; cur = cur.next end\n"
      "g = engine.hash_state(deep)\n"
      "h = engine.hash_state({ [print] = 1, fn = print })\n"
      "s1 = engine.hash_state(); math.random(); s2 = engine.hash_state()\n");
  TEST_ASSERT_EQUAL_STRING(gstr("a"), gstr("b"));
  TEST_ASSERT_FALSE(strcmp(gstr("a"), gstr("c")) == 0);
  TEST_ASSERT_FALSE(strcmp(gstr("d"), gstr("e")) == 0);   /* integer and float differ */
  TEST_ASSERT_FALSE(strcmp(gstr("s1"), gstr("s2")) == 0); /* random draws change the state */
  int before = (int)F.host.ticks;
  F.host.ticks = (uint64_t)before + 1;
  exec("s3 = engine.hash_state()");
  TEST_ASSERT_FALSE(strcmp(gstr("s2"), gstr("s3")) == 0);
}

static void test_packages_and_credits(void) {
  package("assets/base", "base", "name = 'Base'", NULL);
  put("assets/base/credits.lua", "return { { file = 'a.png', author = 'A', license = 'CC-BY-4.0' }, 'not a table' }");
  put("assets/base/data/third_party.lua", "return { { name = 'Lua', license = 'MIT' }, 5 }");
  package("mods/mod", "mod", "requires = { base = '>=1.0' }", NULL);
  put("mods/mod/credits.lua", "return { { file = 'b.png', author = 'B', license = 'CC0-1.0', size = 1.5 } }");
  package("mods/broken", "broken", "requires = { base = '>=1.0' }", NULL);
  put("mods/broken/credits.lua", "return { oops ");
  package("mods/future", "future", "requires = { base = '>=9.0' }", NULL);
  start();
  run();
  exec(
      "local p = engine.packages()\n"
      "pk = {}\n"
      "for _, e in ipairs(p) do pk[#pk + 1] = e.id .. ':' .. tostring(e.enabled) .. ':' .. "
      "tostring(e.load_order) end\n"
      "pk = table.concat(pk, ',')\n"
      "for _, e in ipairs(p) do if not e.enabled then why = e.error end end\n"
      "local c = engine.credits()\n"
      "cr = {}\n"
      "for _, e in ipairs(c) do cr[#cr + 1] = e.package .. ':' .. e.kind .. ':' .. (e.file or e.name) end\n"
      "cr = table.concat(cr, ',')\n"
      "size = c[2].size\n");
  TEST_ASSERT_EQUAL_STRING("base:true:1,broken:true:2,mod:true:3,future:false:nil", gstr("pk"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("why"), "requires 'base'"));
  TEST_ASSERT_EQUAL_STRING("base:asset:a.png,mod:asset:b.png,engine:code:Lua", gstr("cr"));
  TEST_ASSERT_EQUAL_DOUBLE(1.5, gnum("size"));
  TEST_ASSERT_TRUE(g_warnings >= 1); /* broken credits */
}

/* ---- vfs API ------------------------------------------------------------------------------ */

static void test_vfs_api(void) {
  package("assets/base", "base", NULL, NULL);
  put("assets/base/data/a.txt", "alpha");
  put("assets/base/data/sub/b.txt", "beta");
  start();
  run();
  exec(
      "e1 = vfs.exists('data/a.txt'); e2 = vfs.exists('data/zzz')\n"
      "r1 = vfs.read('data/a.txt'); r2, r2e = vfs.read('data/zzz')\n"
      "local l = vfs.list('data'); names = {}\n"
      "for _, e in ipairs(l) do names[#names + 1] = e.name .. (e.dir and '/' or '') end\n"
      "names = table.concat(names, ',')\n"
      "l2, l2e = vfs.list('nowhere')\n"
      "root = #vfs.list()\n"
      "p = vfs.load('data/sub/b.txt', function(d, err) got = d; goterr = err end)\n"
      "vfs.load('data/none', function(d, err) miss = err end)\n"
      "vfs.load('data/a.txt', function() error('callback failure') end)\n");
  TEST_ASSERT_TRUE(gbool("e1"));
  TEST_ASSERT_FALSE(gbool("e2"));
  TEST_ASSERT_EQUAL_STRING("alpha", gstr("r1"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("r2e"), "data/zzz"));
  TEST_ASSERT_EQUAL_STRING("a.txt,sub/", gstr("names"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("l2e"), "nowhere"));
  TEST_ASSERT_TRUE(gnum("root") >= 2);
  TEST_ASSERT_EQUAL_STRING("pending", gstr("p"));
  TEST_ASSERT_EQUAL_STRING("(nil)", gstr("got"));
  settle();
  TEST_ASSERT_EQUAL_STRING("beta", gstr("got"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("miss"), "data/none"));
  TEST_ASSERT_TRUE(error_contains("callback failure"));
}

/* ---- input API ----------------------------------------------------------------------------- */

static void test_input_api(void) {
  package("assets/base", "base", NULL, NULL);
  start();
  run();
  exec(
      "id = input.define('jump')\n"
      "input.define('move', 'axis2'); input.define('zoom', 'axis1')\n"
      "bad_type = select(2, pcall(input.define, 'x', 'trackball'))\n"
      "conflict = select(2, pcall(input.define, 'jump', 'axis1'))\n"
      "input.bind('jump', 'key:space'); input.bind('jump', 'gamepad:a')\n"
      "bad_bind = select(2, pcall(input.bind, 'jump', 'key:nope'))\n"
      "unknown = select(2, pcall(input.bind, 'fly', 'key:f'))\n"
      "b = table.concat(input.bindings('jump'), ',')\n"
      "input.clear('jump'); nb = #input.bindings('jump')\n"
      "mode = input.mode()\n"
      "input.add_stick('move', 0.2, 0.7, 0.15); input.add_button('jump', 0.8, 0.7, 0.1, 0.1)\n"
      "ok_saved = input.set_user_binding('jump', { 'key:j' })\n"
      "too_many = select(2, pcall(input.set_user_binding, 'jump', { 'key:a','key:b','key:c','key:d','key:e',"
      "'key:f','key:g','key:h','key:i' }))\n"
      "invalid_user = select(2, pcall(input.set_user_binding, 'jump', { 'key:nope' }))\n"
      "input.inject('jump'); input.inject('move', 0.5, -0.25, 2)\n");
  TEST_ASSERT_NOT_NULL(strstr(gstr("bad_type"), "button, axis1 or axis2 expected"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("conflict"), "cannot define action 'jump'"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("bad_bind"), "invalid binding 'key:nope'"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("unknown"), "unknown action 'fly'"));
  TEST_ASSERT_EQUAL_STRING("key:space,gamepad:a", gstr("b"));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, gnum("nb"));
  TEST_ASSERT_EQUAL_STRING("desktop", gstr("mode"));
  TEST_ASSERT_TRUE(gbool("ok_saved"));
  TEST_ASSERT_EQUAL_INT(1, F.host.saves);
  TEST_ASSERT_NOT_NULL(strstr(gstr("too_many"), "too many bindings"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("invalid_user"), "invalid binding for action 'jump'"));
  zb_input_sample(F.input, 0);
  exec(
      "local j = input.state('jump'); local m = input.state('move')\n"
      "st = tostring(j.pressed) .. tostring(j.held) .. tostring(j.released) .. ':' .. m.x .. ':' .. m.y\n");
  TEST_ASSERT_EQUAL_STRING("truetruefalse:0.5:-0.25", gstr("st"));
  zb_input_set_mode(F.input, ZB_INPUT_TOUCH);
  exec(
      "mode = input.mode()\n"
      "stick_err = select(2, pcall(input.add_stick, 'zoom', 0, 0, 0))\n"
      "button_err = select(2, pcall(input.add_button, '', 0, 0, 1, 1))\n");
  TEST_ASSERT_EQUAL_STRING("touch", gstr("mode"));
  F.host.save_result = ZB_ERR_IO;
  exec("saved = input.set_user_binding('jump', { 'key:k' })");
  TEST_ASSERT_FALSE(gbool("saved"));
}

/* ---- render API ---------------------------------------------------------------------------- */

static void put_real(const char *memfs_path, const char *repo_path) {
  char full[1024];
  snprintf(full, sizeof full, "%s/%s", zb_test_source_dir(), repo_path);
  size_t len = 0;
  char *data = zb_test_read_file(full, &len);
  TEST_ASSERT_NOT_NULL_MESSAGE(data, full);
  zb_memfs_put(F.fs, memfs_path, data, len);
  free(data);
}

static void test_render_api(void) {
  package("assets/base", "base", NULL, NULL);
  put_real("assets/base/models/carpet.glb", "assets/base/models/carpet.glb");
  put_real("assets/base/fonts/pixel.lua", "assets/base/fonts/pixel.lua");
  put_real("assets/base/fonts/pixel.png", "assets/base/fonts/pixel.png");
  put_real("assets/base/sprites/mote.lua", "assets/base/sprites/mote.lua");
  put_real("assets/base/sprites/mote.png", "assets/base/sprites/mote.png");
  start();
  run();
  exec(
      "carpet = render.model('models/carpet.glb'); font = render.font('fonts/pixel.lua')\n"
      "mote = render.sprite('sprites/mote.lua'); tex = render.texture('sprites/mote.png')\n"
      "s0 = render.state(carpet)\n");
  TEST_ASSERT_EQUAL_STRING("pending", gstr("s0"));
  settle();
  F.host.time = 1.0;
  zb_render_begin(F.render, 1280, 720);
  exec(
      "s1 = render.state(carpet); s2 = render.state(9999)\n"
      "render.camera({ eye = { 0, 1, 5 }, target = { 0, 0, 0 }, fov = 50 })\n"
      "render.camera({ eye = 3, up = { 0, 1 } })\n"
      "render.camera({ eye = { 0, 1, 5 } })\n"
      "render.light({ direction = { 0, -1, 0 }, color = { 1, 0.9, 0.8 }, ambient = 0.2 })\n"
      "render.clear_color(0.1, 0.2, 0.3)\n"
      "ih = render.internal_height(); ih2 = render.internal_height(240)\n"
      "render.internal_height(360)\n"
      "render.draw_model(carpet)\n"
      "render.draw_model(carpet, { position = { 1, 0, 0 }, rotation = { 0, 90, 0 }, scale = 2 })\n"
      "render.draw_sprite(mote, { 0, 1, 0 })\n"
      "render.draw_sprite(mote, { 0, 1, 0 }, 2, 0.5)\n"
      "render.draw_text(font, 'Hi', 4, 4)\n"
      "render.draw_text(font, 'Ho', 4, 14, 2, { 1, 0, 0 })\n"
      "frame0 = render.sprite_frame(mote, 0); frame1 = render.sprite_frame(mote, 10)\n"
      "glyphs = #render.text_glyphs(font, 'abc')\n");
  zb_render_end(F.render);
  exec(
      "local st = render.stats()\n"
      "stats = st.model_draws .. ',' .. st.sprite_draws .. ',' .. st.glyphs .. ',' .. st.views .. ',' .. "
      "st.internal_width .. 'x' .. st.internal_height .. ',' .. st.scale .. ',' .. st.backend .. ',' .. st.frames "
      ".. ',' .. st.drawable_width .. ',' .. st.out_width .. ',' .. st.culled .. ',' .. st.out_x .. ',' .. "
      "st.out_y .. ',' .. st.out_height .. ',' .. st.drawable_height\n");
  TEST_ASSERT_EQUAL_STRING("ready", gstr("s1"));
  TEST_ASSERT_EQUAL_STRING("failed", gstr("s2"));
  TEST_ASSERT_EQUAL_DOUBLE(360.0, gnum("ih"));
  TEST_ASSERT_EQUAL_DOUBLE(240.0, gnum("ih2"));
  TEST_ASSERT_EQUAL_DOUBLE(3.0, gnum("glyphs"));
  TEST_ASSERT_TRUE(gnum("frame1") > gnum("frame0"));
  TEST_ASSERT_EQUAL_STRING("2,2,4,1,640x360,2,fake,1,1280,1280,0,0,0,720,720", gstr("stats"));
  TEST_ASSERT_EQUAL_INT(1, F.backend.frames);
}

/* ---- audio API ------------------------------------------------------------------------------ */

static void test_audio_api(void) {
  package("assets/base", "base", NULL, NULL);
  put_real("assets/base/sounds/chime.wav", "assets/base/sounds/chime.wav");
  put_real("assets/base/music/loop.ogg", "assets/base/music/loop.ogg");
  start();
  run();
  zb_audio_set_device(F.audio, true);
  exec(
      "chime = audio.play('sounds/chime.wav')\n"
      "music = audio.play('music/loop.ogg', { group = 'music', loop = true, volume = 0.5, pitch = 1.0 })\n"
      "fx = audio.play('sounds/chime.wav', { group = 7 })\n");
  settle();
  float buf[256 * 2];
  zb_audio_mix(F.audio, buf, 256);
  exec(
      "st = audio.state(chime) .. ',' .. audio.state(music)\n"
      "voices = audio.voices()\n"
      "bytes = audio.decoder_bytes(music)\n"
      "audio.set_volume(music, 0.25)\n"
      "mv = audio.group_volume('music', 0.5); ev = audio.group_volume('effects')\n"
      "audio.group_volume('master', 0.8)\n"
      "locked = audio.locked()\n"
      "audio.stop(chime); after = audio.state(chime)\n");
  TEST_ASSERT_EQUAL_STRING("playing,playing", gstr("st"));
  TEST_ASSERT_EQUAL_DOUBLE(3.0, gnum("voices"));
  TEST_ASSERT_EQUAL_DOUBLE((double)ZB_AUDIO_STREAM_BUFFER, gnum("bytes"));
  TEST_ASSERT_EQUAL_DOUBLE(0.5, gnum("mv"));
  TEST_ASSERT_EQUAL_DOUBLE(1.0, gnum("ev"));
  TEST_ASSERT_FALSE(gbool("locked"));
  TEST_ASSERT_EQUAL_STRING("stopped", gstr("after"));
}

/* ---- save API -------------------------------------------------------------------------------- */

static void test_save_api(void) {
  package("assets/base", "base", NULL, NULL);
  start();
  run();
  exec(
      "save.set('name', 'Lei \"the\" heir'); save.set('level', 3); save.set('ratio', 0.5)\n"
      "save.set('flags', { a = true, list = { 1, 2, 3 }, nested = { deep = 'x' } })\n"
      "save.set('gone', nil)\n"
      "bad = select(2, pcall(save.set, 'f', print))\n"
      "local deep = {}; local cur = deep; for i = 1, 12 do cur.n = {}; cur = cur.n end\n"
      "save.set('deep', deep); save.set('odd', { [true] = 1, f = print })\n"
      "ok = save.flush()\n");
  TEST_ASSERT_TRUE(gbool("ok"));
  TEST_ASSERT_TRUE(g_user_data_changes > 0);
  TEST_ASSERT_NOT_NULL(strstr(gstr("bad"), "nil, boolean, number, string or table expected"));
  stop();
  start();
  run();
  exec(
      "n = save.get('name'); l = save.get('level'); r = save.get('ratio')\n"
      "f = save.get('flags'); fl = tostring(f.a) .. #f.list .. f.nested.deep\n"
      "missing = save.get('nothing')\n");
  TEST_ASSERT_EQUAL_STRING("Lei \"the\" heir", gstr("n"));
  TEST_ASSERT_EQUAL_DOUBLE(3.0, gnum("l"));
  TEST_ASSERT_EQUAL_DOUBLE(0.5, gnum("r"));
  TEST_ASSERT_EQUAL_STRING("true3x", gstr("fl"));
  /* a corrupt save starts empty with a warning */
  stop();
  put("user/save.lua", "return {{{");
  start();
  run();
  exec("c = save.get('name')");
  TEST_ASSERT_EQUAL_STRING("(nil)", gstr("c"));
  TEST_ASSERT_TRUE(g_warnings >= 1);
  stop();
  /* without a file system, flushing fails */
  configure();
  F.cfg.fs = NULL;
  F.cfg.user_dir = NULL;
  start_with(&F.cfg);
  exec("save.set('a', 1); nofs = save.flush()");
  TEST_ASSERT_FALSE(gbool("nofs"));
}

/* ---- Lua tests and scenarios -------------------------------------------------------------- */

/* spec: test-automation: Run Lua tests */
static void test_lua_test_runner(void) {
  package("assets/base", "base", NULL, NULL);
  put("assets/base/scripts/lib.lua", "return { twice = function(x) return 2 * x end }\n");
  put("tests/lua/test_math.lua",
      "local lib = require('lib')\n"
      "test.case('passes', function()\n"
      "  test.expect(true)\n"
      "  test.expect_eq(lib.twice(2), 4)\n"
      "  test.expect_near(0.1 + 0.2, 0.3, 1e-9)\n"
      "  test.expect_error(function() error('boom') end, 'boom')\n"
      "end)\n"
      "test.case('fails <xml & \"escape\">', function() test.expect_eq(1, 2, 'numbers') end)\n");
  put("tests/lua/strings_test.lua",
      "test.case('string mismatch', function() test.expect_eq('a', 'b') end)\n"
      "test.case('near', function() test.expect_near(1, 2, 0.5, 'far') end)\n"
      "test.case('no error', function() test.expect_error(function() end) end)\n"
      "test.case('wrong error', function() test.expect_error(function() error('x') end, 'y') end)\n"
      "test.case('plain', function() test.expect(false, 'nope') end)\n"
      "test.case('table', function() test.expect_eq({}, 1) end)\n");
  put("tests/lua/helper.lua", "error('helpers are not test files')\n");
  put("tests/lua/broken_test.lua", "test.case('x', function() end\n");
  put("tests/lua/test_raises.lua", "error('raised while loading')\n");
  start();
  run();
  char junit[512];
  snprintf(junit, sizeof junit, "%s/lua.xml", zb_test_tmp_dir());
  int failures = zb_script_run_tests(F.script, "tests/lua", junit);
  TEST_ASSERT_EQUAL_INT(1 + 6 + 2, failures);
  size_t len = 0;
  char *xml = zb_test_read_file(junit, &len);
  TEST_ASSERT_NOT_NULL(xml);
  TEST_ASSERT_NOT_NULL(strstr(xml, "tests=\"8\""));
  TEST_ASSERT_NOT_NULL(strstr(xml, "name=\"passes\"></testcase>"));
  TEST_ASSERT_NOT_NULL(strstr(xml, "fails &lt;xml &amp; &quot;escape&quot;&gt;"));
  TEST_ASSERT_NOT_NULL(strstr(xml, "numbers: expected 2, got 1"));
  free(xml);
  TEST_ASSERT_EQUAL_INT(1, zb_script_run_tests(F.script, "tests/lua/test_math.lua", NULL));
  TEST_ASSERT_EQUAL_INT(-1, zb_script_run_tests(F.script, "tests/missing", NULL));
  TEST_ASSERT_EQUAL_INT(1, zb_script_run_tests(F.script, "tests/lua/test_raises.lua", "/nonexistent/dir/x.xml"));
}

static void test_require_without_packages(void) {
  put("tests/lua/test_req.lua", "test.case('req', function() require('anything') end)\n");
  put("assets/base/package.lua", "return { id = 'base', version = '1.0.0', api = '9.0', license = 'MIT' }");
  start();
  run();
  TEST_ASSERT_EQUAL_INT(1, zb_script_run_tests(F.script, "tests/lua/test_req.lua", NULL));
  TEST_ASSERT_TRUE(error_contains("no packages are enabled"));
}

/* spec: test-automation: Passing scenario */
/* spec: test-automation: Failing scenario */
static void test_scenarios(void) {
  package("assets/base", "base", NULL, "engine.on('tick', function() _G.n = (_G.n or 0) + 1 end)\n");
  put("tests/scenarios/pass.lua",
      "scenario { name = 'counts ticks' }\n"
      "step(3)\n"
      "expect(n == 3, 'three ticks')\n"
      "expect_eq(n, 3)\n"
      "expect_near(n, 3.0, 0.1)\n"
      "expect_error(function() error('e') end)\n"
      "step()\n"
      "step(0)\n");
  put("tests/scenarios/fail.lua",
      "scenario { name = 'fails twice' }\n"
      "step(1)\n"
      "expect(false, 'first')\n"
      "expect_eq(1, 2, 'second')\n"
      "step(1)\n"
      "_G.after_failures = true\n"
      "step(-1)\n");
  put("tests/scenarios/syntax.lua", "scenario {\n");
  start();
  run();
  uint64_t ticks;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_script_scenario_start(F.script, "tests/scenarios/pass.lua"));
  int resumes = 0;
  while (zb_script_scenario_resume(F.script, &ticks)) {
    for (uint64_t i = 0; i < ticks; ++i) {
      F.host.ticks++;
      zb_script_tick(F.script, 1.0 / 60.0);
    }
    resumes++;
  }
  TEST_ASSERT_EQUAL_INT(3, resumes);
  TEST_ASSERT_EQUAL_STRING("counts ticks", zb_script_scenario_name(F.script));
  TEST_ASSERT_EQUAL_INT(0, zb_script_scenario_failures(F.script));
  TEST_ASSERT_FALSE(zb_script_scenario_resume(F.script, &ticks)); /* finished */
  stop();
  start();
  run();
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_script_scenario_start(F.script, "tests/scenarios/fail.lua"));
  while (zb_script_scenario_resume(F.script, &ticks)) {
    F.host.ticks += ticks;
  }
  TEST_ASSERT_TRUE(gbool("after_failures"));                       /* failed expectations do not stop the scenario */
  TEST_ASSERT_EQUAL_INT(3, zb_script_scenario_failures(F.script)); /* two expectations and step(-1) */
  TEST_ASSERT_NOT_NULL(strstr(g_last_error, "must not be negative"));
  stop();
  start();
  run();
  TEST_ASSERT_EQUAL_INT(ZB_ERR_PARSE, zb_script_scenario_start(F.script, "tests/scenarios/syntax.lua"));
  TEST_ASSERT_FALSE(zb_script_scenario_resume(F.script, &ticks));
  TEST_ASSERT_EQUAL_INT(1, zb_script_scenario_failures(F.script));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, zb_script_scenario_start(F.script, "tests/scenarios/none.lua"));
}

static void test_capture_and_golden(void) {
  package("assets/base", "base", NULL, NULL);
  put("tests/scenarios/golden.lua",
      "scenario { name = 'golden' }\n"
      "_G.no_capture = select(2, pcall(render.pixel, 0, 0))\n"
      "local c = render.capture()\n"
      "_G.size = c.width .. 'x' .. c.height\n"
      "local w = render.capture('window')\n"
      "_G.wsize = w.width .. 'x' .. w.height\n"
      "local r, g, b, a = render.pixel(3, 1)\n"
      "_G.px = r .. ',' .. g .. ',' .. b .. ',' .. a\n"
      "_G.outside = select(2, pcall(render.pixel, 8, 0))\n"
      "_G.ok1, _G.info1 = render.match_golden('frame')\n"
      "step(1)\n"
      "_G.ok2, _G.info2 = render.match_golden('frame')\n"
      "step(1)\n"
      "_G.ok3, _G.info3 = render.match_golden('frame', 'internal')\n"
      "_G.ok4, _G.info4 = render.match_golden('corrupt')\n");
  put("golden/corrupt.png", "not a png");
  F.backend.shade = 10;
  start();
  run();
  zb_script_set_golden_dirs(F.script, "golden", "out");
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_script_scenario_start(F.script, "tests/scenarios/golden.lua"));
  uint64_t ticks;
  TEST_ASSERT_TRUE(zb_script_scenario_resume(F.script, &ticks));
  TEST_ASSERT_NOT_NULL(strstr(gstr("no_capture"), "no capture yet"));
  TEST_ASSERT_EQUAL_STRING("4x2", gstr("size"));
  TEST_ASSERT_EQUAL_STRING("8x2", gstr("wsize"));
  TEST_ASSERT_EQUAL_STRING("10,11,7,255", gstr("px"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("outside"), "pixel outside the capture"));
  TEST_ASSERT_FALSE(gbool("ok1"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("info1"), "golden image missing: golden/frame.png"));
  zb_fs_stat st;
  const zb_fs_api *mfs = zb_memfs_api(F.fs);
  TEST_ASSERT_EQUAL_INT(ZB_OK, mfs->stat(mfs->ctx, "out/frame.png", &st));
  zb_test_setenv("ZB_UPDATE_GOLDEN", "1");
  TEST_ASSERT_TRUE(zb_script_scenario_resume(F.script, &ticks));
  zb_test_setenv("ZB_UPDATE_GOLDEN", NULL);
  TEST_ASSERT_TRUE(gbool("ok2"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("info2"), "golden image created"));
  F.backend.shade = 200; /* the frame changes */
  TEST_ASSERT_FALSE(zb_script_scenario_resume(F.script, &ticks));
  TEST_ASSERT_FALSE(gbool("ok3"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("info3"), "100.000% pixels differ"));
  TEST_ASSERT_FALSE(gbool("ok4"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("info4"), "cannot be decoded"));
  TEST_ASSERT_EQUAL_INT(0, zb_script_scenario_failures(F.script));
}

static void test_capture_unsupported(void) {
  package("assets/base", "base", NULL, NULL);
  configure();
  F.vfs = zb_vfs_create(zb_test_alloc(), zb_memfs_api(F.fs), NULL);
  zb_vfs_add_root(F.vfs, "assets");
  zb_vfs_mount(F.vfs, 1, 0);
  F.render = zb_render_create(zb_test_alloc(), F.vfs, zb_render_null_backend(zb_test_alloc()));
  zb_script_config c = F.cfg;
  c.vfs = F.vfs;
  c.render = F.render;
  char err[64];
  F.script = zb_script_create(&c, err, sizeof err);
  exec("c, cerr = render.capture(); ok, info = render.match_golden('x')");
  TEST_ASSERT_EQUAL_STRING("unsupported", gstr("cerr"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("info"), "capture failed"));
}

/* ---- coverage, errors, allocation ------------------------------------------------------- */

static void test_coverage(void) {
  package("assets/base", "base", NULL, "local a = 1\nlocal b = 2\n\nif a > b then\n  print('never')\nend\n");
  configure();
  F.cfg.coverage_path = "cov.stats";
  start_with(&F.cfg);
  run();
  stop();
  char *data;
  size_t len;
  const zb_fs_api *fs = zb_memfs_api(F.fs);
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->read(fs->ctx, "cov.stats", zb_test_alloc(), &data, &len));
  TEST_ASSERT_NOT_NULL_MESSAGE(strstr(data, ":assets/base/scripts/main.lua\n1 1 0 1 "), data);
  zb_free(zb_test_alloc(), data);
  start_with(&F.cfg); /* a second run merges its counts */
  run();
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_script_write_coverage(F.script));
  stop();
  TEST_ASSERT_EQUAL_INT(ZB_OK, fs->read(fs->ctx, "cov.stats", zb_test_alloc(), &data, &len));
  TEST_ASSERT_NOT_NULL_MESSAGE(strstr(data, ":assets/base/scripts/main.lua\n2 2 0 2 "), data);
  zb_free(zb_test_alloc(), data);
  /* malformed stats are ignored */
  put("cov.stats", "garbage");
  start_with(&F.cfg);
  run();
  stop();
  put("cov.stats", "3:x.lua");
  start_with(&F.cfg);
  stop();
  put("cov.stats", "3:x.lua\n1 two");
  start_with(&F.cfg);
  stop();
  /* without coverage, writing is a no-op */
  configure();
  start_with(&F.cfg);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_script_write_coverage(F.script));
}

static void test_error_ring(void) {
  package("assets/base", "base", NULL, NULL);
  start();
  for (int i = 0; i < 20; ++i) {
    char msg[32];
    snprintf(msg, sizeof msg, "error %d\nsecond line", i);
    zb_script_report(F.script, i == 0 ? NULL : "pkg", msg);
  }
  TEST_ASSERT_EQUAL_size_t(20, zb_script_error_count(F.script));
  TEST_ASSERT_EQUAL_STRING("[pkg] error 4", zb_script_error(F.script, 0));
  TEST_ASSERT_EQUAL_STRING("[pkg] error 19", zb_script_error(F.script, 15));
  TEST_ASSERT_NULL(zb_script_error(F.script, 16));
  luaL_loadstring(LS(), "error(setmetatable({}, { __tostring = function() return 'custom object' end }))");
  TEST_ASSERT_FALSE(zb_script_pcall(F.script, 0, 0, "x", false));
  TEST_ASSERT_TRUE(error_contains("custom object"));
}

static void test_create_out_of_memory(void) {
  package("assets/base", "base", NULL, "_G.x = 1\n");
  configure();
  F.vfs = zb_vfs_create(zb_test_alloc(), zb_memfs_api(F.fs), NULL);
  zb_vfs_add_root(F.vfs, "assets");
  zb_vfs_mount(F.vfs, 1, 0);
  for (long budget = 0; budget < 4; ++budget) {
    zb_failing_allocator fa;
    zb_script_config c = F.cfg;
    c.a = zb_failing_init(&fa, zb_test_alloc(), budget);
    c.vfs = F.vfs;
    char err[64] = "";
    zb_script *s = zb_script_create(&c, err, sizeof err);
    TEST_ASSERT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("out of memory", err);
  }
  zb_script_destroy(NULL);
}

/* ---- net --------------------------------------------------------------------------- */

/* Encodes the global `v`, decodes it into the global `back`. */
static bool codec_round_trip(void) {
  zb_array buf;
  zb_array_init(&buf, zb_test_alloc(), 1);
  lua_getglobal(LS(), "v");
  const char *err = zb_script_net_encode(LS(), -1, &buf);
  lua_pop(LS(), 1);
  bool ok = !err && zb_script_net_decode(LS(), buf.data, buf.len);
  if (ok) {
    lua_setglobal(LS(), "back");
  }
  zb_array_free(&buf);
  return ok;
}

static bool decodes(const void *data, size_t len) {
  int top = lua_gettop(LS());
  bool ok = zb_script_net_decode(LS(), data, len);
  lua_settop(LS(), top);
  return ok;
}

static const char *encode_error(const char *expr) {
  char code[256];
  snprintf(code, sizeof code, "v = %s", expr);
  exec(code);
  zb_array buf;
  zb_array_init(&buf, zb_test_alloc(), 1);
  lua_getglobal(LS(), "v");
  const char *err = zb_script_net_encode(LS(), -1, &buf);
  lua_pop(LS(), 1);
  zb_array_free(&buf);
  return err ? err : "";
}

/* spec: netcode: Values on channels */
static void test_net_codec(void) {
  package("assets/base", "base", NULL, "");
  start();
  exec(
      "v = { 1, 2.5, -7, 'text', true, false, nested = { deep = { 'x' }, [3.25] = 'f', [true] = 1 },"
      " big = math.maxinteger, small = math.mininteger, empty = {}, nan = 0/0, inf = 1/0 }");
  TEST_ASSERT_TRUE(codec_round_trip());
  exec(
      "ok = back[1] == 1 and math.type(back[1]) == 'integer' and back[2] == 2.5 and back[3] == -7"
      " and back[4] == 'text' and back[5] == true and back[6] == false and back.nested.deep[1] == 'x'"
      " and back.nested[3.25] == 'f' and back.nested[true] == 1 and back.big == math.maxinteger"
      " and back.small == math.mininteger and next(back.empty) == nil and back.nan ~= back.nan"
      " and back.inf == 1/0");
  TEST_ASSERT_TRUE(gbool("ok"));
  exec("v = nil");
  TEST_ASSERT_TRUE(codec_round_trip());
  exec("v = string.rep('z', 1000)");
  TEST_ASSERT_TRUE(codec_round_trip());
  TEST_ASSERT_EQUAL_STRING("only nil, booleans, numbers, strings and tables can be sent", encode_error("print"));
  TEST_ASSERT_EQUAL_STRING("table keys must be strings, numbers or booleans", encode_error("{ [{}] = 1 }"));
  TEST_ASSERT_EQUAL_STRING("tables nested too deeply",
                           encode_error("(function() local t = {} for _ = 1, 20 do t = { t } end return t end)()"));
  TEST_ASSERT_EQUAL_STRING("message too large", encode_error("string.rep('x', 300000)"));
  TEST_ASSERT_EQUAL_STRING("message too large", encode_error("{ string.rep('x', 262140) }"));
  TEST_ASSERT_EQUAL_STRING("message too large", encode_error("{ string.rep('x', 262114), {} }"));
  /* Malformed input never decodes. */
  static const uint8_t empty[1] = {0};
  static const uint8_t bad_tag[] = {99};
  static const uint8_t short_int[] = {3, 1, 2};
  static const uint8_t short_float[] = {4, 1};
  static const uint8_t short_string[] = {5, 9, 0, 0, 0, 'a'};
  static const uint8_t short_len[] = {5, 1};
  static const uint8_t huge_table[] = {6, 255, 255, 255, 255};
  static const uint8_t nil_key[] = {6, 1, 0, 0, 0, 0, 1};
  static const uint8_t table_key[] = {6, 1, 0, 0, 0, 6, 0, 0, 0, 0, 1};
  static const uint8_t nan_key[] = {6, 1, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0xF8, 0x7F, 1};
  static const uint8_t short_value[] = {6, 1, 0, 0, 0, 1};
  static const uint8_t short_key[] = {6, 1, 0, 0, 0};
  static const uint8_t trailing[] = {1, 1};
  static const uint8_t ok_table[] = {6, 1, 0, 0, 0, 2, 3, 5, 0, 0, 0, 0, 0, 0, 0};
  TEST_ASSERT_FALSE(decodes(empty, 0));
  TEST_ASSERT_FALSE(decodes(bad_tag, sizeof bad_tag));
  TEST_ASSERT_FALSE(decodes(short_int, sizeof short_int));
  TEST_ASSERT_FALSE(decodes(short_float, sizeof short_float));
  TEST_ASSERT_FALSE(decodes(short_string, sizeof short_string));
  TEST_ASSERT_FALSE(decodes(short_len, sizeof short_len));
  TEST_ASSERT_FALSE(decodes(huge_table, sizeof huge_table));
  TEST_ASSERT_FALSE(decodes(nil_key, sizeof nil_key));
  TEST_ASSERT_FALSE(decodes(table_key, sizeof table_key));
  TEST_ASSERT_FALSE(decodes(nan_key, sizeof nan_key));
  TEST_ASSERT_FALSE(decodes(short_value, sizeof short_value));
  TEST_ASSERT_FALSE(decodes(short_key, sizeof short_key));
  TEST_ASSERT_FALSE(decodes(trailing, sizeof trailing));
  TEST_ASSERT_TRUE(decodes(ok_table, sizeof ok_table));
  uint8_t deep[40];
  for (size_t i = 0; i < sizeof deep; i += 5) {
    const uint8_t level[] = {6, 1, 0, 0, 0}; /* { [{...}] ... } never finishes: depth limit first */
    memcpy(deep + i, level, 5);
  }
  TEST_ASSERT_FALSE(decodes(deep, sizeof deep));
}

static void test_net_unavailable(void) {
  package("assets/base", "base", NULL, "");
  start();
  exec(
      "p, perr = net.host(0); j, jerr = net.join('ws://127.0.0.1:1'); s, serr = net.send('c', 1);"
      " st = net.status(); n = #net.receive(); net.leave()");
  TEST_ASSERT_EQUAL_STRING("networking is not available", gstr("perr"));
  TEST_ASSERT_EQUAL_STRING("networking is not available", gstr("jerr"));
  TEST_ASSERT_EQUAL_STRING("networking is not available", gstr("serr"));
  exec("ok = st.role == 'none' and st.player == -1 and #st.players == 0 and n == 0");
  TEST_ASSERT_TRUE(gbool("ok"));
}

static zb_net *test_net_create(void) {
  zb_net_config nc;
  memset(&nc, 0, sizeof nc);
  nc.a = zb_test_alloc();
  nc.world_seed = 3;
  return zb_net_create(&nc);
}

/* spec: netcode: Scripts host and join */
static void test_net_api(void) {
  package("assets/base", "base", NULL, "");
  configure();
  zb_net *host = test_net_create();
  F.cfg.net = host;
  start_with(&F.cfg);
  exec("j, jerr = net.join('http://nowhere')");
  TEST_ASSERT_EQUAL_STRING("not a ws:// address: http://nowhere", gstr("jerr"));
  exec("ok, err = pcall(net.host, 70000)");
  TEST_ASSERT_FALSE(gbool("ok"));
  if (!zb_sock_supported()) {
    exec("p, perr = net.host(0)");
    TEST_ASSERT_EQUAL_INT(0, (int)gnum("p"));
    stop();
    zb_net_destroy(host);
    TEST_IGNORE_MESSAGE("no sockets in this build");
  }
  exec("port = net.host(0)");
  int port = (int)gnum("port");
  TEST_ASSERT_TRUE(port > 0);
  char again[64];
  snprintf(again, sizeof again, "p2, perr = net.host(%d)", port);
  exec(again);
  TEST_ASSERT_EQUAL_INT(0, (int)gnum("p2"));
  TEST_ASSERT_NOT_NULL(strstr(gstr("perr"), "cannot listen"));
  char url[64], err[128];
  snprintf(url, sizeof url, "ws://127.0.0.1:%d", port);
  zb_net *client = test_net_create();
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_join(client, zb_ws_client_create(zb_test_alloc(), url, err, sizeof err)));
  for (int i = 0; i < 3000 && zb_net_role_of(client) != ZB_NET_CLIENT; ++i) {
    zb_net_update(host);
    zb_net_update(client);
    zb_test_sleep_ms(1);
  }
  TEST_ASSERT_EQUAL_INT(ZB_NET_CLIENT, zb_net_role_of(client));
  exec("msgs = net.receive(); st = net.status()");
  exec(
      "ok = msgs[1].event == 'joined' and msgs[1].player == 1 and st.role == 'host' and st.player == 0"
      " and #st.players == 2 and st.players[2] == 1 and st.error == '' and #st.rtt_ms == 4");
  TEST_ASSERT_TRUE(gbool("ok"));
  /* Host to client: a table arrives intact. */
  exec("sent = net.send('hello', { a = 1, b = { 2.5, 'x' } }, 1)");
  TEST_ASSERT_TRUE(gbool("sent"));
  exec("bad, why = net.send('hello', print)");
  TEST_ASSERT_EQUAL_STRING("only nil, booleans, numbers, strings and tables can be sent", gstr("why"));
  exec("nobody, why = net.send('hello', 1, 3)");
  TEST_ASSERT_EQUAL_STRING(zb_result_str(ZB_ERR_NOT_FOUND), gstr("why"));
  zb_net_event ev;
  bool got = false;
  for (int i = 0; i < 3000 && !got; ++i) {
    zb_net_update(host);
    zb_net_update(client);
    while (zb_net_next(client, &ev)) {
      if (ev.kind == ZB_NET_EV_DATA) {
        TEST_ASSERT_EQUAL_STRING("hello", ev.channel);
        TEST_ASSERT_TRUE(zb_script_net_decode(LS(), ev.data, ev.len));
        lua_setglobal(LS(), "arrived");
        got = true;
      }
    }
    zb_test_sleep_ms(1);
  }
  TEST_ASSERT_TRUE(got);
  exec("ok = arrived.a == 1 and arrived.b[1] == 2.5 and arrived.b[2] == 'x'");
  TEST_ASSERT_TRUE(gbool("ok"));
  /* Client to host: a malformed value arrives without a value. */
  static const uint8_t junk[] = {99};
  zb_net_send(client, 0, "junk", junk, sizeof junk);
  for (int i = 0; i < 3000; ++i) {
    zb_net_update(client);
    zb_net_update(host);
    exec("msgs = net.receive(); if #msgs > 0 then got_junk = msgs[1] end");
    if (gbool("got_junk")) {
      break;
    }
    zb_test_sleep_ms(1);
  }
  exec(
      "ok = got_junk.event == 'data' and got_junk.channel == 'junk' and got_junk.player == 1"
      " and got_junk.value == nil");
  TEST_ASSERT_TRUE(gbool("ok"));
  exec("net.leave(); st = net.status()");
  exec("ok = st.role == 'none' and st.sent > 0 and st.received > 0");
  TEST_ASSERT_TRUE(gbool("ok"));
  /* Joining from a script: a host in C, the script as its client. */
  zb_net *other = test_net_create();
  uint16_t other_port = 0;
  TEST_ASSERT_EQUAL_INT(ZB_OK,
                        zb_net_host(other, zb_ws_server_create(zb_test_alloc(), 0, &other_port, err, sizeof err)));
  snprintf(url, sizeof url, "joined = net.join('ws://127.0.0.1:%u')", (unsigned)other_port);
  exec(url);
  TEST_ASSERT_TRUE(gbool("joined"));
  for (int i = 0; i < 3000 && zb_net_role_of(host) != ZB_NET_CLIENT; ++i) {
    zb_net_update(other);
    zb_net_update(host);
    zb_test_sleep_ms(1);
  }
  exec("st = net.status(); ok = st.role == 'client' and st.player == 1");
  TEST_ASSERT_TRUE(gbool("ok"));
  zb_net_destroy(other);
  zb_net_destroy(client);
  stop();
  zb_net_destroy(host);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_entry_scripts_and_require);
  RUN_TEST(test_failing_entry_script);
  RUN_TEST(test_hooks);
  RUN_TEST(test_failing_tick_hook);
  RUN_TEST(test_instruction_budget);
  RUN_TEST(test_hot_reload);
  RUN_TEST(test_reload_disabled_in_release);
  RUN_TEST(test_forbidden_calls);
  RUN_TEST(test_binary_chunk_rejected);
  RUN_TEST(test_repeatable_randomness);
  RUN_TEST(test_engine_info);
  RUN_TEST(test_engine_info_without_host);
  RUN_TEST(test_engine_controls);
  RUN_TEST(test_hash_state);
  RUN_TEST(test_packages_and_credits);
  RUN_TEST(test_vfs_api);
  RUN_TEST(test_input_api);
  RUN_TEST(test_render_api);
  RUN_TEST(test_audio_api);
  RUN_TEST(test_save_api);
  RUN_TEST(test_lua_test_runner);
  RUN_TEST(test_require_without_packages);
  RUN_TEST(test_scenarios);
  RUN_TEST(test_capture_and_golden);
  RUN_TEST(test_capture_unsupported);
  RUN_TEST(test_coverage);
  RUN_TEST(test_error_ring);
  RUN_TEST(test_create_out_of_memory);
  RUN_TEST(test_net_codec);
  RUN_TEST(test_net_unavailable);
  RUN_TEST(test_net_api);
  return UNITY_END();
}

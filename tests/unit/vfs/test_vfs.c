/* SPDX-License-Identifier: MIT */
#include "core/log.h"
#include "core/memfs.h"
#include "support/test_support.h"
#include "support/test_threads.h"
#include "unity.h"
#include "vfs/vfs.h"

#include <string.h>

static zb_memfs *g_fs;

void setUp(void) {
  zb_test_begin();
  zb_log_reset();
  zb_log_set_stderr(false);
  g_fs = zb_memfs_create(zb_test_alloc());
}

void tearDown(void) {
  zb_memfs_destroy(g_fs);
  zb_log_reset();
  zb_test_end();
}

static void put(const char *path, const char *text) { zb_memfs_put(g_fs, path, text, strlen(text)); }

static void manifest(const char *root, const char *id, const char *version, const char *api, const char *extra) {
  char path[256], text[1024];
  snprintf(path, sizeof path, "%s/package.lua", root);
  snprintf(text, sizeof text,
           "return { id = \"%s\", version = \"%s\", api = \"%s\", license = { code = \"MIT\", assets = \"CC-BY-4.0\" "
           "}, %s }",
           id, version, api, extra ? extra : "");
  put(path, text);
}

static zb_vfs *mount(void) {
  zb_vfs *v = zb_vfs_create(zb_test_alloc(), zb_memfs_api(g_fs), NULL);
  zb_vfs_add_root(v, "assets");
  zb_vfs_add_root(v, "mods");
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_mount(v, 1, 2));
  return v;
}

/* spec: asset-packages: Valid packages are found */
static void test_valid_packages_found(void) {
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  manifest("mods/example", "example", "1.2.0", "1.0", "requires = { base = \">=0.1\" }, name = \"Example\"");
  put("assets/base/textures/a.png", "A");
  zb_vfs *v = mount();
  TEST_ASSERT_EQUAL_size_t(2, zb_vfs_package_count(v));
  TEST_ASSERT_EQUAL_size_t(2, zb_vfs_load_count(v));
  const zb_package *ex = zb_vfs_find_package(v, "example");
  TEST_ASSERT_NOT_NULL(ex);
  TEST_ASSERT_EQUAL_STRING("1.2.0", ex->version);
  TEST_ASSERT_EQUAL_STRING("CC-BY-4.0", ex->license_assets);
  TEST_ASSERT_EQUAL_STRING("Example", ex->name);
  TEST_ASSERT_NULL(zb_vfs_find_package(v, "nope"));
  TEST_ASSERT_NOT_NULL(zb_vfs_package(v, 0));
  TEST_ASSERT_NULL(zb_vfs_package(v, 9));
  TEST_ASSERT_NULL(zb_vfs_load_order(v, 9));
  zb_vfs_destroy(v);
}

/* spec: asset-packages: Invalid manifest */
static void test_invalid_manifest(void) {
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  put("mods/broken/package.lua", "return { id = \"broken\", version = ");
  put("mods/noid/package.lua", "return { version = \"1.0\", api = \"1.0\", license = \"MIT\" }");
  put("mods/badver/package.lua", "return { id = \"badver\", version = \"x\", api = \"1.0\", license = \"MIT\" }");
  put("mods/badapi/package.lua", "return { id = \"badapi\", version = \"1.0\", api = \"one\", license = \"MIT\" }");
  put("mods/nolicense/package.lua", "return { id = \"nolicense\", version = \"1.0\", api = \"1.0\" }");
  put("mods/notable/package.lua", "return 42");
  put("mods/baddeps/package.lua",
      "return { id = \"baddeps\", version = \"1.0\", api = \"1.0\", license = \"MIT\", requires = { \"base\" } }");
  put("mods/strlicense/package.lua",
      "return { id = \"strlicense\", version = \"1.0\", api = \"1.0\", license = \"MIT\" }");
  put("mods/notapackage/readme.txt", "no manifest here");
  put("mods/.hidden/package.lua", "return {}");
  zb_vfs *v = mount();
  TEST_ASSERT_EQUAL_size_t(2, zb_vfs_load_count(v)); /* base + strlicense */
  const zb_package *strl = zb_vfs_find_package(v, "strlicense");
  TEST_ASSERT_EQUAL_STRING("MIT", strl->license_assets);
  for (size_t i = 0; i < zb_vfs_package_count(v); ++i) {
    const zb_package *p = zb_vfs_package(v, i);
    if (strcmp(p->root, "mods/broken") == 0) {
      TEST_ASSERT_FALSE(p->enabled);
      TEST_ASSERT_NOT_NULL(strstr(p->error, "package.lua:1:"));
    }
    if (strcmp(p->root, "mods/noid") == 0) {
      TEST_ASSERT_NOT_NULL(strstr(p->error, "'id'"));
    }
  }
  TEST_ASSERT_FALSE(zb_vfs_find_package(v, "baddeps")->enabled);
  zb_vfs_destroy(v);
}

/* spec: asset-packages: Dependent mod loads after base */
static void test_dependency_order(void) {
  manifest("mods/aardvark", "aardvark", "1.0", "1.0", "requires = { base = \">=0.1\" }");
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  manifest("mods/zz", "zz", "1.0", "1.0", "optional = { aardvark = \"*\", missing = \"*\" }");
  zb_vfs *v = mount();
  TEST_ASSERT_EQUAL_STRING("base", zb_vfs_load_order(v, 0)->id);
  TEST_ASSERT_EQUAL_STRING("aardvark", zb_vfs_load_order(v, 1)->id);
  TEST_ASSERT_EQUAL_STRING("zz", zb_vfs_load_order(v, 2)->id);
  zb_vfs_destroy(v);
}

/* spec: asset-packages: Missing dependency */
static void test_missing_dependency(void) {
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  manifest("mods/needy", "needy", "1.0", "1.0", "requires = { ghost = \">=1\" }");
  manifest("mods/chain", "chain", "1.0", "1.0", "requires = { needy = \"*\" }");
  manifest("mods/tooold", "tooold", "1.0", "1.0", "requires = { base = \">=2.0\" }");
  manifest("mods/badconstraint", "badconstraint", "1.0", "1.0", "requires = { base = \">=x\" }");
  zb_vfs *v = mount();
  TEST_ASSERT_EQUAL_size_t(1, zb_vfs_load_count(v));
  const zb_package *needy = zb_vfs_find_package(v, "needy");
  TEST_ASSERT_FALSE(needy->enabled);
  TEST_ASSERT_NOT_NULL(strstr(needy->error, "missing dependency 'ghost'"));
  TEST_ASSERT_NOT_NULL(strstr(zb_vfs_find_package(v, "chain")->error, "'needy'"));
  TEST_ASSERT_NOT_NULL(strstr(zb_vfs_find_package(v, "tooold")->error, "0.1.0 is installed"));
  TEST_ASSERT_NOT_NULL(strstr(zb_vfs_find_package(v, "badconstraint")->error, "invalid version constraint"));
  zb_vfs_destroy(v);
}

/* spec: asset-packages: Dependency cycle */
static void test_dependency_cycle(void) {
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  manifest("mods/a", "a", "1.0", "1.0", "requires = { b = \"*\" }");
  manifest("mods/b", "b", "1.0", "1.0", "requires = { a = \"*\" }");
  zb_vfs *v = mount();
  TEST_ASSERT_EQUAL_size_t(1, zb_vfs_load_count(v));
  const zb_package *a = zb_vfs_find_package(v, "a");
  TEST_ASSERT_FALSE(a->enabled);
  TEST_ASSERT_NOT_NULL(strstr(a->error, "dependency cycle among: a, b"));
  TEST_ASSERT_FALSE(zb_vfs_find_package(v, "b")->enabled);
  zb_vfs_destroy(v);
}

/* spec: asset-packages: Package targets a future API */
static void test_future_api(void) {
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  manifest("mods/future", "future", "1.0", "1.4", NULL);
  manifest("mods/othermajor", "othermajor", "1.0", "2.0", NULL);
  zb_vfs *v = mount(); /* engine API 1.2 */
  const zb_package *f = zb_vfs_find_package(v, "future");
  TEST_ASSERT_FALSE(f->enabled);
  TEST_ASSERT_NOT_NULL(strstr(f->error, "requires scripting API 1.4 but the engine provides 1.2"));
  TEST_ASSERT_FALSE(zb_vfs_find_package(v, "othermajor")->enabled);
  zb_vfs_destroy(v);
}

static void test_duplicate_ids(void) {
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  manifest("mods/base2", "base", "0.2.0", "1.0", NULL);
  zb_vfs *v = mount();
  TEST_ASSERT_EQUAL_size_t(1, zb_vfs_load_count(v));
  TEST_ASSERT_EQUAL_STRING("0.1.0", zb_vfs_load_order(v, 0)->version);
  zb_vfs_destroy(v);
}

typedef struct names {
  char list[16][64];
  bool dir[16];
  int n;
  int stop_after;
} names;

static bool collect(const char *name, bool is_dir, void *user) {
  names *nm = user;
  snprintf(nm->list[nm->n], 64, "%s", name);
  nm->dir[nm->n] = is_dir;
  nm->n++;
  return nm->stop_after == 0 || nm->n < nm->stop_after;
}

/* spec: asset-packages: Mod overrides a texture */
/* spec: asset-packages: Merged listing */
static void test_overlay_and_listing(void) {
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  manifest("mods/mod", "mod", "1.0", "1.0", "requires = { base = \"*\" }");
  put("assets/base/textures/carpet.png", "BASE");
  put("assets/base/sounds/a.wav", "a");
  put("assets/base/sounds/b.wav", "b");
  put("assets/base/sounds/deep/x.wav", "x");
  put("assets/base/.hidden", "x");
  put("mods/mod/textures/carpet.png", "MOD");
  put("mods/mod/sounds/b.wav", "b2");
  put("mods/mod/sounds/c.wav", "c");
  zb_vfs *v = mount();
  char *data;
  size_t len;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_read(v, "textures/carpet.png", zb_test_alloc(), &data, &len));
  TEST_ASSERT_EQUAL_STRING("MOD", data);
  zb_free(zb_test_alloc(), data);
  TEST_ASSERT_EQUAL_STRING("mod", zb_vfs_owner(v, "textures/carpet.png")->id);
  TEST_ASSERT_EQUAL_STRING("base", zb_vfs_owner(v, "sounds/a.wav")->id);
  TEST_ASSERT_NULL(zb_vfs_owner(v, "nope.png"));
  TEST_ASSERT_TRUE(zb_vfs_exists(v, "./sounds//c.wav"));
  TEST_ASSERT_FALSE(zb_vfs_exists(v, ".hidden"));
  /* package-specific read bypasses the overlay */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_read_package(v, zb_vfs_find_package(v, "base"), "textures/carpet.png",
                                                   zb_test_alloc(), &data, &len));
  TEST_ASSERT_EQUAL_STRING("BASE", data);
  zb_free(zb_test_alloc(), data);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID,
                        zb_vfs_read_package(v, zb_vfs_find_package(v, "base"), "../x", zb_test_alloc(), &data, &len));
  names nm;
  memset(&nm, 0, sizeof nm);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_list(v, "sounds", collect, &nm));
  TEST_ASSERT_EQUAL_INT(4, nm.n);
  TEST_ASSERT_EQUAL_STRING("a.wav", nm.list[0]);
  TEST_ASSERT_EQUAL_STRING("b.wav", nm.list[1]);
  TEST_ASSERT_EQUAL_STRING("c.wav", nm.list[2]);
  TEST_ASSERT_EQUAL_STRING("deep", nm.list[3]);
  TEST_ASSERT_TRUE(nm.dir[3]);
  memset(&nm, 0, sizeof nm);
  nm.stop_after = 1;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_list(v, "", collect, &nm));
  TEST_ASSERT_EQUAL_INT(1, nm.n);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, zb_vfs_list(v, "nothing", collect, &nm));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_list(v, "/abs", collect, &nm));
  TEST_ASSERT_TRUE(zb_vfs_file_count(v) >= 6);
  char real[256];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_real_path(v, "sounds/b.wav", real, sizeof real));
  TEST_ASSERT_EQUAL_STRING("mods/mod/sounds/b.wav", real);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, zb_vfs_real_path(v, "missing", real, sizeof real));
  TEST_ASSERT_NOT_EQUAL(0, zb_vfs_package_set_hash(v));
  zb_vfs_destroy(v);
}

/* spec: asset-packages: Escape attempt */
static void test_path_safety(void) {
  char out[64];
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_normalize("../../secret.txt", out, sizeof out));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_normalize("/etc/passwd", out, sizeof out));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_normalize("\\windows", out, sizeof out));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_normalize("C:/x", out, sizeof out));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_normalize("a/../../b", out, sizeof out));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_normalize(NULL, out, sizeof out));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_normalize("a/./b/../c\\d.png", out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("a/c/d.png", out);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_normalize("x/..", out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("", out);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_normalize("very/long/path/name", out, 8));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_normalize("", out, 0));
  char deep[400] = {0};
  for (int i = 0; i < 70; ++i) {
    strcat(deep, "a/");
  }
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_normalize(deep, out, sizeof out));
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  put("secret.txt", "s");
  zb_vfs *v = mount();
  char *data;
  size_t len;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_vfs_read(v, "../../secret.txt", zb_test_alloc(), &data, &len));
  zb_vfs_destroy(v);
}

static void test_versions(void) {
  zb_version v;
  TEST_ASSERT_TRUE(zb_version_parse("1.2.3", &v));
  TEST_ASSERT_EQUAL_INT(3, v.patch);
  TEST_ASSERT_TRUE(zb_version_parse("2", &v));
  TEST_ASSERT_TRUE(zb_version_parse("1.0.0-beta+7", &v));
  TEST_ASSERT_FALSE(zb_version_parse("1.x", &v));
  TEST_ASSERT_FALSE(zb_version_parse("1.2.y", &v));
  TEST_ASSERT_FALSE(zb_version_parse("v1", &v));
  TEST_ASSERT_FALSE(zb_version_parse("1.2.3x", &v));
  TEST_ASSERT_FALSE(zb_version_parse("99999999", &v));
  zb_version a = {1, 2, 0}, b = {1, 10, 0};
  TEST_ASSERT_EQUAL_INT(-1, zb_version_compare(a, b));
  TEST_ASSERT_EQUAL_INT(1, zb_version_compare(b, a));
  TEST_ASSERT_EQUAL_INT(0, zb_version_compare(a, a));
  zb_version c = {2, 0, 0}, d = {1, 0, 5};
  TEST_ASSERT_EQUAL_INT(1, zb_version_compare(c, d));
  TEST_ASSERT_EQUAL_INT(-1, zb_version_compare((zb_version){1, 0, 1}, d));
  bool valid;
  TEST_ASSERT_TRUE(zb_version_satisfies(a, "*", &valid));
  TEST_ASSERT_TRUE(zb_version_satisfies(a, "", &valid));
  TEST_ASSERT_TRUE(zb_version_satisfies(a, ">=1.2 <2", &valid));
  TEST_ASSERT_FALSE(zb_version_satisfies(a, ">1.2", &valid));
  TEST_ASSERT_TRUE(zb_version_satisfies(a, "<=1.2", &valid));
  TEST_ASSERT_TRUE(zb_version_satisfies(a, "=1.2.0", &valid));
  TEST_ASSERT_TRUE(zb_version_satisfies(a, "1.2", &valid));
  TEST_ASSERT_TRUE(zb_version_satisfies(b, "^1.2", &valid));
  TEST_ASSERT_FALSE(zb_version_satisfies(c, "^1.2", &valid));
  TEST_ASSERT_FALSE(zb_version_satisfies(a, ">=nope", &valid));
  TEST_ASSERT_FALSE(valid);
}

static void test_mount_without_roots(void) {
  zb_vfs *v = zb_vfs_create(zb_test_alloc(), zb_memfs_api(g_fs), NULL);
  zb_vfs_add_root(v, "nowhere");
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, zb_vfs_mount(v, 1, 0));
  zb_vfs_destroy(v);
}

typedef struct cb_count {
  int calls;
  zb_vfs_req_state state;
  zb_vfs *v;
  bool release;
} cb_count;

static void on_done(zb_vfs_req *r, void *user) {
  cb_count *c = user;
  c->calls++;
  c->state = zb_vfs_req_state_of(r);
  if (c->release) {
    zb_vfs_req_release(c->v, r);
  }
}

/* spec: asset-packages: Load completes later */
static void test_async_requests(bool threaded) {
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  put("assets/base/models/m.glb", "MODEL");
  /* Worker threads allocate concurrently: use the (thread-safe) heap there. */
  const zb_allocator *a = threaded ? zb_heap_allocator() : zb_test_alloc();
  zb_jobs *jobs = threaded ? zb_jobs_create(a, zb_test_thread_api(), 2) : zb_jobs_create(a, NULL, 0);
  zb_vfs *v = zb_vfs_create(a, zb_memfs_api(g_fs), jobs);
  zb_vfs_add_root(v, "assets");
  zb_vfs_mount(v, 1, 0);
  cb_count ok = {0, ZB_REQ_PENDING, v, false}, missing = {0, ZB_REQ_PENDING, v, true};
  zb_vfs_req *r = zb_vfs_request(v, "models/m.glb", on_done, &ok);
  zb_vfs_req *m = zb_vfs_request(v, "models/none.glb", on_done, &missing);
  (void)m;
  TEST_ASSERT_EQUAL_INT(ZB_REQ_PENDING, zb_vfs_req_state_of(r));
  TEST_ASSERT_NULL(zb_vfs_req_data(r, NULL));
  TEST_ASSERT_EQUAL_INT(0, ok.calls);
  TEST_ASSERT_EQUAL_size_t(2, zb_vfs_pending_count(v));
  for (int i = 0; i < 2000 && (ok.calls == 0 || missing.calls == 0); ++i) {
    zb_jobs_run_pending(jobs, 8);
    zb_vfs_poll(v);
    if (threaded && (ok.calls == 0 || missing.calls == 0)) {
      zb_test_sleep_ms(1);
    }
  }
  TEST_ASSERT_EQUAL_INT(1, ok.calls);
  TEST_ASSERT_EQUAL_INT(ZB_REQ_READY, ok.state);
  TEST_ASSERT_EQUAL_INT(1, missing.calls);
  TEST_ASSERT_EQUAL_INT(ZB_REQ_FAILED, missing.state);
  size_t len;
  TEST_ASSERT_EQUAL_STRING("MODEL", zb_vfs_req_data(r, &len));
  TEST_ASSERT_EQUAL_size_t(5, len);
  TEST_ASSERT_EQUAL_STRING("models/m.glb", zb_vfs_req_path(r));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_vfs_req_error(r));
  zb_vfs_poll(v); /* nothing more: callbacks fire once */
  TEST_ASSERT_EQUAL_INT(1, ok.calls);
  zb_vfs_req_release(v, r);
  zb_vfs_req_release(v, NULL);
  /* a request released while pending is cancelled */
  zb_vfs_req *p = zb_vfs_request(v, "models/m.glb", NULL, NULL);
  zb_vfs_req_release(v, p);
  /* a request still pending at destroy time is cleaned up */
  zb_vfs_request(v, "models/m.glb", NULL, NULL);
  TEST_ASSERT_EQUAL_PTR(a, zb_vfs_allocator(v));
  zb_vfs_destroy(v);
  zb_jobs_destroy(jobs);
}

static void test_async_inline_jobs(void) { test_async_requests(false); }
static void test_async_threaded_jobs(void) { test_async_requests(true); }

static void test_async_without_jobs(void) {
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  put("assets/base/a.txt", "A");
  zb_vfs *v = mount();
  cb_count c = {0, ZB_REQ_PENDING, v, true};
  zb_vfs_request(v, "a.txt", on_done, &c);
  TEST_ASSERT_EQUAL_INT(0, c.calls); /* completion is delivered by poll, never inline */
  TEST_ASSERT_EQUAL_size_t(1, zb_vfs_poll(v));
  TEST_ASSERT_EQUAL_INT(1, c.calls);
  TEST_ASSERT_EQUAL_INT(ZB_REQ_READY, c.state);
  zb_vfs_destroy(v);
}

typedef struct fake_fetcher {
  zb_vfs_req *pending[4];
  int n;
} fake_fetcher;

static void fetch_start(void *ctx, zb_vfs_req *req, const char *real) {
  (void)real;
  fake_fetcher *f = ctx;
  f->pending[f->n++] = req;
}

static void test_external_fetcher(void) {
  manifest("assets/base", "base", "0.1.0", "1.0", NULL);
  put("assets/base/a.txt", "A");
  zb_vfs *v = mount();
  fake_fetcher f = {{0}, 0};
  zb_vfs_fetcher fetcher = {&f, fetch_start};
  zb_vfs_set_fetcher(v, &fetcher);
  cb_count c = {0, ZB_REQ_PENDING, v, false};
  zb_vfs_req *r = zb_vfs_request(v, "a.txt", on_done, &c);
  zb_vfs_req *released = zb_vfs_request(v, "a.txt", on_done, &c);
  zb_vfs_req *failing = zb_vfs_request(v, "a.txt", NULL, NULL);
  TEST_ASSERT_EQUAL_INT(3, f.n);
  zb_vfs_req_release(v, released); /* kept until the fetch completes */
  zb_vfs_poll(v);
  TEST_ASSERT_EQUAL_INT(0, c.calls);
  char *data = zb_alloc(zb_test_alloc(), 4);
  memcpy(data, "WEB", 4);
  zb_vfs_req_complete(r, ZB_OK, data, 3);
  char *data2 = zb_alloc(zb_test_alloc(), 1);
  zb_vfs_req_complete(released, ZB_OK, data2, 0);
  char *junk = zb_alloc(zb_test_alloc(), 4);
  zb_vfs_req_complete(failing, ZB_ERR_IO, junk, 0);
  zb_vfs_poll(v);
  TEST_ASSERT_EQUAL_INT(1, c.calls);
  TEST_ASSERT_EQUAL_STRING("WEB", zb_vfs_req_data(r, NULL));
  TEST_ASSERT_EQUAL_INT(ZB_REQ_FAILED, zb_vfs_req_state_of(failing));
  zb_vfs_req_release(v, r);
  zb_vfs_req_release(v, failing);
  zb_vfs_set_fetcher(v, NULL);
  zb_vfs_destroy(v);
}

static void test_out_of_memory(void) {
  manifest("assets/base", "base", "0.1.0", "1.0", "requires = { }");
  put("assets/base/scripts/main.lua", "x");
  put("assets/base/textures/t.png", "x");
  for (long budget = 0; budget < 60; ++budget) {
    zb_failing_allocator fa;
    const zb_allocator *a = zb_failing_init(&fa, zb_test_alloc(), budget);
    zb_vfs *v = zb_vfs_create(a, zb_memfs_api(g_fs), NULL);
    if (!v) {
      continue;
    }
    if (zb_vfs_add_root(v, "assets") == ZB_OK) {
      zb_vfs_mount(v, 1, 0);
      zb_vfs_req *r = zb_vfs_request(v, "textures/t.png", NULL, NULL);
      zb_vfs_poll(v);
      zb_vfs_req_release(v, r);
    }
    zb_vfs_destroy(v);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_packages_found);
  RUN_TEST(test_invalid_manifest);
  RUN_TEST(test_dependency_order);
  RUN_TEST(test_missing_dependency);
  RUN_TEST(test_dependency_cycle);
  RUN_TEST(test_future_api);
  RUN_TEST(test_duplicate_ids);
  RUN_TEST(test_overlay_and_listing);
  RUN_TEST(test_path_safety);
  RUN_TEST(test_versions);
  RUN_TEST(test_mount_without_roots);
  RUN_TEST(test_async_inline_jobs);
  RUN_TEST(test_async_threaded_jobs);
  RUN_TEST(test_async_without_jobs);
  RUN_TEST(test_external_fetcher);
  RUN_TEST(test_out_of_memory);
  return UNITY_END();
}

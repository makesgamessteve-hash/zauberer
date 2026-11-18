/* SPDX-License-Identifier: MIT */
#include "app/engine.h"

#include "app/daylight.h"
#include "world/bundle.h"

#include "core/hash.h"
#include "core/log.h"
#include "core/luon.h"
#include "core/version.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADLESS_BATCH 600u
#define CONFIG_FILE "config.lua"

/* ---- Script host callbacks ---------------------------------------------------- */

static void host_quit(void *ctx, int code) {
  zb_engine *e = ctx;
  e->quit = true;
  e->exit_code = code;
}

static void host_info(void *ctx, zb_script_info *out) {
  zb_engine *e = ctx;
  out->platform = zb_platform_name();
#ifdef ZB_DEV
  out->build = "development";
#else
  out->build = "release";
#endif
  out->input_mode = zb_input_get_mode(e->input) == ZB_INPUT_TOUCH ? "touch" : "desktop";
  out->drawable_w = e->drawable_w;
  out->drawable_h = e->drawable_h;
  out->seed = e->seed;
}

static double host_time(void *ctx) {
  zb_engine *e = ctx;
  return (double)e->ticks * zb_loop_tick_seconds();
}

static uint64_t host_ticks(void *ctx) { return ((zb_engine *)ctx)->ticks; }

static void host_pause(void *ctx, bool paused) { zb_loop_set_paused(&((zb_engine *)ctx)->loop, paused); }

static void host_time_scale(void *ctx, double scale) { zb_loop_set_time_scale(&((zb_engine *)ctx)->loop, scale); }

static void host_fullscreen(void *ctx, bool on) { zb_platform_set_fullscreen(((zb_engine *)ctx)->platform, on); }

static zb_result write_config(zb_engine *e) {
  char bindings[8192];
  zb_input_write_user(e->input, bindings, sizeof bindings);
  char text[8400];
  int n = snprintf(text, sizeof text, "return {\n  bindings = %s,\n}\n", bindings);
  char path[1100];
  snprintf(path, sizeof path, "%s%s", e->user_dir, CONFIG_FILE);
  const zb_fs_api *fs = zb_platform_fs(e->platform);
  zb_result r = fs->write(fs->ctx, path, text, (size_t)n);
  zb_platform_user_data_sync(e->platform);
  return r;
}

static zb_result host_save_bindings(void *ctx) { return write_config(ctx); }

static void host_user_data_changed(void *ctx) { zb_platform_user_data_sync(((zb_engine *)ctx)->platform); }

static uint64_t world_clock(void *ctx) { return zb_platform_now_ns(ctx); }

static zb_world *make_world(zb_engine *e, const zb_world_bundle *bundle, char *err, size_t err_len) {
  zb_world_desc wd;
  memset(&wd, 0, sizeof wd);
  wd.a = e->a;
  wd.jobs = e->jobs;
  wd.seed = e->seed;
  wd.bundle = bundle;
  wd.flora = e->flora.count ? &e->flora : NULL;
#ifdef __EMSCRIPTEN__
  wd.profile = zb_input_get_mode(e->input) == ZB_INPUT_TOUCH ? ZB_STREAM_MOBILE : ZB_STREAM_WEB;
#else
  wd.profile = zb_input_get_mode(e->input) == ZB_INPUT_TOUCH ? ZB_STREAM_MOBILE : ZB_STREAM_DESKTOP;
#endif
  wd.clock_ns = world_clock;
  wd.clock_ctx = e->platform;
  return zb_world_create(&wd, err, err_len);
}

/* Plays in a created world: a new world and terrain view on its bundle (NULL:
 * the noise world of the engine's seed). */
static bool host_use_bundle(void *ctx, zb_world_bundle *bundle, char *err, size_t err_len) {
  zb_engine *e = ctx;
  zb_world *w = make_world(e, bundle, err, err_len);
  zb_terrain_view *tv = w ? zb_terrain_view_create(e->a, e->render, e->vfs, "textures/terrain/atlas.lua", NULL) : NULL;
  if (!tv) {
    zb_world_destroy(w);
    if (w) {
      snprintf(err, err_len, "out of memory");
    }
    return false;
  }
  zb_terrain_view_destroy(e->terrain_view);
  zb_world_destroy(e->world);
  if (e->bundle) {
    zb_world_bundle_free(e->bundle);
    zb_free(e->a, e->bundle);
  }
  e->world = w;
  e->terrain_view = tv;
  e->bundle = bundle;
  zb_script_set_world(e->script, w);
  if (bundle) {
    zb_info("playing a created world (seed %llu, %s quality)", (unsigned long long)bundle->options.seed,
            zb_world_quality_name(bundle->options.quality));
  } else {
    zb_info("playing the noise world (seed %llu)", (unsigned long long)e->seed);
  }
  return true;
}

static void host_window_size(void *ctx, int w, int h) {
  zb_platform_set_window_size(((zb_engine *)ctx)->platform, w, h);
}

/* ---- Setup helpers ------------------------------------------------------------- */

static void ensure_slash(char *path, size_t len) {
  size_t n = strlen(path);
  if (n > 0 && path[n - 1] != '/' && path[n - 1] != '\\' && n + 1 < len) {
    path[n] = '/';
    path[n + 1] = '\0';
  }
}

/* Materials: data/materials.lua of every package, merged in load order. The
 * simulations need them; without usable data they stay off (NULL). */
static void load_materials(zb_engine *e) {
  zb_registry r;
  zb_registry_init(&r, e->a, "materials");
  zb_result res = ZB_OK;
  for (size_t i = 0; i < zb_vfs_load_count(e->vfs) && res == ZB_OK; ++i) {
    const zb_package *pkg = zb_vfs_load_order(e->vfs, i);
    char *text = NULL;
    size_t len = 0;
    if (zb_vfs_read_package(e->vfs, pkg, "data/materials.lua", e->a, &text, &len) != ZB_OK) {
      continue;
    }
    zb_luon_doc doc;
    zb_luon_error le;
    if (zb_luon_parse(&doc, e->a, text, len, &le) != ZB_OK) {
      zb_warn("materials of %s: data/materials.lua:%d: %s", pkg->id, le.line, le.message);
    } else {
      res = zb_registry_merge(&r, pkg->id, &doc.root);
      zb_luon_free(&doc);
    }
    zb_free(e->a, text);
  }
  char err[160] = "out of memory";
  e->materials = res == ZB_OK ? zb_alloc(e->a, sizeof *e->materials) : NULL;
  if (!e->materials) {
    zb_registry_free(&r);
  } else if (zb_materials_build(e->materials, &r, err, sizeof err) != ZB_OK) {
    zb_materials_free(e->materials);
    zb_free(e->a, e->materials);
    e->materials = NULL;
  }
  if (!e->materials) {
    zb_warn("no materials: %s", err);
  }
}

static void load_input_defaults(zb_engine *e) {
  for (size_t i = 0; i < zb_vfs_load_count(e->vfs); ++i) {
    const zb_package *p = zb_vfs_load_order(e->vfs, i);
    char *text = NULL;
    size_t len = 0;
    if (zb_vfs_read_package(e->vfs, p, "data/input.lua", e->a, &text, &len) != ZB_OK) {
      continue;
    }
    zb_luon_doc doc;
    zb_luon_error err;
    if (zb_luon_parse(&doc, e->a, text, len, &err) == ZB_OK) {
      char msg[256];
      if (zb_input_apply_defaults(e->input, &doc.root, msg, sizeof msg) != ZB_OK) {
        zb_error("[%s] data/input.lua: %s", p->id, msg);
      }
      zb_luon_free(&doc);
    } else {
      zb_error("[%s] data/input.lua:%d: %s", p->id, err.line, err.message);
    }
    zb_free(e->a, text);
  }
}

static void load_user_config(zb_engine *e) {
  char path[1100];
  snprintf(path, sizeof path, "%s%s", e->user_dir, CONFIG_FILE);
  const zb_fs_api *fs = zb_platform_fs(e->platform);
  char *text = NULL;
  size_t len = 0;
  if (fs->read(fs->ctx, path, e->a, &text, &len) != ZB_OK) {
    return;
  }
  zb_luon_doc doc;
  zb_luon_error err;
  if (zb_luon_parse(&doc, e->a, text, len, &err) == ZB_OK) {
    zb_input_apply_user(e->input, zb_table_get(&doc.root, "bindings"));
    const zb_value *vol = zb_table_get(&doc.root, "volume");
    if (vol) {
      zb_audio_set_master_volume(e->audio, (float)zb_value_num(zb_table_get(vol, "master"), 1.0));
      zb_audio_set_group_volume(e->audio, ZB_AUDIO_MUSIC, (float)zb_value_num(zb_table_get(vol, "music"), 1.0));
      zb_audio_set_group_volume(e->audio, ZB_AUDIO_EFFECTS, (float)zb_value_num(zb_table_get(vol, "effects"), 1.0));
    }
    zb_luon_free(&doc);
  } else {
    zb_warn("%s:%d: %s (configuration ignored)", path, err.line, err.message);
  }
  zb_free(e->a, text);
}

static void audio_fill(void *user, float *out, int frames) { zb_audio_mix(user, out, frames); }

static void host_sleep(void *ctx, unsigned ms) {
  zb_engine *e = ctx;
  zb_platform_sleep_ms(e->platform, ms);
}

static zb_gl_fn gl_loader(void *ctx, const char *name) { return (zb_gl_fn)zb_platform_gl_proc(ctx, name); }

/* Reads "<tick> <16 hex digits>" lines of <replay>.hashes, if present. */
static void load_expected_hashes(zb_engine *e) {
  char path[1100];
  snprintf(path, sizeof path, "%s.hashes", e->opt.replay_path);
  const zb_fs_api *fs = zb_platform_fs(e->platform);
  char *text = NULL;
  size_t len = 0;
  if (fs->read(fs->ctx, path, e->a, &text, &len) != ZB_OK) {
    return;
  }
  const char *p = text;
  while (*p) {
    char *end;
    unsigned long long tick = strtoull(p, &end, 10);
    if (end == p) {
      break;
    }
    p = end;
    unsigned long long hash = strtoull(p, &end, 16);
    if (end == p) {
      break;
    }
    p = end;
    zb_tick_hash th = {tick, hash};
    zb_array_push(&e->expected_hashes, &th);
    while (*p == '\n' || *p == '\r' || *p == ' ') {
      p++;
    }
  }
  zb_free(e->a, text);
  zb_info("replay: checking %zu state hash(es) from %s", e->expected_hashes.len, path);
}

static void write_recorded_hashes(zb_engine *e) {
  char path[1100];
  snprintf(path, sizeof path, "%s.hashes", e->opt.record_path);
  size_t cap = e->recorded_hashes.len * 40 + 1;
  char *text = zb_alloc(e->a, cap);
  if (!text) {
    return;
  }
  size_t n = 0;
  for (size_t i = 0; i < e->recorded_hashes.len; ++i) {
    const zb_tick_hash *th = zb_array_at(&e->recorded_hashes, i);
    char hex[17];
    zb_hash_hex(th->hash, hex);
    n += (size_t)snprintf(text + n, cap - n, "%llu %s\n", (unsigned long long)th->tick, hex);
  }
  const zb_fs_api *fs = zb_platform_fs(e->platform);
  if (fs->write(fs->ctx, path, text, n) != ZB_OK) {
    zb_error("could not write the state hashes to %s", path);
  }
  zb_free(e->a, text);
}

/* After every tick: record or check the state hash. */
static void check_state_hash(zb_engine *e) {
  if (e->recorder && e->ticks % ZB_HASH_INTERVAL == 0) {
    zb_tick_hash th = {e->ticks, zb_script_state_hash(e->script)};
    zb_array_push(&e->recorded_hashes, &th);
  }
  if (e->next_expected < e->expected_hashes.len) {
    const zb_tick_hash *want = zb_array_at(&e->expected_hashes, e->next_expected);
    if (want->tick == e->ticks) {
      e->next_expected++;
      uint64_t got = zb_script_state_hash(e->script);
      if (got != want->hash) {
        char a[17], b[17];
        zb_hash_hex(want->hash, a);
        zb_hash_hex(got, b);
        zb_error("replay diverged at tick %llu: expected state hash %s, got %s", (unsigned long long)e->ticks, a, b);
        e->replay_diverged = true;
        e->quit = true;
      }
    }
  }
}

static bool setup_replay(zb_engine *e) {
  if (e->opt.replay_path) {
    load_expected_hashes(e);
    const zb_fs_api *fs = zb_platform_fs(e->platform);
    size_t len = 0;
    if (fs->read(fs->ctx, e->opt.replay_path, e->a, &e->replay_data, &len) != ZB_OK) {
      zb_platform_show_error(e->platform, "Zauberer", "The replay file could not be read.");
      return false;
    }
    char err[200];
    e->replay = zb_input_replay_open(e->a, e->input, (const unsigned char *)e->replay_data, len, err, sizeof err);
    if (!e->replay) {
      zb_error("replay %s: %s", e->opt.replay_path, err);
      return false;
    }
    if (!e->opt.seed_set) {
      e->seed = zb_input_replay_seed(e->replay);
    }
  }
  if (e->opt.record_path) {
    e->recorder = zb_input_recorder_create(e->a, e->input, e->seed, zb_vfs_package_set_hash(e->vfs));
  }
  return true;
}

#ifdef __EMSCRIPTEN__
/* ---- Web: asset fetching and state for test drivers ----------------------------- */

typedef struct web_fetch {
  zb_engine *e;
  zb_vfs_req *req;
  char path[ZB_VFS_MAX_PATH * 2];
} web_fetch;

static void web_fetch_done(void *user, zb_result result) {
  web_fetch *wf = user;
  char *data = NULL;
  size_t len = 0;
  if (result == ZB_OK) {
    const zb_fs_api *fs = zb_platform_fs(wf->e->platform);
    result = fs->read(fs->ctx, wf->path, zb_vfs_allocator(wf->e->vfs), &data, &len);
  }
  zb_vfs_req_complete(wf->req, result, data, len);
  zb_free(wf->e->a, wf);
}

static void web_fetch_start(void *ctx, zb_vfs_req *req, const char *real) {
  zb_engine *e = ctx;
  web_fetch *wf = zb_alloc(e->a, sizeof(*wf));
  if (!wf) {
    zb_vfs_req_complete(req, ZB_ERR_NOMEM, NULL, 0);
    return;
  }
  wf->e = e;
  wf->req = req;
  snprintf(wf->path, sizeof wf->path, "%s", real);
  zb_result r = zb_platform_fetch(e->platform, real, web_fetch_done, wf);
  if (r != ZB_OK) {
    zb_free(e->a, wf);
    zb_vfs_req_complete(req, r, NULL, 0);
  }
}

static void web_state(void *user, char *out, size_t len) {
  zb_engine *e = user;
  bool ready = e->script != NULL && e->scripts_ready;
  snprintf(out, len,
           "{\"ticks\":%llu,\"frames\":%llu,\"input_mode\":\"%s\",\"audio_locked\":%s,"
           "\"audio_device\":%s,\"voices\":%zu,\"script_errors\":%zu,\"drawable\":[%d,%d],\"quit\":%s,"
           "\"xr\":{\"available\":%s,\"active\":%s,\"frames\":%llu}}",
           (unsigned long long)e->ticks, (unsigned long long)(e->render ? zb_render_get_stats(e->render)->frames : 0),
           e->input && zb_input_get_mode(e->input) == ZB_INPUT_TOUCH ? "touch" : "desktop",
           e->audio && zb_audio_is_locked(e->audio) ? "true" : "false",
           e->audio && zb_audio_has_device(e->audio) ? "true" : "false",
           e->audio ? zb_audio_active_voices(e->audio) : (size_t)0,
           ready ? zb_script_error_count(e->script) : (size_t)0, e->drawable_w, e->drawable_h,
           e->quit ? "true" : "false", e->xr.state.available ? "true" : "false", e->xr.state.active ? "true" : "false",
           (unsigned long long)e->xr.state.frames);
}
#endif

static void fail(zb_engine *e, const char *message) { zb_platform_show_error(e->platform, "Zauberer", message); }

/* ---- Creation --------------------------------------------------------------------- */

zb_host_status zb_engine_create(int argc, char **argv, zb_engine **out) {
  *out = NULL;
  zb_options opt;
  char err[1600];
  if (zb_options_parse(&opt, argc, argv, err, sizeof err) != ZB_OK) {
    fprintf(stderr, "zauberer: %s\n", err);
    return ZB_HOST_FAILURE;
  }
  if (opt.help || opt.version) {
    fputs(opt.help ? zb_options_usage() : "zauberer " ZB_VERSION " (scripting API " ZB_API_VERSION ")\n", stdout);
    return ZB_HOST_SUCCESS;
  }
  const zb_allocator *a = zb_heap_allocator();
  zb_engine *e = zb_calloc(a, sizeof(*e));
  if (!e) {
    return ZB_HOST_FAILURE;
  }
  *out = e;
  e->a = a;
  zb_array_init(&e->expected_hashes, a, sizeof(zb_tick_hash));
  zb_array_init(&e->recorded_hashes, a, sizeof(zb_tick_hash));
  e->opt = opt;
  e->mode = opt.test_path ? ZB_MODE_TEST : (opt.scenario_path ? ZB_MODE_SCENARIO : ZB_MODE_GAME);
  e->headless = opt.headless || e->mode == ZB_MODE_TEST;
  e->seed = opt.seed_set ? opt.seed : 0x5a617562u;
  zb_loop_init(&e->loop);
  e->platform = zb_platform_create(a);
  if (!e->platform) {
    return ZB_HOST_FAILURE;
  }
  const zb_fs_api *fs = zb_platform_fs(e->platform);

  /* Content folder next to the binary (or --assets). */
  if (opt.assets_dir) {
    snprintf(e->assets_dir, sizeof e->assets_dir, "%s", opt.assets_dir);
  } else {
    snprintf(e->assets_dir, sizeof e->assets_dir, "%sassets", zb_platform_base_path(e->platform));
  }
  zb_fs_stat st;
  if (fs->stat(fs->ctx, e->assets_dir, &st) != ZB_OK || !st.is_dir) {
    snprintf(err, sizeof err,
             "The game content was not found.\n\nExpected the 'assets' folder at:\n%s\n\n"
             "Reinstall the game or start it with --assets <folder>.",
             e->assets_dir);
    fail(e, err);
    return ZB_HOST_FAILURE;
  }

  /* User data directory. */
  if (opt.user_dir) {
    snprintf(e->user_dir, sizeof e->user_dir, "%s", opt.user_dir);
    fs->mkdirs(fs->ctx, e->user_dir);
  } else if (zb_platform_user_dir(e->platform, e->user_dir, sizeof e->user_dir) != ZB_OK) {
    snprintf(e->user_dir, sizeof e->user_dir, "./user/");
  }
  ensure_slash(e->user_dir, sizeof e->user_dir);

  /* Jobs, packages. */
  unsigned cpus = zb_platform_cpu_count(e->platform);
  unsigned workers = cpus > 1 ? cpus - 1 : 1;
#ifdef __EMSCRIPTEN__
  workers = workers > 3 ? 3 : workers; /* the page's pthread pool holds 4 workers */
#endif
  e->jobs = zb_jobs_create(a, zb_platform_threads(e->platform), workers);
  if (!e->jobs) {
    zb_warn("worker threads could not be started; loading runs on the main thread");
    e->jobs = zb_jobs_create(a, NULL, 0);
  }
  e->vfs = zb_vfs_create(a, fs, e->jobs);
  if (!e->jobs || !e->vfs) {
    return ZB_HOST_FAILURE;
  }
#ifdef __EMSCRIPTEN__
  if (zb_platform_has_fetch(e->platform)) {
    zb_vfs_fetcher fetcher = {e, web_fetch_start};
    zb_vfs_set_fetcher(e->vfs, &fetcher);
  }
  zb_platform_set_state_provider(e->platform, web_state, e);
#endif
  char root[1100];
  zb_vfs_add_root(e->vfs, e->assets_dir);
  snprintf(root, sizeof root, "%smods", zb_platform_base_path(e->platform));
  zb_vfs_add_root(e->vfs, root);
  snprintf(root, sizeof root, "%smods", e->user_dir);
  zb_vfs_add_root(e->vfs, root);
  if (zb_vfs_mount(e->vfs, ZB_API_VERSION_MAJOR, ZB_API_VERSION_MINOR) != ZB_OK || zb_vfs_load_count(e->vfs) == 0) {
    snprintf(err, sizeof err, "No usable content package was found in:\n%s", e->assets_dir);
    fail(e, err);
    return ZB_HOST_FAILURE;
  }

  /* Window and renderer. */
  zb_render_backend *backend = NULL;
  if (!e->headless) {
    zb_window_desc desc = {"Zauberer", 1280, 720, false};
    const char *size = getenv("ZB_WINDOW_SIZE");
    if (size) {
      sscanf(size, "%dx%d", &desc.width, &desc.height);
    }
    if (zb_platform_open_window(e->platform, &desc, err, sizeof err) != ZB_OK) {
      fail(e, err);
      return ZB_HOST_FAILURE;
    }
    zb_platform_drawable_size(e->platform, &e->drawable_w, &e->drawable_h);
    backend =
        zb_render_gl_backend(a, gl_loader, e->platform, zb_platform_is_gles(e->platform), e->vfs, err, sizeof err);
    if (!backend) {
      fail(e, err);
      return ZB_HOST_FAILURE;
    }
  } else {
    backend = zb_render_null_backend(a);
    e->drawable_w = 1280;
    e->drawable_h = 720;
  }
  e->render = zb_render_create(a, e->vfs, backend);
  if (!e->render) {
    return ZB_HOST_FAILURE;
  }
  zb_render_set_view_count(e->render, opt.debug_views);

  /* Audio. */
  e->audio = zb_audio_create(a, e->vfs, zb_platform_threads(e->platform));
#ifdef __EMSCRIPTEN__
  zb_audio_set_locked(e->audio, !e->headless);
#else
  if (!e->headless) {
    int rate = 0;
    zb_audio_set_device(e->audio, zb_platform_audio_open(e->platform, audio_fill, e->audio, &rate) == ZB_OK);
  }
#endif

  /* Input. */
  e->input = zb_input_create(a);
  if (!e->audio || !e->input) {
    return ZB_HOST_FAILURE;
  }
  if (zb_platform_touch_primary(e->platform)) {
    zb_input_set_mode(e->input, ZB_INPUT_TOUCH);
  }
  load_input_defaults(e);
  load_user_config(e);
  if (!setup_replay(e)) {
    return ZB_HOST_FAILURE;
  }

  /* Plants: the species registry against the sprite atlas (none if missing). */
  char *species = NULL, *atlas = NULL;
  size_t species_len = 0, atlas_len = 0;
  if (zb_vfs_read(e->vfs, "data/flora.lua", a, &species, &species_len) == ZB_OK &&
      zb_vfs_read(e->vfs, "sprites/flora.lua", a, &atlas, &atlas_len) == ZB_OK &&
      zb_flora_parse(&e->flora, a, species, species_len, atlas, atlas_len, err, sizeof err) != ZB_OK) {
    zb_warn("no plants: %s", err);
  }
  zb_free(a, species);
  zb_free(a, atlas);

  load_materials(e);

  /* World: planet, terrain and chunk streaming around the scripts' focus points. */
  e->world = make_world(e, NULL, err, sizeof err);
  if (!e->world) {
    fail(e, err);
    return ZB_HOST_FAILURE;
  }
  e->terrain_view = zb_terrain_view_create(a, e->render, e->vfs, "textures/terrain/atlas.lua", NULL);
  if (!e->terrain_view) {
    return ZB_HOST_FAILURE;
  }

  /* Co-op session: the same world (seed) and package set on every peer. */
  zb_net_config nc;
  memset(&nc, 0, sizeof nc);
  nc.a = a;
  nc.world_seed = e->seed;
  nc.package_hash = zb_vfs_package_set_hash(e->vfs);
  nc.clock_ns = world_clock;
  nc.clock_ctx = e->platform;
  e->net = zb_net_create(&nc);
  if (!e->net) {
    return ZB_HOST_FAILURE;
  }
  if (opt.host_port >= 0 || opt.join_url) {
    uint16_t bound = 0;
    zb_transport *t = opt.join_url ? zb_ws_client_create(a, opt.join_url, err, sizeof err)
                                   : zb_ws_server_create(a, (uint16_t)opt.host_port, &bound, err, sizeof err);
    if (!t) {
      fail(e, err);
      return ZB_HOST_FAILURE;
    }
    if (opt.join_url) {
      zb_net_join(e->net, t);
      zb_info("joining %s", opt.join_url);
    } else {
      zb_net_host(e->net, t);
      zb_info("hosting a co-op session on port %u", (unsigned)bound);
    }
  }

  /* Scripts. */
  zb_script_config sc;
  memset(&sc, 0, sizeof sc);
  sc.a = a;
  sc.vfs = e->vfs;
  sc.input = e->input;
  sc.render = e->render;
  sc.audio = e->audio;
  sc.fs = fs;
  sc.user_dir = e->user_dir;
  sc.host.ctx = e;
  sc.host.quit = host_quit;
  sc.host.get_info = host_info;
  sc.host.sim_time = host_time;
  sc.host.tick_count = host_ticks;
  sc.host.set_paused = host_pause;
  sc.host.set_time_scale = host_time_scale;
  sc.host.set_fullscreen = host_fullscreen;
  sc.host.save_bindings = host_save_bindings;
  sc.host.set_window_size = host_window_size;
  sc.host.sleep_ms = host_sleep;
  sc.host.user_data_changed = host_user_data_changed;
  sc.host.use_bundle = host_use_bundle;
  zb_engine_xr_attach(e, &sc.host);
  sc.seed = e->seed;
  sc.world = e->world;
  sc.materials = e->materials;
  sc.net = e->net;
  sc.jobs = e->jobs;
  sc.test_api = e->mode != ZB_MODE_GAME;
#if defined(ZB_DEV)
  sc.dev = true;
#endif
#if defined(ZB_DEV) || !defined(NDEBUG)
  sc.instruction_budget = ZB_SCRIPT_DEFAULT_BUDGET;
#endif
  sc.coverage_path = opt.lua_coverage_path;
  e->script = zb_script_create(&sc, err, sizeof err);
  if (!e->script) {
    fail(e, err);
    return ZB_HOST_FAILURE;
  }
  zb_script_run_packages(e->script);
  e->scripts_ready = true;
  zb_script_init_hooks(e->script);
  if (zb_input_take_mode_change(e->input)) {
    zb_script_event(e->script, "input_mode", 0, 0, zb_input_get_mode(e->input) == ZB_INPUT_TOUCH ? "touch" : "desktop");
  }
  e->overlay_font = zb_render_load_font(e->render, "fonts/pixel.lua");

  if (e->mode == ZB_MODE_TEST) {
    int failures = zb_script_run_tests(e->script, opt.test_path, opt.junit_path);
    e->quit = true;
    e->exit_code = failures == 0 ? 0 : 1;
  } else if (e->mode == ZB_MODE_SCENARIO) {
    char golden[1100];
    snprintf(golden, sizeof golden, "%s", opt.scenario_path);
    char *slash = strrchr(golden, '/');
    if (slash) {
      snprintf(slash, sizeof golden - (size_t)(slash - golden), "/../golden");
    } else {
      snprintf(golden, sizeof golden, "../golden");
    }
    const char *outdir = getenv("ZB_GOLDEN_OUT");
    zb_script_set_golden_dirs(e->script, golden, outdir ? outdir : "golden-out");
    if (zb_script_scenario_start(e->script, opt.scenario_path) != ZB_OK) {
      return ZB_HOST_FAILURE;
    }
  }
  return ZB_HOST_CONTINUE;
}

/* ---- Running ------------------------------------------------------------------------ */

static void tick(zb_engine *e) {
  zb_vfs_poll(e->vfs);
  if (e->replay) {
    if (!zb_input_replay_next(e->replay, e->input)) {
      zb_info("replay finished after %llu ticks", (unsigned long long)e->ticks);
      e->quit = true;
      return;
    }
  } else {
    zb_input_sample(e->input, zb_platform_now_ns(e->platform));
  }
  if (e->recorder) {
    zb_input_recorder_add(e->recorder, e->input);
  }
  zb_audio_tick(e->audio, zb_loop_tick_seconds());
  e->ticks++;
  zb_world_advance_time(e->world, zb_loop_tick_seconds());
  zb_net_update(e->net);
  zb_script_tick(e->script, zb_loop_tick_seconds());
  check_state_hash(e);
}

static void frame(zb_engine *e, double alpha);

/* One step of the headless and scenario modes: a tick unless paused, then a frame. */
static void step(zb_engine *e) {
  e->steps++;
  if (!e->loop.paused) {
    tick(e);
  } else {
    zb_vfs_poll(e->vfs);
  }
  frame(e, 0.0);
}

/* Day and night from the world's sky over the camera, when the scripts ask for it. */
static void apply_daylight(zb_engine *e, zb_vec3 eye) {
  zb_daylight d;
  zb_daylight_compute(zb_world_calendar(e->world), zb_world_time(e->world), zb_world_origin(e->world), eye, &d);
  zb_atmosphere a = d.atmosphere;
  const zb_atmosphere *cur = zb_render_atmosphere(e->render);
  a.fog_density = cur->fog_density; /* the scripts' visibility stays */
  zb_render_set_atmosphere(e->render, &a);
  zb_render_set_light(e->render, d.light_dir, d.light_color, d.ambient);
  zb_terrain_view_set_season(e->terrain_view, (float)d.sky.season);
}

/* Terrain and the sky's horizon, for the camera the scripts set. */
static void draw_world(zb_engine *e) {
  zb_vec3 eye = zb_render_camera_eye(e->render);
  if (zb_render_daylight(e->render)) {
    apply_daylight(e, eye);
  }
  if (!e->headless) { /* headless runs draw nothing: no meshes to build */
    zb_terrain_view_draw(e->terrain_view, e->world, eye);
  }
  const zb_atmosphere *cur = zb_render_atmosphere(e->render);
  if (!cur->enabled) {
    return;
  }
  zb_atmosphere a = *cur;
  const zb_origin *o = zb_world_origin(e->world);
  zb_dvec3 c = zb_origin_to_cart(o, eye);
  double len = sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
  zb_dvec3 d = {c.x / len, c.y / len, c.z / len};
  a.up = zb_v3((float)(d.x * o->east.x + d.y * o->east.y + d.z * o->east.z),
               (float)(d.x * o->up.x + d.y * o->up.y + d.z * o->up.z),
               (float)-(d.x * o->north.x + d.y * o->north.y + d.z * o->north.z));
  a.horizon_dip = zb_horizon_dip(zb_world_planet(e->world)->radius, len - zb_world_planet(e->world)->radius);
  zb_render_set_atmosphere(e->render, &a);
}

static void frame(zb_engine *e, double alpha) {
  zb_world_update(e->world);
  zb_render_begin(e->render, e->drawable_w, e->drawable_h);
  /* Game time animates the water, so captures of a tick are reproducible. */
  zb_render_set_time(e->render, ((double)e->ticks + alpha) / (double)ZB_TICK_HZ);
  zb_script_frame(e->script, alpha);
  draw_world(e);
#ifdef ZB_DEV
  /* On-screen script errors in development builds. */
  size_t n = zb_script_error_count(e->script);
  static const float red[4] = {1.0f, 0.3f, 0.3f, 1.0f};
  for (size_t i = 0; i < n && i < 4; ++i) {
    zb_render_draw_text(e->render, e->overlay_font, zb_script_error(e->script, n - 1 - i), 4.0f,
                        4.0f + (float)i * 10.0f, 1.0f, red);
  }
#endif
  zb_render_end(e->render);
}

static zb_host_status finish(zb_engine *e) {
  if (e->script) {
    char hex[17];
    zb_hash_hex(zb_script_state_hash(e->script), hex);
    zb_info("state hash at tick %llu: %s", (unsigned long long)e->ticks, hex);
  }
  if (e->replay_diverged) {
    return ZB_HOST_FAILURE;
  }
  if (e->mode == ZB_MODE_SCENARIO) {
    int failures = zb_script_scenario_failures(e->script);
    if (failures) {
      zb_error("scenario '%s': %d failure(s)", zb_script_scenario_name(e->script), failures);
      return ZB_HOST_FAILURE;
    }
    zb_info("scenario '%s': passed after %llu ticks", zb_script_scenario_name(e->script), (unsigned long long)e->ticks);
  }
  return e->exit_code == 0 ? ZB_HOST_SUCCESS : ZB_HOST_FAILURE;
}

static void run_scenario(zb_engine *e) {
  /* Windowed scenarios run one step per iteration so window events arrive in between. */
  unsigned budget = e->headless ? HEADLESS_BATCH : 1u;
  while (budget-- > 0 && !e->quit) {
    if (e->scenario_pending == 0) {
      uint64_t ticks = 0;
      if (!zb_script_scenario_resume(e->script, &ticks)) {
        e->quit = true;
        return;
      }
      e->scenario_pending = ticks;
      if (ticks == 0) {
        zb_vfs_poll(e->vfs);
        frame(e, 0.0);
        continue;
      }
    }
    step(e);
    e->scenario_pending--;
  }
}

zb_host_status zb_engine_iterate(zb_engine *e) {
  if (e->quit) {
    return finish(e);
  }
  if (zb_engine_xr_takes_over(e)) {
    return e->quit ? finish(e) : ZB_HOST_CONTINUE; /* a running XR session's frames drive the step */
  }
  uint64_t now = zb_platform_now_ns(e->platform);
  zb_script_poll_reload(e->script, now);
  if (e->mode == ZB_MODE_SCENARIO) {
    run_scenario(e);
  } else if (e->headless) {
    for (unsigned i = 0; i < HEADLESS_BATCH && !e->quit; ++i) {
      if (e->opt.ticks >= 0 && e->steps >= (uint64_t)e->opt.ticks) {
        e->quit = true;
        break;
      }
      step(e);
    }
  } else {
    unsigned n = zb_loop_advance(&e->loop, now);
    for (unsigned i = 0; i < n && !e->quit; ++i) {
      if (e->opt.ticks >= 0 && e->ticks >= (uint64_t)e->opt.ticks) {
        e->quit = true;
        break;
      }
      tick(e);
    }
    zb_vfs_poll(e->vfs);
    frame(e, zb_loop_alpha(&e->loop));
  }
  if (!e->headless && !e->xr.in_frame) {
    zb_platform_swap(e->platform);
    if (!e->first_frame_shown) {
      e->first_frame_shown = true;
      zb_info("first frame presented (%dx%d)", e->drawable_w, e->drawable_h);
    }
  }
  /* Inside an XR frame the session ends first; the window loop finishes. */
  return e->quit && !e->xr.in_frame ? finish(e) : ZB_HOST_CONTINUE;
}

zb_host_status zb_engine_event(zb_engine *e, const zb_event *ev) {
  switch (ev->type) {
    case ZB_EV_QUIT: e->quit = true; return finish(e);
    case ZB_EV_RESIZE:
      e->drawable_w = ev->u.resize.width;
      e->drawable_h = ev->u.resize.height;
      zb_script_event(e->script, "resize", e->drawable_w, e->drawable_h, NULL);
      break;
    case ZB_EV_AUDIO_UNLOCKED:
      if (zb_audio_is_locked(e->audio)) {
        int rate = 0;
        zb_audio_set_device(e->audio, zb_platform_audio_open(e->platform, audio_fill, e->audio, &rate) == ZB_OK);
        zb_audio_set_locked(e->audio, false);
        zb_script_event(e->script, "audio_unlocked", 0, 0, NULL);
      }
      break;
    default:
      if (!e->replay) {
        zb_input_handle_event(e->input, ev);
      }
      break;
  }
  if (zb_input_take_mode_change(e->input)) {
    zb_script_event(e->script, "input_mode", 0, 0, zb_input_get_mode(e->input) == ZB_INPUT_TOUCH ? "touch" : "desktop");
  }
  return ZB_HOST_CONTINUE;
}

void zb_engine_destroy(zb_engine *e) {
  if (!e) {
    return;
  }
  if (e->script && e->scripts_ready) {
    zb_script_shutdown(e->script);
  }
  if (e->recorder && e->platform) {
    if (e->script && e->ticks % ZB_HASH_INTERVAL != 0) {
      zb_tick_hash th = {e->ticks, zb_script_state_hash(e->script)};
      zb_array_push(&e->recorded_hashes, &th);
    }
    write_recorded_hashes(e);
    size_t len = 0;
    const unsigned char *bytes = zb_input_recorder_bytes(e->recorder, &len);
    const zb_fs_api *fs = zb_platform_fs(e->platform);
    if (!bytes || fs->write(fs->ctx, e->opt.record_path, bytes, len) != ZB_OK) {
      zb_error("could not write the input recording to %s", e->opt.record_path);
    }
  }
  zb_script_destroy(e->script);
  zb_terrain_view_destroy(e->terrain_view);
  zb_net_destroy(e->net);
  zb_world_destroy(e->world);
  if (e->materials) {
    zb_materials_free(e->materials);
    zb_free(e->a, e->materials);
  }
  if (e->bundle) {
    zb_world_bundle_free(e->bundle);
    zb_free(e->a, e->bundle);
  }
  zb_input_recorder_destroy(e->recorder);
  zb_input_replay_close(e->replay);
  zb_free(e->a, e->replay_data);
  zb_array_free(&e->expected_hashes);
  zb_array_free(&e->recorded_hashes);
  if (e->platform) {
    zb_platform_audio_close(e->platform);
  }
  zb_audio_destroy(e->audio);
  zb_render_destroy(e->render);
  zb_input_destroy(e->input);
  zb_vfs_destroy(e->vfs);
  zb_jobs_destroy(e->jobs);
  if (e->platform) {
    zb_platform_user_data_sync(e->platform);
  }
  zb_platform_destroy(e->platform);
  zb_free(e->a, e);
}

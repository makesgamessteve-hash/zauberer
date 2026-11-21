/* SPDX-License-Identifier: MIT */
/* The Lua 5.4 host: package entry scripts, lifecycle hooks, sandbox, error
 * isolation, deterministic randomness, hot reload, instruction budget, line
 * coverage, and the Lua test and scenario runners. */
#ifndef ZB_SCRIPT_SCRIPT_H
#define ZB_SCRIPT_SCRIPT_H

#include "audio/audio.h"
#include "core/fs.h"
#include "core/xr.h"
#include "input/input.h"
#include "net/session.h"
#include "render/render.h"
#include "vfs/vfs.h"
#include "world/world.h"

typedef struct zb_script zb_script;

typedef struct zb_script_info {
  const char *platform;
  const char *build; /* "development" or "release" */
  const char *input_mode;
  int drawable_w, drawable_h;
  uint64_t seed;
  const char *language; /* the player's preferred language ("en", "de") */
  bool play;            /* a player's game (not a test, a scenario or a replay) */
} zb_script_info;

/* Application-level services the scripts can use. */
typedef struct zb_script_host {
  void *ctx;
  void (*quit)(void *ctx, int code);
  void (*get_info)(void *ctx, zb_script_info *out);
  double (*sim_time)(void *ctx);
  uint64_t (*tick_count)(void *ctx);
  void (*set_paused)(void *ctx, bool paused);
  /* Holds the world still (no game time, no simulation) while input and scripts go on. */
  void (*hold_world)(void *ctx, bool held);
  void (*set_time_scale)(void *ctx, double scale);
  void (*set_fullscreen)(void *ctx, bool on);
  /* Persists the user's input bindings. */
  zb_result (*save_bindings)(void *ctx);
  /* Turns a package on or off from the next start; false with a message if it cannot be. */
  bool (*set_package)(void *ctx, const char *id, bool on, char *err, size_t err_len);
  /* How bright flashes of light (lightning) are, 0 .. 1. */
  void (*set_flash)(void *ctx, float strength);
  /* Save data was written (the web build persists it to IndexedDB). */
  void (*user_data_changed)(void *ctx);
  /* Test API: resizes the window (no-op without one). */
  void (*set_window_size)(void *ctx, int w, int h);
  /* Test API: waits in real time (two-process tests). */
  void (*sleep_ms)(void *ctx, unsigned ms);
  /* The WebXR session as scripts see it (NULL: never a session). */
  void (*xr_state)(void *ctx, zb_xr_state *out);
  /* Test API: starts or moves an emulated headset with the head pose; NULL ends it. */
  void (*xr_emulate)(void *ctx, const zb_xr_pose *head);
  /* Test API: a controller button event (zb_xr_hand, zb_xr_button). */
  void (*xr_button)(void *ctx, int hand, int button, bool down);
  /* Test API: the emulated headset's controller of a hand (ZB_XR_HAND_LEFT or ZB_XR_HAND_RIGHT). */
  void (*xr_controller)(void *ctx, int hand, const zb_xr_controller *c);
  /* Plays in a created world from now on, taking ownership of the bundle;
   * false with a message if the world cannot be made (the bundle stays the
   * caller's). NULL: scripts keep a private world. */
  bool (*use_bundle)(void *ctx, struct zb_world_bundle *bundle, char *err, size_t err_len);
  /* Starts the game over (as at its first start) after this tick: saved games
   * are deleted and the scripts made anew; false with a reason if it cannot be. */
  bool (*reset)(void *ctx, char *err, size_t err_len);
} zb_script_host;

typedef struct zb_script_config {
  const zb_allocator *a;
  zb_vfs *vfs;
  zb_input *input;
  zb_render *render;
  zb_audio *audio;
  const zb_fs_api *fs;  /* real file system: user data, tests */
  const char *user_dir; /* with trailing '/' */
  zb_script_host host;
  uint64_t seed;
  bool dev;                             /* hot reload, on-screen errors */
  bool test_api;                        /* test and scenario helpers, input injection */
  uint64_t instruction_budget;          /* per hook invocation; 0 = unlimited */
  const char *coverage_path;            /* LuaCov-compatible stats file, or NULL */
  zb_world *world;                      /* NULL: a private world (inline generation) */
  const struct zb_materials *materials; /* the material registry; NULL: none */
  struct zb_fire *fire;                 /* heat and fire; NULL: none */
  struct zb_surface_water *water;       /* surface water; NULL: none */
  struct zb_air *air;                   /* wind and gases; NULL: none */
  struct zb_weather_control *weather;   /* the weather in use; NULL: none */
  struct zb_tier *tier;                 /* the active tier the three above belong to; NULL: none */
  zb_net *net;                          /* co-op session; NULL: networking unavailable */
  zb_jobs *jobs;                        /* world creation runs here; NULL: on the calling thread */
  double planet_radius;                 /* private world only; 0 = ZB_PLANET_DEFAULT_RADIUS */
} zb_script_config;

#define ZB_SCRIPT_DEFAULT_BUDGET 10000000u

/* Fails when the private world's planet radius is out of range. */
zb_script *zb_script_create(const zb_script_config *cfg, char *err, size_t err_len);
/* Writes coverage (if enabled) and closes the Lua state. */
void zb_script_destroy(zb_script *s);
/* The engine's world was replaced (a created world): scripts use the new one. */
void zb_script_set_world(zb_script *s, zb_world *world);

/* Runs scripts/main.lua of every enabled package in load order. Errors are
 * isolated per package; returns the number of packages whose script failed. */
size_t zb_script_run_packages(zb_script *s);

/* Hook dispatch (in package load order; shutdown in reverse). */
void zb_script_init_hooks(zb_script *s);
void zb_script_tick(zb_script *s, double dt);
void zb_script_frame(zb_script *s, double alpha);
void zb_script_shutdown(zb_script *s);
/* Canonical simulation state hash: tick, input snapshot, random-number
 * counter and the values returned by the packages' "hash" hooks. */
uint64_t zb_script_state_hash(zb_script *s);
/* Named events: "resize" (w, h), "input_mode" (mode), "audio_unlocked",
 * "xr_session" ("started" or "ended"). */
void zb_script_event(zb_script *s, const char *name, int a, int b, const char *text);

/* Development: checks script files for changes (at most every 250 ms) and
 * reloads changed packages; returns the number of packages reloaded. */
size_t zb_script_poll_reload(zb_script *s, uint64_t now_ns);

/* Errors reported so far (for the on-screen overlay). */
size_t zb_script_error_count(const zb_script *s);
const char *zb_script_error(const zb_script *s, size_t i);

/* Runs Lua test files (a file or a directory of *_test.lua / test_*.lua);
 * writes JUnit XML when junit_path is set. Returns the number of failures
 * (or -1 when nothing could be run). */
int zb_script_run_tests(zb_script *s, const char *path, const char *junit_path);

/* Scenario: loads the file as a coroutine. Each resume returns the number of
 * ticks to run before the next resume; false when the scenario finished. */
zb_result zb_script_scenario_start(zb_script *s, const char *path);
bool zb_script_scenario_resume(zb_script *s, uint64_t *ticks);
/* Number of failed expectations (or errors) of the scenario. */
int zb_script_scenario_failures(const zb_script *s);
const char *zb_script_scenario_name(const zb_script *s);
/* Sets the directory for golden images and their output (scenario runs). */
void zb_script_set_golden_dirs(zb_script *s, const char *golden_dir, const char *out_dir);

/* Coverage: flushes collected line counts into the stats file. */
zb_result zb_script_write_coverage(zb_script *s);

#endif

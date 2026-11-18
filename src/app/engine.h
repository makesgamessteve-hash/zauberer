/* SPDX-License-Identifier: MIT */
/* The engine instance: composition root that wires the modules together and
 * runs the game, headless, test and scenario modes. */
#ifndef ZB_APP_ENGINE_H
#define ZB_APP_ENGINE_H

#include "app/terrain_view.h"
#include "app/xr.h"
#include "audio/audio.h"
#include "core/array.h"
#include "core/cli.h"
#include "core/jobs.h"
#include "core/loop.h"
#include "input/input.h"
#include "platform/host.h"
#include "platform/platform.h"
#include "render/render.h"
#include "script/script.h"
#include "vfs/vfs.h"

typedef enum zb_engine_mode { ZB_MODE_GAME, ZB_MODE_TEST, ZB_MODE_SCENARIO } zb_engine_mode;

typedef struct zb_engine {
  zb_options opt;
  zb_engine_mode mode;
  const zb_allocator *a;
  zb_platform *platform;
  zb_jobs *jobs;
  zb_vfs *vfs;
  zb_input *input;
  zb_render *render;
  zb_audio *audio;
  zb_script *script;
  zb_world *world;
  struct zb_world_bundle *bundle; /* the created world being played (owned), or NULL */
  zb_terrain_view *terrain_view;
  zb_net *net;
  zb_loop loop;
  bool headless;
  bool quit;
  int exit_code;
  uint64_t ticks;            /* simulation ticks run */
  uint64_t scenario_pending; /* ticks still to run before resuming */
  uint64_t seed;
  int drawable_w, drawable_h;
  char assets_dir[1024];
  char user_dir[1024];
  zb_input_recorder *recorder;
  zb_input_replay *replay;
  char *replay_data;
  uint32_t overlay_font;
  bool scripts_ready;
  bool first_frame_shown;
  uint64_t steps; /* ticks plus paused steps (headless and scenario modes) */
  /* Replay state hashes: expected (from <replay>.hashes) and recorded. */
  zb_array expected_hashes; /* zb_tick_hash */
  size_t next_expected;
  bool replay_diverged;
  zb_array recorded_hashes; /* zb_tick_hash */
  zb_engine_xr xr;          /* WebXR session (app/xr.c) */
} zb_engine;

typedef struct zb_tick_hash {
  uint64_t tick;
  uint64_t hash;
} zb_tick_hash;

/* Interval of the state hashes written next to an input recording. */
#define ZB_HASH_INTERVAL 60u

/* Creates the engine from command-line arguments. On failure the returned
 * status is ZB_HOST_FAILURE (errors already reported); --help and --version
 * return ZB_HOST_SUCCESS with *out == NULL. */
zb_host_status zb_engine_create(int argc, char **argv, zb_engine **out);
zb_host_status zb_engine_iterate(zb_engine *e);
zb_host_status zb_engine_event(zb_engine *e, const zb_event *ev);
void zb_engine_destroy(zb_engine *e);

#endif

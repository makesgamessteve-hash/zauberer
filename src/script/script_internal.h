/* SPDX-License-Identifier: MIT */
/* Shared internals of the script module (not part of its public API). */
#ifndef ZB_SCRIPT_INTERNAL_H
#define ZB_SCRIPT_INTERNAL_H

#include "core/array.h"
#include "core/hash.h"
#include "script/script.h"
#include "world/world.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#define ZB_SCRIPT_MAX_ERRORS 16
#define ZB_SCRIPT_REGISTRY_HOOKS "zb.hooks"
#define ZB_SCRIPT_REGISTRY_PRESERVE "zb.preserve"
#define ZB_SCRIPT_REGISTRY_LOADED "zb.loaded"
#define ZB_SCRIPT_REGISTRY_ENGINE "zb.engine"

typedef struct watched_file {
  size_t pkg;
  char *real;
  uint64_t mtime;
} watched_file;

typedef struct cov_file {
  char *name;
  uint32_t *hits;
  size_t max;
} cov_file;

struct zb_script {
  zb_script_config cfg;
  lua_State *L;
  zb_rng rng;
  zb_world *world; /* the engine's, or a private one (own_world) */
  bool own_world;
  /* World creation (script_create.c). */
  struct zb_world_creation *creation;
  zb_job *creation_job; /* pieces of creation running on a worker */
  int creation_pieces;
  int creation_frames;                    /* frames complete when the last job finished */
  double creation_fraction, creation_myr; /* progress then, shown while the next job works */
  int creation_stage, creation_attempt;
  struct zb_world_bundle *bundle; /* the private world's, when scripts made or loaded one without a host */
  uint32_t globe_mesh, globe_index_count;
  int globe_frame;
  int32_t *globe_nearest;
  const void *globe_source;
  size_t pkg_count;
  int *env_refs;
  bool *pkg_failed;
  int current_pkg;
  uint64_t budget_count;
  bool budget_active;
  char errors[ZB_SCRIPT_MAX_ERRORS][512];
  size_t error_count;
  zb_array watched; /* watched_file */
  uint64_t last_poll_ns;
  bool coverage;
  zb_array cov_files; /* cov_file */
  zb_map cov_index;   /* hash(name) -> index */
  /* scenario */
  lua_State *co;
  int co_ref;
  int scenario_failures;
  bool scenario_started;
  char scenario_name[128];
  char golden_dir[512];
  char out_dir[512];
  char scenario_dir[512];
};

/* Records an error message for the log and the overlay. */
void zb_script_report(zb_script *s, const char *pkg, const char *msg);
/* pcall with a traceback message handler; reports and returns false on error. */
bool zb_script_pcall(zb_script *s, int nargs, int nresults, const char *pkg, bool budget);
/* The script owning L (stored in the state's extra space). */
zb_script *zb_script_from(lua_State *L);
/* Registers engine, vfs, input, render, audio and save in the global table. */
void zb_script_open_api(zb_script *s);
/* Registers world (planet coordinates). */
void zb_script_open_world(zb_script *s);
/* Registers world creation, saved worlds and the globe (after the world module). */
void zb_script_open_create(zb_script *s);
/* Releases the creation and the globe (before the world goes). */
void zb_script_close_create(zb_script *s);
/* Frees a bundle scripts adopted without a host (after the world goes). */
void zb_script_free_bundle(zb_script *s);
/* A PlanetPos argument (errors name its first bad field). */
zb_planet_pos zb_script_check_pos(lua_State *L, int idx);
/* Registers net (co-op sessions). */
void zb_script_open_net(zb_script *s);
/* The network codec: encodes the value at idx into out (NULL or an error
 * message), and pushes a decoded value (false on malformed data). */
const char *zb_script_net_encode(lua_State *L, int idx, zb_array *out);
bool zb_script_net_decode(lua_State *L, const void *data, size_t len);
/* Registers xr (WebXR session and head tracking; test helpers with the test API). */
void zb_script_open_xr(zb_script *s);
/* Adds the test helpers (test.*) and scenario helpers to the global table. */
void zb_script_open_test_api(zb_script *s);
/* Canonical hash of a Lua value (sorted keys, IEEE bit patterns). */
void zb_script_hash_value(lua_State *L, int idx, zb_hasher *h, int depth);
/* Starts a state hash with the tick, input snapshot and random-number counter. */
void zb_script_hash_base(zb_script *s, zb_hasher *h);
/* Makes a sandboxed environment table (index -> globals) on the stack. */
void zb_script_new_env(zb_script *s, int pkg);

#endif

/* SPDX-License-Identifier: MIT */
#include "script/script_internal.h"

#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUDGET_STEP 1000
#define RELOAD_INTERVAL_NS 250000000ull

static const char *const k_events[] = {"init", "tick", "frame", "event", "shutdown", "reload", "hash"};

zb_script *zb_script_from(lua_State *L) { return *(zb_script **)lua_getextraspace(L); }

/* ---- Allocation and errors -------------------------------------------------- */

static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)osize;
  const zb_allocator *a = ud;
  if (nsize == 0) {
    zb_free(a, ptr);
    return NULL;
  }
  return zb_realloc(a, ptr, nsize);
}

void zb_script_report(zb_script *s, const char *pkg, const char *msg) {
  zb_error("[%s] %s", pkg ? pkg : "script", msg);
  char *slot = s->errors[s->error_count % ZB_SCRIPT_MAX_ERRORS];
  snprintf(slot, sizeof s->errors[0], "[%s] %s", pkg ? pkg : "script", msg);
  char *nl = strchr(slot, '\n');
  if (nl) {
    *nl = '\0';
  }
  s->error_count++;
}

static int msgh(lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (!msg) {
    msg = luaL_tolstring(L, 1, NULL);
  }
  luaL_traceback(L, L, msg, 1);
  return 1;
}

bool zb_script_pcall(zb_script *s, int nargs, int nresults, const char *pkg, bool budget) {
  lua_State *L = s->L;
  int base = lua_gettop(L) - nargs;
  lua_pushcfunction(L, msgh);
  lua_insert(L, base);
  bool prev = s->budget_active;
  if (budget) {
    s->budget_count = 0;
    s->budget_active = true;
  }
  int status = lua_pcall(L, nargs, nresults, base);
  s->budget_active = prev;
  lua_remove(L, base);
  if (status != LUA_OK) {
    zb_script_report(s, pkg, lua_tostring(L, -1));
    lua_pop(L, 1);
    return false;
  }
  return true;
}

/* ---- Debug hook: instruction budget and line coverage ------------------------- */

static cov_file *cov_get(zb_script *s, const char *name) {
  uint64_t key = zb_hash_str(name);
  uint64_t idx;
  if (zb_map_get(&s->cov_index, key, &idx)) {
    return zb_array_at(&s->cov_files, (size_t)idx);
  }
  cov_file f = {zb_strdup(s->cfg.a, name), NULL, 0};
  if (!f.name || !zb_array_push(&s->cov_files, &f)) {
    zb_free(s->cfg.a, f.name);
    return NULL;
  }
  zb_map_put(&s->cov_index, key, s->cov_files.len - 1);
  return zb_array_at(&s->cov_files, s->cov_files.len - 1);
}

static void cov_hit(zb_script *s, const char *name, int line, uint32_t count) {
  cov_file *f = cov_get(s, name);
  if (!f || line <= 0) {
    return;
  }
  if ((size_t)line > f->max) {
    size_t max = (size_t)line + 64;
    uint32_t *hits = zb_realloc(s->cfg.a, f->hits, max * sizeof(uint32_t));
    if (!hits) {
      return;
    }
    memset(hits + f->max, 0, (max - f->max) * sizeof(uint32_t));
    f->hits = hits;
    f->max = max;
  }
  f->hits[line - 1] += count;
}

static void debug_hook(lua_State *L, lua_Debug *ar) {
  zb_script *s = zb_script_from(L);
  if (ar->event == LUA_HOOKLINE) {
    if (lua_getinfo(L, "S", ar) && ar->source[0] == '@') {
      cov_hit(s, ar->source + 1, ar->currentline, 1);
    }
    return;
  }
  if (s->budget_active && s->cfg.instruction_budget) {
    s->budget_count += BUDGET_STEP;
    if (s->budget_count > s->cfg.instruction_budget) {
      s->budget_count = 0;
      char msg[96];
      snprintf(msg, sizeof msg, "instruction budget of %llu exceeded (runaway script?)",
               (unsigned long long)s->cfg.instruction_budget);
      luaL_error(L, "%s", msg);
    }
  }
}

/* ---- Sandbox ------------------------------------------------------------------ */

static int l_print(lua_State *L) {
  int n = lua_gettop(L);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (int i = 1; i <= n; ++i) {
    if (i > 1) {
      luaL_addchar(&b, '\t');
    }
    luaL_tolstring(L, i, NULL);
    luaL_addvalue(&b);
  }
  luaL_pushresult(&b);
  zb_info("[lua] %s", lua_tostring(L, -1));
  return 0;
}

static int l_unavailable(lua_State *L) {
  return luaL_error(L, "%s is unavailable in the Zauberer sandbox", lua_tostring(L, lua_upvalueindex(1)));
}

static int l_load(lua_State *L) {
  /* load(chunk [, chunkname [, mode [, env]]]) with the mode forced to text. */
  size_t len;
  const char *chunk = luaL_checklstring(L, 1, &len);
  const char *name = luaL_optstring(L, 2, "=(load)");
  int status = luaL_loadbufferx(L, chunk, len, name, "t");
  if (status != LUA_OK) {
    lua_pushnil(L);
    lua_insert(L, -2);
    return 2;
  }
  if (!lua_isnoneornil(L, 4)) {
    lua_pushvalue(L, 4);
    if (!lua_setupvalue(L, -2, 1)) {
      lua_pop(L, 1);
    }
  }
  return 1;
}

static int l_random(lua_State *L) {
  zb_script *s = zb_script_from(L);
  int n = lua_gettop(L);
  if (n == 0) {
    lua_pushnumber(L, zb_rng_next_double(&s->rng));
    return 1;
  }
  lua_Integer lo = 1, hi;
  if (n == 1) {
    hi = luaL_checkinteger(L, 1);
  } else {
    lo = luaL_checkinteger(L, 1);
    hi = luaL_checkinteger(L, 2);
  }
  luaL_argcheck(L, lo <= hi, n, "interval is empty");
  lua_pushinteger(L, (lua_Integer)zb_rng_range(&s->rng, lo, hi));
  return 1;
}

static int l_randomseed(lua_State *L) {
  zb_script *s = zb_script_from(L);
  lua_Integer seed = luaL_optinteger(L, 1, (lua_Integer)s->cfg.seed);
  s->rng = zb_rng_make((uint64_t)seed, 0x72616e64);
  return 0;
}

static void block(lua_State *L, int table, const char *field, const char *what) {
  lua_pushstring(L, what);
  lua_pushcclosure(L, l_unavailable, 1);
  lua_setfield(L, table, field);
}

static void open_sandbox(zb_script *s) {
  lua_State *L = s->L;
  luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
  lua_pop(L, 1);
  luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
  lua_pop(L, 1);
  luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
  lua_pop(L, 1);
  luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
  lua_pop(L, 1);
  luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
  lua_pop(L, 1);
  luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
  lua_pop(L, 1);
  lua_pushglobaltable(L);
  int g = lua_gettop(L);
  block(L, g, "dofile", "dofile");
  block(L, g, "loadfile", "loadfile");
  lua_pushcfunction(L, l_load);
  lua_setfield(L, g, "load");
  lua_pushcfunction(L, l_print);
  lua_setfield(L, g, "print");
  /* io and os do not exist; give a clear error instead of "nil value". */
  lua_newtable(L);
  block(L, lua_gettop(L), "open", "io.open");
  block(L, lua_gettop(L), "popen", "io.popen");
  block(L, lua_gettop(L), "read", "io.read");
  block(L, lua_gettop(L), "write", "io.write");
  block(L, lua_gettop(L), "lines", "io.lines");
  lua_setfield(L, g, "io");
  lua_newtable(L);
  block(L, lua_gettop(L), "execute", "os.execute");
  block(L, lua_gettop(L), "exit", "os.exit");
  block(L, lua_gettop(L), "remove", "os.remove");
  block(L, lua_gettop(L), "rename", "os.rename");
  block(L, lua_gettop(L), "getenv", "os.getenv");
  block(L, lua_gettop(L), "time", "os.time (use engine.time)");
  block(L, lua_gettop(L), "clock", "os.clock (use engine.time)");
  block(L, lua_gettop(L), "date", "os.date");
  lua_setfield(L, g, "os");
  lua_newtable(L);
  block(L, lua_gettop(L), "loadlib", "package.loadlib");
  lua_setfield(L, g, "package");
  lua_getfield(L, g, "string");
  block(L, lua_gettop(L), "dump", "string.dump");
  lua_pop(L, 1);
  lua_getfield(L, g, "math");
  lua_pushcfunction(L, l_random);
  lua_setfield(L, -2, "random");
  lua_pushcfunction(L, l_randomseed);
  lua_setfield(L, -2, "randomseed");
  lua_pop(L, 1);
  lua_pop(L, 1); /* globals */
}

/* ---- Packages ----------------------------------------------------------------- */

static const char *pkg_id(zb_script *s, size_t pkg) {
  const zb_package *p = zb_vfs_load_order(s->cfg.vfs, pkg);
  return p ? p->id : "?";
}

static void watch(zb_script *s, size_t pkg, const char *real) {
  if (!s->cfg.dev) {
    return;
  }
  for (size_t i = 0; i < s->watched.len; ++i) {
    watched_file *w = zb_array_at(&s->watched, i);
    if (w->pkg == pkg && strcmp(w->real, real) == 0) {
      return;
    }
  }
  zb_fs_stat st = {false, 0, 0};
  s->cfg.fs->stat(s->cfg.fs->ctx, real, &st);
  watched_file w = {pkg, zb_strdup(s->cfg.a, real), st.mtime_ns};
  if (!w.real || !zb_array_push(&s->watched, &w)) {
    zb_free(s->cfg.a, w.real);
  }
}

/* Loads source text as a chunk named "@<real path>" with _ENV = env at env_idx. */
static bool load_chunk(zb_script *s, const char *text, size_t len, const char *real, int env_idx, const char *pkg) {
  lua_State *L = s->L;
  char name[ZB_VFS_MAX_PATH * 2 + 2];
  snprintf(name, sizeof name, "@%s", real);
  if (luaL_loadbufferx(L, text, len, name, "t") != LUA_OK) {
    zb_script_report(s, pkg, lua_tostring(L, -1));
    lua_pop(L, 1);
    return false;
  }
  lua_pushvalue(L, env_idx);
  lua_setupvalue(L, -2, 1);
  return true;
}

/* require(name): scripts/<name with dots as slashes>.lua through the overlay. */
static int l_require(lua_State *L) {
  zb_script *s = zb_script_from(L);
  const char *name = luaL_checkstring(L, 1);
  int pkg = (int)lua_tointeger(L, lua_upvalueindex(1));
  if (pkg < 0 || (size_t)pkg >= s->pkg_count || s->env_refs[pkg] == LUA_NOREF) {
    /* Tests and scenarios load modules in the first package's environment. */
    if (s->pkg_count == 0) {
      return luaL_error(L, "module '%s' cannot be loaded: no packages are enabled", name);
    }
    pkg = 0;
    if (s->env_refs[0] == LUA_NOREF) {
      zb_script_new_env(s, 0);
      s->env_refs[0] = luaL_ref(L, LUA_REGISTRYINDEX);
    }
  }
  lua_getfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_LOADED);
  lua_getfield(L, -1, name);
  if (!lua_isnil(L, -1)) {
    return 1;
  }
  lua_pop(L, 1);
  char path[ZB_VFS_MAX_PATH];
  snprintf(path, sizeof path, "scripts/%s.lua", name);
  for (char *p = path + 8; *p; ++p) {
    if (*p == '.' && strcmp(p, ".lua") != 0) {
      *p = '/';
    }
  }
  char real[ZB_VFS_MAX_PATH * 2];
  char *text = NULL;
  size_t len = 0;
  if (zb_vfs_real_path(s->cfg.vfs, path, real, sizeof real) != ZB_OK ||
      zb_vfs_read(s->cfg.vfs, path, s->cfg.a, &text, &len) != ZB_OK) {
    return luaL_error(L, "module '%s' not found (looked for %s in the loaded packages)", name, path);
  }
  lua_rawgeti(L, LUA_REGISTRYINDEX, s->env_refs[pkg]);
  int env = lua_gettop(L);
  char chunkname[ZB_VFS_MAX_PATH * 2 + 2];
  snprintf(chunkname, sizeof chunkname, "@%s", real);
  int status = luaL_loadbufferx(L, text, len, chunkname, "t");
  zb_free(s->cfg.a, text);
  if (status != LUA_OK) {
    return lua_error(L);
  }
  lua_pushvalue(L, env);
  lua_setupvalue(L, -2, 1);
  watch(s, (size_t)pkg, real);
  lua_pushstring(L, name);
  lua_call(L, 1, 1);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
  }
  lua_pushvalue(L, -1);
  lua_setfield(L, env - 1, name); /* loaded[name] = value */
  return 1;
}

/* engine.on(event, fn) bound to a package (upvalue 1). */
static int l_on(lua_State *L) {
  int pkg = (int)lua_tointeger(L, lua_upvalueindex(1));
  const char *event = luaL_checkstring(L, 1);
  bool known = false;
  for (size_t i = 0; i < ZB_ARRAY_LEN(k_events); ++i) {
    known = known || strcmp(event, k_events[i]) == 0;
  }
  luaL_argcheck(L, known, 1, "unknown event (init, tick, frame, event, shutdown, reload, hash)");
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_getfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_HOOKS);
  lua_rawgeti(L, -1, pkg + 1);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_rawseti(L, -3, pkg + 1);
  }
  lua_pushvalue(L, 2);
  lua_setfield(L, -2, event);
  return 0;
}

/* engine.preserve(tbl): kept across hot reloads of this package. */
static int l_preserve(lua_State *L) {
  int pkg = (int)lua_tointeger(L, lua_upvalueindex(1));
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_getfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_PRESERVE);
  lua_pushvalue(L, 1);
  lua_rawseti(L, -2, pkg + 1);
  return 0;
}

void zb_script_new_env(zb_script *s, int pkg) {
  lua_State *L = s->L;
  lua_newtable(L); /* env */
  lua_newtable(L); /* metatable */
  lua_pushglobaltable(L);
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);
  lua_pushinteger(L, pkg);
  lua_pushcclosure(L, l_require, 1);
  lua_setfield(L, -2, "require");
  if (pkg >= 0) {
    /* engine proxy with package-bound functions */
    lua_newtable(L);
    lua_pushinteger(L, pkg);
    lua_pushcclosure(L, l_on, 1);
    lua_setfield(L, -2, "on");
    lua_pushinteger(L, pkg);
    lua_pushcclosure(L, l_preserve, 1);
    lua_setfield(L, -2, "preserve");
    lua_newtable(L);
    lua_getfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_ENGINE);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    lua_setfield(L, -2, "engine");
    lua_pushstring(L, pkg_id(s, (size_t)pkg));
    lua_setfield(L, -2, "package_id");
  }
}

static bool run_package(zb_script *s, size_t pkg) {
  lua_State *L = s->L;
  const zb_package *p = zb_vfs_load_order(s->cfg.vfs, pkg);
  char *text = NULL;
  size_t len = 0;
  if (zb_vfs_read_package(s->cfg.vfs, p, "scripts/main.lua", s->cfg.a, &text, &len) != ZB_OK) {
    return true; /* no entry script: data-only package */
  }
  char real[ZB_VFS_MAX_PATH * 2];
  zb_path_join(real, sizeof real, p->root, "scripts/main.lua");
  if (s->env_refs[pkg] != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, s->env_refs[pkg]);
  }
  zb_script_new_env(s, (int)pkg);
  lua_pushvalue(L, -1);
  s->env_refs[pkg] = luaL_ref(L, LUA_REGISTRYINDEX);
  int env = lua_gettop(L);
  bool ok = load_chunk(s, text, len, real, env, p->id);
  zb_free(s->cfg.a, text);
  watch(s, pkg, real);
  s->current_pkg = (int)pkg;
  if (ok) {
    ok = zb_script_pcall(s, 0, 0, p->id, false);
  }
  s->current_pkg = -1;
  lua_pop(L, 1); /* env */
  s->pkg_failed[pkg] = !ok;
  return ok;
}

size_t zb_script_run_packages(zb_script *s) {
  size_t failed = 0;
  for (size_t i = 0; i < s->pkg_count; ++i) {
    if (!run_package(s, i)) {
      failed++;
    }
  }
  return failed;
}

/* ---- Hooks --------------------------------------------------------------------- */

typedef int (*push_args_fn)(lua_State *L, const void *ud);

static void dispatch(zb_script *s, const char *event, push_args_fn push, const void *ud, bool reverse) {
  lua_State *L = s->L;
  lua_getfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_HOOKS);
  int hooks = lua_gettop(L);
  for (size_t k = 0; k < s->pkg_count; ++k) {
    size_t pkg = reverse ? s->pkg_count - 1 - k : k;
    if (lua_rawgeti(L, hooks, (lua_Integer)pkg + 1) != LUA_TTABLE) {
      lua_pop(L, 1);
      continue;
    }
    int t = lua_gettop(L);
    if (lua_getfield(L, t, event) == LUA_TFUNCTION) {
      int nargs = push ? push(L, ud) : 0;
      if (!zb_script_pcall(s, nargs, 0, pkg_id(s, pkg), true)) {
        /* Error isolation: this package's hook is disabled. */
        lua_pushnil(L);
        lua_setfield(L, t, event);
        zb_warn("[%s] '%s' hook disabled after an error", pkg_id(s, pkg), event);
      }
    } else {
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
}

static int push_number(lua_State *L, const void *ud) {
  lua_pushnumber(L, *(const double *)ud);
  return 1;
}

void zb_script_init_hooks(zb_script *s) { dispatch(s, "init", NULL, NULL, false); }

void zb_script_tick(zb_script *s, double dt) { dispatch(s, "tick", push_number, &dt, false); }

void zb_script_frame(zb_script *s, double alpha) { dispatch(s, "frame", push_number, &alpha, false); }

void zb_script_shutdown(zb_script *s) { dispatch(s, "shutdown", NULL, NULL, true); }

typedef struct event_args {
  const char *name;
  int a, b;
  const char *text;
} event_args;

static int push_event(lua_State *L, const void *ud) {
  const event_args *e = ud;
  lua_pushstring(L, e->name);
  lua_newtable(L);
  lua_pushinteger(L, e->a);
  lua_setfield(L, -2, "a");
  lua_pushinteger(L, e->b);
  lua_setfield(L, -2, "b");
  if (strcmp(e->name, "resize") == 0) {
    lua_pushinteger(L, e->a);
    lua_setfield(L, -2, "width");
    lua_pushinteger(L, e->b);
    lua_setfield(L, -2, "height");
  }
  if (e->text) {
    lua_pushstring(L, e->text);
    lua_setfield(L, -2, "value");
  }
  return 2;
}

void zb_script_event(zb_script *s, const char *name, int a, int b, const char *text) {
  event_args e = {name, a, b, text};
  dispatch(s, "event", push_event, &e, false);
}

/* ---- State hash --------------------------------------------------------------------- */

void zb_script_hash_base(zb_script *s, zb_hasher *h) {
  zb_hasher_init(h, 0x7374617465);
  zb_hasher_u64(h, s->cfg.host.tick_count ? s->cfg.host.tick_count(s->cfg.host.ctx) : 0);
  if (s->cfg.input) {
    for (size_t i = 0; i < zb_input_action_count(s->cfg.input); ++i) {
      const zb_action_state *st = zb_input_state(s->cfg.input, (int)i);
      zb_hasher_u64(h, (uint64_t)(st->held | (st->pressed << 1) | (st->released << 2)));
      zb_hasher_f64(h, st->x);
      zb_hasher_f64(h, st->y);
    }
  }
  zb_hasher_u64(h, s->rng.counter);
}

uint64_t zb_script_state_hash(zb_script *s) {
  lua_State *L = s->L;
  zb_hasher h;
  zb_script_hash_base(s, &h);
  lua_getfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_HOOKS);
  int hooks = lua_gettop(L);
  for (size_t pkg = 0; pkg < s->pkg_count; ++pkg) {
    if (lua_rawgeti(L, hooks, (lua_Integer)pkg + 1) == LUA_TTABLE && lua_getfield(L, -1, "hash") == LUA_TFUNCTION) {
      /* Keyed by package id and skipped for nil, so packages without state
       * (or with none yet) leave the hash unchanged. */
      if (zb_script_pcall(s, 0, 1, pkg_id(s, pkg), true) && !lua_isnil(L, -1)) {
        const char *id = pkg_id(s, pkg);
        zb_hasher_bytes(&h, id, strlen(id) + 1);
        zb_script_hash_value(L, -1, &h, 0);
      }
    } else {
      lua_pop(L, 1);
    }
    lua_settop(L, hooks);
  }
  lua_pop(L, 1);
  return zb_hasher_final(&h);
}

/* ---- Hot reload ------------------------------------------------------------------ */

static void reload_package(zb_script *s, size_t pkg) {
  lua_State *L = s->L;
  zb_info("[%s] scripts changed; reloading", pkg_id(s, pkg));
  /* forget hooks and modules of the package */
  lua_getfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_HOOKS);
  lua_pushnil(L);
  lua_rawseti(L, -2, (lua_Integer)pkg + 1);
  lua_pop(L, 1);
  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_LOADED);
  run_package(s, pkg);
  lua_getfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_HOOKS);
  if (lua_rawgeti(L, -1, (lua_Integer)pkg + 1) == LUA_TTABLE) {
    if (lua_getfield(L, -1, "reload") == LUA_TFUNCTION) {
      lua_getfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_PRESERVE);
      lua_rawgeti(L, -1, (lua_Integer)pkg + 1);
      lua_remove(L, -2);
      zb_script_pcall(s, 1, 0, pkg_id(s, pkg), true);
    } else {
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 2);
}

size_t zb_script_poll_reload(zb_script *s, uint64_t now_ns) {
  if (!s->cfg.dev || (s->last_poll_ns && now_ns - s->last_poll_ns < RELOAD_INTERVAL_NS)) {
    return 0;
  }
  s->last_poll_ns = now_ns;
  bool *changed = zb_calloc(s->cfg.a, s->pkg_count + 1);
  if (!changed) {
    return 0;
  }
  for (size_t i = 0; i < s->watched.len; ++i) {
    watched_file *w = zb_array_at(&s->watched, i);
    zb_fs_stat st;
    if (s->cfg.fs->stat(s->cfg.fs->ctx, w->real, &st) == ZB_OK && st.mtime_ns != w->mtime) {
      w->mtime = st.mtime_ns;
      changed[w->pkg] = true;
    }
  }
  size_t reloaded = 0;
  for (size_t pkg = 0; pkg < s->pkg_count; ++pkg) {
    if (changed[pkg]) {
      reload_package(s, pkg);
      reloaded++;
    }
  }
  zb_free(s->cfg.a, changed);
  return reloaded;
}

/* ---- Lifecycle ------------------------------------------------------------------ */

zb_script *zb_script_create(const zb_script_config *cfg, char *err, size_t err_len) {
  zb_script *s = zb_calloc(cfg->a, sizeof(*s));
  if (!s) {
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  s->cfg = *cfg;
  s->current_pkg = -1;
  s->co_ref = LUA_NOREF;
  s->rng = zb_rng_make(cfg->seed, 0x72616e64);
  zb_array_init(&s->watched, cfg->a, sizeof(watched_file));
  zb_array_init(&s->cov_files, cfg->a, sizeof(cov_file));
  zb_map_init(&s->cov_index, cfg->a);
  s->pkg_count = zb_vfs_load_count(cfg->vfs);
  s->env_refs = zb_alloc(cfg->a, (s->pkg_count + 1) * sizeof(int));
  s->pkg_failed = zb_calloc(cfg->a, s->pkg_count + 1);
  s->L = lua_newstate(l_alloc, (void *)cfg->a);
  if (!s->env_refs || !s->pkg_failed || !s->L) {
    snprintf(err, err_len, "out of memory");
    zb_script_destroy(s);
    return NULL;
  }
  for (size_t i = 0; i <= s->pkg_count; ++i) {
    s->env_refs[i] = LUA_NOREF;
  }
  *(zb_script **)lua_getextraspace(s->L) = s;
  s->coverage = cfg->coverage_path != NULL;
  int mask = (cfg->instruction_budget ? LUA_MASKCOUNT : 0) | (s->coverage ? LUA_MASKLINE : 0);
  if (mask) {
    lua_sethook(s->L, debug_hook, mask, BUDGET_STEP);
  }
  open_sandbox(s);
  lua_newtable(s->L);
  lua_setfield(s->L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_HOOKS);
  lua_newtable(s->L);
  lua_setfield(s->L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_PRESERVE);
  lua_newtable(s->L);
  lua_setfield(s->L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_LOADED);
  zb_script_open_api(s);
  if (cfg->test_api) {
    zb_script_open_test_api(s);
  }
  return s;
}

/* Merges collected counts into the LuaCov stats file. */
zb_result zb_script_write_coverage(zb_script *s) {
  if (!s->coverage || !s->cfg.coverage_path) {
    return ZB_OK;
  }
  const zb_fs_api *fs = s->cfg.fs;
  char *old = NULL;
  size_t old_len = 0;
  if (fs->read(fs->ctx, s->cfg.coverage_path, s->cfg.a, &old, &old_len) == ZB_OK) {
    const char *p = old;
    while (*p) {
      char *end;
      long max = strtol(p, &end, 10);
      if (end == p || *end != ':') {
        break;
      }
      const char *name = end + 1;
      const char *nl = strchr(name, '\n');
      if (!nl) {
        break;
      }
      char fname[1024];
      snprintf(fname, sizeof fname, "%.*s", (int)(nl - name), name);
      p = nl + 1;
      for (long i = 1; i <= max; ++i) {
        long hits = strtol(p, &end, 10);
        if (end == p) {
          break;
        }
        p = end;
        if (hits > 0) {
          cov_hit(s, fname, (int)i, (uint32_t)hits);
        }
        while (*p == ' ') {
          p++;
        }
      }
      while (*p == '\n' || *p == ' ') {
        p++;
      }
    }
    zb_free(s->cfg.a, old);
  }
  zb_array buf;
  zb_array_init(&buf, s->cfg.a, 1);
  char tmp[64];
  for (size_t i = 0; i < s->cov_files.len; ++i) {
    cov_file *f = zb_array_at(&s->cov_files, i);
    size_t max = f->max;
    while (max > 0 && f->hits[max - 1] == 0) {
      max--;
    }
    int n = snprintf(tmp, sizeof tmp, "%zu:", max);
    for (int k = 0; k < n; ++k) {
      zb_array_push(&buf, &tmp[k]);
    }
    for (const char *c = f->name; *c; ++c) {
      zb_array_push(&buf, c);
    }
    zb_array_push(&buf, "\n");
    for (size_t l = 0; l < max; ++l) {
      n = snprintf(tmp, sizeof tmp, "%u ", (unsigned)f->hits[l]);
      for (int k = 0; k < n; ++k) {
        zb_array_push(&buf, &tmp[k]);
      }
    }
    zb_array_push(&buf, "\n");
  }
  zb_result r = fs->write(fs->ctx, s->cfg.coverage_path, buf.data ? buf.data : (void *)"", buf.len);
  zb_array_free(&buf);
  if (r == ZB_OK) {
    /* The file now holds every count: later writes add only new hits. */
    for (size_t i = 0; i < s->cov_files.len; ++i) {
      cov_file *f = zb_array_at(&s->cov_files, i);
      if (f->hits) {
        memset(f->hits, 0, f->max * sizeof(uint32_t));
      }
    }
  }
  return r;
}

void zb_script_destroy(zb_script *s) {
  if (!s) {
    return;
  }
  if (s->L) {
    zb_script_write_coverage(s);
    lua_close(s->L);
  }
  for (size_t i = 0; i < s->watched.len; ++i) {
    zb_free(s->cfg.a, ((watched_file *)zb_array_at(&s->watched, i))->real);
  }
  for (size_t i = 0; i < s->cov_files.len; ++i) {
    cov_file *f = zb_array_at(&s->cov_files, i);
    zb_free(s->cfg.a, f->name);
    zb_free(s->cfg.a, f->hits);
  }
  zb_array_free(&s->watched);
  zb_array_free(&s->cov_files);
  zb_map_free(&s->cov_index);
  zb_free(s->cfg.a, s->env_refs);
  zb_free(s->cfg.a, s->pkg_failed);
  zb_free(s->cfg.a, s);
}

size_t zb_script_error_count(const zb_script *s) { return s->error_count; }

const char *zb_script_error(const zb_script *s, size_t i) {
  if (i >= s->error_count || i >= ZB_SCRIPT_MAX_ERRORS) {
    return NULL;
  }
  size_t first = s->error_count > ZB_SCRIPT_MAX_ERRORS ? s->error_count - ZB_SCRIPT_MAX_ERRORS : 0;
  return s->errors[(first + i) % ZB_SCRIPT_MAX_ERRORS];
}

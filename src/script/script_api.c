/* SPDX-License-Identifier: MIT */
/* Lua bindings: engine, vfs, input, render, audio, save (world: script_world.c). */
#include "script/script_internal.h"

#include "core/log.h"
#include "core/luon.h"
#include "core/version.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_FILE "save.lua"
#define REG_SAVE "zb.save"

static zb_script *S(lua_State *L) { return zb_script_from(L); }

/* ---- API types (documentation, see tools/gen_api_docs.py) ---------------------
 *
 * @luaclass EngineInfo
 * @field version string Engine version, e.g. "0.1.0"
 * @field api string Scripting API version, e.g. "1.0"
 * @field platform string "windows", "linux", "macos", "web" or "web-node"
 * @field build string "development" or "release"
 * @field input_mode string "desktop" or "touch"
 * @field drawable_size integer[] Drawable width and height in pixels
 * @field seed integer The run's random seed
 *
 * @luaclass PackageInfo
 * @field id string Package id
 * @field name string Display name
 * @field version string Package version
 * @field license string License of the package's code
 * @field license_assets string License of the package's assets
 * @field enabled boolean Whether the package is loaded
 * @field load_order integer? Position in the load order (enabled packages)
 * @field error string? Why the package is disabled
 *
 * @luaclass Credit
 * @field package string Package id, or "engine" for third-party code
 * @field kind string "asset" or "code"
 * @field file string? Asset path inside the package
 * @field name string? Name of the third-party component
 * @field author string? Author of the asset
 * @field source string? Where the asset or code comes from
 * @field license string SPDX license identifier
 *
 * @luaclass DirEntry
 * @field name string File or directory name
 * @field dir boolean Whether it is a directory
 *
 * @luaclass ActionState
 * @field pressed boolean Went down this tick
 * @field released boolean Went up this tick
 * @field held boolean Is down
 * @field x number Axis value (axis1, axis2)
 * @field y number Second axis value (axis2)
 *
 * @luaclass Camera
 * @field eye number[]? Eye position {x, y, z} (default {0, 1.5, 4})
 * @field target number[]? Point looked at (default the origin)
 * @field up number[]? Up direction (default {0, 1, 0})
 * @field fov number? Vertical field of view in degrees (default 60)
 * @field near number? Near plane (default 0.1)
 * @field far number? Far plane (default 500)
 *
 * @luaclass Atmosphere
 * @field enabled boolean? Draw the sky and apply fog (default true)
 * @field zenith number[]? Sky colour overhead {r, g, b}
 * @field horizon number[]? Sky colour at the horizon, also the fog colour
 * @field sun number[]? Direction towards the sun (render coordinates)
 * @field sun_color number[]? Colour of the sun disc and glow
 * @field fog_distance number? Distance at which fog hides 98 % of the scene (0 = no fog)
 *
 * @luaclass Palette
 * @field lut string? 32x32x32 lookup table image (default "palettes/base_lut.png")
 * @field bayer string? 8x8 ordered-dither matrix image (default "palettes/bayer8.png")
 * @field dither number? Dither amplitude in colour units (default 0.08)
 *
 * @luaclass Light
 * @field direction number[]? Direction the light travels
 * @field color number[]? Light colour
 * @field ambient number[]|number? Ambient colour
 *
 * @luaclass Transform
 * @field position number[]? Translation {x, y, z}
 * @field rotation number[]? Euler angles in degrees (applied Y, X, Z)
 * @field scale number[]|number? Scale per axis or uniform
 *
 * @luaclass RenderStats
 * @field internal_width integer Internal framebuffer width
 * @field internal_height integer Internal framebuffer height
 * @field scale integer Integer upscale factor, 0 when scaled fractionally
 * @field out_x integer Destination rectangle in the window
 * @field out_y integer Destination rectangle in the window
 * @field out_width integer Destination rectangle in the window
 * @field out_height integer Destination rectangle in the window
 * @field drawable_width integer Window drawable width in pixels
 * @field drawable_height integer Window drawable height in pixels
 * @field views integer Rendered views (2 in stereo)
 * @field model_draws integer Model draw items
 * @field sprite_draws integer Sprite draw items
 * @field terrain_items integer Terrain chunk draw items (after culling)
 * @field terrain_triangles integer Terrain triangles drawn
 * @field glyphs integer Text glyphs
 * @field culled integer Items culled by the view frustum
 * @field frames integer Frames rendered so far
 * @field backend string "gl" or "null"
 *
 * @luaclass PlayOptions
 * @field volume number? Volume (default 1)
 * @field pitch number? Playback rate (default 1)
 * @field loop boolean? Repeat until stopped
 * @field group string? "music" or "effects" (default)
 *
 * @luavar engine.version: string
 * The engine version.
 *
 * @luavar engine.api_version: string
 * The scripting API version packages declare in package.lua ("api").
 *
 * @luavar package_id: string
 * Id of the package whose script is running (package environments only).
 */

/* ---- Canonical value hashing ------------------------------------------------- */

typedef struct key_ref {
  int type;
  bool is_int;
  lua_Integer i;
  lua_Number n;
  const char *s;
  size_t len;
} key_ref;

static int cmp_keys(const void *pa, const void *pb) {
  const key_ref *a = pa, *b = pb;
  if (a->type != b->type) {
    return a->type < b->type ? -1 : 1;
  }
  if (a->type == LUA_TSTRING) {
    size_t n = a->len < b->len ? a->len : b->len;
    int c = memcmp(a->s, b->s, n);
    return c ? c : (a->len < b->len ? -1 : (a->len > b->len ? 1 : 0));
  }
  if (a->type == LUA_TNUMBER) {
    double x = a->is_int ? (double)a->i : a->n, y = b->is_int ? (double)b->i : b->n;
    return x < y ? -1 : (x > y ? 1 : 0);
  }
  return (int)a->i - (int)b->i; /* booleans */
}

static void push_key(lua_State *L, const key_ref *k) {
  if (k->type == LUA_TSTRING) {
    lua_pushlstring(L, k->s, k->len);
  } else if (k->type == LUA_TNUMBER) {
    if (k->is_int) {
      lua_pushinteger(L, k->i);
    } else {
      lua_pushnumber(L, k->n);
    }
  } else {
    lua_pushboolean(L, (int)k->i);
  }
}

void zb_script_hash_value(lua_State *L, int idx, zb_hasher *h, int depth) {
  idx = lua_absindex(L, idx);
  int t = lua_type(L, idx);
  zb_hasher_u64(h, (uint64_t)t);
  switch (t) {
    case LUA_TBOOLEAN: zb_hasher_u64(h, (uint64_t)lua_toboolean(L, idx)); break;
    case LUA_TNUMBER:
      if (lua_isinteger(L, idx)) {
        zb_hasher_u64(h, 1);
        zb_hasher_u64(h, (uint64_t)lua_tointeger(L, idx));
      } else {
        zb_hasher_u64(h, 2);
        zb_hasher_f64(h, (double)lua_tonumber(L, idx));
      }
      break;
    case LUA_TSTRING: {
      size_t len;
      const char *s = lua_tolstring(L, idx, &len);
      zb_hasher_u64(h, (uint64_t)len);
      zb_hasher_bytes(h, s, len);
      break;
    }
    case LUA_TTABLE: {
      if (depth > 16) {
        break;
      }
      zb_script *s = S(L);
      size_t n = 0, cap = 16;
      key_ref *keys = zb_alloc(s->cfg.a, cap * sizeof(key_ref));
      lua_pushnil(L);
      while (keys && lua_next(L, idx)) {
        lua_pop(L, 1);
        int kt = lua_type(L, -1);
        if (kt != LUA_TSTRING && kt != LUA_TNUMBER && kt != LUA_TBOOLEAN) {
          continue;
        }
        if (n == cap) {
          key_ref *grown = zb_realloc(s->cfg.a, keys, cap * 2 * sizeof(key_ref));
          if (!grown) {
            break;
          }
          keys = grown;
          cap *= 2;
        }
        key_ref *k = &keys[n++];
        memset(k, 0, sizeof(*k));
        k->type = kt;
        if (kt == LUA_TSTRING) {
          k->s = lua_tolstring(L, -1, &k->len);
        } else if (kt == LUA_TNUMBER) {
          k->is_int = lua_isinteger(L, -1);
          k->i = lua_tointeger(L, -1);
          k->n = lua_tonumber(L, -1);
        } else {
          k->i = lua_toboolean(L, -1);
        }
      }
      if (keys) {
        qsort(keys, n, sizeof(key_ref), cmp_keys);
        zb_hasher_u64(h, (uint64_t)n);
        for (size_t i = 0; i < n; ++i) {
          push_key(L, &keys[i]);
          zb_script_hash_value(L, -1, h, depth + 1);
          lua_gettable(L, idx);
          zb_script_hash_value(L, -1, h, depth + 1);
          lua_pop(L, 1);
        }
        zb_free(s->cfg.a, keys);
      }
      break;
    }
    default: break;
  }
}

/* @lua engine.info(): EngineInfo
 * Engine version, scripting API, platform, build, input mode, drawable size and seed.
 */
static int e_info(lua_State *L) {
  zb_script *s = S(L);
  zb_script_info info;
  memset(&info, 0, sizeof info);
  if (s->cfg.host.get_info) {
    s->cfg.host.get_info(s->cfg.host.ctx, &info);
  }
  lua_newtable(L);
  lua_pushstring(L, ZB_VERSION);
  lua_setfield(L, -2, "version");
  lua_pushstring(L, ZB_API_VERSION);
  lua_setfield(L, -2, "api");
  lua_pushstring(L, info.platform ? info.platform : "unknown");
  lua_setfield(L, -2, "platform");
  lua_pushstring(L, info.build ? info.build : "release");
  lua_setfield(L, -2, "build");
  lua_pushstring(L, info.input_mode ? info.input_mode : "desktop");
  lua_setfield(L, -2, "input_mode");
  lua_newtable(L);
  lua_pushinteger(L, info.drawable_w);
  lua_rawseti(L, -2, 1);
  lua_pushinteger(L, info.drawable_h);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "drawable_size");
  lua_pushinteger(L, (lua_Integer)info.seed);
  lua_setfield(L, -2, "seed");
  return 1;
}

/* @lua engine.quit(code?: integer)
 * Ends the game after this tick; shutdown hooks run in reverse load order. The process exits with code
 * (default 0).
 */
static int e_quit(lua_State *L) {
  zb_script *s = S(L);
  int code = (int)luaL_optinteger(L, 1, 0);
  if (s->cfg.host.quit) {
    s->cfg.host.quit(s->cfg.host.ctx, code);
  }
  return 0;
}

/* @lua engine.time(): number
 * Simulation time in seconds (ticks / 60). Use it instead of wall-clock time.
 */
static int e_time(lua_State *L) {
  zb_script *s = S(L);
  lua_pushnumber(L, s->cfg.host.sim_time ? s->cfg.host.sim_time(s->cfg.host.ctx) : 0.0);
  return 1;
}

/* @lua engine.tick(): integer
 * Number of simulation ticks run so far.
 */
static int e_tick(lua_State *L) {
  zb_script *s = S(L);
  lua_pushinteger(L, s->cfg.host.tick_count ? (lua_Integer)s->cfg.host.tick_count(s->cfg.host.ctx) : 0);
  return 1;
}

/* @lua engine.pause(paused: boolean)
 * Pauses or resumes the simulation; frames and frame hooks continue while paused.
 */
static int e_pause(lua_State *L) {
  zb_script *s = S(L);
  if (s->cfg.host.set_paused) {
    s->cfg.host.set_paused(s->cfg.host.ctx, lua_toboolean(L, 1));
  }
  return 0;
}

/* @lua engine.set_time_scale(scale: number)
 * Development and test builds only: simulation speed factor (0.1 to 10).
 */
static int e_time_scale(lua_State *L) {
  zb_script *s = S(L);
  double scale = luaL_checknumber(L, 1);
  if (!s->cfg.dev && !s->cfg.test_api) {
    return luaL_error(L, "engine.set_time_scale is only available in development builds");
  }
  if (s->cfg.host.set_time_scale) {
    s->cfg.host.set_time_scale(s->cfg.host.ctx, scale);
  }
  return 0;
}

/* @lua engine.set_fullscreen(on: boolean)
 * Switches the window to fullscreen or back; scripts receive a resize event.
 */
static int e_fullscreen(lua_State *L) {
  zb_script *s = S(L);
  if (s->cfg.host.set_fullscreen) {
    s->cfg.host.set_fullscreen(s->cfg.host.ctx, lua_toboolean(L, 1));
  }
  return 0;
}

/* @lua engine.log(level: "debug"|"info"|"warn"|"error", message: string)
 * Writes a message to the engine log.
 */
static int e_log(lua_State *L) {
  const char *level = luaL_checkstring(L, 1);
  const char *msg = luaL_checkstring(L, 2);
  zb_log_level lv = strcmp(level, "error") == 0   ? ZB_LOG_ERROR
                    : strcmp(level, "warn") == 0  ? ZB_LOG_WARN
                    : strcmp(level, "debug") == 0 ? ZB_LOG_DEBUG
                                                  : ZB_LOG_INFO;
  zb_log(lv, "[lua] %s", msg);
  return 0;
}

/* @lua engine.hash_state(value?: any): string
 * Canonical 64-bit state hash as 16 hex digits. Without an argument: tick, input, random-number counter and
 * every package's "hash" hook; with a value: the same base extended by that value (sorted keys, exact
 * numbers).
 */
static int e_hash_state(lua_State *L) {
  zb_script *s = S(L);
  uint64_t value;
  if (lua_isnoneornil(L, 1)) {
    value = zb_script_state_hash(s);
  } else {
    zb_hasher h;
    zb_script_hash_base(s, &h);
    zb_script_hash_value(L, 1, &h, 0);
    value = zb_hasher_final(&h);
  }
  char hex[17];
  zb_hash_hex(value, hex);
  lua_pushstring(L, hex);
  return 1;
}

/* @lua engine.resize_window(width: integer, height: integer)
 * Test API (scenarios and Lua tests only): resizes the window.
 */
static int e_resize_window(lua_State *L) {
  zb_script *s = S(L);
  int w = (int)luaL_checkinteger(L, 1), h = (int)luaL_checkinteger(L, 2);
  luaL_argcheck(L, w > 0 && h > 0, 1, "size must be positive");
  if (s->cfg.host.set_window_size) {
    s->cfg.host.set_window_size(s->cfg.host.ctx, w, h);
  }
  return 0;
}

static void set_str(lua_State *L, const char *k, const char *v) {
  lua_pushstring(L, v ? v : "");
  lua_setfield(L, -2, k);
}

/* @lua engine.packages(): PackageInfo[]
 * Enabled packages in load order, then disabled ones with the reason.
 */
static int e_packages(lua_State *L) {
  zb_script *s = S(L);
  lua_newtable(L);
  lua_Integer n = 0;
  /* enabled packages in load order, then disabled ones */
  for (int pass = 0; pass < 2; ++pass) {
    for (size_t i = 0; i < zb_vfs_package_count(s->cfg.vfs); ++i) {
      const zb_package *p = zb_vfs_package(s->cfg.vfs, i);
      if ((pass == 0) != p->enabled) {
        continue;
      }
      lua_newtable(L);
      set_str(L, "id", p->id ? p->id : "?");
      set_str(L, "name", p->name);
      set_str(L, "version", p->version);
      set_str(L, "license", p->license_code);
      set_str(L, "license_assets", p->license_assets);
      lua_pushboolean(L, p->enabled);
      lua_setfield(L, -2, "enabled");
      if (!p->enabled) {
        set_str(L, "error", p->error);
      }
      if (p->enabled) {
        size_t order = 0;
        for (size_t k = 0; k < zb_vfs_load_count(s->cfg.vfs); ++k) {
          if (zb_vfs_load_order(s->cfg.vfs, k) == p) {
            order = k + 1;
          }
        }
        lua_pushinteger(L, (lua_Integer)order);
        lua_setfield(L, -2, "load_order");
      }
      lua_rawseti(L, -2, ++n);
    }
  }
  return 1;
}

static void push_luon(lua_State *L, const zb_value *v) {
  switch (v ? v->type : ZB_VAL_NIL) {
    case ZB_VAL_BOOL: lua_pushboolean(L, v->as.b); break;
    case ZB_VAL_NUMBER:
      if (v->as.num == floor(v->as.num) && fabs(v->as.num) < 9e15) {
        lua_pushinteger(L, (lua_Integer)v->as.num);
      } else {
        lua_pushnumber(L, v->as.num);
      }
      break;
    case ZB_VAL_STRING: lua_pushlstring(L, v->as.str.ptr, v->as.str.len); break;
    case ZB_VAL_TABLE:
      lua_newtable(L);
      for (size_t i = 0; i < zb_table_count(v); ++i) {
        const zb_table_entry *e = zb_table_entry_at(v, i);
        push_luon(L, &e->key);
        push_luon(L, &e->value);
        lua_settable(L, -3);
      }
      break;
    default: lua_pushnil(L); break;
  }
}

/* @lua engine.credits(): Credit[]
 * Asset credits of every loaded package (credits.lua) and the third-party code notices.
 */
static int e_credits(lua_State *L) {
  zb_script *s = S(L);
  lua_newtable(L);
  lua_Integer n = 0;
  for (size_t i = 0; i < zb_vfs_load_count(s->cfg.vfs); ++i) {
    const zb_package *p = zb_vfs_load_order(s->cfg.vfs, i);
    char *text = NULL;
    size_t len = 0;
    if (zb_vfs_read_package(s->cfg.vfs, p, "credits.lua", s->cfg.a, &text, &len) != ZB_OK) {
      continue;
    }
    zb_luon_doc doc;
    zb_luon_error err;
    if (zb_luon_parse(&doc, s->cfg.a, text, len, &err) == ZB_OK) {
      for (size_t k = 1; k <= zb_table_len(&doc.root); ++k) {
        push_luon(L, zb_table_index(&doc.root, k));
        if (!lua_istable(L, -1)) {
          lua_pop(L, 1);
          continue;
        }
        set_str(L, "package", p->id);
        set_str(L, "kind", "asset");
        lua_rawseti(L, -2, ++n);
      }
      zb_luon_free(&doc);
    } else {
      zb_warn("[%s] credits.lua:%d: %s", p->id, err.line, err.message);
    }
    zb_free(s->cfg.a, text);
  }
  char *text = NULL;
  size_t len = 0;
  if (zb_vfs_read(s->cfg.vfs, "data/third_party.lua", s->cfg.a, &text, &len) == ZB_OK) {
    zb_luon_doc doc;
    zb_luon_error err;
    if (zb_luon_parse(&doc, s->cfg.a, text, len, &err) == ZB_OK) {
      for (size_t k = 1; k <= zb_table_len(&doc.root); ++k) {
        push_luon(L, zb_table_index(&doc.root, k));
        if (lua_istable(L, -1)) {
          set_str(L, "package", "engine");
          set_str(L, "kind", "code");
          lua_rawseti(L, -2, ++n);
        } else {
          lua_pop(L, 1);
        }
      }
      zb_luon_free(&doc);
    }
    zb_free(s->cfg.a, text);
  }
  return 1;
}

/* @lua vfs.exists(path: string): boolean
 * Whether a file exists in the merged package view.
 */
static int v_exists(lua_State *L) {
  lua_pushboolean(L, zb_vfs_exists(S(L)->cfg.vfs, luaL_checkstring(L, 1)));
  return 1;
}

/* @lua vfs.read(path: string): string?, string?
 * Reads a file synchronously through the overlay; returns nil and an error message on failure. Prefer vfs.load
 * for large files.
 */
static int v_read(lua_State *L) {
  zb_script *s = S(L);
  const char *path = luaL_checkstring(L, 1);
  char *data = NULL;
  size_t len = 0;
  zb_result r = zb_vfs_read(s->cfg.vfs, path, s->cfg.a, &data, &len);
  if (r != ZB_OK) {
    lua_pushnil(L);
    lua_pushfstring(L, "%s: %s", path, zb_result_str(r));
    return 2;
  }
  lua_pushlstring(L, data, len);
  zb_free(s->cfg.a, data);
  return 1;
}

typedef struct list_ctx {
  lua_State *L;
  lua_Integer n;
} list_ctx;

static bool list_cb(const char *name, bool is_dir, void *user) {
  list_ctx *lc = user;
  lua_newtable(lc->L);
  lua_pushstring(lc->L, name);
  lua_setfield(lc->L, -2, "name");
  lua_pushboolean(lc->L, is_dir);
  lua_setfield(lc->L, -2, "dir");
  lua_rawseti(lc->L, -2, ++lc->n);
  return true;
}

/* @lua vfs.list(dir?: string): DirEntry[]?, string?
 * Merged listing of a directory across packages.
 */
static int v_list(lua_State *L) {
  zb_script *s = S(L);
  const char *dir = luaL_optstring(L, 1, "");
  lua_newtable(L);
  list_ctx lc = {L, 0};
  zb_result r = zb_vfs_list(s->cfg.vfs, dir, list_cb, &lc);
  if (r != ZB_OK) {
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_pushfstring(L, "%s: %s", dir, zb_result_str(r));
    return 2;
  }
  return 1;
}

typedef struct load_box {
  zb_script *s;
  int ref;
} load_box;

static void v_loaded(zb_vfs_req *req, void *user) {
  load_box *box = user;
  zb_script *s = box->s;
  lua_State *L = s->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, box->ref);
  luaL_unref(L, LUA_REGISTRYINDEX, box->ref);
  size_t len = 0;
  const char *data = zb_vfs_req_data(req, &len);
  if (data) {
    lua_pushlstring(L, data, len);
    lua_pushnil(L);
  } else {
    lua_pushnil(L);
    lua_pushfstring(L, "%s: %s", zb_vfs_req_path(req), zb_result_str(zb_vfs_req_error(req)));
  }
  zb_free(s->cfg.a, box);
  zb_vfs_req_release(s->cfg.vfs, req);
  zb_script_pcall(s, 2, 0, "vfs.load", true);
}

/* @lua vfs.load(path: string, callback: fun(data: string?, err: string?)): "pending"
 * Loads a file asynchronously; callback runs in a later frame.
 */
static int v_load(lua_State *L) {
  zb_script *s = S(L);
  const char *path = luaL_checkstring(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  load_box *box = zb_alloc(s->cfg.a, sizeof(*box));
  if (!box) {
    return luaL_error(L, "out of memory");
  }
  lua_pushvalue(L, 2);
  box->ref = luaL_ref(L, LUA_REGISTRYINDEX);
  box->s = s;
  if (!zb_vfs_request(s->cfg.vfs, path, v_loaded, box)) {
    luaL_unref(L, LUA_REGISTRYINDEX, box->ref);
    zb_free(s->cfg.a, box);
    return luaL_error(L, "out of memory");
  }
  lua_pushstring(L, "pending");
  return 1;
}

/* ---- input ------------------------------------------------------------------------ */

static int action_arg(lua_State *L, int idx) {
  zb_script *s = S(L);
  const char *name = luaL_checkstring(L, idx);
  int id = zb_input_find(s->cfg.input, name);
  if (id < 0) {
    luaL_error(L, "unknown action '%s'", name);
  }
  return id;
}

/* @lua input.define(name: string, kind?: "button"|"axis1"|"axis2"): integer
 * Defines an action (or returns the existing one of the same kind).
 */
static int i_define(lua_State *L) {
  zb_script *s = S(L);
  const char *name = luaL_checkstring(L, 1);
  const char *type = luaL_optstring(L, 2, "button");
  zb_action_type t = strcmp(type, "axis1") == 0   ? ZB_ACTION_AXIS1
                     : strcmp(type, "axis2") == 0 ? ZB_ACTION_AXIS2
                                                  : ZB_ACTION_BUTTON;
  luaL_argcheck(L, strcmp(type, "button") == 0 || t != ZB_ACTION_BUTTON, 2, "button, axis1 or axis2 expected");
  int id = zb_input_define(s->cfg.input, name, t);
  if (id < 0) {
    return luaL_error(L, "cannot define action '%s' (type conflict or too many actions)", name);
  }
  lua_pushinteger(L, id);
  return 1;
}

/* @lua input.bind(action: string, binding: string)
 * Adds a binding such as "key:space", "keys:w,s,a,d", "mouse:left", "gamepad:left_stick", "touch:button:spin".
 */
static int i_bind(lua_State *L) {
  zb_script *s = S(L);
  int id = action_arg(L, 1);
  const char *binding = luaL_checkstring(L, 2);
  if (zb_input_bind(s->cfg.input, id, binding) != ZB_OK) {
    return luaL_error(L, "invalid binding '%s' for action '%s'", binding, lua_tostring(L, 1));
  }
  return 0;
}

/* @lua input.clear(action: string)
 * Removes all bindings of an action.
 */
static int i_clear(lua_State *L) {
  zb_input_clear_bindings(S(L)->cfg.input, action_arg(L, 1));
  return 0;
}

/* @lua input.bindings(action: string): string[]
 * Current bindings of an action.
 */
static int i_bindings(lua_State *L) {
  zb_script *s = S(L);
  int id = action_arg(L, 1);
  lua_newtable(L);
  for (size_t i = 0; i < zb_input_binding_count(s->cfg.input, id); ++i) {
    lua_pushstring(L, zb_input_binding(s->cfg.input, id, i));
    lua_rawseti(L, -2, (lua_Integer)i + 1);
  }
  return 1;
}

/* @lua input.state(action: string): ActionState
 * The action's snapshot for this tick.
 */
static int i_state(lua_State *L) {
  zb_script *s = S(L);
  const zb_action_state *st = zb_input_state(s->cfg.input, action_arg(L, 1));
  lua_newtable(L);
  lua_pushboolean(L, st->pressed);
  lua_setfield(L, -2, "pressed");
  lua_pushboolean(L, st->released);
  lua_setfield(L, -2, "released");
  lua_pushboolean(L, st->held);
  lua_setfield(L, -2, "held");
  lua_pushnumber(L, st->x);
  lua_setfield(L, -2, "x");
  lua_pushnumber(L, st->y);
  lua_setfield(L, -2, "y");
  return 1;
}

/* @lua input.mode(): "desktop"|"touch"
 * The current input mode.
 */
static int i_mode(lua_State *L) {
  lua_pushstring(L, zb_input_get_mode(S(L)->cfg.input) == ZB_INPUT_TOUCH ? "touch" : "desktop");
  return 1;
}

/* @lua input.add_stick(name: string, x: number, y: number, radius: number)
 * Adds an on-screen stick (normalised screen coordinates) for "touch:stick:<name>" bindings.
 */
static int i_add_stick(lua_State *L) {
  zb_script *s = S(L);
  int id = zb_input_add_stick(s->cfg.input, luaL_checkstring(L, 1), (float)luaL_checknumber(L, 2),
                              (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
  if (id < 0) {
    return luaL_error(L, "cannot add stick '%s'", lua_tostring(L, 1));
  }
  return 0;
}

/* @lua input.add_button(name: string, x: number, y: number, w: number, h: number)
 * Adds an on-screen button for "touch:button:<name>" bindings.
 */
static int i_add_button(lua_State *L) {
  zb_script *s = S(L);
  int id =
      zb_input_add_button(s->cfg.input, luaL_checkstring(L, 1), (float)luaL_checknumber(L, 2),
                          (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5));
  if (id < 0) {
    return luaL_error(L, "cannot add button '%s'", lua_tostring(L, 1));
  }
  return 0;
}

/* @lua input.set_user_binding(action: string, bindings: string[]): boolean
 * Replaces the player's bindings of an action and saves the configuration; returns whether saving succeeded.
 */
static int i_set_user_binding(lua_State *L) {
  zb_script *s = S(L);
  int id = action_arg(L, 1);
  luaL_checktype(L, 2, LUA_TTABLE);
  const char *list[ZB_INPUT_MAX_BINDINGS];
  size_t n = (size_t)luaL_len(L, 2);
  luaL_argcheck(L, n <= ZB_INPUT_MAX_BINDINGS, 2, "too many bindings");
  for (size_t i = 0; i < n; ++i) {
    lua_rawgeti(L, 2, (lua_Integer)i + 1);
    list[i] = luaL_checkstring(L, -1);
    lua_pop(L, 1); /* strings stay alive in the table */
  }
  if (zb_input_set_user_binding(s->cfg.input, id, list, n) != ZB_OK) {
    return luaL_error(L, "invalid binding for action '%s'", lua_tostring(L, 1));
  }
  zb_result r = s->cfg.host.save_bindings ? s->cfg.host.save_bindings(s->cfg.host.ctx) : ZB_OK;
  lua_pushboolean(L, r == ZB_OK);
  return 1;
}

/* @lua input.inject(action: string, x?: number, y?: number, ticks?: integer)
 * Test API: overrides an action's value for the next ticks (default 1, 0, 1).
 */
static int i_inject(lua_State *L) {
  zb_script *s = S(L);
  int id = action_arg(L, 1);
  zb_input_inject(s->cfg.input, id, (float)luaL_optnumber(L, 2, 1.0), (float)luaL_optnumber(L, 3, 0.0),
                  (unsigned)luaL_optinteger(L, 4, 1));
  return 0;
}

/* ---- render ----------------------------------------------------------------------- */

static int r_load(lua_State *L, uint32_t (*fn)(zb_render *, const char *)) {
  lua_pushinteger(L, (lua_Integer)fn(S(L)->cfg.render, luaL_checkstring(L, 1)));
  return 1;
}

/* @lua render.model(path: string): integer
 * Starts loading a glTF model (.glb/.gltf); returns its handle.
 */
static int r_model(lua_State *L) { return r_load(L, zb_render_load_model); }
/* @lua render.texture(path: string): integer
 * Starts loading a PNG texture; returns its handle.
 */
static int r_texture(lua_State *L) { return r_load(L, zb_render_load_texture); }
/* @lua render.sprite(path: string): integer
 * Starts loading a sprite descriptor (texture, frame size, frames, fps, pivot, size).
 */
static int r_sprite(lua_State *L) { return r_load(L, zb_render_load_sprite); }
/* @lua render.font(path: string): integer
 * Starts loading a bitmap-font descriptor.
 */
static int r_font(lua_State *L) { return r_load(L, zb_render_load_font); }

/* @lua render.state(handle: integer): "pending"|"ready"|"failed"
 * Loading state of an asset; failed assets draw placeholders.
 */
static int r_state(lua_State *L) {
  zb_asset_state st = zb_render_asset_state(S(L)->cfg.render, (uint32_t)luaL_checkinteger(L, 1));
  lua_pushstring(L, st == ZB_ASSET_READY ? "ready" : (st == ZB_ASSET_PENDING ? "pending" : "failed"));
  return 1;
}

/* Reads {x, y, z} at idx (missing components keep their defaults). */
static zb_vec3 vec_at(lua_State *L, int idx, zb_vec3 def) {
  idx = lua_absindex(L, idx);
  lua_rawgeti(L, idx, 1);
  lua_rawgeti(L, idx, 2);
  lua_rawgeti(L, idx, 3);
  zb_vec3 v = zb_v3((float)luaL_optnumber(L, -3, def.x), (float)luaL_optnumber(L, -2, def.y),
                    (float)luaL_optnumber(L, -1, def.z));
  lua_pop(L, 3);
  return v;
}

static zb_vec3 vec_field(lua_State *L, int t, const char *key, zb_vec3 def) {
  zb_vec3 v = def;
  if (lua_getfield(L, t, key) == LUA_TTABLE) {
    v = vec_at(L, -1, def);
  } else if (lua_isnumber(L, -1)) {
    float f = (float)lua_tonumber(L, -1);
    v = zb_v3(f, f, f);
  }
  lua_pop(L, 1);
  return v;
}

static float num_field(lua_State *L, int t, const char *key, float def) {
  lua_getfield(L, t, key);
  float v = (float)luaL_optnumber(L, -1, def);
  lua_pop(L, 1);
  return v;
}

/* @lua render.camera(camera: Camera)
 * Sets the camera for the following frames.
 */
static int r_camera(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  zb_render_set_camera(S(L)->cfg.render, vec_field(L, 1, "eye", zb_v3(0, 1.5f, 4)),
                       vec_field(L, 1, "target", zb_v3(0, 0, 0)), vec_field(L, 1, "up", zb_v3(0, 1, 0)),
                       num_field(L, 1, "fov", 60.0f), num_field(L, 1, "near", 0.1f), num_field(L, 1, "far", 500.0f));
  return 0;
}

/* @lua render.light(light: Light)
 * Sets the directional light and ambient term.
 */
static int r_light(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  zb_render_set_light(S(L)->cfg.render, vec_field(L, 1, "direction", zb_v3(-0.4f, -1, -0.3f)),
                      vec_field(L, 1, "color", zb_v3(1, 1, 1)), vec_field(L, 1, "ambient", zb_v3(0.3f, 0.3f, 0.3f)));
  return 0;
}

/* @lua render.atmosphere(atmosphere: Atmosphere|false)
 * Sky and fog. The engine keeps the horizon's up direction and dip in step with the camera
 * over the planet. false switches both off.
 */
static int r_atmosphere(lua_State *L) {
  zb_render *r = S(L)->cfg.render;
  zb_atmosphere a = *zb_render_atmosphere(r);
  if (lua_isboolean(L, 1) && !lua_toboolean(L, 1)) {
    a.enabled = false;
    zb_render_set_atmosphere(r, &a);
    return 0;
  }
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_getfield(L, 1, "enabled");
  a.enabled = lua_isnil(L, -1) ? true : lua_toboolean(L, -1);
  lua_pop(L, 1);
  a.zenith = vec_field(L, 1, "zenith", zb_v3(0.25f, 0.45f, 0.8f));
  a.horizon = vec_field(L, 1, "horizon", zb_v3(0.7f, 0.8f, 0.9f));
  a.sun_dir = zb_v3_normalize(vec_field(L, 1, "sun", zb_v3(0.3f, 0.6f, -0.7f)));
  a.sun_color = vec_field(L, 1, "sun_color", zb_v3(1.0f, 0.95f, 0.8f));
  if (zb_v3_dot(a.up, a.up) < 0.5f) {
    a.up = zb_v3(0.0f, 1.0f, 0.0f);
  }
  a.fog_density = zb_fog_density_for(num_field(L, 1, "fog_distance", 0.0f), 0.98f);
  zb_render_set_atmosphere(r, &a);
  return 0;
}

/* @lua render.palette(palette: Palette|false)
 * Quantises the scene to the palette with ordered dithering (text stays as drawn); false
 * switches it off.
 */
static int r_palette(lua_State *L) {
  zb_render *r = S(L)->cfg.render;
  if (lua_isboolean(L, 1) && !lua_toboolean(L, 1)) {
    zb_render_set_palette(r, 0, 0, 0.0f);
    return 0;
  }
  const char *lut = "palettes/base_lut.png", *bayer = "palettes/bayer8.png";
  float dither = 0.08f;
  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "lut");
    lut = luaL_optstring(L, -1, lut);
    lua_getfield(L, 1, "bayer");
    bayer = luaL_optstring(L, -1, bayer);
    dither = num_field(L, 1, "dither", dither);
  }
  zb_render_set_palette(r, zb_render_load_texture(r, lut), zb_render_load_texture(r, bayer), dither);
  if (lua_istable(L, 1)) {
    lua_pop(L, 2);
  }
  return 0;
}

/* @lua render.clear_color(r: number, g: number, b: number)
 * Background colour.
 */
static int r_clear_color(lua_State *L) {
  zb_render_set_clear_color(S(L)->cfg.render, (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2),
                            (float)luaL_checknumber(L, 3));
  return 0;
}

/* @lua render.internal_height(height?: integer): integer
 * Gets or sets the internal rendering height (default 360).
 */
static int r_internal_height(lua_State *L) {
  zb_script *s = S(L);
  if (!lua_isnoneornil(L, 1)) {
    zb_render_set_internal_height(s->cfg.render, (int)luaL_checkinteger(L, 1));
  }
  lua_pushinteger(L, zb_render_internal_height(s->cfg.render));
  return 1;
}

/* @lua render.draw_model(model: integer, transform?: Transform)
 * Draws a model this frame (call from a frame hook).
 */
static int r_draw_model(lua_State *L) {
  uint32_t h = (uint32_t)luaL_checkinteger(L, 1);
  zb_mat4 m = zb_m4_identity();
  if (lua_istable(L, 2)) {
    zb_vec3 pos = vec_field(L, 2, "position", zb_v3(0, 0, 0));
    zb_vec3 rot = vec_field(L, 2, "rotation", zb_v3(0, 0, 0)); /* Euler degrees, Y-X-Z */
    zb_vec3 scale = vec_field(L, 2, "scale", zb_v3(1, 1, 1));
    float d = ZB_PI / 180.0f;
    zb_quat q = zb_quat_mul(
        zb_quat_axis_angle(zb_v3(0, 1, 0), rot.y * d),
        zb_quat_mul(zb_quat_axis_angle(zb_v3(1, 0, 0), rot.x * d), zb_quat_axis_angle(zb_v3(0, 0, 1), rot.z * d)));
    m = zb_m4_trs(pos, q, scale);
  }
  zb_render_draw_model(S(L)->cfg.render, h, m);
  return 0;
}

/* @lua render.draw_sprite(sprite: integer, position: number[], scale?: number, time?: number)
 * Draws a camera-facing sprite; the animation frame follows time (default: simulation time).
 */
static int r_draw_sprite(lua_State *L) {
  zb_script *s = S(L);
  uint32_t h = (uint32_t)luaL_checkinteger(L, 1);
  luaL_checktype(L, 2, LUA_TTABLE);
  zb_vec3 pos = vec_at(L, 2, zb_v3(0, 0, 0));
  float scale = (float)luaL_optnumber(L, 3, 1.0);
  double t = luaL_optnumber(L, 4, s->cfg.host.sim_time ? s->cfg.host.sim_time(s->cfg.host.ctx) : 0.0);
  zb_render_draw_sprite(s->cfg.render, h, pos, scale, t);
  return 0;
}

/* @lua render.draw_text(font: integer, text: string, x: number, y: number, scale?: number, color?: number[])
 * Draws UTF-8 text in internal pixels (origin top-left); missing characters use the replacement glyph.
 */
static int r_draw_text(lua_State *L) {
  zb_script *s = S(L);
  uint32_t font = (uint32_t)luaL_checkinteger(L, 1);
  const char *text = luaL_checkstring(L, 2);
  float x = (float)luaL_checknumber(L, 3), y = (float)luaL_checknumber(L, 4);
  float scale = (float)luaL_optnumber(L, 5, 1.0);
  float rgba[4] = {1, 1, 1, 1};
  if (lua_istable(L, 6)) {
    for (int i = 0; i < 4; ++i) {
      lua_rawgeti(L, 6, i + 1);
      rgba[i] = (float)luaL_optnumber(L, -1, 1.0);
      lua_pop(L, 1);
    }
  }
  zb_render_draw_text(s->cfg.render, font, text, x, y, scale, rgba);
  return 0;
}

/* @lua render.stats(): RenderStats
 * Layout and draw statistics of the last frame.
 */
static int r_stats(lua_State *L) {
  const zb_render_stats *st = zb_render_get_stats(S(L)->cfg.render);
  lua_newtable(L);
  lua_pushinteger(L, st->layout.internal_w);
  lua_setfield(L, -2, "internal_width");
  lua_pushinteger(L, st->layout.internal_h);
  lua_setfield(L, -2, "internal_height");
  lua_pushinteger(L, st->layout.scale);
  lua_setfield(L, -2, "scale");
  lua_pushinteger(L, st->layout.out_x);
  lua_setfield(L, -2, "out_x");
  lua_pushinteger(L, st->layout.out_y);
  lua_setfield(L, -2, "out_y");
  lua_pushinteger(L, st->layout.out_w);
  lua_setfield(L, -2, "out_width");
  lua_pushinteger(L, st->layout.out_h);
  lua_setfield(L, -2, "out_height");
  lua_pushinteger(L, st->layout.drawable_w);
  lua_setfield(L, -2, "drawable_width");
  lua_pushinteger(L, st->layout.drawable_h);
  lua_setfield(L, -2, "drawable_height");
  lua_pushinteger(L, st->views);
  lua_setfield(L, -2, "views");
  lua_pushinteger(L, (lua_Integer)st->model_items);
  lua_setfield(L, -2, "model_draws");
  lua_pushinteger(L, (lua_Integer)st->sprite_items);
  lua_setfield(L, -2, "sprite_draws");
  lua_pushinteger(L, (lua_Integer)st->terrain_items);
  lua_setfield(L, -2, "terrain_items");
  lua_pushinteger(L, (lua_Integer)st->terrain_triangles);
  lua_setfield(L, -2, "terrain_triangles");
  lua_pushinteger(L, (lua_Integer)st->text_glyphs);
  lua_setfield(L, -2, "glyphs");
  lua_pushinteger(L, (lua_Integer)st->culled);
  lua_setfield(L, -2, "culled");
  lua_pushinteger(L, (lua_Integer)st->frames);
  lua_setfield(L, -2, "frames");
  lua_pushstring(L, zb_render_backend_of(S(L)->cfg.render)->name);
  lua_setfield(L, -2, "backend");
  return 1;
}

/* @lua render.sprite_frame(sprite: integer, time: number): integer
 * Number of animation frames the sprite has advanced at time.
 */
static int r_sprite_frame(lua_State *L) {
  lua_pushinteger(L, (lua_Integer)zb_render_sprite_frame_number(S(L)->cfg.render, (uint32_t)luaL_checkinteger(L, 1),
                                                                luaL_checknumber(L, 2)));
  return 1;
}

/* @lua render.text_glyphs(font: integer, text: string): integer[]
 * Glyph indices the font uses for text.
 */
static int r_text_glyphs(lua_State *L) {
  uint32_t glyphs[256];
  size_t n = zb_render_text_glyphs(S(L)->cfg.render, (uint32_t)luaL_checkinteger(L, 1), luaL_checkstring(L, 2), glyphs,
                                   ZB_ARRAY_LEN(glyphs));
  lua_newtable(L);
  for (size_t i = 0; i < n; ++i) {
    lua_pushinteger(L, glyphs[i]);
    lua_rawseti(L, -2, (lua_Integer)i + 1);
  }
  return 1;
}

/* @lua audio.play(path: string, options?: PlayOptions): integer
 * Plays a WAV or OGG file; returns a voice handle (0 when all voices are busy).
 */
static int a_play(lua_State *L) {
  zb_script *s = S(L);
  const char *path = luaL_checkstring(L, 1);
  zb_play_desc d = zb_play_defaults();
  if (lua_istable(L, 2)) {
    d.volume = num_field(L, 2, "volume", 1.0f);
    d.pitch = num_field(L, 2, "pitch", 1.0f);
    lua_getfield(L, 2, "loop");
    d.loop = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "group");
    d.group = (lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), "music") == 0) ? ZB_AUDIO_MUSIC : ZB_AUDIO_EFFECTS;
    lua_pop(L, 1);
  }
  lua_pushinteger(L, (lua_Integer)zb_audio_play(s->cfg.audio, path, &d));
  return 1;
}

/* @lua audio.stop(voice: integer)
 * Stops a voice.
 */
static int a_stop(lua_State *L) {
  zb_audio_stop(S(L)->cfg.audio, (uint32_t)luaL_checkinteger(L, 1));
  return 0;
}

/* @lua audio.state(voice: integer): "playing"|"stopped"
 * Whether a voice still plays.
 */
static int a_state(lua_State *L) {
  zb_voice_state st = zb_audio_voice_state(S(L)->cfg.audio, (uint32_t)luaL_checkinteger(L, 1));
  lua_pushstring(L, st == ZB_VOICE_PLAYING ? "playing" : "stopped");
  return 1;
}

/* @lua audio.set_volume(voice: integer, volume: number)
 * Changes a voice's volume.
 */
static int a_set_volume(lua_State *L) {
  zb_audio_set_voice_volume(S(L)->cfg.audio, (uint32_t)luaL_checkinteger(L, 1), (float)luaL_checknumber(L, 2));
  return 0;
}

/* @lua audio.group_volume(group: "master"|"music"|"effects", volume?: number): number?
 * Gets or sets a volume group (setting "master" returns nothing).
 */
static int a_group_volume(lua_State *L) {
  zb_script *s = S(L);
  const char *g = luaL_checkstring(L, 1);
  zb_audio_group group = strcmp(g, "music") == 0 ? ZB_AUDIO_MUSIC : ZB_AUDIO_EFFECTS;
  if (strcmp(g, "master") == 0) {
    zb_audio_set_master_volume(s->cfg.audio, (float)luaL_checknumber(L, 2));
    return 0;
  }
  if (!lua_isnoneornil(L, 2)) {
    zb_audio_set_group_volume(s->cfg.audio, group, (float)luaL_checknumber(L, 2));
  }
  lua_pushnumber(L, zb_audio_group_volume(s->cfg.audio, group));
  return 1;
}

/* @lua audio.voices(): integer
 * Number of active voices.
 */
static int a_voices(lua_State *L) {
  lua_pushinteger(L, (lua_Integer)zb_audio_active_voices(S(L)->cfg.audio));
  return 1;
}

/* @lua audio.decoder_bytes(voice: integer): integer
 * Decoder memory of a streaming voice.
 */
static int a_decoder_bytes(lua_State *L) {
  lua_pushinteger(L, (lua_Integer)zb_audio_voice_decoder_bytes(S(L)->cfg.audio, (uint32_t)luaL_checkinteger(L, 1)));
  return 1;
}

/* @lua audio.locked(): boolean
 * Web: true until the first user gesture allows audio.
 */
static int a_locked(lua_State *L) {
  lua_pushboolean(L, zb_audio_is_locked(S(L)->cfg.audio));
  return 1;
}

/* ---- save ---------------------------------------------------------------------------- */

static void push_save_table(lua_State *L) {
  if (lua_getfield(L, LUA_REGISTRYINDEX, REG_SAVE) == LUA_TTABLE) {
    return;
  }
  lua_pop(L, 1);
  zb_script *s = S(L);
  lua_newtable(L);
  char path[1100];
  snprintf(path, sizeof path, "%s%s", s->cfg.user_dir ? s->cfg.user_dir : "", SAVE_FILE);
  char *text = NULL;
  size_t len = 0;
  if (s->cfg.fs && s->cfg.fs->read(s->cfg.fs->ctx, path, s->cfg.a, &text, &len) == ZB_OK) {
    zb_luon_doc doc;
    zb_luon_error err;
    if (zb_luon_parse(&doc, s->cfg.a, text, len, &err) == ZB_OK && zb_value_is(&doc.root, ZB_VAL_TABLE)) {
      lua_pop(L, 1);
      push_luon(L, &doc.root);
      zb_luon_free(&doc);
    } else {
      zb_warn("save data %s is corrupt (%d: %s); starting empty", path, err.line, err.message);
    }
    zb_free(s->cfg.a, text);
  }
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_REGISTRYINDEX, REG_SAVE);
}

/* @lua save.get(key: string): any
 * Reads a value from the player's save data.
 */
static int s_get(lua_State *L) {
  const char *key = luaL_checkstring(L, 1);
  push_save_table(L);
  lua_getfield(L, -1, key);
  return 1;
}

/* @lua save.set(key: string, value: nil|boolean|number|string|table)
 * Stores a value (nested tables of plain values) in the save data.
 */
static int s_set(lua_State *L) {
  const char *key = luaL_checkstring(L, 1);
  int t = lua_type(L, 2);
  luaL_argcheck(L, t == LUA_TNIL || t == LUA_TBOOLEAN || t == LUA_TNUMBER || t == LUA_TSTRING || t == LUA_TTABLE, 2,
                "nil, boolean, number, string or table expected");
  push_save_table(L);
  lua_pushvalue(L, 2);
  lua_setfield(L, -2, key);
  return 0;
}

static void write_value(lua_State *L, int idx, luaL_Buffer *b, int depth) {
  idx = lua_absindex(L, idx);
  switch (lua_type(L, idx)) {
    case LUA_TBOOLEAN: luaL_addstring(b, lua_toboolean(L, idx) ? "true" : "false"); break;
    case LUA_TNUMBER: {
      char tmp[64];
      if (lua_isinteger(L, idx)) {
        snprintf(tmp, sizeof tmp, "%lld", (long long)lua_tointeger(L, idx));
      } else {
        snprintf(tmp, sizeof tmp, "%.17g", (double)lua_tonumber(L, idx));
      }
      luaL_addstring(b, tmp);
      break;
    }
    case LUA_TSTRING: {
      lua_getglobal(L, "string");
      lua_getfield(L, -1, "format");
      lua_pushstring(L, "%q");
      lua_pushvalue(L, idx);
      lua_call(L, 2, 1);
      lua_remove(L, -2);
      luaL_addvalue(b);
      break;
    }
    case LUA_TTABLE:
      if (depth > 8) {
        luaL_addstring(b, "nil");
        break;
      }
      luaL_addstring(b, "{");
      lua_pushnil(L);
      while (lua_next(L, idx)) {
        int kt = lua_type(L, -2), vt = lua_type(L, -1);
        if ((kt == LUA_TSTRING || kt == LUA_TNUMBER) &&
            (vt == LUA_TBOOLEAN || vt == LUA_TNUMBER || vt == LUA_TSTRING || vt == LUA_TTABLE)) {
          luaL_addstring(b, "[");
          write_value(L, -2, b, depth + 1);
          luaL_addstring(b, "]=");
          write_value(L, -1, b, depth + 1);
          luaL_addstring(b, ",");
        }
        lua_pop(L, 1);
      }
      luaL_addstring(b, "}");
      break;
    default: luaL_addstring(b, "nil"); break;
  }
}

/* @lua save.flush(): boolean
 * Writes the save data to the user data directory.
 */
static int s_flush(lua_State *L) {
  zb_script *s = S(L);
  push_save_table(L);
  int t = lua_gettop(L);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addstring(&b, "return ");
  write_value(L, t, &b, 0);
  luaL_addstring(&b, "\n");
  luaL_pushresult(&b);
  size_t len;
  const char *text = lua_tolstring(L, -1, &len);
  char path[1100];
  snprintf(path, sizeof path, "%s%s", s->cfg.user_dir ? s->cfg.user_dir : "", SAVE_FILE);
  zb_result r = s->cfg.fs ? s->cfg.fs->write(s->cfg.fs->ctx, path, text, len) : ZB_ERR_UNSUPPORTED;
  if (r != ZB_OK) {
    zb_warn("could not write save data to %s (%s)", path, zb_result_str(r));
  } else if (s->cfg.host.user_data_changed) {
    s->cfg.host.user_data_changed(s->cfg.host.ctx);
  }
  lua_pushboolean(L, r == ZB_OK);
  return 1;
}

/* ---- registration -------------------------------------------------------------------- */

static void reg(lua_State *L, const char *name, const luaL_Reg *fns) {
  lua_newtable(L);
  luaL_setfuncs(L, fns, 0);
  lua_setglobal(L, name);
}

void zb_script_open_api(zb_script *s) {
  lua_State *L = s->L;
  static const luaL_Reg engine_fns[] = {{"info", e_info},
                                        {"quit", e_quit},
                                        {"time", e_time},
                                        {"tick", e_tick},
                                        {"pause", e_pause},
                                        {"set_time_scale", e_time_scale},
                                        {"set_fullscreen", e_fullscreen},
                                        {"log", e_log},
                                        {"hash_state", e_hash_state},
                                        {"packages", e_packages},
                                        {"credits", e_credits},
                                        {NULL, NULL}};
  luaL_newlib(L, engine_fns);
  lua_pushstring(L, ZB_VERSION);
  lua_setfield(L, -2, "version");
  lua_pushstring(L, ZB_API_VERSION);
  lua_setfield(L, -2, "api_version");
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_REGISTRYINDEX, ZB_SCRIPT_REGISTRY_ENGINE);
  lua_setglobal(L, "engine");
  static const luaL_Reg vfs_fns[] = {
      {"exists", v_exists}, {"read", v_read}, {"list", v_list}, {"load", v_load}, {NULL, NULL}};
  reg(L, "vfs", vfs_fns);
  static const luaL_Reg input_fns[] = {{"define", i_define},
                                       {"bind", i_bind},
                                       {"clear", i_clear},
                                       {"bindings", i_bindings},
                                       {"state", i_state},
                                       {"mode", i_mode},
                                       {"add_stick", i_add_stick},
                                       {"add_button", i_add_button},
                                       {"set_user_binding", i_set_user_binding},
                                       {NULL, NULL}};
  reg(L, "input", input_fns);
  if (s->cfg.test_api) {
    lua_getglobal(L, "engine");
    lua_pushcfunction(L, e_resize_window);
    lua_setfield(L, -2, "resize_window");
    lua_pop(L, 1);
    lua_getglobal(L, "input");
    lua_pushcfunction(L, i_inject);
    lua_setfield(L, -2, "inject");
    lua_pop(L, 1);
  }
  static const luaL_Reg render_fns[] = {{"model", r_model},
                                        {"texture", r_texture},
                                        {"sprite", r_sprite},
                                        {"font", r_font},
                                        {"state", r_state},
                                        {"camera", r_camera},
                                        {"light", r_light},
                                        {"clear_color", r_clear_color},
                                        {"atmosphere", r_atmosphere},
                                        {"palette", r_palette},
                                        {"internal_height", r_internal_height},
                                        {"draw_model", r_draw_model},
                                        {"draw_sprite", r_draw_sprite},
                                        {"draw_text", r_draw_text},
                                        {"stats", r_stats},
                                        {"sprite_frame", r_sprite_frame},
                                        {"text_glyphs", r_text_glyphs},
                                        {NULL, NULL}};
  reg(L, "render", render_fns);
  static const luaL_Reg audio_fns[] = {{"play", a_play},
                                       {"stop", a_stop},
                                       {"state", a_state},
                                       {"set_volume", a_set_volume},
                                       {"group_volume", a_group_volume},
                                       {"voices", a_voices},
                                       {"decoder_bytes", a_decoder_bytes},
                                       {"locked", a_locked},
                                       {NULL, NULL}};
  reg(L, "audio", audio_fns);
  static const luaL_Reg save_fns[] = {{"get", s_get}, {"set", s_set}, {"flush", s_flush}, {NULL, NULL}};
  reg(L, "save", save_fns);
  zb_script_open_world(s);
}

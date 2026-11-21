/* SPDX-License-Identifier: MIT */
/* Saved games: a folder per world under saves/ in the user folder,
 *
 *   saves/<name>/world.lua      format, kind, seed, radius, generator, time
 *   saves/<name>/bundle.zbw     the created world (world creation writes it)
 *   saves/<name>/player.lua     whatever the scripts keep about the player
 *   saves/<name>/bodies.lua     the rigid bodies (as sim.spawn takes them)
 *   saves/<name>/regions/     the players' edits, a .bin file per 16 x 16 chunks
 *
 * Every file is written atomically. Saves of older formats are migrated when
 * loaded: format 0 is a created world kept as worlds/<name>.zbw before saves
 * existed, which moves into saves/<name>/. */
#include "script/script_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/fs.h"
#include "core/log.h"
#include "world/bundle.h"
#include "world/delta.h"

#define SAVE_FORMAT 1

static zb_script *S(lua_State *L) { return zb_script_from(L); }

static int fail2(lua_State *L, const char *why) {
  lua_pushnil(L);
  lua_pushstring(L, why);
  return 2;
}

static bool save_path(zb_script *s, const char *name, const char *file, char *out, size_t len) {
  int n = snprintf(out, len, "%ssaves/%s/%s", s->cfg.user_dir ? s->cfg.user_dir : "", name, file);
  return n > 0 && (size_t)n < len;
}

static zb_result write_text(zb_script *s, const char *name, const char *file, const char *text, size_t len) {
  char path[600];
  if (!save_path(s, name, file, path, sizeof path)) {
    return ZB_ERR_INVALID;
  }
  return s->cfg.fs->write(s->cfg.fs->ctx, path, text, len);
}

typedef struct region_save {
  zb_script *s;
  const char *name;
  const zb_deltas *deltas;
  zb_result result;
  int written;
} region_save;

static void save_region(void *ctx, int face, int32_t rx, int32_t ry) {
  region_save *rs = ctx;
  if (rs->result != ZB_OK) {
    return;
  }
  char file[96], rel[128];
  zb_region_file_name(face, rx, ry, file, sizeof file);
  snprintf(rel, sizeof rel, "regions/%s", file);
  zb_array image;
  zb_array_init(&image, rs->s->cfg.a, 1);
  rs->result = zb_deltas_write_region(rs->deltas, face, rx, ry, &image);
  if (rs->result == ZB_OK) {
    rs->result = write_text(rs->s, rs->name, rel, (const char *)image.data, image.len);
    rs->written++;
  }
  zb_array_free(&image);
}

/* world.lua, the bundle if it is not there yet, player.lua (if player_idx is a
 * table) and every region with edits. */
static zb_result save_game(lua_State *L, const char *name, int player_idx, int *regions) {
  zb_script *s = S(L);
  zb_world *w = s->world;
  const zb_world_bundle *b = zb_world_get_bundle(w);
  char text[512];
  int n =
      snprintf(text, sizeof text,
               "-- A saved world of Zauberer.\nreturn {\n  format = %d,\n"
               "  kind = \"%s\",\n  seed = \"%016llx\",\n  radius = %.17g,\n  generator = %d,\n  time = %.17g,\n}\n",
               SAVE_FORMAT, b ? "created" : "noise", (unsigned long long)(b ? b->options.seed : s->cfg.seed),
               zb_world_planet(w)->radius, ZB_GENERATOR_VERSION, zb_world_time(w));
  zb_result r = write_text(s, name, "world.lua", text, (size_t)n);
  char path[600];
  zb_fs_stat st;
  if (r == ZB_OK && b && save_path(s, name, "bundle.zbw", path, sizeof path) &&
      s->cfg.fs->stat(s->cfg.fs->ctx, path, &st) != ZB_OK) {
    zb_array image;
    zb_array_init(&image, s->cfg.a, 1);
    r = zb_world_bundle_write(b, &image);
    if (r == ZB_OK) {
      r = s->cfg.fs->write(s->cfg.fs->ctx, path, image.data, image.len);
    }
    zb_array_free(&image);
  }
  if (r == ZB_OK && player_idx && lua_istable(L, player_idx)) {
    luaL_Buffer buf;
    luaL_buffinit(L, &buf);
    luaL_addstring(&buf, "-- The player of a saved world of Zauberer.\nreturn ");
    zb_script_write_luon(L, player_idx, &buf, 0);
    luaL_addstring(&buf, "\n");
    luaL_pushresult(&buf);
    size_t len;
    const char *t = lua_tolstring(L, -1, &len);
    r = write_text(s, name, "player.lua", t, len);
    lua_pop(L, 1);
  }
  if (r == ZB_OK) { /* the rigid bodies, as sim.spawn takes them */
    zb_script_push_bodies(L);
    luaL_Buffer buf;
    luaL_buffinit(L, &buf);
    luaL_addstring(&buf, "-- The rigid bodies of a saved world of Zauberer.\nreturn ");
    zb_script_write_luon(L, lua_gettop(L) - 1, &buf, 0);
    luaL_addstring(&buf, "\n");
    luaL_pushresult(&buf);
    size_t len;
    const char *t = lua_tolstring(L, -1, &len);
    r = write_text(s, name, "bodies.lua", t, len);
    lua_pop(L, 2);
  }
  region_save rs = {s, name, zb_world_deltas(w), ZB_OK, 0};
  if (r == ZB_OK) {
    r = zb_deltas_each_region(rs.deltas, save_region, &rs);
    r = r == ZB_OK ? rs.result : r;
  }
  if (r == ZB_OK) {
    zb_deltas_clear_dirty(zb_world_deltas(w));
  }
  if (s->cfg.host.user_data_changed) {
    s->cfg.host.user_data_changed(s->cfg.host.ctx);
  }
  *regions = rs.written;
  return r;
}

/* @lua world.save_game(name: string, player?: table): boolean, string?
 * Saves the world being played under saves/<name>/: its time, the players'
 * edits and a player table (literal values only) for the scripts.
 */
static int w_save_game(lua_State *L) {
  zb_script *s = S(L);
  const char *name = luaL_checkstring(L, 1);
  if (!lua_isnoneornil(L, 2)) {
    luaL_checktype(L, 2, LUA_TTABLE);
  }
  if (!zb_script_valid_name(name)) {
    lua_pushboolean(L, false);
    lua_pushstring(L, "save names use letters, digits, - and _");
    return 2;
  }
  if (!s->cfg.fs) {
    lua_pushboolean(L, false);
    lua_pushstring(L, "no file system");
    return 2;
  }
  int regions = 0;
  zb_result r = save_game(L, name, lua_isnoneornil(L, 2) ? 0 : 2, &regions);
  if (r != ZB_OK) {
    lua_pushboolean(L, false);
    lua_pushstring(L, zb_result_str(r));
    return 2;
  }
  zb_info("saved the game %s (%d region(s) of edits)", name, regions);
  lua_pushboolean(L, true);
  return 1;
}

typedef struct region_load {
  zb_script *s;
  const char *dir;
  int read, damaged;
} region_load;

static bool load_region(const char *entry, bool is_dir, void *ctx) {
  region_load *rl = ctx;
  size_t n = strlen(entry);
  if (is_dir || n < 5 || strcmp(entry + n - 4, ".bin") != 0) {
    return true;
  }
  char path[700], err[160];
  snprintf(path, sizeof path, "%s%s", rl->dir, entry);
  char *data = NULL;
  size_t len = 0;
  if (rl->s->cfg.fs->read(rl->s->cfg.fs->ctx, path, rl->s->cfg.a, &data, &len) != ZB_OK) {
    return true;
  }
  if (zb_deltas_read_region(zb_world_deltas(rl->s->world), data, len, err, sizeof err) == ZB_OK) {
    rl->read++;
  } else {
    zb_warn("skipping the edits in %s: %s", path, err);
    rl->damaged++;
  }
  zb_free(rl->s->cfg.a, data);
  return true;
}

/* Reads a LUON file of the save into a document; false if it is missing or broken. */
static bool read_doc(zb_script *s, const char *name, const char *file, zb_luon_doc *doc, char *err, size_t err_len) {
  char path[600];
  char *text = NULL;
  size_t len = 0;
  if (!save_path(s, name, file, path, sizeof path) ||
      s->cfg.fs->read(s->cfg.fs->ctx, path, s->cfg.a, &text, &len) != ZB_OK) {
    snprintf(err, err_len, "no such save");
    return false;
  }
  zb_luon_error le;
  zb_result r = zb_luon_parse(doc, s->cfg.a, text, len, &le);
  zb_free(s->cfg.a, text);
  if (r != ZB_OK) {
    snprintf(err, err_len, "the save is damaged (%s:%d)", file, le.line);
    return false;
  }
  return true;
}

/* @lua world.load_game(name: string): table|boolean|nil, string?
 * Plays the saved world <name> again: its world, time and the players' edits.
 * Returns the player table it was saved with (true if there was none), or nil
 * and a message. Created worlds from before saves (worlds/<name>.zbw) are
 * moved into saves/<name>/.
 */
static int w_load_game(lua_State *L) {
  zb_script *s = S(L);
  const char *name = luaL_checkstring(L, 1);
  char err[160];
  if (!zb_script_valid_name(name)) {
    return fail2(L, "save names use letters, digits, - and _");
  }
  if (!s->cfg.fs) {
    return fail2(L, "no file system");
  }
  zb_luon_doc doc;
  if (!read_doc(s, name, "world.lua", &doc, err, sizeof err)) {
    if (strcmp(err, "no such save") != 0) {
      return fail2(L, err);
    }
    /* Format 0: a created world without a save folder. */
    bool legacy;
    if (!zb_script_load_world(s, name, &legacy, err, sizeof err)) {
      return fail2(L, strcmp(err, "no such world") == 0 ? "no such save" : err);
    }
    int regions;
    zb_result r = save_game(L, name, 0, &regions);
    if (r != ZB_OK) {
      return fail2(L, zb_result_str(r));
    }
    zb_info("moved the world %s into saves/%s/ (save format %d)", name, name, SAVE_FORMAT);
    lua_pushboolean(L, true);
    return 1;
  }
  int format = (int)zb_value_num(zb_table_get(&doc.root, "format"), -1.0);
  int generator = (int)zb_value_num(zb_table_get(&doc.root, "generator"), -1.0);
  const char *kind = zb_value_str(zb_table_get(&doc.root, "kind"), "");
  unsigned long long seed = strtoull(zb_value_str(zb_table_get(&doc.root, "seed"), "0"), NULL, 16);
  double time = zb_value_num(zb_table_get(&doc.root, "time"), -1.0);
  const char *why = NULL;
  if (format > SAVE_FORMAT) {
    why = "the save comes from a newer version of the game";
  } else if (generator > ZB_GENERATOR_VERSION) {
    why = "the world was made by a newer world generator";
  } else if (format < 1 || generator < 1 || time < 0.0 || (strcmp(kind, "created") && strcmp(kind, "noise"))) {
    why = "the save is damaged (world.lua)";
  } else if (!strcmp(kind, "noise") && seed != (unsigned long long)s->cfg.seed) {
    why = "the save belongs to a world with another seed";
  }
  bool created = !strcmp(kind, "created");
  zb_luon_free(&doc); /* kind lived in the document */
  if (why) {
    return fail2(L, why);
  }
  bool legacy = false;
  bool ok = created ? zb_script_load_world(s, name, &legacy, err, sizeof err)
                    : (zb_world_get_bundle(s->world) == NULL || zb_script_adopt_bundle(s, NULL, err, sizeof err));
  if (!ok) {
    return fail2(L, err);
  }
  zb_world_set_time(s->world, time);
  char dir[600];
  save_path(s, name, "regions/", dir, sizeof dir);
  region_load rl = {s, dir, 0, 0};
  s->cfg.fs->list(s->cfg.fs->ctx, dir, load_region, &rl);
  int bodies = 0;
  if (read_doc(s, name, "bodies.lua", &doc, err, sizeof err)) {
    zb_script_push_luon(L, &doc.root);
    zb_luon_free(&doc);
    bodies = zb_script_restore_bodies(L, -1);
    lua_pop(L, 1);
  } else {
    lua_pushnil(L);
    zb_script_restore_bodies(L, -1); /* a save without bodies: none */
    lua_pop(L, 1);
  }
  zb_info("loaded the game %s (%d region(s) of edits%s, %d bod%s)", name, rl.read, rl.damaged ? ", some damaged" : "",
          bodies, bodies == 1 ? "y" : "ies");
  if (read_doc(s, name, "player.lua", &doc, err, sizeof err)) {
    zb_script_push_luon(L, &doc.root);
    zb_luon_free(&doc);
    if (!lua_istable(L, -1)) {
      lua_pop(L, 1);
      lua_pushboolean(L, true);
    }
  } else {
    lua_pushboolean(L, true);
  }
  return 1;
}

typedef struct save_list {
  lua_State *L;
  int n;
} save_list;

static bool list_save(const char *entry, bool is_dir, void *ctx) {
  save_list *sl = ctx;
  if (is_dir && zb_script_valid_name(entry)) {
    lua_pushstring(sl->L, entry);
    lua_rawseti(sl->L, -2, ++sl->n);
  }
  return true;
}

/* @lua world.saves(): string[]
 * The names of the saved games in the user folder, sorted.
 */
static int w_saves(lua_State *L) {
  zb_script *s = S(L);
  lua_newtable(L);
  char dir[600];
  snprintf(dir, sizeof dir, "%ssaves/", s->cfg.user_dir ? s->cfg.user_dir : "");
  save_list sl = {L, 0};
  if (s->cfg.fs) {
    s->cfg.fs->list(s->cfg.fs->ctx, dir, list_save, &sl);
  }
  /* sort with table.sort */
  lua_getglobal(L, "table");
  lua_getfield(L, -1, "sort");
  lua_pushvalue(L, -3);
  lua_call(L, 1, 0);
  lua_pop(L, 1);
  return 1;
}

/* @lua world.delete_save(name: string): boolean, string?
 * Deletes the saved game <name> (its folder under saves/); false and a
 * message for an invalid name or a save that cannot be removed.
 */
static int w_delete_save(lua_State *L) {
  zb_script *s = S(L);
  const char *name = luaL_checkstring(L, 1);
  if (!zb_script_valid_name(name)) {
    return fail2(L, "save names use letters, digits, - and _");
  }
  if (!s->cfg.fs) {
    return fail2(L, "no file system");
  }
  char dir[600];
  snprintf(dir, sizeof dir, "%ssaves/%s", s->cfg.user_dir ? s->cfg.user_dir : "", name);
  if (zb_fs_remove_tree(s->cfg.fs, s->cfg.a, dir) != ZB_OK) {
    return fail2(L, "the save could not be removed");
  }
  if (s->cfg.host.user_data_changed) {
    s->cfg.host.user_data_changed(s->cfg.host.ctx);
  }
  lua_pushboolean(L, 1);
  return 1;
}

/* @lua world.remove_plants(pos: PlanetPos, radius: number): integer
 * Removes the plants within radius metres (up to 256) of a position from the
 * loaded chunks; the edit is kept with the world (and saved). Returns how many.
 */
static int w_remove_plants(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  double radius = luaL_checknumber(L, 2);
  luaL_argcheck(L, radius >= 0.0 && radius <= 256.0, 2, "radius must be 0..256 metres");
  lua_pushinteger(L, zb_world_remove_plants(S(L)->world, &pos, radius));
  return 1;
}

/* @lua world.cut_plant(id: integer): boolean
 * Cuts a plant (its id from world.plants): felled, dug up or picked whole, it
 * is gone until it has grown back (ground cover in weeks, shrubs in months,
 * trees in years); the edit is kept with the world. False when the plant is
 * not loaded or already gone.
 */
static int w_cut_plant(lua_State *L) {
  lua_Integer id = luaL_checkinteger(L, 1);
  luaL_argcheck(L, id >= 0, 1, "not a plant id");
  zb_chunk c = zb_chunk_from_key((uint64_t)(id / ZB_PLANTS_MAX));
  lua_pushboolean(L, zb_world_cut_plant(S(L)->world, c, (uint32_t)(id % ZB_PLANTS_MAX)) == ZB_OK);
  return 1;
}

/* @lua world.set_surface(pos: PlanetPos, material: string)
 * Sets the surface material ("soil", "sand", "rock", "gravel", "snow", "ice",
 * "mud") of the 1 m cell at a position; the edit is kept with the world.
 */
static int w_set_surface(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  static const char *const names[] = {"soil", "sand", "rock", "gravel", "snow", "ice", "mud", NULL};
  int material = luaL_checkoption(L, 2, NULL, names) + 1;
  if (zb_world_set_material(S(L)->world, &pos, material) != ZB_OK) {
    return luaL_error(L, "out of memory");
  }
  return 0;
}

void zb_script_open_save(zb_script *s) {
  static const luaL_Reg fns[] = {{"save_game", w_save_game},
                                 {"load_game", w_load_game},
                                 {"saves", w_saves},
                                 {"delete_save", w_delete_save},
                                 {"remove_plants", w_remove_plants},
                                 {"cut_plant", w_cut_plant},
                                 {"set_surface", w_set_surface},
                                 {NULL, NULL}};
  lua_getglobal(s->L, "world");
  luaL_setfuncs(s->L, fns, 0);
  lua_pop(s->L, 1);
}

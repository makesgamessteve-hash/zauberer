/* SPDX-License-Identifier: MIT */
/* Lua bindings: world creation (deep time, climate, the bundle), saved
 * worlds, what a created world knows about a place (climate, biome, strata,
 * minerals, province), and the time-lapse globe.
 *
 * @luaclass WorldOptions
 * @field seed integer? World seed (default: the engine's)
 * @field radius number? Planet radius in metres (default 2,000 km)
 * @field target string? "earth", "pangaea", "archipelago", "ice world" or "hothouse"
 * @field land number? Share of land (0.35)
 * @field temperature number? Global temperature offset, -10 .. 10 C
 * @field tilt number? Axial tilt in degrees (23)
 * @field quality string? "quick", "normal" or "high"
 *
 * @luaclass CreationProgress
 * @field stage string "deep time", "relief", "climate" or "done"
 * @field label string What is happening, for the player
 * @field fraction number 0..1
 * @field myr_ago number Million years before present (deep time)
 * @field frames integer Time-lapse frames recorded so far
 * @field attempt integer Re-runs of the final phase for Earth-likeness
 * @field done boolean
 *
 * @luaclass WorldInfo
 * @field created boolean A created world (false: the noise world)
 * @field seed integer
 * @field radius number
 * @field target string?
 * @field quality string?
 * @field generator integer? Generator version
 * @field continents integer?
 * @field land number?
 * @field provinces integer?
 *
 * @luaclass Stratum
 * @field rock string
 * @field environment string
 * @field thickness number Metres
 * @field age number Million years before present
 * @field fossil string? What it holds, if anything
 */
#include "script/script_internal.h"

#include <stdio.h>
#include <string.h>

#include "core/math.h"
#include "render/mesh.h"
#include "world/creation.h"
#include "world/dvec.h"
#include "world/water.h"

#define GLOBE_RES 24        /* globe cells per cube-face edge, the creation time-lapse */
#define GLOBE_RES_PLAYED 96 /* ... and the played world (the explorer zooms in on it) */

static zb_script *S(lua_State *L) { return zb_script_from(L); }

/* ---- Creation ------------------------------------------------------------------------------- */

static const char *const TARGETS[] = {"earth", "pangaea", "archipelago", "ice world", "hothouse", NULL};
static const char *const QUALITIES[] = {"quick", "normal", "high", NULL};

static int string_option(lua_State *L, int idx, const char *field, const char *const *names, int fallback) {
  lua_getfield(L, idx, field);
  int r = lua_isnil(L, -1) ? fallback : luaL_checkoption(L, -1, NULL, names);
  lua_pop(L, 1);
  return r;
}

static double number_option(lua_State *L, int idx, const char *field, double fallback) {
  lua_getfield(L, idx, field);
  double v = lua_isnil(L, -1) ? fallback : luaL_checknumber(L, -1);
  lua_pop(L, 1);
  return v;
}

/* Waits for the creation job in flight, if any. */
static void finish_job(zb_script *s) {
  if (s->creation_job) {
    zb_job_wait(s->cfg.jobs, s->creation_job);
    zb_job_release(s->cfg.jobs, s->creation_job);
    s->creation_job = NULL;
  }
}

static void creation_job(void *arg) {
  zb_script *s = arg;
  for (int k = 0; k < s->creation_pieces; ++k) {
    if (zb_world_creation_advance(s->creation)) {
      break;
    }
  }
}

static void release_globe(zb_script *s) {
  if (s->globe_mesh) {
    zb_render_release_mesh(s->cfg.render, s->globe_mesh);
  }
  zb_free(s->cfg.a, s->globe_nearest);
  s->globe_mesh = 0;
  s->globe_nearest = NULL;
  s->globe_frame = -2;
  s->globe_source = NULL;
}

void zb_script_close_create(zb_script *s) {
  finish_job(s);
  if (s->cfg.render) {
    release_globe(s);
  }
  zb_world_creation_destroy(s->creation);
  s->creation = NULL;
}

void zb_script_free_bundle(zb_script *s) {
  if (s->bundle) {
    zb_world_bundle_free(s->bundle);
    zb_free(s->cfg.a, s->bundle);
    s->bundle = NULL;
  }
}

/* @lua world.create(options?: WorldOptions): boolean, string?
 * Starts creating a new world (one at a time); world.create_step does the work.
 */
static int w_create(lua_State *L) {
  zb_script *s = S(L);
  zb_world_options o;
  zb_world_default_options(&o);
  o.seed = s->cfg.seed;
  if (!lua_isnoneornil(L, 1)) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getfield(L, 1, "seed");
    if (!lua_isnil(L, -1)) {
      o.seed = (uint64_t)luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);
    o.radius = number_option(L, 1, "radius", o.radius);
    o.target = (zb_geology_target)string_option(L, 1, "target", TARGETS, (int)o.target);
    o.land_fraction = number_option(L, 1, "land", o.land_fraction);
    o.temperature_offset = number_option(L, 1, "temperature", o.temperature_offset);
    o.axial_tilt = number_option(L, 1, "tilt", o.axial_tilt);
    o.quality = (zb_world_quality)string_option(L, 1, "quality", QUALITIES, (int)o.quality);
  }
  if (s->creation) {
    lua_pushboolean(L, false);
    lua_pushstring(L, "a world is already being created");
    return 2;
  }
  char err[128];
  s->creation = zb_world_creation_create(s->cfg.a, &o, err, sizeof err);
  lua_pushboolean(L, s->creation != NULL);
  if (!s->creation) {
    lua_pushstring(L, err);
    return 2;
  }
  return 1;
}

static void push_progress(lua_State *L, const zb_creation_progress *p) {
  static const char *const stages[] = {"deep time", "relief", "climate", "done"};
  lua_createtable(L, 0, 7);
  lua_pushstring(L, stages[p->stage]);
  lua_setfield(L, -2, "stage");
  lua_pushstring(L, p->label);
  lua_setfield(L, -2, "label");
  lua_pushnumber(L, p->fraction);
  lua_setfield(L, -2, "fraction");
  lua_pushnumber(L, p->myr_ago);
  lua_setfield(L, -2, "myr_ago");
  lua_pushinteger(L, p->frames);
  lua_setfield(L, -2, "frames");
  lua_pushinteger(L, p->attempt);
  lua_setfield(L, -2, "attempt");
  lua_pushboolean(L, p->stage == ZB_CREATION_DONE);
  lua_setfield(L, -2, "done");
}

/* @lua world.create_step(pieces?: integer): CreationProgress?
 * Does some pieces of work (default 1: a deep-time step, an erosion pass, a
 * month of climate); nil when no world is being created.
 */
static int w_create_step(lua_State *L) {
  zb_script *s = S(L);
  lua_Integer pieces = luaL_optinteger(L, 1, 1);
  if (!s->creation) {
    lua_pushnil(L);
    return 1;
  }
  zb_creation_progress p;
  if (s->cfg.jobs) {
    /* On a worker: progress is read only between jobs, so it never races the work. */
    if (s->creation_job && zb_job_get_state(s->cfg.jobs, s->creation_job) < ZB_JOB_DONE) {
      static const char *const labels[] = {"Drifting continents", "Raising mountains, carving valleys",
                                           "Winds, rain and rivers", "Done"};
      memset(&p, 0, sizeof p);
      p.stage = (zb_creation_stage)s->creation_stage;
      p.label = labels[p.stage];
      p.fraction = s->creation_fraction;
      p.myr_ago = s->creation_myr;
      p.frames = s->creation_frames;
      p.attempt = s->creation_attempt;
      push_progress(L, &p);
      lua_pushboolean(L, true);
      lua_setfield(L, -2, "working");
      return 1;
    }
    finish_job(s);
    zb_world_creation_get_progress(s->creation, &p);
    s->creation_frames = p.frames;
    s->creation_fraction = p.fraction;
    s->creation_myr = p.myr_ago;
    s->creation_stage = (int)p.stage;
    s->creation_attempt = p.attempt;
    if (p.stage != ZB_CREATION_DONE) {
      s->creation_pieces = (int)(pieces > 0 ? pieces : 1);
      s->creation_job = zb_job_submit(s->cfg.jobs, creation_job, s, ZB_JOB_LOW);
    }
  } else {
    for (lua_Integer k = 0; k < pieces; ++k) {
      if (zb_world_creation_advance(s->creation)) {
        break;
      }
    }
    zb_world_creation_get_progress(s->creation, &p);
    s->creation_frames = p.frames;
  }
  push_progress(L, &p);
  return 1;
}

/* @lua world.create_cancel()
 * Abandons the world being created.
 */
static int w_create_cancel(lua_State *L) {
  zb_script *s = S(L);
  finish_job(s);
  if (s->cfg.render && s->globe_source == s->creation) {
    release_globe(s);
  }
  zb_world_creation_destroy(s->creation);
  s->creation = NULL;
  return 0;
}

bool zb_script_valid_name(const char *name) {
  for (const char *c = name; *c; ++c) {
    bool ok =
        (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '_';
    if (!ok) {
      return false;
    }
  }
  return name[0] != '\0' && strlen(name) <= 64;
}

/* saves/<name>/bundle.zbw; legacy: worlds/<name>.zbw (world creation before saves). */
static bool bundle_path(zb_script *s, const char *name, bool legacy, char *out, size_t len) {
  int n = legacy ? snprintf(out, len, "%sworlds/%s.zbw", s->cfg.user_dir ? s->cfg.user_dir : "", name)
                 : snprintf(out, len, "%ssaves/%s/bundle.zbw", s->cfg.user_dir ? s->cfg.user_dir : "", name);
  return zb_script_valid_name(name) && n > 0 && (size_t)n < len;
}

/* Makes the bundle the world everyone plays in (the host's, or the script's
 * own); NULL returns to the noise world. */
bool zb_script_adopt_bundle(zb_script *s, zb_world_bundle *b, char *err, size_t err_len) {
  if (s->cfg.host.use_bundle) {
    return s->cfg.host.use_bundle(s->cfg.host.ctx, b, err, err_len);
  }
  zb_world_desc d;
  memset(&d, 0, sizeof d);
  d.a = s->cfg.a;
  d.seed = s->cfg.seed;
  d.radius = s->cfg.planet_radius;
  d.bundle = b;
  zb_world *w = zb_world_create(&d, err, err_len);
  if (!w) {
    return false;
  }
  if (s->own_world) {
    zb_world_destroy(s->world);
  }
  s->world = w;
  s->own_world = true;
  if (s->bundle) { /* the previous one: no longer referenced */
    zb_world_bundle_free(s->bundle);
    zb_free(s->cfg.a, s->bundle);
  }
  s->bundle = b;
  return true;
}

static int fail2(lua_State *L, const char *why) {
  lua_pushboolean(L, false);
  lua_pushstring(L, why);
  return 2;
}

/* @lua world.create_finish(name: string): boolean, string?
 * Saves the finished world as worlds/<name>.zbw in the user folder and makes
 * it the world being played. Names use letters, digits, - and _.
 */
static int w_create_finish(lua_State *L) {
  zb_script *s = S(L);
  const char *name = luaL_checkstring(L, 1);
  char path[512], err[160];
  if (!bundle_path(s, name, false, path, sizeof path)) {
    return fail2(L, "world names use letters, digits, - and _");
  }
  zb_creation_progress p;
  finish_job(s);
  if (!s->creation || (zb_world_creation_get_progress(s->creation, &p), p.stage != ZB_CREATION_DONE)) {
    return fail2(L, "no finished world");
  }
  zb_world_bundle *b = zb_calloc(s->cfg.a, sizeof *b);
  zb_array file;
  zb_array_init(&file, s->cfg.a, 1);
  zb_result r = b ? zb_world_creation_bundle(s->creation, s->cfg.a, b) : ZB_ERR_NOMEM;
  if (r == ZB_OK) {
    r = zb_world_bundle_write(b, &file);
  }
  if (r == ZB_OK && s->cfg.fs) {
    r = s->cfg.fs->write(s->cfg.fs->ctx, path, file.data, file.len);
  }
  zb_array_free(&file);
  if (r != ZB_OK || !zb_script_adopt_bundle(s, b, err, sizeof err)) {
    if (b) {
      zb_world_bundle_free(b);
      zb_free(s->cfg.a, b);
    }
    return fail2(L, r != ZB_OK ? zb_result_str(r) : err);
  }
  if (s->cfg.host.user_data_changed) {
    s->cfg.host.user_data_changed(s->cfg.host.ctx);
  }
  if (s->cfg.render) {
    release_globe(s);
  }
  zb_world_creation_destroy(s->creation);
  s->creation = NULL;
  lua_pushboolean(L, true);
  return 1;
}

bool zb_script_load_world(zb_script *s, const char *name, bool *legacy, char *err, size_t err_len) {
  char path[512];
  if (!zb_script_valid_name(name)) {
    snprintf(err, err_len, "world names use letters, digits, - and _");
    return false;
  }
  if (!s->cfg.fs) {
    snprintf(err, err_len, "no file system");
    return false;
  }
  char *data = NULL;
  size_t len = 0;
  *legacy = false;
  bundle_path(s, name, false, path, sizeof path);
  if (s->cfg.fs->read(s->cfg.fs->ctx, path, s->cfg.a, &data, &len) != ZB_OK) {
    bundle_path(s, name, true, path, sizeof path);
    if (s->cfg.fs->read(s->cfg.fs->ctx, path, s->cfg.a, &data, &len) != ZB_OK) {
      snprintf(err, err_len, "no such world");
      return false;
    }
    *legacy = true;
  }
  zb_world_bundle *b = zb_calloc(s->cfg.a, sizeof *b);
  zb_result r = b ? zb_world_bundle_read(s->cfg.a, data, len, b, err, err_len) : ZB_ERR_NOMEM;
  zb_free(s->cfg.a, data);
  if (r != ZB_OK || !zb_script_adopt_bundle(s, b, err, err_len)) {
    if (b) {
      zb_world_bundle_free(b);
      zb_free(s->cfg.a, b);
    }
    if (r == ZB_ERR_NOMEM) {
      snprintf(err, err_len, "out of memory");
    }
    return false;
  }
  if (s->cfg.render && s->globe_source) {
    release_globe(s);
  }
  return true;
}

/* @lua world.load(name: string): boolean, string?
 * Plays in the created world <name> from the user folder (saves/<name>/,
 * or worlds/<name>.zbw from before saves).
 */
static int w_load(lua_State *L) {
  char err[160];
  bool legacy;
  if (!zb_script_load_world(S(L), luaL_checkstring(L, 1), &legacy, err, sizeof err)) {
    return fail2(L, err);
  }
  lua_pushboolean(L, true);
  return 1;
}

/* @lua world.export_bundle(): string?
 * The created world being played as bytes (its bundle), to send to heirs
 * joining a co-op session; nil in the noise world.
 */
static int w_export_bundle(lua_State *L) {
  zb_script *s = S(L);
  const zb_world_bundle *b = zb_world_get_bundle(s->world);
  if (!b) {
    lua_pushnil(L);
    return 1;
  }
  zb_array out;
  zb_array_init(&out, s->cfg.a, 1);
  zb_result r = zb_world_bundle_write(b, &out);
  if (r != ZB_OK) {
    zb_array_free(&out);
    return luaL_error(L, "%s", zb_result_str(r));
  }
  lua_pushlstring(L, (const char *)out.data, out.len);
  zb_array_free(&out);
  return 1;
}

/* @lua world.play_bundle(data: string): boolean, string?
 * Plays in a created world given as bytes (a bundle a co-op host sent);
 * nil and why when they are not a world bundle.
 */
static int w_play_bundle(lua_State *L) {
  zb_script *s = S(L);
  size_t len;
  const char *data = luaL_checklstring(L, 1, &len);
  char err[160];
  zb_world_bundle *b = zb_calloc(s->cfg.a, sizeof *b);
  zb_result r = b ? zb_world_bundle_read(s->cfg.a, data, len, b, err, sizeof err) : ZB_ERR_NOMEM;
  if (r != ZB_OK || !zb_script_adopt_bundle(s, b, err, sizeof err)) {
    if (b) {
      zb_world_bundle_free(b);
      zb_free(s->cfg.a, b);
    }
    return fail2(L, r == ZB_ERR_NOMEM ? "out of memory" : err);
  }
  lua_pushboolean(L, true);
  return 1;
}

/* ---- What a created world knows -------------------------------------------------------------- */

static const zb_world_bundle *B(lua_State *L) { return zb_world_get_bundle(S(L)->world); }

static zb_dvec3 pos_dir(lua_State *L, int idx) {
  zb_planet_pos pos = zb_script_check_pos(L, idx);
  const zb_planet *p = zb_world_planet(S(L)->world);
  return zb_dv_norm(zb_planet_to_cart(p, &pos));
}

/* @lua world.info(): WorldInfo
 * The world being played.
 */
static int w_info(lua_State *L) {
  zb_script *s = S(L);
  const zb_world_bundle *b = B(L);
  lua_createtable(L, 0, 9);
  lua_pushboolean(L, b != NULL);
  lua_setfield(L, -2, "created");
  lua_pushinteger(L, (lua_Integer)(b ? b->options.seed : zb_world_terrain(s->world)->seed));
  lua_setfield(L, -2, "seed");
  lua_pushnumber(L, zb_world_planet(s->world)->radius);
  lua_setfield(L, -2, "radius");
  if (b) {
    lua_pushstring(L, zb_geology_target_name(b->options.target));
    lua_setfield(L, -2, "target");
    lua_pushstring(L, zb_world_quality_name(b->options.quality));
    lua_setfield(L, -2, "quality");
    lua_pushinteger(L, b->generator_version);
    lua_setfield(L, -2, "generator");
    lua_pushinteger(L, b->metrics.continents);
    lua_setfield(L, -2, "continents");
    lua_pushnumber(L, b->metrics.land_fraction);
    lua_setfield(L, -2, "land");
    lua_pushinteger(L, b->province_count);
    lua_setfield(L, -2, "provinces");
  }
  return 1;
}

/* @lua world.climate(pos: PlanetPos, month?: integer): number?, number?
 * Mean temperature (C, at the position's altitude) and precipitation (mm) for
 * a month (1-12), or the year when month is omitted; nil in the noise world.
 */
static int w_climate(lua_State *L) {
  const zb_world_bundle *b = B(L);
  zb_dvec3 d = pos_dir(L, 1);
  lua_Integer month = luaL_optinteger(L, 2, 0);
  luaL_argcheck(L, month >= 0 && month <= 12, 2, "month must be 1-12");
  if (!b) {
    lua_pushnil(L);
    return 1;
  }
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  double h = (double)pos.alt, t = 0.0, p = 0.0;
  if (month > 0) {
    zb_world_bundle_climate(b, d, (int)month - 1, h, &t, &p);
  } else {
    for (int m = 0; m < ZB_MONTHS; ++m) {
      double tm, pm;
      zb_world_bundle_climate(b, d, m, h, &tm, &pm);
      t += tm / ZB_MONTHS;
      p += pm;
    }
  }
  lua_pushnumber(L, t);
  lua_pushnumber(L, p);
  return 2;
}

/* @lua world.biome(pos: PlanetPos): string?
 * The biome under a position, nil in the noise world.
 */
static int w_biome(lua_State *L) {
  const zb_world_bundle *b = B(L);
  zb_dvec3 d = pos_dir(L, 1);
  if (!b) {
    lua_pushnil(L);
  } else {
    lua_pushstring(L, zb_biome_name(zb_world_bundle_biome(b, d)));
  }
  return 1;
}

static bool water_cell_is(const zb_world_bundle *b, int32_t c, int kind) {
  uint16_t f = b->cell_flags[c];
  switch (kind) {
    case ZB_WATER_RIVER: return zb_water_cell_has_channel(b, c) && !(f & (ZB_CELL_LAKE | ZB_CELL_OCEAN));
    case ZB_WATER_LAKE: return (f & ZB_CELL_LAKE) != 0;
    default: return (f & ZB_CELL_OCEAN) != 0;
  }
}

/* @lua world.find_water(pos: PlanetPos, kind: "river"|"lake"|"ocean", radius: number?): PlanetPos?
 * The nearest place within radius metres (default 50 km) where a created
 * world has water of a kind: for a river, the middle of the channel leaving
 * the nearest macro cell that has one; for a lake or the sea, the centre of
 * the nearest such cell. nil in the noise world or when there is none.
 */
static int w_find_water(lua_State *L) {
  static const char *const kinds[] = {"ocean", "lake", "river", NULL};
  static const int kind_of[] = {ZB_WATER_OCEAN, ZB_WATER_LAKE, ZB_WATER_RIVER};
  const zb_world_bundle *b = B(L);
  zb_dvec3 d = pos_dir(L, 1);
  int kind = kind_of[luaL_checkoption(L, 2, NULL, kinds)];
  double radius = luaL_optnumber(L, 3, 50000.0);
  if (!b) {
    lua_pushnil(L);
    return 1;
  }
  /* Breadth first over the macro grid, nearest cell centre wins. */
  enum { MAX_VISIT = 8192 };
  int32_t *queue = zb_alloc(S(L)->cfg.a, sizeof(int32_t) * MAX_VISIT);
  if (!queue) {
    return luaL_error(L, "out of memory");
  }
  double limit = radius / b->options.radius, best = 1e30;
  int32_t found = -1;
  size_t n = 0;
  queue[n++] = zb_cubegrid_cell_at(&b->grid, d);
  for (size_t i = 0; i < n; ++i) {
    int32_t c = queue[i];
    double angle = zb_dv_angle(d, zb_cubegrid_dir(&b->grid, c));
    if (water_cell_is(b, c, kind) && angle < best && angle <= limit) {
      best = angle;
      found = c;
    }
    for (int k = 0; k < ZB_CUBEGRID_NBRS && n < MAX_VISIT; ++k) {
      int32_t m = zb_cubegrid_nbr(&b->grid, c, k);
      bool seen = m < 0 || zb_dv_angle(d, zb_cubegrid_dir(&b->grid, m)) > limit;
      for (size_t j = n; j-- > 0 && !seen;) {
        seen = queue[j] == m;
      }
      if (!seen) {
        queue[n++] = m;
      }
    }
  }
  zb_free(S(L)->cfg.a, queue);
  if (found < 0) {
    lua_pushnil(L);
    return 1;
  }
  const zb_planet *p = zb_world_planet(S(L)->world);
  zb_dvec3 at = zb_cubegrid_dir(&b->grid, found);
  if (kind == ZB_WATER_RIVER) {
    zb_river_set rs;
    zb_river_set_init(&rs, S(L)->cfg.a);
    if (zb_rivers_add_cell(b, found, &rs) == ZB_OK && rs.count) {
      const zb_river_piece *mid = &rs.piece[rs.count / 2];
      at = zb_dv_norm(zb_dv_add(mid->a, mid->b));
    }
    zb_river_set_free(&rs);
  }
  zb_planet_pos pos = zb_planet_from_cart(p, zb_dv_scale(at, p->radius));
  pos.alt = 0.0f;
  zb_script_push_pos(L, &pos);
  return 1;
}

/* @lua world.strata(pos: PlanetPos): Stratum[]?
 * The rock record under a position, youngest first, nil in the noise world.
 */
static int w_strata(lua_State *L) {
  const zb_world_bundle *b = B(L);
  zb_dvec3 d = pos_dir(L, 1);
  if (!b) {
    lua_pushnil(L);
    return 1;
  }
  int32_t point = zb_world_bundle_point(b, d);
  const zb_stratum *col = &b->strata[(size_t)point * ZB_STRATA];
  lua_newtable(L);
  int k = 0;
  for (int i = 0; i < ZB_STRATA && col[i].thickness > 0; ++i) {
    lua_createtable(L, 0, 5);
    lua_pushstring(L, zb_rock_name((zb_rock)col[i].rock));
    lua_setfield(L, -2, "rock");
    lua_pushstring(L, zb_rock_env_name((zb_rock_env)col[i].env));
    lua_setfield(L, -2, "environment");
    lua_pushnumber(L, col[i].thickness);
    lua_setfield(L, -2, "thickness");
    lua_pushnumber(L, col[i].age);
    lua_setfield(L, -2, "age");
    zb_fossil f = zb_stratum_fossil(&col[i], (uint64_t)point * ZB_STRATA + (uint64_t)i);
    if (f != ZB_FOSSIL_NONE) {
      lua_pushstring(L, zb_fossil_name(f));
      lua_setfield(L, -2, "fossil");
    }
    lua_rawseti(L, -2, ++k);
  }
  lua_createtable(L, 0, 1); /* the basement below everything */
  lua_pushstring(L, zb_rock_name((zb_rock)b->basement[point]));
  lua_setfield(L, -2, "rock");
  lua_pushstring(L, "basement");
  lua_setfield(L, -2, "environment");
  lua_rawseti(L, -2, ++k);
  return 1;
}

/* @lua world.minerals(pos: PlanetPos): string[]?
 * The mineral deposits under a position, nil in the noise world.
 */
static int w_minerals(lua_State *L) {
  const zb_world_bundle *b = B(L);
  zb_dvec3 d = pos_dir(L, 1);
  if (!b) {
    lua_pushnil(L);
    return 1;
  }
  uint32_t m = b->minerals[zb_world_bundle_point(b, d)];
  lua_newtable(L);
  int k = 0;
  for (int bit = 0; bit < ZB_MIN_COUNT; ++bit) {
    if (m & (1u << bit)) {
      lua_pushstring(L, zb_mineral_name(1u << bit));
      lua_rawseti(L, -2, ++k);
    }
  }
  return 1;
}

/* @lua world.province(pos: PlanetPos): integer?, number?
 * The biogeographic province under a position (1-based) and its isolation
 * (0 well connected .. 1 remote), nil at sea or in the noise world.
 */
static int w_province(lua_State *L) {
  const zb_world_bundle *b = B(L);
  zb_dvec3 d = pos_dir(L, 1);
  int p = b ? b->province[zb_world_bundle_cell(b, d)] : 0;
  if (p == 0) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushinteger(L, p);
  lua_pushnumber(L, b->provinces[p - 1].isolation);
  return 2;
}

/* ---- The globe ---------------------------------------------------------------------------------- */

static void height_color(double h, float out[4]) {
  if (h <= 0.0) {
    double t = h < -6000.0 ? 1.0 : -h / 6000.0;
    out[0] = (float)(0.10 - 0.07 * t);
    out[1] = (float)(0.35 - 0.20 * t);
    out[2] = (float)(0.70 - 0.25 * t);
  } else if (h < 1500.0) {
    double t = h / 1500.0;
    out[0] = (float)(0.25 + 0.35 * t);
    out[1] = (float)(0.55 - 0.10 * t);
    out[2] = (float)(0.20 + 0.05 * t);
  } else {
    double t = h > 5000.0 ? 1.0 : (h - 1500.0) / 3500.0;
    out[0] = (float)(0.60 + 0.35 * t);
    out[1] = (float)(0.45 + 0.50 * t);
    out[2] = (float)(0.25 + 0.70 * t);
  }
  out[3] = 1.0f; /* untextured */
}

static void biome_color(zb_biome b, double h, float out[4]) {
  height_color(h, out);
  switch (zb_biome_group_of(b)) {
    case ZB_GROUP_ARID: out[0] = 0.80f, out[1] = 0.68f, out[2] = 0.42f; break;
    case ZB_GROUP_TROPICAL: out[0] = 0.15f, out[1] = 0.50f, out[2] = 0.18f; break;
    case ZB_GROUP_COLD:
      out[0] = b == ZB_BIOME_ICE_SHEET ? 0.95f : 0.55f, out[1] = b == ZB_BIOME_ICE_SHEET ? 0.97f : 0.60f;
      out[2] = b == ZB_BIOME_ICE_SHEET ? 1.0f : 0.50f;
      break;
    case ZB_GROUP_MAGICAL: out[0] = 0.75f, out[1] = 0.30f, out[2] = 0.85f; break;
    default: break;
  }
}

/* Builds or recolours the globe mesh for a frame (or the finished world). */
static bool build_globe(zb_script *s, const zb_world_creation *c, const zb_world_bundle *b, int frame) {
  int res = c ? GLOBE_RES : GLOBE_RES_PLAYED;
  int n = res + 1;
  size_t verts = (size_t)6 * (size_t)n * (size_t)n, idx = (size_t)6 * (size_t)res * (size_t)res * 6;
  const void *source = c ? (const void *)c : (const void *)b;
  if (s->globe_source != source) {
    release_globe(s);
    s->globe_source = source;
  }
  zb_mesh_data md;
  memset(&md, 0, sizeof md);
  md.vertices = zb_alloc(s->cfg.a, sizeof(zb_vertex) * verts);
  md.indices = zb_alloc(s->cfg.a, sizeof(uint32_t) * idx);
  if (c && !s->globe_nearest) {
    s->globe_nearest = zb_alloc(s->cfg.a, sizeof(int32_t) * verts);
  }
  if (!md.vertices || !md.indices || (c && !s->globe_nearest)) {
    zb_free(s->cfg.a, md.vertices);
    zb_free(s->cfg.a, md.indices);
    return false;
  }
  const int16_t *elev = c ? zb_world_creation_frame_elevation(c, frame) : NULL;
  const zb_lattice *lat = c ? zb_geology_lattice(zb_world_creation_geology(c)) : NULL;
  size_t v = 0, k = 0;
  for (int face = 0; face < 6; ++face) {
    for (int y = 0; y < n; ++y) {
      for (int x = 0; x < n; ++x, ++v) {
        double u = -1.0 + 2.0 * x / res, w = -1.0 + 2.0 * y / res;
        zb_dvec3 d = zb_planet_face_dir(face, u, w);
        zb_vertex *vx = &md.vertices[v];
        vx->pos[0] = vx->normal[0] = (float)d.x;
        vx->pos[1] = vx->normal[1] = (float)d.z; /* render space: y up */
        vx->pos[2] = vx->normal[2] = (float)-d.y;
        vx->uv[0] = vx->uv[1] = 0.0f;
        if (c) {
          if (s->globe_frame == -2) {
            s->globe_nearest[v] = zb_lattice_nearest(lat, d, v > 0 ? s->globe_nearest[v - 1] : -1);
          }
          height_color(elev ? (double)elev[s->globe_nearest[v]] : -4000.0, vx->color);
        } else {
          biome_color(zb_world_bundle_biome(b, d), zb_world_bundle_height(b, d), vx->color);
        }
      }
    }
    for (int y = 0; y < res; ++y) {
      for (int x = 0; x < res; ++x) {
        uint32_t i0 = (uint32_t)((size_t)face * (size_t)n * (size_t)n + (size_t)y * (size_t)n + (size_t)x);
        uint32_t quad[6] = {i0, i0 + 1, i0 + (uint32_t)n, i0 + 1, i0 + (uint32_t)n + 1, i0 + (uint32_t)n};
        for (int q = 0; q < 6; ++q) {
          md.indices[k++] = quad[q];
        }
      }
    }
  }
  md.vertex_count = verts;
  md.index_count = idx;
  zb_mesh_compute_bounds(&md);
  uint32_t mesh = zb_render_upload_mesh(s->cfg.render, &md);
  zb_free(s->cfg.a, md.vertices);
  zb_free(s->cfg.a, md.indices);
  if (s->globe_mesh) {
    zb_render_release_mesh(s->cfg.render, s->globe_mesh);
  }
  s->globe_mesh = mesh;
  s->globe_index_count = (uint32_t)idx;
  s->globe_frame = frame;
  return mesh != 0;
}

/* @lua world.draw_globe(frame: integer?, position: number[], radius: number, spin?: number): boolean
 * Draws the planet as a globe: a time-lapse frame of the world being created
 * (1-based; nil for the newest), or the played world by biome when none is
 * being created. Returns false when there is nothing to show yet.
 */
static int w_draw_globe(lua_State *L) {
  zb_script *s = S(L);
  if (!s->cfg.render) {
    lua_pushboolean(L, false);
    return 1;
  }
  luaL_checktype(L, 2, LUA_TTABLE);
  float pos[3];
  for (int i = 0; i < 3; ++i) {
    lua_rawgeti(L, 2, i + 1);
    pos[i] = (float)luaL_checknumber(L, -1);
    lua_pop(L, 1);
  }
  float radius = (float)luaL_checknumber(L, 3);
  float spin = (float)luaL_optnumber(L, 4, 0.0);
  const zb_world_bundle *b = B(L);
  int frame = -1;
  if (s->creation) {
    int frames = s->creation_frames; /* complete frames only, while a job works on the next */
    frame = lua_isnoneornil(L, 1) ? frames - 1 : (int)luaL_checkinteger(L, 1) - 1;
    frame = frame >= frames ? frames - 1 : frame;
    if (frame < 0) {
      lua_pushboolean(L, false);
      return 1;
    }
  } else if (!b) {
    lua_pushboolean(L, false);
    return 1;
  }
  const void *source = s->creation ? (const void *)s->creation : (const void *)b;
  if (s->globe_source != source || s->globe_frame != frame || !s->globe_mesh) {
    if (!build_globe(s, s->creation, b, frame)) {
      lua_pushboolean(L, false);
      return 1;
    }
  }
  zb_quat q = zb_quat_axis_angle((zb_vec3){0.0f, 1.0f, 0.0f}, spin);
  zb_mat4 m = zb_m4_trs((zb_vec3){pos[0], pos[1], pos[2]}, q, (zb_vec3){radius, radius, radius});
  zb_render_draw_terrain(s->cfg.render, s->globe_mesh, s->globe_index_count, m, (zb_vec3){0.0f, 0.0f, 0.0f}, 1.0f,
                         0.0f);
  lua_pushboolean(L, true);
  return 1;
}

void zb_script_open_create(zb_script *s) {
  static const luaL_Reg fns[] = {{"create", w_create},
                                 {"create_step", w_create_step},
                                 {"create_cancel", w_create_cancel},
                                 {"create_finish", w_create_finish},
                                 {"load", w_load},
                                 {"export_bundle", w_export_bundle},
                                 {"play_bundle", w_play_bundle},
                                 {"info", w_info},
                                 {"climate", w_climate},
                                 {"biome", w_biome},
                                 {"strata", w_strata},
                                 {"minerals", w_minerals},
                                 {"province", w_province},
                                 {"find_water", w_find_water},
                                 {"draw_globe", w_draw_globe},
                                 {NULL, NULL}};
  s->globe_frame = -2;
  lua_getglobal(s->L, "world");
  luaL_setfuncs(s->L, fns, 0);
  lua_pop(s->L, 1);
}

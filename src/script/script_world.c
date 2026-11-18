/* SPDX-License-Identifier: MIT */
/* Lua bindings: world (planet coordinates and terrain).
 *
 * @luaclass PlanetInfo
 * @field radius number Sea-level radius in metres
 * @field chunks_per_edge integer Chunks along one cube-face edge
 * @field chunk_size number Nominal chunk width in metres (256)
 *
 * @luaclass PlanetPos
 * @field face integer Cube face 0..5 (+X, -X, +Y, -Y, +Z, -Z)
 * @field x integer Chunk column on the face
 * @field y integer Chunk row on the face
 * @field lx number Offset inside the chunk, nominal metres 0..256
 * @field ly number Offset inside the chunk, nominal metres 0..256
 * @field alt number Metres above sea level
 */
#include "script/script_internal.h"

#include <math.h>

static zb_world *W(lua_State *L) { return zb_script_from(L)->world; }
static const zb_planet *P(lua_State *L) { return zb_world_planet(W(L)); }
static const zb_terrain *T(lua_State *L) { return zb_world_terrain(W(L)); }

static void push_pos(lua_State *L, const zb_planet_pos *p) {
  lua_createtable(L, 0, 6);
  lua_pushinteger(L, p->chunk.face);
  lua_setfield(L, -2, "face");
  lua_pushinteger(L, p->chunk.x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, p->chunk.y);
  lua_setfield(L, -2, "y");
  lua_pushnumber(L, (lua_Number)p->lx);
  lua_setfield(L, -2, "lx");
  lua_pushnumber(L, (lua_Number)p->ly);
  lua_setfield(L, -2, "ly");
  lua_pushnumber(L, (lua_Number)p->alt);
  lua_setfield(L, -2, "alt");
}

static lua_Integer int_field(lua_State *L, int idx, const char *name) {
  lua_getfield(L, idx, name);
  int ok;
  lua_Integer v = lua_tointegerx(L, -1, &ok);
  lua_pop(L, 1);
  if (!ok) {
    luaL_error(L, "position field '%s' must be an integer", name);
  }
  return v;
}

static float num_field(lua_State *L, int idx, const char *name) {
  lua_getfield(L, idx, name);
  float v = (float)luaL_optnumber(L, -1, 0.0);
  lua_pop(L, 1);
  return v;
}

static zb_planet_pos make_pos(lua_State *L, lua_Integer face, lua_Integer x, lua_Integer y, float lx, float ly,
                              float alt) {
  const zb_planet *p = P(L);
  zb_chunk c = {(int32_t)face, (int32_t)x, (int32_t)y};
  if (face < 0 || face >= ZB_PLANET_FACES || x < 0 || y < 0 || x >= p->n || y >= p->n) {
    luaL_error(L, "chunk (%d, %d, %d) is not on the planet (%d chunks per face edge)", (int)face, (int)x, (int)y,
               (int)p->n);
  }
  zb_planet_pos pos = {c, lx, ly, alt};
  return pos;
}

static zb_planet_pos check_pos(lua_State *L, int idx) {
  luaL_checktype(L, idx, LUA_TTABLE);
  /* Sequenced reads: the first bad field is the one reported. */
  lua_Integer face = int_field(L, idx, "face");
  lua_Integer x = int_field(L, idx, "x");
  lua_Integer y = int_field(L, idx, "y");
  float lx = num_field(L, idx, "lx");
  float ly = num_field(L, idx, "ly");
  return make_pos(L, face, x, y, lx, ly, num_field(L, idx, "alt"));
}

/* @lua world.planet(): PlanetInfo
 * Describes the planet.
 */
static int w_planet(lua_State *L) {
  const zb_planet *p = P(L);
  lua_createtable(L, 0, 3);
  lua_pushnumber(L, p->radius);
  lua_setfield(L, -2, "radius");
  lua_pushinteger(L, p->n);
  lua_setfield(L, -2, "chunks_per_edge");
  lua_pushnumber(L, ZB_CHUNK_SIZE);
  lua_setfield(L, -2, "chunk_size");
  return 1;
}

/* @lua world.position(face: integer, x: integer, y: integer, lx?: number, ly?: number, alt?: number): PlanetPos
 * Makes a position; the offsets default to 0. Errors when the chunk is not on the planet.
 */
static int w_position(lua_State *L) {
  lua_Integer face = luaL_checkinteger(L, 1);
  lua_Integer x = luaL_checkinteger(L, 2);
  lua_Integer y = luaL_checkinteger(L, 3);
  float lx = (float)luaL_optnumber(L, 4, 0.0);
  float ly = (float)luaL_optnumber(L, 5, 0.0);
  zb_planet_pos pos = make_pos(L, face, x, y, lx, ly, (float)luaL_optnumber(L, 6, 0.0));
  push_pos(L, &pos);
  return 1;
}

/* @lua world.to_cartesian(pos: PlanetPos): number, number, number
 * Planet-centred coordinates in metres (+Z is the north pole).
 */
static int w_to_cartesian(lua_State *L) {
  zb_planet_pos pos = check_pos(L, 1);
  zb_dvec3 c = zb_planet_to_cart(P(L), &pos);
  lua_pushnumber(L, c.x);
  lua_pushnumber(L, c.y);
  lua_pushnumber(L, c.z);
  return 3;
}

/* @lua world.from_cartesian(x: number, y: number, z: number): PlanetPos
 * The position of planet-centred coordinates in metres.
 */
static int w_from_cartesian(lua_State *L) {
  zb_dvec3 c = {luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3)};
  zb_planet_pos pos = zb_planet_from_cart(P(L), c);
  push_pos(L, &pos);
  return 1;
}

/* @lua world.distance(a: PlanetPos, b: PlanetPos): number
 * Great-circle distance at sea level in metres.
 */
static int w_distance(lua_State *L) {
  zb_planet_pos a = check_pos(L, 1), b = check_pos(L, 2);
  lua_pushnumber(L, zb_planet_distance(P(L), &a, &b));
  return 1;
}

/* @lua world.chunk_key(pos: PlanetPos): integer
 * The chunk's stable 64-bit key.
 */
static int w_chunk_key(lua_State *L) {
  zb_planet_pos pos = check_pos(L, 1);
  lua_pushinteger(L, (lua_Integer)zb_chunk_key(pos.chunk));
  return 1;
}

/* @lua world.neighbors(pos: PlanetPos): PlanetPos[]
 * Centres of the chunks sharing an edge or corner with the position's chunk (7 or 8).
 */
static int w_neighbors(lua_State *L) {
  zb_planet_pos pos = check_pos(L, 1);
  zb_chunk out[8];
  int n = zb_planet_neighbors(P(L), pos.chunk, out);
  lua_createtable(L, n, 0);
  for (int i = 0; i < n; ++i) {
    zb_planet_pos c = zb_planet_chunk_center(out[i]);
    push_pos(L, &c);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

/* @lua world.height(pos: PlanetPos): number
 * Terrain height above sea level in metres at the position (the analytic terrain, within
 * 0.5 m of the generated heightfield).
 */
static int w_height(lua_State *L) {
  zb_planet_pos pos = check_pos(L, 1);
  zb_dvec3 c = zb_planet_to_cart(P(L), &pos);
  double len = sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
  zb_dvec3 d = {c.x / len, c.y / len, c.z / len};
  lua_pushnumber(L, zb_terrain_height(T(L), d));
  return 1;
}

/* Terrain height at a position moved by (dx, dy) nominal metres. */
static double height_offset(lua_State *L, zb_planet_pos pos, float dx, float dy) {
  pos.lx += dx;
  pos.ly += dy;
  pos.alt = 0.0f;
  zb_dvec3 c = zb_planet_to_cart(P(L), &pos);
  double len = sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
  zb_dvec3 d = {c.x / len, c.y / len, c.z / len};
  return zb_terrain_height(T(L), d);
}

/* @lua world.surface(pos: PlanetPos): string
 * Surface material at the position: "soil", "sand", "rock", "gravel", "snow", "ice" or "mud".
 */
static int w_surface(lua_State *L) {
  zb_planet_pos pos = check_pos(L, 1);
  double h = height_offset(L, pos, 0.0f, 0.0f);
  double w, hh;
  zb_planet_chunk_size(P(L), pos.chunk, &w, &hh);
  double gx = (height_offset(L, pos, 1.0f, 0.0f) - h) / (w / ZB_CHUNK_SIZE);
  double gy = (height_offset(L, pos, 0.0f, 1.0f) - h) / (hh / ZB_CHUNK_SIZE);
  double sin_lat = zb_planet_chunk_dir(P(L), pos.chunk).z;
  zb_material m = zb_terrain_material_rule(h, sqrt(gx * gx + gy * gy), sin_lat, 0.0);
  lua_pushstring(L, zb_material_name(m));
  return 1;
}

static zb_vec3 opt_vec3(lua_State *L, int idx) {
  zb_vec3 v = {0.0f, 0.0f, 0.0f};
  if (lua_isnoneornil(L, idx)) {
    return v;
  }
  luaL_checktype(L, idx, LUA_TTABLE);
  float *c[3] = {&v.x, &v.y, &v.z};
  for (int i = 0; i < 3; ++i) {
    lua_rawgeti(L, idx, i + 1);
    *c[i] = (float)luaL_optnumber(L, -1, 0.0);
    lua_pop(L, 1);
  }
  return v;
}

static int focus_index(lua_State *L) {
  lua_Integer i = luaL_checkinteger(L, 1);
  luaL_argcheck(L, i >= 1 && i <= ZB_STREAM_MAX_FOCUS, 1, "focus index must be 1..4");
  return (int)i - 1;
}

/* @lua world.set_focus(index: integer, pos: PlanetPos, velocity?: number[], view?: number[])
 * Streams terrain around a focus point (1..4, one per player camera). velocity (m/s) and view
 * are {x, y, z} in the local frame at pos (x east, y up, z south).
 */
static int w_set_focus(lua_State *L) {
  int index = focus_index(L);
  zb_stream_focus f;
  f.pos = check_pos(L, 2);
  f.velocity = opt_vec3(L, 3);
  f.view = opt_vec3(L, 4);
  zb_world_set_focus(W(L), index, &f);
  return 0;
}

/* @lua world.clear_focus(index: integer)
 * Removes a focus point; its chunks are released as memory requires.
 */
static int w_clear_focus(lua_State *L) {
  zb_world_set_focus(W(L), focus_index(L), NULL);
  return 0;
}

/* @lua world.ground_height(pos: PlanetPos): number
 * Ground height in metres above sea level from the loaded heightfield (the analytic terrain
 * where the chunk is not loaded at stage 1 or more).
 */
static int w_ground_height(lua_State *L) {
  zb_planet_pos pos = check_pos(L, 1);
  lua_pushnumber(L, zb_world_ground_height(W(L), &pos));
  return 1;
}

/* @lua world.chunk_stage(pos: PlanetPos): integer
 * Stage the position's chunk is loaded at (0 coarse, 1 heightfield, 2 surface), or -1.
 */
static int w_chunk_stage(lua_State *L) {
  zb_planet_pos pos = check_pos(L, 1);
  const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(W(L)), pos.chunk);
  lua_pushinteger(L, ct ? ct->stage : ZB_STAGE_NONE);
  return 1;
}

/* @luaclass StreamStats
 * @field stage0 integer Loaded chunks at stage 0 (coarse)
 * @field stage1 integer Loaded chunks at stage 1 (heightfield)
 * @field stage2 integer Loaded chunks at stage 2 (surface)
 * @field wanted integer Chunks inside the rings
 * @field cached integer Loaded chunks outside the rings
 * @field bytes integer Chunk memory
 * @field jobs integer Generation jobs in flight
 * @field queued integer Chunks waiting for a job
 * @field generated integer Chunks generated so far
 * @field evicted integer Chunks evicted so far
 * @field cancelled integer Jobs cancelled so far
 */

/* @lua world.stream_stats(): StreamStats
 * Chunk streaming statistics.
 */
static int w_stream_stats(lua_State *L) {
  zb_stream_stats st;
  zb_stream_get_stats(zb_world_stream(W(L)), &st);
  lua_createtable(L, 0, 11);
  const struct {
    const char *name;
    lua_Integer value;
  } fields[] = {{"stage0", (lua_Integer)st.chunks[0]},    {"stage1", (lua_Integer)st.chunks[1]},
                {"stage2", (lua_Integer)st.chunks[2]},    {"wanted", (lua_Integer)st.wanted},
                {"cached", (lua_Integer)st.cached},       {"bytes", (lua_Integer)st.bytes},
                {"jobs", (lua_Integer)st.jobs},           {"queued", (lua_Integer)st.queued},
                {"generated", (lua_Integer)st.generated}, {"evicted", (lua_Integer)st.evicted},
                {"cancelled", (lua_Integer)st.cancelled}};
  for (size_t i = 0; i < sizeof fields / sizeof fields[0]; ++i) {
    lua_pushinteger(L, fields[i].value);
    lua_setfield(L, -2, fields[i].name);
  }
  return 1;
}

void zb_script_open_world(zb_script *s) {
  static const luaL_Reg world_fns[] = {{"planet", w_planet},
                                       {"position", w_position},
                                       {"to_cartesian", w_to_cartesian},
                                       {"from_cartesian", w_from_cartesian},
                                       {"distance", w_distance},
                                       {"chunk_key", w_chunk_key},
                                       {"neighbors", w_neighbors},
                                       {"height", w_height},
                                       {"surface", w_surface},
                                       {"set_focus", w_set_focus},
                                       {"clear_focus", w_clear_focus},
                                       {"ground_height", w_ground_height},
                                       {"stream_stats", w_stream_stats},
                                       {"chunk_stage", w_chunk_stage},
                                       {NULL, NULL}};
  lua_newtable(s->L);
  luaL_setfuncs(s->L, world_fns, 0);
  lua_setglobal(s->L, "world");
}

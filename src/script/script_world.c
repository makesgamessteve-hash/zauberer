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

void zb_script_push_pos(lua_State *L, const zb_planet_pos *p) {
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

zb_planet_pos zb_script_check_pos(lua_State *L, int idx) {
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
  zb_script_push_pos(L, &pos);
  return 1;
}

/* @lua world.to_cartesian(pos: PlanetPos): number, number, number
 * Planet-centred coordinates in metres (+Z is the north pole).
 */
static int w_to_cartesian(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
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
  zb_script_push_pos(L, &pos);
  return 1;
}

/* @lua world.distance(a: PlanetPos, b: PlanetPos): number
 * Great-circle distance at sea level in metres.
 */
static int w_distance(lua_State *L) {
  zb_planet_pos a = zb_script_check_pos(L, 1), b = zb_script_check_pos(L, 2);
  lua_pushnumber(L, zb_planet_distance(P(L), &a, &b));
  return 1;
}

/* @lua world.chunk_key(pos: PlanetPos): integer
 * The chunk's stable 64-bit key.
 */
static int w_chunk_key(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  lua_pushinteger(L, (lua_Integer)zb_chunk_key(pos.chunk));
  return 1;
}

/* @lua world.neighbors(pos: PlanetPos): PlanetPos[]
 * Centres of the chunks sharing an edge or corner with the position's chunk (7 or 8).
 */
static int w_neighbors(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  zb_chunk out[8];
  int n = zb_planet_neighbors(P(L), pos.chunk, out);
  lua_createtable(L, n, 0);
  for (int i = 0; i < n; ++i) {
    zb_planet_pos c = zb_planet_chunk_center(out[i]);
    zb_script_push_pos(L, &c);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

/* @lua world.height(pos: PlanetPos): number
 * Terrain height above sea level in metres at the position (the analytic terrain, within
 * 0.5 m of the generated heightfield).
 */
static int w_height(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
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
  zb_planet_pos pos = zb_script_check_pos(L, 1);
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
  f.pos = zb_script_check_pos(L, 2);
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

/* @lua world.water(pos: PlanetPos): string?, number?, number?, number?, number?
 * Water at a position: its kind ("ocean", "lake" or "river"; nil on dry land),
 * the water surface in metres above sea level, its depth, and the current
 * towards east and north in m/s.
 */
static int w_water(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  pos.alt = 0.0f;
  zb_dvec3 c = zb_planet_to_cart(P(L), &pos);
  double len = sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
  zb_dvec3 d = {c.x / len, c.y / len, c.z / len};
  zb_water_sample w;
  zb_terrain_water(T(L), d, &w);
  static const char *const kinds[] = {NULL, "ocean", "lake", "river"};
  if (w.kind == ZB_WATER_NONE) {
    lua_pushnil(L);
    return 1;
  }
  /* East and north at the place (the pole axis is +Z). */
  double ex = -d.y, ey = d.x, el = sqrt(ex * ex + ey * ey);
  ex = el > 0.0 ? ex / el : 1.0;
  ey = el > 0.0 ? ey / el : 0.0;
  zb_dvec3 north = {-d.z * ey, d.z * ex, d.x * ey - d.y * ex};
  lua_pushstring(L, kinds[w.kind]);
  lua_pushnumber(L, w.surface);
  lua_pushnumber(L, w.depth);
  lua_pushnumber(L, w.flow.x * ex + w.flow.y * ey);
  lua_pushnumber(L, w.flow.x * north.x + w.flow.y * north.y + w.flow.z * north.z);
  return 5;
}

/* @lua world.time(): WorldTime
 * The world's time and date (the clock at longitude 0).
 * @field time number Game seconds since the world began
 * @field year integer From 0
 * @field season string "spring", "summer", "autumn" or "winter" (northern)
 * @field day integer Day of the season, from 1
 * @field hour integer
 * @field minute integer
 * @field year_fraction number 0 .. 1 from the spring equinox
 * @field day_fraction number 0 .. 1 from midnight
 */
static int w_time(lua_State *L) {
  double t = zb_world_time(W(L));
  zb_date d;
  zb_calendar_date(t, &d);
  static const char *const seasons[4] = {"spring", "summer", "autumn", "winter"};
  lua_createtable(L, 0, 8);
  lua_pushnumber(L, t);
  lua_setfield(L, -2, "time");
  lua_pushinteger(L, (lua_Integer)d.year);
  lua_setfield(L, -2, "year");
  lua_pushstring(L, seasons[d.season]);
  lua_setfield(L, -2, "season");
  lua_pushinteger(L, d.day + 1);
  lua_setfield(L, -2, "day");
  lua_pushinteger(L, d.hour);
  lua_setfield(L, -2, "hour");
  lua_pushinteger(L, d.minute);
  lua_setfield(L, -2, "minute");
  lua_pushnumber(L, d.year_fraction);
  lua_setfield(L, -2, "year_fraction");
  lua_pushnumber(L, d.day_fraction);
  lua_setfield(L, -2, "day_fraction");
  return 1;
}

/* @lua world.set_time(seconds: number)
 * Sets the world's time (game seconds since the world began), e.g. after sleeping.
 */
static int w_set_time(lua_State *L) {
  double t = luaL_checknumber(L, 1);
  luaL_argcheck(L, t >= 0.0, 1, "time must not be negative");
  zb_world_set_time(W(L), t);
  return 0;
}

/* @lua world.sky(pos: PlanetPos): Sky
 * The sky over a position now.
 * @field sun_elevation number Radians above the horizon (negative at night)
 * @field moon_elevation number Radians above the horizon
 * @field moon_phase number 0 new, 0.5 full
 * @field moon_light number Lit fraction of the moon, 0 .. 1
 * @field daylight number 0 night .. 1 day
 * @field season number Local season 0 .. 4 (0 spring; half a year on south of the equator)
 */
static int w_sky(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  pos.alt = 0.0f;
  zb_dvec3 c = zb_planet_to_cart(P(L), &pos);
  double len = sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
  zb_dvec3 d = {c.x / len, c.y / len, c.z / len};
  zb_sky_place s;
  zb_calendar_sky(zb_world_calendar(W(L)), zb_world_time(W(L)), d, &s);
  lua_createtable(L, 0, 6);
  const struct {
    const char *name;
    double value;
  } fields[] = {{"sun_elevation", s.sun_elevation}, {"moon_elevation", s.moon_elevation},
                {"moon_phase", s.moon_phase},       {"moon_light", s.moon_light},
                {"daylight", s.daylight},           {"season", s.season}};
  for (size_t i = 0; i < sizeof fields / sizeof fields[0]; ++i) {
    lua_pushnumber(L, fields[i].value);
    lua_setfield(L, -2, fields[i].name);
  }
  return 1;
}

/* @lua world.flora(): Species[]
 * The plant species of the world, in registry order.
 * @field id string Species id
 * @field name string Display name
 * @field layer string "tree", "shrub" or "ground"
 */
static int w_flora(lua_State *L) {
  const zb_flora *f = T(L)->flora;
  int n = f ? f->count : 0;
  static const char *const layers[ZB_PLANT_LAYERS] = {"tree", "shrub", "ground"};
  lua_createtable(L, n, 0);
  for (int i = 0; i < n; ++i) {
    lua_createtable(L, 0, 3);
    lua_pushstring(L, f->species[i].id);
    lua_setfield(L, -2, "id");
    lua_pushstring(L, f->species[i].name);
    lua_setfield(L, -2, "name");
    lua_pushstring(L, layers[f->species[i].layer]);
    lua_setfield(L, -2, "layer");
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

/* @lua world.plants(pos: PlanetPos, radius: number): Plant[]
 * The plants within radius metres (up to 256) of a position, from the chunks
 * loaded with their plants (stage 3); at most 4096.
 * @field species string Species id
 * @field layer string "tree", "shrub" or "ground"
 * @field height number Metres
 * @field pos PlanetPos Where it stands (on the ground)
 */
static int w_plants(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  double radius = luaL_checknumber(L, 2);
  luaL_argcheck(L, radius >= 0.0 && radius <= 256.0, 2, "radius must be 0..256 metres");
  const zb_flora *f = T(L)->flora;
  zb_chunk chunks[9];
  chunks[0] = pos.chunk;
  int n = 1 + zb_planet_neighbors(P(L), pos.chunk, &chunks[1]);
  static const char *const layers[ZB_PLANT_LAYERS] = {"tree", "shrub", "ground"};
  lua_newtable(L);
  int found = 0;
  for (int c = 0; c < n && f; ++c) {
    const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(W(L)), chunks[c]);
    for (uint32_t i = 0; ct && i < ct->plant_count && found < 4096; ++i) {
      const zb_plant *pl = &ct->plants[i];
      zb_planet_pos at = {chunks[c], (float)pl->x / 256.0f, (float)pl->y / 256.0f, 0.0f};
      if (pl->layer == ZB_PLANT_REMOVED || zb_planet_distance(P(L), &pos, &at) > radius) {
        continue;
      }
      const zb_species *sp = &f->species[pl->species];
      at.alt = zb_chunk_height_at(ct, at.lx, at.ly);
      lua_createtable(L, 0, 4);
      lua_pushstring(L, sp->id);
      lua_setfield(L, -2, "species");
      lua_pushstring(L, layers[sp->layer]);
      lua_setfield(L, -2, "layer");
      lua_pushnumber(L, (double)sp->height[0] + (double)(sp->height[1] - sp->height[0]) * pl->size / 255.0);
      lua_setfield(L, -2, "height");
      zb_script_push_pos(L, &at);
      lua_setfield(L, -2, "pos");
      lua_rawseti(L, -2, ++found);
    }
  }
  return 1;
}

/* @lua world.ground_surface(pos: PlanetPos): string?
 * The surface material of the 1 m cell at a position from its loaded chunk
 * (stage 2 or more, with the players' edits), or nil when it is not loaded.
 */
static int w_ground_surface(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(W(L)), pos.chunk);
  if (!ct || !ct->material) {
    lua_pushnil(L);
    return 1;
  }
  int x = (int)pos.lx, y = (int)pos.ly;
  x = x < 0 ? 0 : (x >= ZB_SURFACE_RES ? ZB_SURFACE_RES - 1 : x);
  y = y < 0 ? 0 : (y >= ZB_SURFACE_RES ? ZB_SURFACE_RES - 1 : y);
  lua_pushstring(L, zb_material_name((zb_material)ct->material[y * ZB_SURFACE_RES + x]));
  return 1;
}

/* @lua world.ground_height(pos: PlanetPos): number
 * Ground height in metres above sea level from the loaded heightfield (the analytic terrain
 * where the chunk is not loaded at stage 1 or more).
 */
static int w_ground_height(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  lua_pushnumber(L, zb_world_ground_height(W(L), &pos));
  return 1;
}

static void push_vec(lua_State *L, zb_vec3 v) {
  lua_createtable(L, 3, 0);
  lua_pushnumber(L, v.x);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, v.y);
  lua_rawseti(L, -2, 2);
  lua_pushnumber(L, v.z);
  lua_rawseti(L, -2, 3);
}

/* @lua world.to_local(pos: PlanetPos): number[]
 * Render coordinates {x, y, z} of a position (relative to the floating origin; x east,
 * y up, z south at the origin).
 */
static int w_to_local(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  push_vec(L, zb_origin_to_local(zb_world_origin(W(L)), P(L), &pos));
  return 1;
}

/* @lua world.from_local(x: number, y: number, z: number): PlanetPos
 * The position at render coordinates.
 */
static int w_from_local(lua_State *L) {
  zb_vec3 l = zb_v3((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3));
  zb_planet_pos pos = zb_origin_to_pos(zb_world_origin(W(L)), P(L), l);
  zb_script_push_pos(L, &pos);
  return 1;
}

/* @lua world.up_at(pos: PlanetPos): number[]
 * The local up direction at a position, in render coordinates (it tilts with the
 * planet's curvature away from the origin).
 */
static int w_up_at(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  const zb_origin *o = zb_world_origin(W(L));
  zb_dvec3 c = zb_planet_to_cart(P(L), &pos);
  double len = sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
  zb_dvec3 d = {c.x / len, c.y / len, c.z / len};
  zb_vec3 up = zb_v3((float)(d.x * o->east.x + d.y * o->east.y + d.z * o->east.z),
                     (float)(d.x * o->up.x + d.y * o->up.y + d.z * o->up.z),
                     (float)-(d.x * o->north.x + d.y * o->north.y + d.z * o->north.z));
  push_vec(L, up);
  return 1;
}

/* @lua world.view_distance(near?: number, far?: number): number, number
 * Gets or sets the streaming rings: chunks get full detail within near metres and coarse
 * detail within far metres (growing with altitude up to twice far).
 */
static int w_view_distance(lua_State *L) {
  zb_stream *st = zb_world_stream(W(L));
  float near_r, far_r;
  zb_stream_get_radii(st, &near_r, &far_r);
  if (!lua_isnoneornil(L, 1)) {
    near_r = (float)luaL_checknumber(L, 1);
    far_r = (float)luaL_optnumber(L, 2, far_r);
    luaL_argcheck(L, near_r >= 0.0f && far_r >= near_r && far_r <= 100000.0f, 1, "0 <= near <= far <= 100 km");
    zb_stream_set_radii(st, near_r, far_r);
  }
  lua_pushnumber(L, near_r);
  lua_pushnumber(L, far_r);
  return 2;
}

/* @lua world.move(pos: PlanetPos, heading: number, forward: number, right?: number, up?: number): PlanetPos, number
 * Moves a position forward metres along heading (radians clockwise from north), right metres
 * to the side and up metres up; returns the new position and the heading carried along the
 * path (straight flight follows a great circle, also across the poles).
 */
static int w_move(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  double heading = luaL_checknumber(L, 2);
  double forward = luaL_checknumber(L, 3);
  double right = luaL_optnumber(L, 4, 0.0);
  double up = luaL_optnumber(L, 5, 0.0);
  double out_heading;
  zb_planet_pos moved = zb_planet_move(P(L), &pos, heading, forward, right, up, &out_heading);
  zb_script_push_pos(L, &moved);
  lua_pushnumber(L, out_heading);
  return 2;
}

/* @lua world.chunk_hash(pos: PlanetPos, stage?: integer): string
 * Generates the position's chunk to a stage (default 1) and returns the hash of its data as
 * 16 hex digits: peers compare it to check that they generate the same world.
 */
static int w_chunk_hash(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  lua_Integer stage = luaL_optinteger(L, 2, ZB_STAGE_TERRAIN);
  luaL_argcheck(L, stage >= ZB_STAGE_COARSE && stage <= ZB_STAGE_SURFACE, 2, "stage 0-2");
  zb_script *s = zb_script_from(L);
  zb_chunk_terrain ct;
  zb_chunk_terrain_init(&ct, pos.chunk);
  if (zb_terrain_generate(T(L), s->cfg.a, &ct, (int)stage) != ZB_OK) {
    zb_chunk_terrain_release(s->cfg.a, &ct);
    return luaL_error(L, "out of memory");
  }
  char hex[17];
  zb_hash_hex(zb_chunk_terrain_hash(&ct), hex);
  zb_chunk_terrain_release(s->cfg.a, &ct);
  lua_pushstring(L, hex);
  return 1;
}

/* @lua world.chunk_stage(pos: PlanetPos): integer
 * Stage the position's chunk is loaded at (0 coarse, 1 heightfield, 2 surface, 3 plants), or -1.
 */
static int w_chunk_stage(lua_State *L) {
  zb_planet_pos pos = zb_script_check_pos(L, 1);
  const zb_chunk_terrain *ct = zb_stream_find(zb_world_stream(W(L)), pos.chunk);
  lua_pushinteger(L, ct ? ct->stage : ZB_STAGE_NONE);
  return 1;
}

/* @luaclass StreamStats
 * @field stage0 integer Loaded chunks at stage 0 (coarse)
 * @field stage1 integer Loaded chunks at stage 1 (heightfield)
 * @field stage2 integer Loaded chunks at stage 2 (surface)
 * @field stage3 integer Loaded chunks at stage 3 (plants)
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
  lua_createtable(L, 0, 12);
  const struct {
    const char *name;
    lua_Integer value;
  } fields[] = {{"stage0", (lua_Integer)st.chunks[0]}, {"stage1", (lua_Integer)st.chunks[1]},
                {"stage2", (lua_Integer)st.chunks[2]}, {"stage3", (lua_Integer)st.chunks[3]},
                {"wanted", (lua_Integer)st.wanted},    {"cached", (lua_Integer)st.cached},
                {"bytes", (lua_Integer)st.bytes},      {"jobs", (lua_Integer)st.jobs},
                {"queued", (lua_Integer)st.queued},    {"generated", (lua_Integer)st.generated},
                {"evicted", (lua_Integer)st.evicted},  {"cancelled", (lua_Integer)st.cancelled}};
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
                                       {"ground_surface", w_ground_surface},
                                       {"water", w_water},
                                       {"flora", w_flora},
                                       {"time", w_time},
                                       {"set_time", w_set_time},
                                       {"sky", w_sky},
                                       {"plants", w_plants},
                                       {"stream_stats", w_stream_stats},
                                       {"chunk_stage", w_chunk_stage},
                                       {"to_local", w_to_local},
                                       {"from_local", w_from_local},
                                       {"up_at", w_up_at},
                                       {"view_distance", w_view_distance},
                                       {"move", w_move},
                                       {"chunk_hash", w_chunk_hash},
                                       {NULL, NULL}};
  lua_newtable(s->L);
  luaL_setfuncs(s->L, world_fns, 0);
  lua_setglobal(s->L, "world");
}

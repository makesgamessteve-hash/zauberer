/* SPDX-License-Identifier: MIT */
/* gesture.*: recognising the forms of weaves drawn by the player. */
#include "input/gesture.h"
#include "script/script_internal.h"

#include <stdlib.h>

/* @lua gesture.forms(): string[]
 * The eight forms: tap, line, circle, spiral, triangle, square, zigzag, wave.
 */
static int g_forms(lua_State *L) {
  lua_createtable(L, ZB_FORMS, 0);
  for (int k = 0; k < ZB_FORMS; ++k) {
    lua_pushstring(L, zb_form_name((zb_form)k));
    lua_rawseti(L, -2, k + 1);
  }
  return 1;
}

/* @luaclass Gesture
 * @field form string The form, or "none" when the stroke is like none
 * @field accuracy number 0 .. 1: how well it matches (the weave's control)
 * @field second string The next best form
 * @field second_accuracy number
 * @field size number Half the stroke's extent, in the units of the points
 * @field x number Its centre
 * @field y number
 * @field angle number Radians from the first point to the last
 */

/* @lua gesture.recognize(points: number[], options?: GestureOptions): Gesture
 * Recognises a stroke given as { x1, y1, x2, y2, ... } (up to 4096 points),
 * in any orientation, drawn either way round. Strokes shorter than tap_size
 * are taps.
 * @field tap_size number? Default 12 (input units)
 * @field assist boolean? Gesture assist: kinder to sloppy shapes
 */
static int g_recognize(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_Integer len = (lua_Integer)lua_rawlen(L, 1);
  luaL_argcheck(L, len % 2 == 0 && len / 2 <= ZB_GESTURE_MAX_POINTS, 1, "points must be x, y pairs (up to 4096)");
  zb_gesture_options opt = {0.0, false};
  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "tap_size");
    opt.tap_size = luaL_optnumber(L, -1, 0.0);
    lua_getfield(L, 2, "assist");
    opt.assist = lua_toboolean(L, -1);
    lua_pop(L, 2);
  }
  int n = (int)(len / 2);
  float *xy = n > 0 ? malloc(sizeof(float) * 2 * (size_t)n) : NULL;
  if (n > 0 && !xy) {
    return luaL_error(L, "out of memory");
  }
  for (int k = 0; k < 2 * n; ++k) {
    lua_rawgeti(L, 1, k + 1);
    xy[k] = (float)luaL_checknumber(L, -1);
    lua_pop(L, 1);
  }
  zb_gesture g;
  zb_gesture_recognize(xy, n, &opt, &g);
  free(xy);
  lua_createtable(L, 0, 8);
  lua_pushstring(L, zb_form_name(g.form));
  lua_setfield(L, -2, "form");
  lua_pushnumber(L, g.accuracy);
  lua_setfield(L, -2, "accuracy");
  lua_pushstring(L, zb_form_name(g.second));
  lua_setfield(L, -2, "second");
  lua_pushnumber(L, g.second_accuracy);
  lua_setfield(L, -2, "second_accuracy");
  lua_pushnumber(L, g.size);
  lua_setfield(L, -2, "size");
  lua_pushnumber(L, g.cx);
  lua_setfield(L, -2, "x");
  lua_pushnumber(L, g.cy);
  lua_setfield(L, -2, "y");
  lua_pushnumber(L, g.angle);
  lua_setfield(L, -2, "angle");
  return 1;
}

/* @lua gesture.template(form: string, count?: integer): number[]
 * The stroke of a form at unit size as { x1, y1, ... } with count points
 * (default 32, 2 .. 512): for tracing guides and tests.
 */
static int g_template(lua_State *L) {
  zb_form f = zb_form_by_name(luaL_checkstring(L, 1));
  luaL_argcheck(L, f != ZB_FORM_NONE, 1, "no such form");
  lua_Integer count = luaL_optinteger(L, 2, 32);
  luaL_argcheck(L, count >= 2 && count <= 512, 2, "count must be 2..512");
  float xy[2 * 512];
  zb_gesture_template(f, xy, (int)count);
  lua_createtable(L, (int)count * 2, 0);
  for (int k = 0; k < 2 * count; ++k) {
    lua_pushnumber(L, xy[k]);
    lua_rawseti(L, -2, k + 1);
  }
  return 1;
}

void zb_script_open_gesture(zb_script *s) {
  static const luaL_Reg fns[] = {
      {"forms", g_forms}, {"recognize", g_recognize}, {"template", g_template}, {NULL, NULL}};
  lua_newtable(s->L);
  luaL_setfuncs(s->L, fns, 0);
  lua_setglobal(s->L, "gesture");
}

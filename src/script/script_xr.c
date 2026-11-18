/* SPDX-License-Identifier: MIT */
/* Lua bindings: xr (WebXR session and head tracking).
 *
 * @luaclass XrPose
 * @field position number[] {x, y, z} in metres, relative to the session origin
 * @field orientation number[] Rotation quaternion {x, y, z, w}
 *
 * @luaclass XrState
 * @field available boolean An immersive VR session can be started (web build with a headset)
 * @field active boolean A session is running
 * @field emulated boolean The running session is emulated (test API)
 * @field views integer Views of the last XR frame (0 without a session)
 * @field frames integer XR frames rendered in the running session
 * @field size integer[] Width and height of the XR framebuffer in pixels
 * @field head XrPose Viewer (head) pose relative to the session origin
 */
#include "script/script_internal.h"

#include <string.h>

static void push_floats(lua_State *L, const float *v, int n) {
  lua_createtable(L, n, 0);
  for (int i = 0; i < n; ++i) {
    lua_pushnumber(L, (lua_Number)v[i]);
    lua_rawseti(L, -2, i + 1);
  }
}

/* @lua xr.state(): XrState
 * The WebXR session. While it runs, the camera set with render.camera is the session origin (the seat on
 * the carpet): every eye sees the scene from the head pose relative to it, with the headset's field of
 * view. The head pose is updated before the frame hooks of every XR frame; it is presentation data and
 * not part of replays. A session start or end reaches the event hook as "xr_session" (value "started" or
 * "ended"); controller buttons are bindings such as "xr:select" (see input.bind).
 */
static int x_state(lua_State *L) {
  zb_script *s = zb_script_from(L);
  zb_xr_state st;
  memset(&st, 0, sizeof st);
  st.frame.head.orientation = zb_quat_identity();
  if (s->cfg.host.xr_state) {
    s->cfg.host.xr_state(s->cfg.host.ctx, &st);
  }
  lua_createtable(L, 0, 7);
  lua_pushboolean(L, st.available);
  lua_setfield(L, -2, "available");
  lua_pushboolean(L, st.active);
  lua_setfield(L, -2, "active");
  lua_pushboolean(L, st.emulated);
  lua_setfield(L, -2, "emulated");
  lua_pushinteger(L, st.active ? st.frame.view_count : 0);
  lua_setfield(L, -2, "views");
  lua_pushinteger(L, (lua_Integer)st.frames);
  lua_setfield(L, -2, "frames");
  lua_createtable(L, 2, 0);
  lua_pushinteger(L, st.frame.width);
  lua_rawseti(L, -2, 1);
  lua_pushinteger(L, st.frame.height);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "size");
  const zb_xr_pose *h = &st.frame.head;
  const float position[3] = {h->position.x, h->position.y, h->position.z};
  const float orientation[4] = {h->orientation.x, h->orientation.y, h->orientation.z, h->orientation.w};
  lua_createtable(L, 0, 2);
  push_floats(L, position, 3);
  lua_setfield(L, -2, "position");
  push_floats(L, orientation, 4);
  lua_setfield(L, -2, "orientation");
  lua_setfield(L, -2, "head");
  return 1;
}

/* Reads n numbers of the array field `name` of the table at index 1 (kept when absent). */
static void read_floats(lua_State *L, const char *name, float *out, int n) {
  if (lua_getfield(L, 1, name) == LUA_TTABLE) {
    for (int i = 0; i < n; ++i) {
      lua_rawgeti(L, -1, i + 1);
      int ok = 0;
      lua_Number v = lua_tonumberx(L, -1, &ok);
      if (!ok) {
        luaL_error(L, "head.%s needs %d numbers", name, n);
      }
      out[i] = (float)v;
      lua_pop(L, 1);
    }
  } else if (!lua_isnil(L, -1)) {
    luaL_error(L, "head.%s must be a table of %d numbers", name, n);
  }
  lua_pop(L, 1);
}

/* @lua xr.emulate(head?: XrPose|false)
 * Test API: starts or moves an emulated headset at the head pose (default: the session origin). Its two
 * views render side by side into the window, eyes 64 mm apart, through the same path as a real session.
 * false ends the session.
 */
static int x_emulate(lua_State *L) {
  zb_script *s = zb_script_from(L);
  const zb_xr_pose *arg = NULL;
  zb_xr_pose head = {zb_v3(0.0f, 0.0f, 0.0f), zb_quat_identity()};
  if (!(lua_isboolean(L, 1) && !lua_toboolean(L, 1))) {
    if (!lua_isnoneornil(L, 1)) {
      luaL_checktype(L, 1, LUA_TTABLE);
      float p[3] = {0.0f, 0.0f, 0.0f}, q[4] = {0.0f, 0.0f, 0.0f, 1.0f};
      read_floats(L, "position", p, 3);
      read_floats(L, "orientation", q, 4);
      head.position = zb_v3(p[0], p[1], p[2]);
      head.orientation = (zb_quat){q[0], q[1], q[2], q[3]};
    }
    arg = &head;
  }
  if (s->cfg.host.xr_emulate) {
    s->cfg.host.xr_emulate(s->cfg.host.ctx, arg);
  }
  return 0;
}

/* @lua xr.emulate_button(hand: "left"|"right"|"none", button: "select"|"squeeze", down: boolean)
 * Test API: a controller button event, as a headset reports it ("none" for sources without a hand).
 */
static int x_emulate_button(lua_State *L) {
  static const char *const hands[] = {"none", "left", "right", NULL}; /* zb_xr_hand */
  static const char *const buttons[] = {"select", "squeeze", NULL};   /* zb_xr_button */
  int hand = luaL_checkoption(L, 1, NULL, hands);
  int button = luaL_checkoption(L, 2, NULL, buttons);
  luaL_checktype(L, 3, LUA_TBOOLEAN);
  zb_script *s = zb_script_from(L);
  if (s->cfg.host.xr_button) {
    s->cfg.host.xr_button(s->cfg.host.ctx, hand, button, lua_toboolean(L, 3));
  }
  return 0;
}

void zb_script_open_xr(zb_script *s) {
  lua_State *L = s->L;
  lua_newtable(L);
  lua_pushcfunction(L, x_state);
  lua_setfield(L, -2, "state");
  if (s->cfg.test_api) {
    lua_pushcfunction(L, x_emulate);
    lua_setfield(L, -2, "emulate");
    lua_pushcfunction(L, x_emulate_button);
    lua_setfield(L, -2, "emulate_button");
  }
  lua_setglobal(L, "xr");
}

/* SPDX-License-Identifier: MIT */
/* Lua bindings: net (co-op sessions) and the value codec that carries Lua
 * values over the network.
 *
 * @luaclass NetStatus
 * @field role string "none", "host", "joining" or "client"
 * @field player integer This game's player index (0 = host), -1 when not in a session
 * @field players integer[] Player indices in the session
 * @field error string Why the last session ended or failed ("" if it did not)
 * @field rtt_ms integer[] Last round trip per player in milliseconds
 * @field sent integer Bytes sent
 * @field received integer Bytes received
 *
 * @luaclass NetMessage
 * @field event string "data", "joined", "left", "welcome" or "ended"
 * @field player integer The sender (data) or the player concerned
 * @field channel string? Channel name (data)
 * @field value any? The value sent (data)
 */
#include "script/script_internal.h"

#include <string.h>

#include "net/session.h"

#define CODEC_MAX_DEPTH 16
#define CODEC_MAX_BYTES (256u * 1024u)

enum { T_NIL, T_FALSE, T_TRUE, T_INT, T_FLOAT, T_STRING, T_TABLE };

/* ---- Codec ---------------------------------------------------------------------- */

static bool put(zb_array *out, const void *data, size_t len) {
  if (out->len + len > CODEC_MAX_BYTES || zb_array_reserve(out, out->len + len) != ZB_OK) {
    return false;
  }
  memcpy(out->data + out->len, data, len);
  out->len += len;
  return true;
}

static bool put_u8(zb_array *out, uint8_t v) { return put(out, &v, 1); }

static bool put_le(zb_array *out, uint64_t v, int bytes) {
  uint8_t b[8];
  for (int i = 0; i < bytes; ++i) {
    b[i] = (uint8_t)(v >> (8 * i));
  }
  return put(out, b, (size_t)bytes);
}

static const char *encode(lua_State *L, int idx, zb_array *out, int depth) {
  idx = lua_absindex(L, idx);
  switch (lua_type(L, idx)) {
    case LUA_TNIL: return put_u8(out, T_NIL) ? NULL : "message too large";
    case LUA_TBOOLEAN: return put_u8(out, lua_toboolean(L, idx) ? T_TRUE : T_FALSE) ? NULL : "message too large";
    case LUA_TNUMBER:
      if (lua_isinteger(L, idx)) {
        return put_u8(out, T_INT) && put_le(out, (uint64_t)lua_tointeger(L, idx), 8) ? NULL : "message too large";
      } else {
        double d = lua_tonumber(L, idx);
        uint64_t bits;
        memcpy(&bits, &d, sizeof bits);
        return put_u8(out, T_FLOAT) && put_le(out, bits, 8) ? NULL : "message too large";
      }
    case LUA_TSTRING: {
      size_t len;
      const char *s = lua_tolstring(L, idx, &len);
      return put_u8(out, T_STRING) && put_le(out, len, 4) && put(out, s, len) ? NULL : "message too large";
    }
    case LUA_TTABLE: {
      if (depth >= CODEC_MAX_DEPTH) {
        return "tables nested too deeply";
      }
      size_t count = 0;
      lua_pushnil(L);
      while (lua_next(L, idx)) {
        lua_pop(L, 1);
        count++;
      }
      if (!put_u8(out, T_TABLE) || !put_le(out, count, 4)) {
        return "message too large";
      }
      lua_pushnil(L);
      while (lua_next(L, idx)) {
        int kt = lua_type(L, -2);
        if (kt != LUA_TSTRING && kt != LUA_TNUMBER && kt != LUA_TBOOLEAN) {
          lua_pop(L, 2);
          return "table keys must be strings, numbers or booleans";
        }
        const char *err = encode(L, -2, out, depth + 1);
        if (!err) {
          err = encode(L, -1, out, depth + 1);
        }
        lua_pop(L, 1);
        if (err) {
          lua_pop(L, 1);
          return err;
        }
      }
      return NULL;
    }
    default: return "only nil, booleans, numbers, strings and tables can be sent";
  }
}

typedef struct reader {
  const uint8_t *p;
  size_t len, pos;
} reader;

static bool get_le(reader *r, int bytes, uint64_t *v) {
  if (r->len - r->pos < (size_t)bytes) {
    return false;
  }
  *v = 0;
  for (int i = bytes - 1; i >= 0; --i) {
    *v = *v << 8 | r->p[r->pos + (size_t)i];
  }
  r->pos += (size_t)bytes;
  return true;
}

/* Pushes the decoded value; false (nothing pushed) on malformed input. */
static bool decode(lua_State *L, reader *r, int depth) {
  if (r->pos >= r->len || depth > CODEC_MAX_DEPTH || !lua_checkstack(L, 3)) {
    return false;
  }
  uint8_t tag = r->p[r->pos++];
  uint64_t v;
  switch (tag) {
    case T_NIL: lua_pushnil(L); return true;
    case T_FALSE:
    case T_TRUE: lua_pushboolean(L, tag == T_TRUE); return true;
    case T_INT:
      if (!get_le(r, 8, &v)) {
        return false;
      }
      lua_pushinteger(L, (lua_Integer)v);
      return true;
    case T_FLOAT: {
      if (!get_le(r, 8, &v)) {
        return false;
      }
      double d;
      memcpy(&d, &v, sizeof d);
      lua_pushnumber(L, d);
      return true;
    }
    case T_STRING:
      if (!get_le(r, 4, &v) || r->len - r->pos < v) {
        return false;
      }
      lua_pushlstring(L, (const char *)r->p + r->pos, (size_t)v);
      r->pos += (size_t)v;
      return true;
    case T_TABLE: {
      if (!get_le(r, 4, &v) || v > r->len - r->pos) {
        return false;
      }
      lua_createtable(L, 0, (int)(v < 64 ? v : 64));
      for (uint64_t i = 0; i < v; ++i) {
        if (!decode(L, r, depth + 1)) {
          lua_pop(L, 1);
          return false;
        }
        if (lua_isnil(L, -1) || lua_type(L, -1) == LUA_TTABLE ||
            (lua_type(L, -1) == LUA_TNUMBER && lua_tonumber(L, -1) != lua_tonumber(L, -1))) {
          lua_pop(L, 2); /* invalid key */
          return false;
        }
        if (!decode(L, r, depth + 1)) {
          lua_pop(L, 2);
          return false;
        }
        lua_rawset(L, -3);
      }
      return true;
    }
    default: return false;
  }
}

const char *zb_script_net_encode(lua_State *L, int idx, zb_array *out) {
  out->len = 0;
  return encode(L, idx, out, 0);
}

bool zb_script_net_decode(lua_State *L, const void *data, size_t len) {
  reader r = {data, len, 0};
  if (!decode(L, &r, 0)) {
    return false;
  }
  if (r.pos != len) {
    lua_pop(L, 1);
    return false;
  }
  return true;
}

/* ---- API --------------------------------------------------------------------------- */

static zb_net *N(lua_State *L) { return zb_script_from(L)->cfg.net; }

static int unavailable(lua_State *L) {
  lua_pushnil(L);
  lua_pushstring(L, "networking is not available");
  return 2;
}

/* @lua net.host(port?: integer): integer?, string?
 * Hosts a co-op session (desktop only); returns the port in use (port 0 or omitted picks a free
 * one), or nil and why it failed.
 */
static int n_host(lua_State *L) {
  zb_script *s = zb_script_from(L);
  if (!s->cfg.net) {
    return unavailable(L);
  }
  lua_Integer port = luaL_optinteger(L, 1, 0);
  luaL_argcheck(L, port >= 0 && port <= 65535, 1, "port must be 0-65535");
  char err[128];
  uint16_t bound = 0;
  zb_transport *t = zb_ws_server_create(s->cfg.a, (uint16_t)port, &bound, err, sizeof err);
  if (!t) {
    lua_pushnil(L);
    lua_pushstring(L, err);
    return 2;
  }
  zb_net_host(s->cfg.net, t);
  lua_pushinteger(L, bound);
  return 1;
}

/* @lua net.join(url: string): boolean, string?
 * Starts joining the session at a ws:// address; net.receive reports "welcome" or "ended".
 */
static int n_join(lua_State *L) {
  zb_script *s = zb_script_from(L);
  const char *url = luaL_checkstring(L, 1);
  if (!s->cfg.net) {
    lua_pushboolean(L, false);
    lua_pushstring(L, "networking is not available");
    return 2;
  }
  char err[160];
  zb_transport *t = zb_ws_client_create(s->cfg.a, url, err, sizeof err);
  if (!t) {
    lua_pushboolean(L, false);
    lua_pushstring(L, err);
    return 2;
  }
  zb_net_join(s->cfg.net, t);
  lua_pushboolean(L, true);
  return 1;
}

/* @lua net.leave()
 * Leaves the session (a host ends it for everyone).
 */
static int n_leave(lua_State *L) {
  if (N(L)) {
    zb_net_leave(N(L));
  }
  return 0;
}

/* @lua net.status(): NetStatus
 * The session's state.
 */
static int n_status(lua_State *L) {
  zb_net *n = N(L);
  static const char *const roles[] = {"none", "host", "joining", "client"};
  lua_createtable(L, 0, 7);
  lua_pushstring(L, n ? roles[zb_net_role_of(n)] : "none");
  lua_setfield(L, -2, "role");
  lua_pushinteger(L, n ? zb_net_player(n) : -1);
  lua_setfield(L, -2, "player");
  lua_newtable(L);
  unsigned mask = n ? zb_net_players(n) : 0;
  int k = 0;
  for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
    if (mask & (1u << i)) {
      lua_pushinteger(L, i);
      lua_rawseti(L, -2, ++k);
    }
  }
  lua_setfield(L, -2, "players");
  lua_pushstring(L, n ? zb_net_error(n) : "networking is not available");
  lua_setfield(L, -2, "error");
  zb_net_stats st;
  memset(&st, 0, sizeof st);
  if (n) {
    zb_net_get_stats(n, &st);
  }
  lua_createtable(L, ZB_NET_MAX_PLAYERS, 0);
  for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
    lua_pushinteger(L, st.rtt_ms[i]);
    lua_rawseti(L, -2, i + 1);
  }
  lua_setfield(L, -2, "rtt_ms");
  lua_pushinteger(L, (lua_Integer)st.bytes_sent);
  lua_setfield(L, -2, "sent");
  lua_pushinteger(L, (lua_Integer)st.bytes_received);
  lua_setfield(L, -2, "received");
  return 1;
}

/* @lua net.send(channel: string, value: any, to?: integer): boolean, string?
 * Sends a value (nil, booleans, numbers, strings and tables of them) on a channel to a player,
 * or to everyone when to is omitted (clients' messages travel through the host).
 */
static int n_send(lua_State *L) {
  zb_script *s = zb_script_from(L);
  const char *channel = luaL_checkstring(L, 1);
  luaL_checkany(L, 2);
  int to = (int)luaL_optinteger(L, 3, ZB_NET_TO_ALL);
  if (!s->cfg.net) {
    lua_pushboolean(L, false);
    lua_pushstring(L, "networking is not available");
    return 2;
  }
  zb_array buf;
  zb_array_init(&buf, s->cfg.a, 1);
  const char *err = zb_script_net_encode(L, 2, &buf);
  zb_result r = err ? ZB_ERR_INVALID : zb_net_send(s->cfg.net, to, channel, buf.data, buf.len);
  zb_array_free(&buf);
  lua_pushboolean(L, r == ZB_OK);
  if (r == ZB_OK) {
    return 1;
  }
  lua_pushstring(L, err ? err : zb_result_str(r));
  return 2;
}

/* @lua net.receive(): NetMessage[]
 * Messages and session events since the last call, in arrival order.
 */
static int n_receive(lua_State *L) {
  zb_net *n = N(L);
  lua_newtable(L);
  if (!n) {
    return 1;
  }
  static const char *const kinds[] = {"joined", "left", "data", "welcome", "ended"};
  zb_net_event ev;
  int k = 0;
  while (zb_net_next(n, &ev)) {
    lua_createtable(L, 0, 4);
    lua_pushstring(L, kinds[ev.kind]);
    lua_setfield(L, -2, "event");
    lua_pushinteger(L, ev.player);
    lua_setfield(L, -2, "player");
    if (ev.kind == ZB_NET_EV_DATA) {
      lua_pushstring(L, ev.channel);
      lua_setfield(L, -2, "channel");
      if (zb_script_net_decode(L, ev.data, ev.len)) {
        lua_setfield(L, -2, "value");
      }
    }
    lua_rawseti(L, -2, ++k);
  }
  return 1;
}

void zb_script_open_net(zb_script *s) {
  static const luaL_Reg net_fns[] = {{"host", n_host}, {"join", n_join},       {"leave", n_leave}, {"status", n_status},
                                     {"send", n_send}, {"receive", n_receive}, {NULL, NULL}};
  lua_newtable(s->L);
  luaL_setfuncs(s->L, net_fns, 0);
  lua_setglobal(s->L, "net");
}

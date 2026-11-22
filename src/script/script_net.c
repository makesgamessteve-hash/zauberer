/* SPDX-License-Identifier: MIT */
/* Lua bindings: net (co-op sessions) and the value codec that carries Lua
 * values over the network.
 *
 * @luaclass NetStatus
 * @field role string "none", "host", "joining" or "client"
 * @field player integer This game's player index (0 = host), -1 when not in a session
 * @field players integer[] Player indices in the session (players who are away included)
 * @field away integer[] Players whose connection was lost and who may still come back
 * @field error string Why the last session ended or failed ("" if it did not)
 * @field mismatches NetMismatch[] Packages that differed when the host refused the join
 * @field can_rejoin boolean The connection was lost and net.rejoin may bring this player back
 * @field invite_code string? The relay's invite code while hosting through a relay
 * @field lan boolean LAN discovery queries are being answered
 * @field open boolean New heirs may join (host)
 * @field rtt_ms integer[] Last round trip per player in milliseconds
 * @field sent integer Bytes sent
 * @field received integer Bytes received
 * @field sent_to integer[] Bytes sent to each player (index = player + 1; clients: [1] = the host)
 * @field received_from integer[] Bytes received from each player
 * @field kbps_out integer[] kbit/s sent to each player over the last second
 * @field kbps_in integer[] kbit/s received from each player over the last second
 * @field coalesced integer Latest-only messages replaced by newer ones before they were sent
 *
 * @luaclass NetMismatch
 * @field id string Package id
 * @field host string The host's version ("" when the host does not have it enabled)
 * @field client string This game's version ("" when it is not enabled here)
 *
 * @luaclass NetMessage
 * @field event string "data", "joined", "left", "welcome", "ended", "away", "back", "blob_progress", "blob", "blob_failed" or "blob_sent"
 * @field player integer The sender (data, blobs arriving), the receiver (blobs sent) or the player concerned
 * @field channel string? Channel name (data)
 * @field value any? The value sent (data)
 * @field name string? The blob's name (blob events)
 * @field received integer? Bytes of the blob received so far (blob_progress)
 * @field total integer? The blob's size (blob_progress, blob)
 * @field hash string? The blob's SHA-1 in hex (blob events)
 * @field data string? The blob (blob)
 * @field reason string? Why a blob failed (blob_failed)
 * @field error string? Why the session ended (ended)
 * @field mismatches NetMismatch[]? Packages that differed (ended)
 *
 * @luaclass LanSession
 * @field name string The session's name
 * @field address string The host's IPv4 address
 * @field port integer The port to join on
 * @field url string ws:// address for net.join
 * @field players integer Players in the session
 * @field max_players integer Players a session can hold
 * @field protocol integer The host's network protocol (joining needs the same)
 */
#include "script/script_internal.h"

#include <stdio.h>
#include <string.h>

#include "net/cache.h"
#include "net/lan.h"
#include "net/relay.h"
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

#define STATE_KEY "zb.net"
#define SCAN_INTERVAL_NS 1000000000ull
#define SCAN_MAX_AGE_NS 3000000000ull

/* What the bindings remember between calls (a userdata in the registry). */
typedef struct net_state {
  char join_url[256];
  char join_code[ZB_RELAY_CODE_LEN + 1]; /* "" = a direct connection */
  uint16_t host_port;                    /* last port hosted on directly (for LAN announcements) */
  zb_lan *scan;
  bool queried;
  uint64_t last_query;
} net_state;

static int state_gc(lua_State *L) {
  net_state *st = lua_touserdata(L, 1);
  zb_lan_close(st->scan);
  st->scan = NULL;
  return 0;
}

static net_state *S(lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, STATE_KEY);
  net_state *st = lua_touserdata(L, -1);
  lua_pop(L, 1);
  return st;
}

static zb_net *N(lua_State *L) { return zb_script_from(L)->cfg.net; }

static int unavailable(lua_State *L) {
  lua_pushnil(L);
  lua_pushstring(L, "networking is not available");
  return 2;
}

static int failed(lua_State *L, const char *why) {
  lua_pushboolean(L, false);
  lua_pushstring(L, why);
  return 2;
}

static uint64_t net_clock(void *ctx) { return zb_net_now(ctx); }

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
  S(L)->host_port = bound;
  lua_pushinteger(L, bound);
  return 1;
}

/* @lua net.host_relay(url: string): boolean, string?
 * Hosts a co-op session through a relay at a ws:// address (also in the browser); once the relay
 * answers, net.status().invite_code holds the code to give to friends. If the relay refuses or is
 * lost, net.receive reports "ended" with the reason.
 */
static int n_host_relay(lua_State *L) {
  zb_script *s = zb_script_from(L);
  const char *url = luaL_checkstring(L, 1);
  if (!s->cfg.net) {
    return failed(L, "networking is not available");
  }
  char err[160];
  zb_transport *up = zb_ws_client_create(s->cfg.a, url, err, sizeof err);
  zb_transport *t = up ? zb_relay_host_transport(s->cfg.a, up, net_clock, s->cfg.net) : NULL;
  if (!t) {
    return failed(L, up ? "out of memory" : err);
  }
  zb_net_host(s->cfg.net, t);
  S(L)->host_port = 0;
  lua_pushboolean(L, true);
  return 1;
}

/* Joins (or resumes) through url, directly or with a relay code; remembers how for net.rejoin. */
static int join_with(lua_State *L, const char *url, const char *code, bool resume) {
  zb_script *s = zb_script_from(L);
  char err[160];
  zb_transport *t = zb_ws_client_create(s->cfg.a, url, err, sizeof err);
  if (t && code[0]) {
    t = zb_relay_join_transport(s->cfg.a, t, code);
    snprintf(err, sizeof err, "out of memory");
  }
  if (!t) {
    return failed(L, err);
  }
  net_state *st = S(L);
  if (st->join_url != url) {
    snprintf(st->join_url, sizeof st->join_url, "%s", url);
    snprintf(st->join_code, sizeof st->join_code, "%s", code);
  }
  if (resume) {
    zb_net_resume(s->cfg.net, t);
  } else {
    zb_net_join(s->cfg.net, t);
  }
  lua_pushboolean(L, true);
  return 1;
}

/* @lua net.join(url: string): boolean, string?
 * Starts joining the session at a ws:// address; net.receive reports "welcome" or "ended".
 */
static int n_join(lua_State *L) {
  const char *url = luaL_checkstring(L, 1);
  if (!N(L)) {
    return failed(L, "networking is not available");
  }
  return join_with(L, url, "", false);
}

/* @lua net.join_code(url: string, code: string): boolean, string?
 * Starts joining the session with an invite code through the relay at a ws:// address (case,
 * spaces and dashes in the code do not matter); net.receive reports "welcome" or "ended" (with
 * the relay's reason for unknown or expired codes and full sessions).
 */
static int n_join_code(lua_State *L) {
  const char *url = luaL_checkstring(L, 1);
  const char *code = luaL_checkstring(L, 2);
  if (!N(L)) {
    return failed(L, "networking is not available");
  }
  char normal[ZB_RELAY_CODE_LEN + 1];
  if (!zb_relay_normalize_code(code, normal)) {
    char why[96];
    snprintf(why, sizeof why, "not an invite code: %.40s", code);
    return failed(L, why);
  }
  return join_with(L, url, normal, false);
}

/* @lua net.rejoin(): boolean, string?
 * After the connection to the host was lost (net.status().can_rejoin), connects again the same
 * way (address or invite code); within the host's grace period (30 s) this game gets its player
 * index back. net.receive reports "welcome" or "ended" as for a join.
 */
static int n_rejoin(lua_State *L) {
  zb_net *n = N(L);
  if (!n) {
    return failed(L, "networking is not available");
  }
  if (!zb_net_can_resume(n)) {
    return failed(L, "there is no lost session to rejoin");
  }
  net_state *st = S(L);
  return join_with(L, st->join_url, st->join_code, true);
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

/* @lua net.kick(player: integer): boolean, string?
 * Host: removes a joined player from the session (they are told why; their heir rests in the pocket home).
 */
static int n_kick(lua_State *L) {
  zb_net *n = N(L);
  lua_Integer player = luaL_checkinteger(L, 1);
  if (!n || player < 0 || player > 255 || zb_net_kick(n, (int)player) != ZB_OK) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, "only the host removes a player who is in the session");
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}

/* @lua net.set_open(open: boolean)
 * Host: lets new heirs join, or not (those away may still come back).
 */
static int n_set_open(lua_State *L) {
  if (N(L)) {
    zb_net_set_open(N(L), lua_toboolean(L, 1));
  }
  return 0;
}

static void push_players(lua_State *L, unsigned mask) {
  lua_newtable(L);
  int k = 0;
  for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
    if (mask & (1u << i)) {
      lua_pushinteger(L, i);
      lua_rawseti(L, -2, ++k);
    }
  }
}

static void push_per_player32(lua_State *L, const uint32_t *v) {
  lua_createtable(L, ZB_NET_MAX_PLAYERS, 0);
  for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
    lua_pushinteger(L, (lua_Integer)v[i]);
    lua_rawseti(L, -2, i + 1);
  }
}

static void push_per_player64(lua_State *L, const uint64_t *v) {
  lua_createtable(L, ZB_NET_MAX_PLAYERS, 0);
  for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
    lua_pushinteger(L, (lua_Integer)v[i]);
    lua_rawseti(L, -2, i + 1);
  }
}

static void push_mismatches(lua_State *L, zb_net *n) {
  const zb_net_mismatch *mm = NULL;
  size_t count = n ? zb_net_mismatches(n, &mm) : 0;
  lua_createtable(L, (int)count, 0);
  for (size_t i = 0; i < count; ++i) {
    lua_createtable(L, 0, 3);
    lua_pushstring(L, mm[i].id);
    lua_setfield(L, -2, "id");
    lua_pushstring(L, mm[i].host_version);
    lua_setfield(L, -2, "host");
    lua_pushstring(L, mm[i].client_version);
    lua_setfield(L, -2, "client");
    lua_rawseti(L, -2, (lua_Integer)i + 1);
  }
}

/* @lua net.status(): NetStatus
 * The session's state.
 */
static int n_status(lua_State *L) {
  zb_net *n = N(L);
  static const char *const roles[] = {"none", "host", "joining", "client"};
  lua_createtable(L, 0, 18);
  lua_pushstring(L, n ? roles[zb_net_role_of(n)] : "none");
  lua_setfield(L, -2, "role");
  lua_pushinteger(L, n ? zb_net_player(n) : -1);
  lua_setfield(L, -2, "player");
  push_players(L, n ? zb_net_players(n) : 0);
  lua_setfield(L, -2, "players");
  push_players(L, n ? zb_net_away(n) : 0);
  lua_setfield(L, -2, "away");
  lua_pushstring(L, n ? zb_net_error(n) : "networking is not available");
  lua_setfield(L, -2, "error");
  push_mismatches(L, n);
  lua_setfield(L, -2, "mismatches");
  lua_pushboolean(L, n && zb_net_can_resume(n));
  lua_setfield(L, -2, "can_rejoin");
  if (n && zb_net_invite_code(n)[0]) {
    lua_pushstring(L, zb_net_invite_code(n));
    lua_setfield(L, -2, "invite_code");
  }
  lua_pushboolean(L, n && zb_net_lan_announcing(n));
  lua_setfield(L, -2, "lan");
  lua_pushboolean(L, n && zb_net_is_open(n));
  lua_setfield(L, -2, "open");
  zb_net_stats st;
  memset(&st, 0, sizeof st);
  if (n) {
    zb_net_get_stats(n, &st);
  }
  push_per_player32(L, st.rtt_ms);
  lua_setfield(L, -2, "rtt_ms");
  lua_pushinteger(L, (lua_Integer)st.bytes_sent);
  lua_setfield(L, -2, "sent");
  lua_pushinteger(L, (lua_Integer)st.bytes_received);
  lua_setfield(L, -2, "received");
  push_per_player64(L, st.sent_to);
  lua_setfield(L, -2, "sent_to");
  push_per_player64(L, st.received_from);
  lua_setfield(L, -2, "received_from");
  push_per_player32(L, st.kbps_out);
  lua_setfield(L, -2, "kbps_out");
  push_per_player32(L, st.kbps_in);
  lua_setfield(L, -2, "kbps_in");
  lua_pushinteger(L, (lua_Integer)st.coalesced);
  lua_setfield(L, -2, "coalesced");
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
    return failed(L, "networking is not available");
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

/* @lua net.set_channel(channel: string, class: string): boolean, string?
 * Sets how a channel travels (the same on every player): "reliable" (the default: every message
 * arrives) or "latest" (while a connection is over its bandwidth budget only the newest unsent
 * message per channel and sender is kept; for snapshots and positions).
 */
static int n_set_channel(lua_State *L) {
  const char *channel = luaL_checkstring(L, 1);
  static const char *const classes[] = {"reliable", "latest", NULL};
  int cls = luaL_checkoption(L, 2, NULL, classes);
  if (!N(L)) {
    return failed(L, "networking is not available");
  }
  zb_result r = zb_net_set_channel_class(N(L), channel, (zb_net_channel_class)cls);
  if (r != ZB_OK) {
    return failed(L, zb_result_str(r));
  }
  lua_pushboolean(L, true);
  return 1;
}

/* @lua net.set_budget(kbps: integer, burst_kbps?: integer)
 * Sets the bandwidth budget per connection (default 64 kbit/s on average with bursts of up to
 * 256 kbit/s for a second). Reliable messages count against it but are always sent.
 */
static int n_set_budget(lua_State *L) {
  lua_Integer kbps = luaL_checkinteger(L, 1);
  lua_Integer burst = luaL_optinteger(L, 2, 0);
  luaL_argcheck(L, kbps > 0 && kbps <= 1000000, 1, "kbps must be 1-1000000");
  luaL_argcheck(L, burst >= 0 && burst <= 1000000, 2, "burst_kbps must be 0-1000000");
  if (N(L)) {
    zb_net_set_budget(N(L), (uint32_t)kbps, (uint32_t)burst);
  }
  return 0;
}

/* @lua net.send_blob(to: integer, name: string, data: string): boolean, string?
 * Sends a large blob (such as the world bundle) to one player (clients: only to the host, 0) in
 * flow-controlled chunks that do not hold up the channels; the receiver gets "blob_progress"
 * events and then "blob" once the content hash matches, the sender "blob_sent" or "blob_failed".
 */
static int n_send_blob(lua_State *L) {
  lua_Integer to = luaL_checkinteger(L, 1);
  const char *name = luaL_checkstring(L, 2);
  size_t len;
  const char *data = luaL_checklstring(L, 3, &len);
  if (!N(L)) {
    return failed(L, "networking is not available");
  }
  zb_result r = to >= 0 && to < ZB_NET_MAX_PLAYERS ? zb_net_send_blob(N(L), (int)to, name, data, len) : ZB_ERR_INVALID;
  if (r != ZB_OK) {
    return failed(L, zb_result_str(r));
  }
  lua_pushboolean(L, true);
  return 1;
}

/* @lua net.blob_hash(data: string): string
 * The content hash used for blobs and the cache (SHA-1 as 40 hex digits).
 */
static int n_blob_hash(lua_State *L) {
  size_t len;
  const char *data = luaL_checklstring(L, 1, &len);
  char hash[ZB_NET_HASH_HEX + 1];
  zb_net_blob_hash(data, len, hash);
  lua_pushstring(L, hash);
  return 1;
}

/* @lua net.blob_cached(hash: string): boolean
 * True when the blob cache in the user directory holds this hash (so a join can skip the transfer).
 */
static int n_blob_cached(lua_State *L) {
  zb_script *s = zb_script_from(L);
  const char *hash = luaL_checkstring(L, 1);
  lua_pushboolean(L, s->cfg.fs && zb_net_cache_has(s->cfg.fs, s->cfg.user_dir, hash));
  return 1;
}

/* @lua net.blob_load(hash: string): string?
 * A cached blob, checked against its hash, or nil (a damaged file is removed).
 */
static int n_blob_load(lua_State *L) {
  zb_script *s = zb_script_from(L);
  const char *hash = luaL_checkstring(L, 1);
  char *data = NULL;
  size_t len = 0;
  if (!s->cfg.fs || zb_net_cache_load(s->cfg.fs, s->cfg.user_dir, hash, s->cfg.a, &data, &len) != ZB_OK) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushlstring(L, data, len);
  zb_free(s->cfg.a, data);
  return 1;
}

/* @lua net.blob_store(data: string): string?, string?
 * Stores a blob in the cache in the user directory; returns its hash, or nil and why it failed.
 */
static int n_blob_store(lua_State *L) {
  zb_script *s = zb_script_from(L);
  size_t len;
  const char *data = luaL_checklstring(L, 1, &len);
  if (!s->cfg.fs) {
    lua_pushnil(L);
    lua_pushstring(L, "no user directory");
    return 2;
  }
  char hash[ZB_NET_HASH_HEX + 1];
  zb_result r = zb_net_cache_store(s->cfg.fs, s->cfg.user_dir, data, len, hash);
  if (r != ZB_OK) {
    lua_pushnil(L);
    lua_pushstring(L, zb_result_str(r));
    return 2;
  }
  lua_pushstring(L, hash);
  return 1;
}

/* @lua net.lan_announce(on: boolean, name?: string, port?: integer): boolean, string?
 * Answers LAN discovery queries while hosting (desktop only; in the browser it fails with
 * "unavailable"), with the session's name and the port to join on (default: the port of the last
 * net.host). net.lan_announce(false) stops.
 */
static int n_lan_announce(lua_State *L) {
  bool on = lua_toboolean(L, 1);
  const char *name = luaL_optstring(L, 2, "Zauberer");
  lua_Integer port = luaL_optinteger(L, 3, S(L)->host_port);
  luaL_argcheck(L, port >= 0 && port <= 65535, 3, "port must be 0-65535");
  zb_net *n = N(L);
  if (!n) {
    return failed(L, "networking is not available");
  }
  if (!on) {
    zb_net_lan_stop(n);
    lua_pushboolean(L, true);
    return 1;
  }
  if (port == 0) {
    return failed(L, "no port to announce (host with net.host first)");
  }
  char err[96];
  if (zb_net_lan_announce(n, name, (uint16_t)port, 0, err, sizeof err) != ZB_OK) {
    return failed(L, err);
  }
  lua_pushboolean(L, true);
  return 1;
}

/* @lua net.lan_scan(): LanSession[]?, string?
 * Sessions announced on the local network (desktop only; in the browser nil and "unavailable").
 * Each call collects answers and asks again once a second, so the first call usually returns an
 * empty list: call it every frame or so while showing the list. Sessions not heard from for three
 * seconds drop out.
 */
static int n_lan_scan(lua_State *L) {
  zb_script *s = zb_script_from(L);
  zb_net *n = N(L);
  if (!n) {
    return unavailable(L);
  }
  net_state *st = S(L);
  char err[96];
  if (!st->scan && !(st->scan = zb_lan_open(s->cfg.a, 0, NULL, err, sizeof err))) {
    lua_pushnil(L);
    lua_pushstring(L, err);
    return 2;
  }
  uint64_t t = zb_net_now(n);
  if (!st->queried || t - st->last_query >= SCAN_INTERVAL_NS) {
    st->queried = true;
    st->last_query = t;
    zb_lan_query(st->scan, "255.255.255.255", ZB_LAN_PORT);
    zb_lan_query(st->scan, "127.0.0.1", ZB_LAN_PORT); /* this machine, also without a network */
  }
  size_t count = zb_lan_collect(st->scan, t, SCAN_MAX_AGE_NS);
  lua_createtable(L, (int)count, 0);
  for (size_t i = 0; i < count; ++i) {
    const zb_lan_session *f = zb_lan_found(st->scan, i);
    char url[96];
    snprintf(url, sizeof url, "ws://%s:%u/", f->address, (unsigned)f->port);
    lua_createtable(L, 0, 7);
    lua_pushstring(L, f->name);
    lua_setfield(L, -2, "name");
    lua_pushstring(L, f->address);
    lua_setfield(L, -2, "address");
    lua_pushinteger(L, f->port);
    lua_setfield(L, -2, "port");
    lua_pushstring(L, url);
    lua_setfield(L, -2, "url");
    lua_pushinteger(L, f->players);
    lua_setfield(L, -2, "players");
    lua_pushinteger(L, f->max_players);
    lua_setfield(L, -2, "max_players");
    lua_pushinteger(L, f->protocol);
    lua_setfield(L, -2, "protocol");
    lua_rawseti(L, -2, (lua_Integer)i + 1);
  }
  return 1;
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
  static const char *const kinds[] = {"joined", "left",          "data", "welcome",     "ended",    "away",
                                      "back",   "blob_progress", "blob", "blob_failed", "blob_sent"};
  zb_net_event ev;
  int k = 0;
  while (zb_net_next(n, &ev)) {
    lua_createtable(L, 0, 6);
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
    } else if (ev.kind == ZB_NET_EV_ENDED) {
      lua_pushstring(L, zb_net_error(n));
      lua_setfield(L, -2, "error");
      push_mismatches(L, n);
      lua_setfield(L, -2, "mismatches");
    } else if (ev.kind >= ZB_NET_EV_BLOB_PROGRESS) {
      lua_pushstring(L, ev.channel);
      lua_setfield(L, -2, "name");
      lua_pushstring(L, ev.hash);
      lua_setfield(L, -2, "hash");
      lua_pushinteger(L, (lua_Integer)ev.done);
      lua_setfield(L, -2, "received");
      lua_pushinteger(L, (lua_Integer)ev.total);
      lua_setfield(L, -2, "total");
      if (ev.kind == ZB_NET_EV_BLOB || ev.kind == ZB_NET_EV_BLOB_FAILED) {
        lua_pushlstring(L, (const char *)ev.data, ev.len);
        lua_setfield(L, -2, ev.kind == ZB_NET_EV_BLOB ? "data" : "reason");
      }
    }
    lua_rawseti(L, -2, ++k);
  }
  return 1;
}

/* The enabled packages, for the package-set check when joining. */
static void announce_packages(zb_script *s) {
  if (!s->cfg.net || !s->cfg.vfs) {
    return;
  }
  size_t count = zb_vfs_load_count(s->cfg.vfs);
  zb_net_package *list = count ? zb_alloc(s->cfg.a, count * sizeof *list) : NULL;
  if (count && !list) {
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    const zb_package *p = zb_vfs_load_order(s->cfg.vfs, i);
    list[i].id = p->id;
    list[i].version = p->version ? p->version : "";
  }
  zb_net_set_packages(s->cfg.net, list, count);
  zb_free(s->cfg.a, list);
}

void zb_script_open_net(zb_script *s) {
  static const luaL_Reg net_fns[] = {{"host", n_host},
                                     {"host_relay", n_host_relay},
                                     {"join", n_join},
                                     {"join_code", n_join_code},
                                     {"rejoin", n_rejoin},
                                     {"leave", n_leave},
                                     {"kick", n_kick},
                                     {"set_open", n_set_open},
                                     {"status", n_status},
                                     {"send", n_send},
                                     {"receive", n_receive},
                                     {"set_channel", n_set_channel},
                                     {"set_budget", n_set_budget},
                                     {"send_blob", n_send_blob},
                                     {"blob_hash", n_blob_hash},
                                     {"blob_cached", n_blob_cached},
                                     {"blob_load", n_blob_load},
                                     {"blob_store", n_blob_store},
                                     {"lan_announce", n_lan_announce},
                                     {"lan_scan", n_lan_scan},
                                     {NULL, NULL}};
  net_state *st = lua_newuserdatauv(s->L, sizeof *st, 0);
  memset(st, 0, sizeof *st);
  lua_createtable(s->L, 0, 1);
  lua_pushcfunction(s->L, state_gc);
  lua_setfield(s->L, -2, "__gc");
  lua_setmetatable(s->L, -2);
  lua_setfield(s->L, LUA_REGISTRYINDEX, STATE_KEY);
  lua_newtable(s->L);
  luaL_setfuncs(s->L, net_fns, 0);
  lua_setglobal(s->L, "net");
  announce_packages(s);
}

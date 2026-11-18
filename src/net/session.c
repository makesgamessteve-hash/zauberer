/* SPDX-License-Identifier: MIT */
#include "net/session.h"

#include <stdio.h>
#include <string.h>

#include "core/array.h"

enum { MSG_HELLO = 1, MSG_WELCOME, MSG_REJECT, MSG_DATA, MSG_JOINED, MSG_LEFT, MSG_PING, MSG_PONG };

#define HELLO_LEN (1 + 2 + 8 + 8)
#define DATA_HEAD 4 /* kind, from, to, channel length */

typedef struct peer {
  bool used;
  bool welcomed;
  uint32_t conn;
  int player;
  uint64_t last_seen;
} peer;

typedef struct stored_event {
  zb_net_event ev;
  uint8_t *data;
} stored_event;

struct zb_net {
  zb_net_config cfg;
  zb_net_role role;
  zb_transport *t;
  int player;
  unsigned players;
  peer peers[8];   /* host: connections (welcomed or not); client: [0] = the host */
  zb_array events; /* stored_event */
  size_t head;
  uint8_t *current;
  zb_array buf;
  uint64_t last_ping;
  char error[128];
  zb_net_stats stats;
};

static uint64_t now(const zb_net *n) { return n->cfg.clock_ns ? n->cfg.clock_ns(n->cfg.clock_ctx) : 0; }

static void put_u64(uint8_t *p, uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    p[i] = (uint8_t)(v >> (8 * i));
  }
}

static uint64_t get_u64(const uint8_t *p) {
  uint64_t v = 0;
  for (int i = 7; i >= 0; --i) {
    v = v << 8 | p[i];
  }
  return v;
}

zb_net *zb_net_create(const zb_net_config *cfg) {
  zb_net *n = zb_calloc(cfg->a, sizeof *n);
  if (!n) {
    return NULL;
  }
  n->cfg = *cfg;
  if (!n->cfg.ping_interval_ns) {
    n->cfg.ping_interval_ns = 1000000000ull;
  }
  if (!n->cfg.timeout_ns) {
    n->cfg.timeout_ns = 10000000000ull;
  }
  n->player = -1;
  zb_array_init(&n->events, cfg->a, sizeof(stored_event));
  zb_array_init(&n->buf, cfg->a, 1);
  return n;
}

static void clear_events(zb_net *n) {
  zb_free(n->cfg.a, n->current);
  n->current = NULL;
  for (size_t i = n->head; i < n->events.len; ++i) {
    zb_free(n->cfg.a, ((stored_event *)zb_array_at(&n->events, i))->data);
  }
  zb_array_clear(&n->events);
  n->head = 0;
}

void zb_net_destroy(zb_net *n) {
  if (!n) {
    return;
  }
  zb_net_leave(n);
  clear_events(n);
  zb_array_free(&n->events);
  zb_array_free(&n->buf);
  zb_free(n->cfg.a, n);
}

static void push_event(zb_net *n, zb_net_event_kind kind, int player, const char *channel, const void *data,
                       size_t len) {
  stored_event se;
  memset(&se, 0, sizeof se);
  se.ev.kind = kind;
  se.ev.player = player;
  if (channel) {
    snprintf(se.ev.channel, sizeof se.ev.channel, "%s", channel);
  }
  if (len) {
    se.data = zb_alloc(n->cfg.a, len);
    if (!se.data) {
      return;
    }
    memcpy(se.data, data, len);
  }
  se.ev.len = len;
  if (!zb_array_push(&n->events, &se)) {
    zb_free(n->cfg.a, se.data);
  }
}

bool zb_net_next(zb_net *n, zb_net_event *ev) {
  zb_free(n->cfg.a, n->current);
  n->current = NULL;
  if (n->head >= n->events.len) {
    zb_array_clear(&n->events);
    n->head = 0;
    return false;
  }
  stored_event *se = zb_array_at(&n->events, n->head++);
  *ev = se->ev;
  ev->data = se->data;
  n->current = se->data;
  return true;
}

static zb_result raw_send(zb_net *n, uint32_t conn, const void *data, size_t len) {
  zb_result r = n->t->send(n->t->ctx, conn, data, len);
  if (r == ZB_OK) {
    n->stats.bytes_sent += len;
    n->stats.messages_sent++;
  }
  return r;
}

static zb_result send_small(zb_net *n, uint32_t conn, uint8_t kind, const void *body, size_t len) {
  uint8_t msg[64];
  if (len + 1 > sizeof msg) {
    return ZB_ERR_INVALID;
  }
  msg[0] = kind;
  memcpy(msg + 1, body, len);
  return raw_send(n, conn, msg, len + 1);
}

/* ---- Host ---------------------------------------------------------------------- */

static peer *peer_by_conn(zb_net *n, uint32_t conn) {
  for (int i = 0; i < 8; ++i) {
    if (n->peers[i].used && n->peers[i].conn == conn) {
      return &n->peers[i];
    }
  }
  return NULL;
}

static void reject(zb_net *n, peer *p, const char *reason) {
  send_small(n, p->conn, MSG_REJECT, reason, strlen(reason));
  n->t->close(n->t->ctx, p->conn);
  memset(p, 0, sizeof *p);
}

static void broadcast_small(zb_net *n, uint8_t kind, uint8_t value, int except) {
  for (int i = 0; i < 8; ++i) {
    peer *p = &n->peers[i];
    if (p->used && p->welcomed && p->player != except) {
      send_small(n, p->conn, kind, &value, 1);
    }
  }
}

static void drop_peer(zb_net *n, peer *p) {
  if (p->welcomed) {
    int player = p->player;
    n->players &= ~(1u << player);
    memset(p, 0, sizeof *p);
    push_event(n, ZB_NET_EV_LEFT, player, NULL, NULL, 0);
    broadcast_small(n, MSG_LEFT, (uint8_t)player, -1);
  } else {
    memset(p, 0, sizeof *p);
  }
}

static void host_hello(zb_net *n, peer *p, const uint8_t *msg, size_t len) {
  if (p->welcomed || len < HELLO_LEN) {
    reject(n, p, "malformed hello");
    return;
  }
  unsigned version = (unsigned)msg[1] | (unsigned)msg[2] << 8;
  if (version != ZB_NET_PROTOCOL) {
    reject(n, p, "different game version (network protocol)");
    return;
  }
  if (get_u64(msg + 3) != n->cfg.world_seed) {
    reject(n, p, "the host plays a different world (seed)");
    return;
  }
  if (get_u64(msg + 11) != n->cfg.package_hash) {
    reject(n, p, "the host has a different package set");
    return;
  }
  int player = -1;
  for (int i = 1; i < ZB_NET_MAX_PLAYERS && player < 0; ++i) {
    if (!(n->players & (1u << i))) {
      player = i;
    }
  }
  if (player < 0) {
    reject(n, p, "the session is full");
    return;
  }
  uint8_t body[10];
  body[0] = (uint8_t)player;
  body[1] = (uint8_t)(n->players | (1u << player));
  put_u64(body + 2, n->cfg.world_seed);
  broadcast_small(n, MSG_JOINED, (uint8_t)player, -1);
  p->welcomed = true;
  p->player = player;
  n->players |= 1u << player;
  send_small(n, p->conn, MSG_WELCOME, body, sizeof body);
  push_event(n, ZB_NET_EV_JOINED, player, NULL, NULL, 0);
}

/* Routes a data message: delivers it here and/or forwards it. */
static void route(zb_net *n, int from, int to, const uint8_t *msg, size_t len) {
  if (len < DATA_HEAD || len < (size_t)DATA_HEAD + msg[3]) {
    return;
  }
  size_t chlen = msg[3];
  char channel[ZB_NET_MAX_CHANNEL + 1];
  if (chlen > ZB_NET_MAX_CHANNEL) {
    return;
  }
  memcpy(channel, msg + DATA_HEAD, chlen);
  channel[chlen] = '\0';
  const uint8_t *payload = msg + DATA_HEAD + chlen;
  size_t plen = len - DATA_HEAD - chlen;
  if (to == n->player || to == ZB_NET_TO_ALL) {
    push_event(n, ZB_NET_EV_DATA, from, channel, payload, plen);
  }
  if (n->role != ZB_NET_HOST) {
    return;
  }
  /* Forward with the verified sender. */
  if (zb_array_reserve(&n->buf, len) != ZB_OK) {
    return;
  }
  memcpy(n->buf.data, msg, len);
  n->buf.data[1] = (uint8_t)from;
  for (int i = 0; i < 8; ++i) {
    peer *p = &n->peers[i];
    if (p->used && p->welcomed && p->player != from && (to == ZB_NET_TO_ALL || to == p->player)) {
      raw_send(n, p->conn, n->buf.data, len);
    }
  }
}

static void handle_common(zb_net *n, peer *p, const uint8_t *msg, size_t len, uint64_t t) {
  if (msg[0] == MSG_PING && len == 9) {
    send_small(n, p->conn, MSG_PONG, msg + 1, 8);
  } else if (msg[0] == MSG_PONG && len == 9) {
    uint64_t sent = get_u64(msg + 1);
    int slot = p->player >= 0 && p->player < ZB_NET_MAX_PLAYERS ? p->player : 0;
    n->stats.rtt_ms[slot] = (uint32_t)((t - sent) / 1000000u);
  }
}

static void host_message(zb_net *n, const zb_tr_event *ev, uint64_t t) {
  peer *p = peer_by_conn(n, ev->conn);
  if (!p || ev->len == 0) {
    return;
  }
  p->last_seen = t;
  const uint8_t *msg = ev->data;
  if (msg[0] == MSG_HELLO) {
    host_hello(n, p, msg, ev->len);
  } else if (!p->welcomed) {
    reject(n, p, "hello expected");
  } else if (msg[0] == MSG_DATA && ev->len >= DATA_HEAD) {
    route(n, p->player, msg[2], msg, ev->len);
  } else {
    handle_common(n, p, msg, ev->len, t);
  }
}

static void host_update(zb_net *n, uint64_t t) {
  zb_tr_event ev;
  while (n->t->next(n->t->ctx, &ev)) {
    if (ev.kind == ZB_TR_OPEN) {
      for (int i = 0; i < 8; ++i) {
        if (!n->peers[i].used) {
          n->peers[i].used = true;
          n->peers[i].conn = ev.conn;
          n->peers[i].player = -1;
          n->peers[i].last_seen = t;
          break;
        }
        if (i == 7) {
          n->t->close(n->t->ctx, ev.conn); /* too many pending connections */
        }
      }
    } else if (ev.kind == ZB_TR_MESSAGE) {
      n->stats.bytes_received += ev.len;
      n->stats.messages_received++;
      host_message(n, &ev, t);
    } else {
      peer *p = peer_by_conn(n, ev.conn);
      if (p) {
        drop_peer(n, p);
      }
    }
  }
  bool ping = t - n->last_ping >= n->cfg.ping_interval_ns;
  if (ping) {
    n->last_ping = t;
  }
  for (int i = 0; i < 8; ++i) {
    peer *p = &n->peers[i];
    if (!p->used) {
      continue;
    }
    if (t - p->last_seen > n->cfg.timeout_ns) {
      n->t->close(n->t->ctx, p->conn);
      drop_peer(n, p);
    } else if (ping && p->welcomed) {
      uint8_t stamp[8];
      put_u64(stamp, t);
      send_small(n, p->conn, MSG_PING, stamp, sizeof stamp);
    }
  }
}

/* ---- Client ------------------------------------------------------------------------- */

static void end_session(zb_net *n, const char *why) {
  if (why && !n->error[0]) {
    snprintf(n->error, sizeof n->error, "%s", why);
  }
  if (n->role != ZB_NET_NONE) {
    push_event(n, ZB_NET_EV_ENDED, n->player, NULL, NULL, 0);
  }
  n->role = ZB_NET_NONE;
  n->players = 0;
}

static void client_message(zb_net *n, const zb_tr_event *ev, uint64_t t) {
  const uint8_t *msg = ev->data;
  size_t len = ev->len;
  n->peers[0].last_seen = t;
  switch (msg[0]) {
    case MSG_WELCOME:
      if (len >= 11 && n->role == ZB_NET_JOINING) {
        n->role = ZB_NET_CLIENT;
        n->player = msg[1];
        n->players = msg[2];
        push_event(n, ZB_NET_EV_WELCOME, n->player, NULL, NULL, 0);
        for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
          if (i != n->player && (n->players & (1u << i))) {
            push_event(n, ZB_NET_EV_JOINED, i, NULL, NULL, 0);
          }
        }
      }
      break;
    case MSG_REJECT: {
      char reason[96];
      size_t rl = len - 1 < sizeof reason - 1 ? len - 1 : sizeof reason - 1;
      memcpy(reason, msg + 1, rl);
      reason[rl] = '\0';
      end_session(n, reason);
      break;
    }
    case MSG_JOINED:
    case MSG_LEFT:
      if (len == 2 && msg[1] < ZB_NET_MAX_PLAYERS && msg[1] != n->player) {
        bool joined = msg[0] == MSG_JOINED;
        n->players = joined ? (n->players | (1u << msg[1])) : (n->players & ~(1u << msg[1]));
        push_event(n, joined ? ZB_NET_EV_JOINED : ZB_NET_EV_LEFT, msg[1], NULL, NULL, 0);
      }
      break;
    case MSG_DATA:
      if (len >= DATA_HEAD) {
        route(n, msg[1], msg[2], msg, len);
      }
      break;
    default: handle_common(n, &n->peers[0], msg, len, t); break;
  }
}

static void client_update(zb_net *n, uint64_t t) {
  zb_tr_event ev;
  while (n->role != ZB_NET_NONE && n->t->next(n->t->ctx, &ev)) {
    if (ev.kind == ZB_TR_OPEN) {
      uint8_t hello[HELLO_LEN];
      hello[0] = MSG_HELLO;
      hello[1] = (uint8_t)(ZB_NET_PROTOCOL & 0xFF);
      hello[2] = (uint8_t)(ZB_NET_PROTOCOL >> 8);
      put_u64(hello + 3, n->cfg.world_seed);
      put_u64(hello + 11, n->cfg.package_hash);
      raw_send(n, 1, hello, sizeof hello);
      n->peers[0].last_seen = t;
    } else if (ev.kind == ZB_TR_MESSAGE && ev.len > 0) {
      n->stats.bytes_received += ev.len;
      n->stats.messages_received++;
      client_message(n, &ev, t);
    } else if (ev.kind == ZB_TR_CLOSE) {
      end_session(n, n->role == ZB_NET_JOINING ? "could not reach the host" : "the host ended the session");
    }
  }
  if (n->role == ZB_NET_NONE) {
    return;
  }
  if (t - n->peers[0].last_seen > n->cfg.timeout_ns) {
    end_session(n, "the host stopped answering");
  } else if (n->role == ZB_NET_CLIENT && t - n->last_ping >= n->cfg.ping_interval_ns) {
    n->last_ping = t;
    uint8_t stamp[8];
    put_u64(stamp, t);
    send_small(n, 1, MSG_PING, stamp, sizeof stamp);
  }
}

/* ---- Public -------------------------------------------------------------------------- */

static void reset(zb_net *n) {
  memset(n->peers, 0, sizeof n->peers);
  n->players = 0;
  n->player = -1;
  n->error[0] = '\0';
  n->last_ping = now(n);
  memset(&n->stats, 0, sizeof n->stats);
}

zb_result zb_net_host(zb_net *n, zb_transport *listener) {
  if (!listener) {
    return ZB_ERR_INVALID;
  }
  zb_net_leave(n);
  reset(n);
  n->t = listener;
  n->role = ZB_NET_HOST;
  n->player = 0;
  n->players = 1;
  return ZB_OK;
}

zb_result zb_net_join(zb_net *n, zb_transport *client) {
  if (!client) {
    return ZB_ERR_INVALID;
  }
  zb_net_leave(n);
  reset(n);
  n->t = client;
  n->role = ZB_NET_JOINING;
  n->peers[0].used = true;
  n->peers[0].conn = 1;
  n->peers[0].player = 0;
  n->peers[0].last_seen = now(n);
  return ZB_OK;
}

void zb_net_leave(zb_net *n) {
  if (!n->t) {
    return;
  }
  if (n->role == ZB_NET_HOST) {
    for (int i = 0; i < 8; ++i) {
      if (n->peers[i].used) {
        n->t->close(n->t->ctx, n->peers[i].conn);
      }
    }
    n->t->update(n->t->ctx); /* flush the close frames */
  } else {
    n->t->close(n->t->ctx, 1);
    n->t->update(n->t->ctx);
  }
  zb_transport_destroy(n->t);
  n->t = NULL;
  n->role = ZB_NET_NONE;
  n->players = 0;
  n->player = -1;
}

void zb_net_update(zb_net *n) {
  if (!n->t) {
    return;
  }
  n->t->update(n->t->ctx);
  uint64_t t = now(n);
  if (n->role == ZB_NET_HOST) {
    host_update(n, t);
  } else {
    client_update(n, t);
  }
  if (n->t) {
    n->t->update(n->t->ctx); /* send what this update queued */
  }
  if (n->role == ZB_NET_NONE && n->t) {
    zb_transport_destroy(n->t);
    n->t = NULL;
  }
}

zb_result zb_net_send(zb_net *n, int to, const char *channel, const void *data, size_t len) {
  size_t chlen = strlen(channel);
  if (chlen == 0 || chlen > ZB_NET_MAX_CHANNEL || (n->role != ZB_NET_HOST && n->role != ZB_NET_CLIENT) ||
      (to != ZB_NET_TO_ALL && (to < 0 || to >= ZB_NET_MAX_PLAYERS || to == n->player))) {
    return n->role == ZB_NET_HOST || n->role == ZB_NET_CLIENT ? ZB_ERR_INVALID : ZB_ERR_STATE;
  }
  size_t total = DATA_HEAD + chlen + len;
  if (zb_array_reserve(&n->buf, total) != ZB_OK) {
    return ZB_ERR_NOMEM;
  }
  uint8_t *m = n->buf.data;
  m[0] = MSG_DATA;
  m[1] = (uint8_t)n->player;
  m[2] = (uint8_t)to;
  m[3] = (uint8_t)chlen;
  memcpy(m + DATA_HEAD, channel, chlen);
  if (len) {
    memcpy(m + DATA_HEAD + chlen, data, len);
  }
  if (n->role == ZB_NET_CLIENT) {
    return raw_send(n, 1, m, total);
  }
  zb_result r = ZB_OK;
  bool sent = false;
  for (int i = 0; i < 8; ++i) {
    peer *p = &n->peers[i];
    if (p->used && p->welcomed && (to == ZB_NET_TO_ALL || to == p->player)) {
      zb_result s = raw_send(n, p->conn, m, total);
      r = s != ZB_OK ? s : r;
      sent = true;
    }
  }
  return to == ZB_NET_TO_ALL || sent ? r : ZB_ERR_NOT_FOUND;
}

zb_net_role zb_net_role_of(const zb_net *n) { return n->role; }
int zb_net_player(const zb_net *n) { return n->player; }
unsigned zb_net_players(const zb_net *n) { return n->players; }
const char *zb_net_error(const zb_net *n) { return n->error; }
void zb_net_get_stats(const zb_net *n, zb_net_stats *out) { *out = n->stats; }

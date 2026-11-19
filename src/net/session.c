/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>

#include "core/hash.h"
#include "net/session_internal.h"

/* kind, version u16, seed u64, package hash u64, resume token u64, package count u16; then the packages */
#define HELLO_LEN 31
#define WELCOME_LEN 19 /* kind, player, players, seed u64, resume token u64 */
#define DATA_HEAD 4    /* kind, from, to, channel length */
#define MAX_REFILL_NS 10000000000ull

static uint64_t now(const zb_net *n) { return n->cfg.clock_ns ? n->cfg.clock_ns(n->cfg.clock_ctx) : 0; }
uint64_t zb_net_now(const zb_net *n) { return now(n); }

void zb_net_put_u32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; ++i) {
    p[i] = (uint8_t)(v >> (8 * i));
  }
}

void zb_net_put_u64(uint8_t *p, uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    p[i] = (uint8_t)(v >> (8 * i));
  }
}

uint32_t zb_net_get_u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

uint64_t zb_net_get_u64(const uint8_t *p) {
  return (uint64_t)zb_net_get_u32(p) | (uint64_t)zb_net_get_u32(p + 4) << 32;
}

static int64_t burst_bytes(const zb_net *n) { return (int64_t)n->cfg.burst_kbps * 125; }

zb_net *zb_net_create(const zb_net_config *cfg) {
  zb_net *n = zb_calloc(cfg->a, sizeof *n);
  if (!n) {
    return NULL;
  }
  n->cfg = *cfg;
  n->cfg.ping_interval_ns = cfg->ping_interval_ns ? cfg->ping_interval_ns : 1000000000ull;
  n->cfg.timeout_ns = cfg->timeout_ns ? cfg->timeout_ns : 10000000000ull;
  n->cfg.resume_grace_ns = cfg->resume_grace_ns ? cfg->resume_grace_ns : 30000000000ull;
  n->cfg.budget_kbps = cfg->budget_kbps ? cfg->budget_kbps : 64;
  n->cfg.burst_kbps = cfg->burst_kbps ? cfg->burst_kbps : 256;
  n->cfg.blob_window = cfg->blob_window ? cfg->blob_window : 256u * 1024u;
  n->cfg.max_blob = cfg->max_blob ? cfg->max_blob : 128ull * 1024u * 1024u;
  n->player = -1;
  zb_array_init(&n->events, cfg->a, sizeof(stored_event));
  zb_array_init(&n->buf, cfg->a, 1);
  zb_array_init(&n->packages, cfg->a, 1);
  zb_array_init(&n->pending, cfg->a, sizeof(pending_msg));
  zb_array_init(&n->out_blobs, cfg->a, sizeof(out_blob *));
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

static void clear_pending(zb_net *n, uint32_t conn, bool all) {
  for (size_t i = 0; i < n->pending.len;) {
    pending_msg *m = zb_array_at(&n->pending, i);
    if (all || m->conn == conn) {
      zb_free(n->cfg.a, m->data);
      zb_array_remove(&n->pending, i);
    } else {
      ++i;
    }
  }
}

/* Frees what belongs to a session that is over. */
static void release(zb_net *n) {
  clear_pending(n, 0, true);
  zb_net_blob_release(n);
}

void zb_net_destroy(zb_net *n) {
  if (!n) {
    return;
  }
  zb_net_leave(n);
  zb_net_lan_stop(n);
  clear_events(n);
  zb_array_free(&n->events);
  zb_array_free(&n->buf);
  zb_array_free(&n->packages);
  zb_array_free(&n->pending);
  zb_array_free(&n->out_blobs);
  zb_free(n->cfg.a, n);
}

stored_event *zb_net_push(zb_net *n, zb_net_event_kind kind, int player, const char *name, const void *data, size_t len,
                          bool own) {
  stored_event se;
  memset(&se, 0, sizeof se);
  se.ev.kind = kind;
  se.ev.player = player;
  if (name) {
    snprintf(se.ev.channel, sizeof se.ev.channel, "%s", name);
  }
  if (own) {
    se.data = (uint8_t *)data;
  } else if (len) {
    se.data = zb_alloc(n->cfg.a, len);
    if (!se.data) {
      return NULL;
    }
    memcpy(se.data, data, len);
  }
  se.ev.len = len;
  stored_event *out = zb_array_push(&n->events, &se);
  if (!out) {
    zb_free(n->cfg.a, se.data);
  }
  return out;
}

static void push_event(zb_net *n, zb_net_event_kind kind, int player, const char *channel, const void *data,
                       size_t len) {
  zb_net_push(n, kind, player, channel, data, len, false);
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

zb_result zb_net_send_raw(zb_net *n, peer *p, const void *data, size_t len, bool charge) {
  zb_result r = n->t->send(n->t->ctx, p->conn, data, len);
  if (r == ZB_OK) {
    n->stats.bytes_sent += len;
    n->stats.messages_sent++;
    if (p->player >= 0 && p->player < ZB_NET_MAX_PLAYERS) {
      n->stats.sent_to[p->player] += len;
      n->window_out[p->player] += len;
    }
    if (charge) {
      p->tokens -= (int64_t)len;
    }
  }
  return r;
}

static zb_result send_small(zb_net *n, peer *p, uint8_t kind, const void *body, size_t len) {
  uint8_t msg[64];
  ZB_ASSERT(len < sizeof msg); /* control messages only */
  msg[0] = kind;
  memcpy(msg + 1, body, len);
  return zb_net_send_raw(n, p, msg, len + 1, true);
}

static peer *peer_by_conn(zb_net *n, uint32_t conn) {
  for (int i = 0; i < ZB_NET_MAX_PEERS; ++i) {
    if (n->peers[i].used && n->peers[i].conn == conn) {
      return &n->peers[i];
    }
  }
  return NULL;
}

peer *zb_net_peer_of(zb_net *n, int player) {
  if (n->role == ZB_NET_CLIENT) {
    return player == 0 ? &n->peers[0] : NULL;
  }
  for (int i = 0; i < ZB_NET_MAX_PEERS; ++i) {
    peer *p = &n->peers[i];
    if (p->used && p->welcomed && p->player == player) {
      return p;
    }
  }
  return NULL;
}

/* ---- Budget and channel classes --------------------------------------------------- */

static zb_net_channel_class class_of(const zb_net *n, const char *channel) {
  for (size_t i = 0; i < n->class_count; ++i) {
    if (strcmp(n->classes[i].name, channel) == 0) {
      return n->classes[i].cls;
    }
  }
  return ZB_NET_RELIABLE;
}

zb_result zb_net_set_channel_class(zb_net *n, const char *channel, zb_net_channel_class cls) {
  size_t len = strlen(channel);
  if (len == 0 || len > ZB_NET_MAX_CHANNEL || (cls != ZB_NET_RELIABLE && cls != ZB_NET_LATEST)) {
    return ZB_ERR_INVALID;
  }
  for (size_t i = 0; i < n->class_count; ++i) {
    if (strcmp(n->classes[i].name, channel) == 0) {
      n->classes[i].cls = cls;
      return ZB_OK;
    }
  }
  if (n->class_count == ZB_NET_MAX_CLASSES) {
    return ZB_ERR_NOMEM;
  }
  memcpy(n->classes[n->class_count].name, channel, len + 1);
  n->classes[n->class_count++].cls = cls;
  return ZB_OK;
}

void zb_net_set_budget(zb_net *n, uint32_t kbps, uint32_t burst_kbps) {
  n->cfg.budget_kbps = kbps ? kbps : n->cfg.budget_kbps;
  n->cfg.burst_kbps = burst_kbps ? burst_kbps : n->cfg.burst_kbps;
}

/* Sends a latest-only message now if the connection has budget and nothing
 * waiting, else keeps it (replacing an older one of the same channel and sender). */
static zb_result send_latest(zb_net *n, peer *p, const char *channel, int from, const uint8_t *m, size_t len) {
  bool waiting = false;
  pending_msg *same = NULL;
  for (size_t i = 0; i < n->pending.len; ++i) {
    pending_msg *e = zb_array_at(&n->pending, i);
    if (e->conn == p->conn) {
      waiting = true;
      if (e->from == from && strcmp(e->channel, channel) == 0) {
        same = e;
      }
    }
  }
  if (!waiting && p->tokens > 0) {
    return zb_net_send_raw(n, p, m, len, true);
  }
  uint8_t *copy = zb_alloc(n->cfg.a, len);
  if (!copy) {
    return ZB_ERR_NOMEM;
  }
  memcpy(copy, m, len);
  if (same) {
    zb_free(n->cfg.a, same->data);
    same->data = copy;
    same->len = len;
    n->stats.coalesced++;
    return ZB_OK;
  }
  pending_msg e;
  memset(&e, 0, sizeof e);
  e.conn = p->conn;
  e.from = from;
  snprintf(e.channel, sizeof e.channel, "%s", channel);
  e.data = copy;
  e.len = len;
  if (!zb_array_push(&n->pending, &e)) {
    zb_free(n->cfg.a, copy);
    return ZB_ERR_NOMEM;
  }
  return ZB_OK;
}

static zb_result deliver(zb_net *n, peer *p, const char *channel, int from, const uint8_t *m, size_t len) {
  if (class_of(n, channel) == ZB_NET_LATEST) {
    return send_latest(n, p, channel, from, m, len);
  }
  return zb_net_send_raw(n, p, m, len, true);
}

/* Refills every connection's budget and sends waiting latest-only messages. */
static void budget_update(zb_net *n, uint64_t t) {
  uint64_t dt = t - n->last_refill;
  dt = dt > MAX_REFILL_NS ? MAX_REFILL_NS : dt;
  n->last_refill = t;
  int64_t add = (int64_t)((uint64_t)n->cfg.budget_kbps * 125u * dt / 1000000000u);
  int64_t cap = burst_bytes(n);
  for (int i = 0; i < ZB_NET_MAX_PEERS; ++i) {
    peer *p = &n->peers[i];
    if (p->used) {
      p->tokens = p->tokens + add > cap ? cap : p->tokens + add;
    }
  }
  for (size_t i = 0; i < n->pending.len;) {
    pending_msg *m = zb_array_at(&n->pending, i);
    peer *p = peer_by_conn(n, m->conn);
    if (p && p->tokens <= 0) {
      ++i;
      continue;
    }
    if (p) {
      zb_net_send_raw(n, p, m->data, m->len, true);
    }
    zb_free(n->cfg.a, m->data);
    zb_array_remove(&n->pending, i);
  }
}

/* ---- Package sets ----------------------------------------------------------------- */

typedef struct pkg_reader {
  const uint8_t *p;
  size_t len, pos;
} pkg_reader;

typedef struct pkg_entry {
  const char *id, *version;
  size_t id_len, version_len;
} pkg_entry;

static bool pkg_next(pkg_reader *r, pkg_entry *e) {
  if (r->pos >= r->len || r->len - r->pos < 2u + r->p[r->pos]) {
    return false;
  }
  e->id_len = r->p[r->pos];
  e->id = (const char *)r->p + r->pos + 1;
  r->pos += 1 + e->id_len;
  e->version_len = r->p[r->pos];
  if (r->len - r->pos < 1u + e->version_len) {
    return false;
  }
  e->version = (const char *)r->p + r->pos + 1;
  r->pos += 1 + e->version_len;
  return true;
}

/* True when the list holds exactly count well-formed entries. */
static bool pkg_valid(const uint8_t *list, size_t len, unsigned count) {
  pkg_reader r = {list, len, 0};
  pkg_entry e;
  for (unsigned i = 0; i < count; ++i) {
    if (!pkg_next(&r, &e)) {
      return false;
    }
  }
  return r.pos == len;
}

static bool pkg_find(const uint8_t *list, size_t len, const pkg_entry *want, pkg_entry *out) {
  pkg_reader r = {list, len, 0};
  while (pkg_next(&r, out)) {
    if (out->id_len == want->id_len && memcmp(out->id, want->id, want->id_len) == 0) {
      return true;
    }
  }
  return false;
}

static void copy_field(char *dst, size_t cap, const char *src, size_t len) {
  len = len < cap - 1 ? len : cap - 1;
  memcpy(dst, src, len);
  dst[len] = '\0';
}

static size_t add_mismatch(zb_net_mismatch *out, size_t count, const pkg_entry *id, const pkg_entry *host,
                           const pkg_entry *client) {
  if (count == ZB_NET_MAX_MISMATCHES) {
    return count;
  }
  zb_net_mismatch *m = &out[count];
  copy_field(m->id, sizeof m->id, id->id, id->id_len);
  copy_field(m->host_version, sizeof m->host_version, host ? host->version : "", host ? host->version_len : 0);
  copy_field(m->client_version, sizeof m->client_version, client ? client->version : "",
             client ? client->version_len : 0);
  return count + 1;
}

/* Packages that differ: the host's in its order, then those only the client has. */
static size_t compare_packages(const uint8_t *host, size_t host_len, const uint8_t *client, size_t client_len,
                               zb_net_mismatch *out) {
  size_t count = 0;
  pkg_reader r = {host, host_len, 0};
  pkg_entry h, c;
  while (pkg_next(&r, &h)) {
    bool found = pkg_find(client, client_len, &h, &c);
    if (!found || c.version_len != h.version_len || memcmp(c.version, h.version, h.version_len) != 0) {
      count = add_mismatch(out, count, &h, &h, found ? &c : NULL);
    }
  }
  r = (pkg_reader){client, client_len, 0};
  while (pkg_next(&r, &c)) {
    if (!pkg_find(host, host_len, &c, &h)) {
      count = add_mismatch(out, count, &c, NULL, &c);
    }
  }
  return count;
}

zb_result zb_net_set_packages(zb_net *n, const zb_net_package *packages, size_t count) {
  if (count > 0xFFFF) {
    return ZB_ERR_INVALID;
  }
  n->packages.len = 0;
  n->package_count = 0;
  for (size_t i = 0; i < count; ++i) {
    size_t il = strlen(packages[i].id), vl = strlen(packages[i].version);
    il = il > 255 ? 255 : il;
    vl = vl > 255 ? 255 : vl;
    size_t at = n->packages.len;
    if (zb_array_reserve(&n->packages, at + 2 + il + vl) != ZB_OK) {
      n->packages.len = 0;
      return ZB_ERR_NOMEM;
    }
    uint8_t *p = n->packages.data + at;
    p[0] = (uint8_t)il;
    memcpy(p + 1, packages[i].id, il);
    p[1 + il] = (uint8_t)vl;
    memcpy(p + 2 + il, packages[i].version, vl);
    n->packages.len = at + 2 + il + vl;
  }
  n->package_count = (uint16_t)count;
  return ZB_OK;
}

size_t zb_net_mismatches(const zb_net *n, const zb_net_mismatch **out) {
  *out = n->mismatches;
  return n->mismatch_count;
}

/* ---- Host ---------------------------------------------------------------------- */

static void broadcast_small(zb_net *n, uint8_t kind, uint8_t value, int except) {
  for (int i = 0; i < ZB_NET_MAX_PEERS; ++i) {
    peer *p = &n->peers[i];
    if (p->used && p->welcomed && p->player != except) {
      send_small(n, p, kind, &value, 1);
    }
  }
}

/* A player is gone for good. */
static void forget_player(zb_net *n, int player) {
  n->players &= ~(1u << player);
  n->away &= ~(1u << player);
  memset(&n->slots[player], 0, sizeof n->slots[player]);
  push_event(n, ZB_NET_EV_LEFT, player, NULL, NULL, 0);
  broadcast_small(n, MSG_LEFT, (uint8_t)player, -1);
}

/* A connection ended: a player who left on purpose is gone, one whose
 * connection was lost is away for the grace period. */
static void drop_peer(zb_net *n, peer *p, uint64_t t, bool left) {
  peer old = *p;
  clear_pending(n, old.conn, false);
  memset(p, 0, sizeof *p);
  if (!old.welcomed) {
    return;
  }
  zb_net_blob_drop_player(n, old.player, left ? "the player left" : "the connection was lost");
  if (left) {
    forget_player(n, old.player);
    return;
  }
  n->away |= 1u << old.player;
  n->slots[old.player].away_until = t + n->cfg.resume_grace_ns;
  push_event(n, ZB_NET_EV_AWAY, old.player, NULL, NULL, 0);
  broadcast_small(n, MSG_AWAY, (uint8_t)old.player, -1);
}

static void reject(zb_net *n, peer *p, const char *reason, const zb_net_mismatch *mm, size_t count) {
  uint8_t msg[160 + ZB_NET_MAX_MISMATCHES * 128];
  size_t rl = strlen(reason), len = 1 + rl;
  msg[0] = MSG_REJECT;
  memcpy(msg + 1, reason, rl);
  if (count) { /* the reason, a zero byte, then the packages that differ */
    msg[len++] = 0;
    msg[len++] = (uint8_t)count;
    for (size_t i = 0; i < count; ++i) {
      const char *fields[3] = {mm[i].id, mm[i].host_version, mm[i].client_version};
      for (int f = 0; f < 3; ++f) {
        size_t fl = strlen(fields[f]);
        msg[len++] = (uint8_t)fl;
        memcpy(msg + len, fields[f], fl);
        len += fl;
      }
    }
  }
  zb_net_send_raw(n, p, msg, len, true);
  n->t->close(n->t->ctx, p->conn);
  drop_peer(n, p, now(n), true);
}

static uint64_t make_token(zb_net *n) {
  uint64_t k = zb_mix64(n->cfg.world_seed ^ zb_mix64(++n->tokens_made ^ now(n) ^ (uint64_t)(uintptr_t)n));
  return k | 1u; /* never 0, which means "no token" */
}

static void host_hello(zb_net *n, peer *p, const uint8_t *msg, size_t len) {
  if (p->welcomed || len < 3) {
    reject(n, p, "malformed hello", NULL, 0);
    return;
  }
  unsigned version = (unsigned)msg[1] | (unsigned)msg[2] << 8;
  if (version != ZB_NET_PROTOCOL) {
    reject(n, p, "different game version (network protocol)", NULL, 0);
    return;
  }
  if (len < HELLO_LEN || !pkg_valid(msg + HELLO_LEN, len - HELLO_LEN, (unsigned)msg[29] | (unsigned)msg[30] << 8)) {
    reject(n, p, "malformed hello", NULL, 0);
    return;
  }
  if (zb_net_get_u64(msg + 3) != n->cfg.world_seed) {
    reject(n, p, "the host plays a different world (seed)", NULL, 0);
    return;
  }
  if (zb_net_get_u64(msg + 11) != n->cfg.package_hash) {
    zb_net_mismatch mm[ZB_NET_MAX_MISMATCHES];
    size_t count = compare_packages(n->packages.data, n->packages.len, msg + HELLO_LEN, len - HELLO_LEN, mm);
    reject(n, p, "the host has a different package set", mm, count);
    return;
  }
  uint64_t token = zb_net_get_u64(msg + 19);
  int player = -1;
  for (int i = 1; i < ZB_NET_MAX_PLAYERS && token; ++i) {
    if ((n->players & (1u << i)) && n->slots[i].token == token) {
      player = i;
    }
  }
  bool back = player > 0;
  if (back) {
    peer *old = zb_net_peer_of(n, player);
    if (old) { /* the old connection is still thought alive: the new one replaces it */
      n->t->close(n->t->ctx, old->conn);
      clear_pending(n, old->conn, false);
      memset(old, 0, sizeof *old);
      zb_net_blob_drop_player(n, player, "the connection was replaced");
    }
  } else {
    for (int i = 1; i < ZB_NET_MAX_PLAYERS && player < 0; ++i) {
      if (!(n->players & (1u << i))) {
        player = i;
      }
    }
    if (player < 0) {
      reject(n, p, "the session is full", NULL, 0);
      return;
    }
    n->slots[player].token = make_token(n);
  }
  n->slots[player].away_until = 0;
  uint8_t body[WELCOME_LEN - 1];
  body[0] = (uint8_t)player;
  body[1] = (uint8_t)(n->players | (1u << player));
  zb_net_put_u64(body + 2, n->cfg.world_seed);
  zb_net_put_u64(body + 10, n->slots[player].token);
  broadcast_small(n, back ? MSG_BACK : MSG_JOINED, (uint8_t)player, -1);
  p->welcomed = true;
  p->player = player;
  n->players |= 1u << player;
  n->away &= ~(1u << player);
  send_small(n, p, MSG_WELCOME, body, sizeof body);
  for (int i = 1; i < ZB_NET_MAX_PLAYERS; ++i) {
    if (n->away & (1u << i)) {
      uint8_t who = (uint8_t)i;
      send_small(n, p, MSG_AWAY, &who, 1);
    }
  }
  push_event(n, back ? ZB_NET_EV_BACK : ZB_NET_EV_JOINED, player, NULL, NULL, 0);
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
  for (int i = 0; i < ZB_NET_MAX_PEERS; ++i) {
    peer *p = &n->peers[i];
    if (p->used && p->welcomed && p->player != from && (to == ZB_NET_TO_ALL || to == p->player)) {
      deliver(n, p, channel, from, n->buf.data, len);
    }
  }
}

static void handle_common(zb_net *n, peer *p, const uint8_t *msg, size_t len, uint64_t t) {
  if (msg[0] == MSG_PING && len == 9) {
    send_small(n, p, MSG_PONG, msg + 1, 8);
  } else if (msg[0] == MSG_PONG && len == 9) {
    uint64_t sent = zb_net_get_u64(msg + 1);
    int at = p->player >= 0 && p->player < ZB_NET_MAX_PLAYERS ? p->player : 0;
    n->stats.rtt_ms[at] = (uint32_t)((t - sent) / 1000000u);
  } else if (msg[0] >= MSG_BLOB_START && msg[0] <= MSG_BLOB_ABORT) {
    zb_net_blob_message(n, p, msg, len);
  }
}

static void count_received(zb_net *n, const peer *p, size_t len) {
  n->stats.bytes_received += len;
  n->stats.messages_received++;
  if (p && p->player >= 0 && p->player < ZB_NET_MAX_PLAYERS) {
    n->stats.received_from[p->player] += len;
    n->window_in[p->player] += len;
  }
}

static void host_message(zb_net *n, const zb_tr_event *ev, uint64_t t) {
  peer *p = peer_by_conn(n, ev->conn);
  count_received(n, p, ev->len);
  if (!p || ev->len == 0) {
    return;
  }
  p->last_seen = t;
  const uint8_t *msg = ev->data;
  if (msg[0] == MSG_HELLO) {
    host_hello(n, p, msg, ev->len);
  } else if (!p->welcomed) {
    reject(n, p, "hello expected", NULL, 0);
  } else if (msg[0] == MSG_DATA && ev->len >= DATA_HEAD) {
    route(n, p->player, msg[2], msg, ev->len);
  } else if (msg[0] == MSG_BYE) {
    n->t->close(n->t->ctx, p->conn);
    drop_peer(n, p, t, true);
  } else {
    handle_common(n, p, msg, ev->len, t);
  }
}

static void host_update(zb_net *n, uint64_t t) {
  zb_tr_event ev;
  while (n->t->next(n->t->ctx, &ev)) {
    if (ev.kind == ZB_TR_OPEN) {
      for (int i = 0; i < ZB_NET_MAX_PEERS; ++i) {
        if (!n->peers[i].used) {
          n->peers[i].used = true;
          n->peers[i].conn = ev.conn;
          n->peers[i].player = -1;
          n->peers[i].last_seen = t;
          n->peers[i].tokens = burst_bytes(n);
          break;
        }
        if (i == ZB_NET_MAX_PEERS - 1) {
          n->t->close(n->t->ctx, ev.conn); /* too many pending connections */
        }
      }
    } else if (ev.kind == ZB_TR_MESSAGE) {
      host_message(n, &ev, t);
    } else {
      peer *p = peer_by_conn(n, ev.conn);
      if (p) {
        drop_peer(n, p, t, false);
      }
    }
  }
  for (int i = 1; i < ZB_NET_MAX_PLAYERS; ++i) {
    if ((n->away & (1u << i)) && t >= n->slots[i].away_until) {
      forget_player(n, i);
    }
  }
  bool ping = t - n->last_ping >= n->cfg.ping_interval_ns;
  if (ping) {
    n->last_ping = t;
  }
  for (int i = 0; i < ZB_NET_MAX_PEERS; ++i) {
    peer *p = &n->peers[i];
    if (!p->used) {
      continue;
    }
    if (t - p->last_seen > n->cfg.timeout_ns) {
      n->t->close(n->t->ctx, p->conn);
      drop_peer(n, p, t, false);
    } else if (ping && p->welcomed) {
      uint8_t stamp[8];
      zb_net_put_u64(stamp, t);
      send_small(n, p, MSG_PING, stamp, sizeof stamp);
    }
  }
}

/* ---- Client ------------------------------------------------------------------------- */

static void end_session(zb_net *n, const char *why, bool resumable) {
  if (why && !n->error[0]) {
    snprintf(n->error, sizeof n->error, "%s", why);
  }
  n->resumable = resumable && n->token != 0;
  if (n->role != ZB_NET_NONE) {
    push_event(n, ZB_NET_EV_ENDED, n->player, NULL, NULL, 0);
  }
  n->role = ZB_NET_NONE;
  n->players = 0;
  n->away = 0;
  release(n);
}

static void client_reject(zb_net *n, const uint8_t *msg, size_t len) {
  char reason[96];
  size_t rl = 0;
  while (1 + rl < len && msg[1 + rl] != 0) {
    ++rl;
  }
  copy_field(reason, sizeof reason, (const char *)msg + 1, rl);
  size_t pos = 1 + rl + 1; /* after the zero byte: the package differences */
  if (pos < len) {
    size_t count = msg[pos++];
    for (size_t i = 0; i < count && n->mismatch_count < ZB_NET_MAX_MISMATCHES; ++i) {
      zb_net_mismatch *m = &n->mismatches[n->mismatch_count];
      char *fields[3] = {m->id, m->host_version, m->client_version};
      size_t caps[3] = {sizeof m->id, sizeof m->host_version, sizeof m->client_version};
      for (int f = 0; f < 3; ++f) {
        if (pos >= len || len - pos < 1u + msg[pos]) {
          n->mismatch_count = 0; /* malformed: keep only the reason */
          end_session(n, reason, false);
          return;
        }
        copy_field(fields[f], caps[f], (const char *)msg + pos + 1, msg[pos]);
        pos += 1u + msg[pos];
      }
      n->mismatch_count++;
    }
  }
  end_session(n, reason, false);
}

static void client_message(zb_net *n, const zb_tr_event *ev, uint64_t t) {
  const uint8_t *msg = ev->data;
  size_t len = ev->len;
  n->peers[0].last_seen = t;
  switch (msg[0]) {
    case MSG_WELCOME:
      if (len >= WELCOME_LEN && n->role == ZB_NET_JOINING) {
        n->role = ZB_NET_CLIENT;
        n->player = msg[1];
        n->players = msg[2];
        n->away = 0;
        n->token = zb_net_get_u64(msg + 11);
        push_event(n, ZB_NET_EV_WELCOME, n->player, NULL, NULL, 0);
        for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
          if (i != n->player && (n->players & (1u << i))) {
            push_event(n, ZB_NET_EV_JOINED, i, NULL, NULL, 0);
          }
        }
      }
      break;
    case MSG_REJECT: client_reject(n, msg, len); break;
    case MSG_BYE: end_session(n, "the host ended the session", false); break;
    case MSG_JOINED:
    case MSG_LEFT:
    case MSG_AWAY:
    case MSG_BACK:
      if (len == 2 && msg[1] < ZB_NET_MAX_PLAYERS && (int)msg[1] != n->player) {
        unsigned b = 1u << msg[1];
        static const zb_net_event_kind kinds[] = {ZB_NET_EV_JOINED, ZB_NET_EV_LEFT, ZB_NET_EV_AWAY, ZB_NET_EV_BACK};
        int k = msg[0] == MSG_JOINED ? 0 : msg[0] == MSG_LEFT ? 1 : msg[0] == MSG_AWAY ? 2 : 3;
        n->players = k == 1 ? n->players & ~b : n->players | b;
        n->away = k == 2 ? n->away | b : n->away & ~b;
        push_event(n, kinds[k], msg[1], NULL, NULL, 0);
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

static void send_hello(zb_net *n) {
  size_t total = HELLO_LEN + n->packages.len;
  if (zb_array_reserve(&n->buf, total) != ZB_OK) {
    return;
  }
  uint8_t *h = n->buf.data;
  h[0] = MSG_HELLO;
  h[1] = (uint8_t)(ZB_NET_PROTOCOL & 0xFF);
  h[2] = (uint8_t)(ZB_NET_PROTOCOL >> 8);
  zb_net_put_u64(h + 3, n->cfg.world_seed);
  zb_net_put_u64(h + 11, n->cfg.package_hash);
  zb_net_put_u64(h + 19, n->token);
  h[29] = (uint8_t)n->package_count;
  h[30] = (uint8_t)(n->package_count >> 8);
  if (n->packages.len) {
    memcpy(h + HELLO_LEN, n->packages.data, n->packages.len);
  }
  zb_net_send_raw(n, &n->peers[0], h, total, true);
}

static const char *transport_failure(const zb_net *n) {
  const char *f = n->t->failure ? n->t->failure(n->t->ctx) : "";
  return f[0] ? f : NULL;
}

static void client_update(zb_net *n, uint64_t t) {
  zb_tr_event ev;
  while (n->role != ZB_NET_NONE && n->t->next(n->t->ctx, &ev)) {
    if (ev.kind == ZB_TR_OPEN) {
      send_hello(n);
      n->peers[0].last_seen = t;
    } else if (ev.kind == ZB_TR_MESSAGE && ev.len > 0) {
      count_received(n, &n->peers[0], ev.len);
      client_message(n, &ev, t);
    } else if (ev.kind == ZB_TR_CLOSE) {
      const char *f = transport_failure(n);
      if (n->role == ZB_NET_JOINING) {
        end_session(n, f ? f : "could not reach the host", true);
      } else {
        end_session(n, "the connection to the host was lost", true);
      }
    }
  }
  if (n->role == ZB_NET_NONE) {
    return;
  }
  if (t - n->peers[0].last_seen > n->cfg.timeout_ns) {
    end_session(n, "the host stopped answering", true);
  } else if (n->role == ZB_NET_CLIENT && t - n->last_ping >= n->cfg.ping_interval_ns) {
    n->last_ping = t;
    uint8_t stamp[8];
    zb_net_put_u64(stamp, t);
    send_small(n, &n->peers[0], MSG_PING, stamp, sizeof stamp);
  }
}

/* ---- LAN ----------------------------------------------------------------------------- */

zb_result zb_net_lan_announce(zb_net *n, const char *name, uint16_t ws_port, uint16_t discovery_port, char *err,
                              size_t err_len) {
  zb_net_lan_stop(n);
  n->lan = zb_lan_open(n->cfg.a, discovery_port ? discovery_port : ZB_LAN_PORT, NULL, err, err_len);
  if (!n->lan) {
    return ZB_ERR_IO;
  }
  snprintf(n->lan_name, sizeof n->lan_name, "%s", name);
  n->lan_port = ws_port;
  return ZB_OK;
}

void zb_net_lan_stop(zb_net *n) {
  zb_lan_close(n->lan);
  n->lan = NULL;
}

bool zb_net_lan_announcing(const zb_net *n) { return n->lan != NULL; }

static void lan_update(zb_net *n) {
  if (!n->lan) {
    return;
  }
  if (n->role != ZB_NET_HOST) {
    zb_lan_answer(n->lan, NULL);
    return;
  }
  zb_lan_session s;
  memset(&s, 0, sizeof s);
  snprintf(s.name, sizeof s.name, "%s", n->lan_name);
  s.port = n->lan_port;
  s.protocol = ZB_NET_PROTOCOL;
  for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
    s.players = (uint8_t)(s.players + ((n->players >> i) & 1u));
  }
  s.max_players = ZB_NET_MAX_PLAYERS;
  zb_lan_answer(n->lan, &s);
}

/* ---- Public -------------------------------------------------------------------------- */

static void reset(zb_net *n) {
  memset(n->peers, 0, sizeof n->peers);
  memset(n->slots, 0, sizeof n->slots);
  n->players = 0;
  n->away = 0;
  n->player = -1;
  n->error[0] = '\0';
  n->token = 0;
  n->resumable = false;
  n->mismatch_count = 0;
  n->last_ping = n->last_refill = n->window_start = now(n);
  memset(n->window_out, 0, sizeof n->window_out);
  memset(n->window_in, 0, sizeof n->window_in);
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
  n->peers[0].tokens = burst_bytes(n);
  return ZB_OK;
}

bool zb_net_can_resume(const zb_net *n) { return n->role == ZB_NET_NONE && n->resumable; }

zb_result zb_net_resume(zb_net *n, zb_transport *client) {
  if (!client) {
    return ZB_ERR_INVALID;
  }
  if (!zb_net_can_resume(n)) {
    zb_transport_destroy(client);
    return ZB_ERR_STATE;
  }
  uint64_t token = n->token;
  zb_net_join(n, client);
  n->token = token;
  return ZB_OK;
}

void zb_net_leave(zb_net *n) {
  n->resumable = false;
  n->token = 0;
  if (!n->t) {
    return;
  }
  static const uint8_t bye = MSG_BYE;
  if (n->role == ZB_NET_HOST) {
    for (int i = 0; i < ZB_NET_MAX_PEERS; ++i) {
      if (n->peers[i].used) {
        if (n->peers[i].welcomed) {
          zb_net_send_raw(n, &n->peers[i], &bye, 1, false);
        }
        n->t->close(n->t->ctx, n->peers[i].conn);
      }
    }
  } else {
    if (n->role == ZB_NET_CLIENT) {
      zb_net_send_raw(n, &n->peers[0], &bye, 1, false);
    }
    n->t->close(n->t->ctx, 1);
  }
  n->t->update(n->t->ctx); /* flush the goodbyes and close frames */
  zb_transport_destroy(n->t);
  n->t = NULL;
  n->role = ZB_NET_NONE;
  n->players = 0;
  n->away = 0;
  n->player = -1;
  release(n);
}

static void stats_window(zb_net *n, uint64_t t) {
  uint64_t ms = (t - n->window_start) / 1000000u;
  if (ms < 1000) {
    return;
  }
  for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
    n->stats.kbps_out[i] = (uint32_t)(n->window_out[i] * 8u / ms);
    n->stats.kbps_in[i] = (uint32_t)(n->window_in[i] * 8u / ms);
    n->window_out[i] = n->window_in[i] = 0;
  }
  n->window_start = t;
}

void zb_net_update(zb_net *n) {
  if (n->t) {
    n->t->update(n->t->ctx);
    uint64_t t = now(n);
    if (n->role == ZB_NET_HOST) {
      host_update(n, t);
      const char *f = transport_failure(n);
      if (f) {
        end_session(n, f, false);
      }
    } else {
      client_update(n, t);
    }
    if (n->role != ZB_NET_NONE) {
      budget_update(n, t);
      zb_net_blob_update(n);
      stats_window(n, t);
    }
    n->t->update(n->t->ctx); /* send what this update queued */
    if (n->role == ZB_NET_NONE) {
      zb_transport_destroy(n->t);
      n->t = NULL;
    }
  }
  lan_update(n);
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
    return deliver(n, &n->peers[0], channel, n->player, m, total);
  }
  zb_result r = ZB_OK;
  bool sent = false;
  for (int i = 0; i < ZB_NET_MAX_PEERS; ++i) {
    peer *p = &n->peers[i];
    if (p->used && p->welcomed && (to == ZB_NET_TO_ALL || to == p->player)) {
      zb_result s = deliver(n, p, channel, n->player, m, total);
      r = s != ZB_OK ? s : r;
      sent = true;
    }
  }
  return to == ZB_NET_TO_ALL || sent ? r : ZB_ERR_NOT_FOUND;
}

const char *zb_net_invite_code(const zb_net *n) {
  return n->t && n->t->invite_code ? n->t->invite_code(n->t->ctx) : "";
}

zb_net_role zb_net_role_of(const zb_net *n) { return n->role; }
int zb_net_player(const zb_net *n) { return n->player; }
unsigned zb_net_players(const zb_net *n) { return n->players; }
unsigned zb_net_away(const zb_net *n) { return n->away; }
const char *zb_net_error(const zb_net *n) { return n->error; }
void zb_net_get_stats(const zb_net *n, zb_net_stats *out) { *out = n->stats; }

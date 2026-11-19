/* SPDX-License-Identifier: MIT */
#include "net/relay.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "core/array.h"
#include "core/hash.h"
#include "net/socket.h"

#define ENDED_CODES 64

typedef enum rc_role { RC_PENDING, RC_HOST, RC_CLIENT } rc_role;

typedef struct rconn {
  uint32_t conn; /* the listener's connection id */
  rc_role role;
  uint32_t session; /* index into sessions */
  uint32_t cid;     /* client: its id within the session, as the host sees it */
  uint64_t opened;
} rconn;

typedef struct rsession {
  bool used;
  char code[ZB_RELAY_CODE_LEN + 1];
  uint32_t host;
  uint32_t clients[ZB_RELAY_MAX_CLIENTS]; /* listener connection ids, 0 = free */
  uint32_t cids[ZB_RELAY_MAX_CLIENTS];
  uint32_t next_cid;
  uint64_t last_activity;
} rsession;

struct zb_relay {
  zb_relay_config cfg;
  zb_transport *t;
  zb_array conns;    /* rconn */
  zb_array sessions; /* rsession; indices stay valid, free ones are reused */
  uint64_t rng;
  char ended[ENDED_CODES][ZB_RELAY_CODE_LEN + 1]; /* recently ended codes, to tell "expired" from "unknown" */
  size_t ended_next;
  zb_array buf;
  zb_relay_stats stats;
};

static uint64_t now(const zb_relay *r) { return r->cfg.clock_ns ? r->cfg.clock_ns(r->cfg.clock_ctx) : 0; }

bool zb_relay_normalize_code(const char *code, char out[ZB_RELAY_CODE_LEN + 1]) {
  size_t n = 0;
  for (const char *p = code; *p; ++p) {
    if (*p == ' ' || *p == '-') {
      continue;
    }
    char c = (char)toupper((unsigned char)*p);
    if (n == ZB_RELAY_CODE_LEN || !strchr(ZB_RELAY_ALPHABET, c)) {
      return false;
    }
    out[n++] = c;
  }
  out[n] = '\0';
  return n == ZB_RELAY_CODE_LEN;
}

zb_relay *zb_relay_create(const zb_relay_config *cfg, zb_transport *listener) {
  zb_relay *r = listener ? zb_calloc(cfg->a, sizeof *r) : NULL;
  if (!r) {
    zb_transport_destroy(listener);
    return NULL;
  }
  r->cfg = *cfg;
  r->cfg.idle_timeout_ns = cfg->idle_timeout_ns ? cfg->idle_timeout_ns : 600000000000ull;
  r->cfg.handshake_timeout_ns = cfg->handshake_timeout_ns ? cfg->handshake_timeout_ns : 10000000000ull;
  r->cfg.max_sessions = cfg->max_sessions ? cfg->max_sessions : 1024;
  r->t = listener;
  r->rng = zb_mix64(cfg->seed ^ 0x72656c6179ull);
  zb_array_init(&r->conns, cfg->a, sizeof(rconn));
  zb_array_init(&r->sessions, cfg->a, sizeof(rsession));
  zb_array_init(&r->buf, cfg->a, 1);
  return r;
}

void zb_relay_destroy(zb_relay *r) {
  if (!r) {
    return;
  }
  zb_transport_destroy(r->t);
  zb_array_free(&r->conns);
  zb_array_free(&r->sessions);
  zb_array_free(&r->buf);
  zb_free(r->cfg.a, r);
}

static rconn *find_conn(zb_relay *r, uint32_t conn) {
  for (size_t i = 0; i < r->conns.len; ++i) {
    rconn *c = zb_array_at(&r->conns, i);
    if (c->conn == conn) {
      return c;
    }
  }
  return NULL;
}

/* Forgets a connection (after closing it or learning that it closed). */
static void forget_conn(zb_relay *r, uint32_t conn) {
  for (size_t i = 0; i < r->conns.len; ++i) {
    if (((rconn *)zb_array_at(&r->conns, i))->conn == conn) {
      zb_array_remove_swap(&r->conns, i);
      return;
    }
  }
}

static rsession *session_at(zb_relay *r, uint32_t i) { return zb_array_at(&r->sessions, i); }

/* Sends kind, an optional connection id and a body. */
static void send_ctl(zb_relay *r, uint32_t conn, uint8_t kind, const uint32_t *cid, const void *body, size_t len) {
  size_t head = cid ? 5 : 1;
  if (zb_array_reserve(&r->buf, head + len) != ZB_OK) {
    return;
  }
  r->buf.data[0] = kind;
  if (cid) {
    for (int i = 0; i < 4; ++i) {
      r->buf.data[1 + i] = (uint8_t)(*cid >> (8 * i));
    }
  }
  if (len) {
    memcpy(r->buf.data + head, body, len);
  }
  r->t->send(r->t->ctx, conn, r->buf.data, head + len);
}

static void refuse(zb_relay *r, uint32_t conn, const char *reason) {
  send_ctl(r, conn, ZB_RELAY_REFUSED, NULL, reason, strlen(reason));
  r->t->close(r->t->ctx, conn);
  forget_conn(r, conn);
  r->stats.refused++;
}

static rsession *find_session(zb_relay *r, const char *code) {
  for (size_t i = 0; i < r->sessions.len; ++i) {
    rsession *s = zb_array_at(&r->sessions, i);
    if (s->used && strcmp(s->code, code) == 0) {
      return s;
    }
  }
  return NULL;
}

static bool recently_ended(const zb_relay *r, const char *code) {
  for (size_t i = 0; i < ENDED_CODES; ++i) {
    if (strcmp(r->ended[i], code) == 0) {
      return true;
    }
  }
  return false;
}

/* A fresh code, unique among live sessions and recently ended ones. */
static void new_code(zb_relay *r, char out[ZB_RELAY_CODE_LEN + 1]) {
  static const char alphabet[] = ZB_RELAY_ALPHABET;
  do {
    for (int i = 0; i < ZB_RELAY_CODE_LEN; ++i) {
      r->rng = zb_mix64(r->rng + 0x9e3779b97f4a7c15ull);
      out[i] = alphabet[(r->rng >> 16) % (sizeof alphabet - 1)];
    }
    out[ZB_RELAY_CODE_LEN] = '\0';
  } while (find_session(r, out) || recently_ended(r, out));
}

/* Ends a session: its clients (and, unless it is already gone, the host) are disconnected. */
static void end_session(zb_relay *r, uint32_t index, bool close_host) {
  rsession *s = session_at(r, index);
  for (int i = 0; i < ZB_RELAY_MAX_CLIENTS; ++i) {
    if (s->clients[i]) {
      r->t->close(r->t->ctx, s->clients[i]);
      forget_conn(r, s->clients[i]);
    }
  }
  if (close_host) {
    r->t->close(r->t->ctx, s->host);
  }
  forget_conn(r, s->host);
  memcpy(r->ended[r->ended_next], s->code, sizeof s->code);
  r->ended_next = (r->ended_next + 1) % ENDED_CODES;
  memset(s, 0, sizeof *s);
}

static void on_register(zb_relay *r, rconn *c, const uint8_t *msg, size_t len) {
  if (len != 2 || msg[1] != ZB_RELAY_VERSION) {
    refuse(r, c->conn, "the relay speaks a different protocol version");
    return;
  }
  uint32_t live = 0, index = (uint32_t)r->sessions.len;
  for (size_t i = 0; i < r->sessions.len; ++i) {
    if (session_at(r, (uint32_t)i)->used) {
      live++;
    } else if (index == r->sessions.len) {
      index = (uint32_t)i;
    }
  }
  rsession fresh;
  memset(&fresh, 0, sizeof fresh);
  if (live >= r->cfg.max_sessions || (index == r->sessions.len && !zb_array_push(&r->sessions, &fresh))) {
    refuse(r, c->conn, "the relay is full");
    return;
  }
  rsession *s = session_at(r, index);
  new_code(r, s->code); /* before the session counts as live, so it does not collide with itself */
  s->used = true;
  s->host = c->conn;
  s->last_activity = now(r);
  c->role = RC_HOST;
  c->session = index;
  send_ctl(r, c->conn, ZB_RELAY_CODE, NULL, s->code, ZB_RELAY_CODE_LEN);
}

static void on_join(zb_relay *r, rconn *c, const uint8_t *msg, size_t len) {
  char code[ZB_RELAY_CODE_LEN + 1], typed[ZB_RELAY_CODE_LEN + 1];
  if (len != 2 + ZB_RELAY_CODE_LEN || msg[1] != ZB_RELAY_VERSION) {
    refuse(r, c->conn, "the relay speaks a different protocol version");
    return;
  }
  memcpy(typed, msg + 2, ZB_RELAY_CODE_LEN);
  typed[ZB_RELAY_CODE_LEN] = '\0';
  bool valid = zb_relay_normalize_code(typed, code);
  rsession *s = valid ? find_session(r, code) : NULL;
  if (!s) {
    refuse(r, c->conn,
           valid && recently_ended(r, code) ? "this invite code has expired"
                                            : "there is no session with this invite code");
    return;
  }
  int slot = -1;
  for (int i = ZB_RELAY_MAX_CLIENTS - 1; i >= 0; --i) {
    slot = s->clients[i] ? slot : i;
  }
  if (slot < 0) {
    refuse(r, c->conn, "the session is full");
    return;
  }
  c->role = RC_CLIENT;
  c->session = (uint32_t)(s - session_at(r, 0));
  c->cid = ++s->next_cid;
  s->clients[slot] = c->conn;
  s->cids[slot] = c->cid;
  s->last_activity = now(r);
  send_ctl(r, c->conn, ZB_RELAY_JOINED, NULL, NULL, 0);
  send_ctl(r, s->host, ZB_RELAY_OPEN, &c->cid, NULL, 0);
}

static int client_slot(const rsession *s, uint32_t cid) {
  for (int i = 0; i < ZB_RELAY_MAX_CLIENTS; ++i) {
    if (s->clients[i] && s->cids[i] == cid) {
      return i;
    }
  }
  return -1;
}

static void on_host(zb_relay *r, rconn *c, const uint8_t *msg, size_t len) {
  rsession *s = session_at(r, c->session);
  s->last_activity = now(r);
  if ((msg[0] != ZB_RELAY_TO && msg[0] != ZB_RELAY_KICK) || len < 5) {
    return; /* keepalives and anything unknown */
  }
  uint32_t cid = (uint32_t)msg[1] | (uint32_t)msg[2] << 8 | (uint32_t)msg[3] << 16 | (uint32_t)msg[4] << 24;
  int slot = client_slot(s, cid);
  if (slot < 0) {
    return; /* that client is already gone */
  }
  if (msg[0] == ZB_RELAY_TO) {
    r->t->send(r->t->ctx, s->clients[slot], msg + 5, len - 5);
    r->stats.forwarded++;
    r->stats.bytes += len - 5;
  } else {
    r->t->close(r->t->ctx, s->clients[slot]);
    forget_conn(r, s->clients[slot]);
    s->clients[slot] = 0;
  }
}

static void on_message(zb_relay *r, uint32_t conn, const uint8_t *msg, size_t len) {
  rconn *c = find_conn(r, conn);
  if (!c) {
    return;
  }
  if (c->role == RC_HOST) {
    if (len > 0) {
      on_host(r, c, msg, len);
    }
  } else if (c->role == RC_CLIENT) {
    rsession *s = session_at(r, c->session);
    s->last_activity = now(r);
    send_ctl(r, s->host, ZB_RELAY_FROM, &c->cid, msg, len);
    r->stats.forwarded++;
    r->stats.bytes += len;
  } else if (len >= 1 && msg[0] == ZB_RELAY_REGISTER) {
    on_register(r, c, msg, len);
  } else if (len >= 1 && msg[0] == ZB_RELAY_JOIN) {
    on_join(r, c, msg, len);
  } else {
    refuse(r, conn, "register or join first");
  }
}

static void on_close(zb_relay *r, uint32_t conn) {
  rconn *c = find_conn(r, conn);
  if (!c) {
    return;
  }
  if (c->role == RC_HOST) {
    end_session(r, c->session, false);
    return;
  }
  if (c->role == RC_CLIENT) {
    rsession *s = session_at(r, c->session);
    for (int i = 0; i < ZB_RELAY_MAX_CLIENTS; ++i) {
      s->clients[i] = s->clients[i] == conn ? 0 : s->clients[i];
    }
    send_ctl(r, s->host, ZB_RELAY_CLOSED, &c->cid, NULL, 0);
  }
  forget_conn(r, conn);
}

void zb_relay_update(zb_relay *r) {
  r->t->update(r->t->ctx);
  uint64_t t = now(r);
  zb_tr_event ev;
  while (r->t->next(r->t->ctx, &ev)) {
    if (ev.kind == ZB_TR_OPEN) {
      rconn c = {ev.conn, RC_PENDING, 0, 0, t};
      if (!zb_array_push(&r->conns, &c)) {
        r->t->close(r->t->ctx, ev.conn);
      }
    } else if (ev.kind == ZB_TR_MESSAGE) {
      on_message(r, ev.conn, ev.data, ev.len);
    } else {
      on_close(r, ev.conn);
    }
  }
  t = now(r); /* activity above was stamped with later readings of a real clock */
  for (size_t i = 0; i < r->conns.len;) {
    rconn *c = zb_array_at(&r->conns, i);
    if (c->role == RC_PENDING && t - c->opened > r->cfg.handshake_timeout_ns) {
      r->t->close(r->t->ctx, c->conn);
      zb_array_remove_swap(&r->conns, i);
    } else {
      ++i;
    }
  }
  for (size_t i = 0; i < r->sessions.len; ++i) {
    rsession *s = session_at(r, (uint32_t)i);
    if (s->used && t - s->last_activity > r->cfg.idle_timeout_ns) {
      end_session(r, (uint32_t)i, true);
    }
  }
  r->t->update(r->t->ctx);
}

void zb_relay_get_stats(const zb_relay *r, zb_relay_stats *out) {
  *out = r->stats;
  out->sessions = out->clients = out->pending = 0;
  for (size_t i = 0; i < r->conns.len; ++i) {
    rc_role role = ((const rconn *)zb_array_at(&r->conns, i))->role;
    out->sessions += role == RC_HOST;
    out->clients += role == RC_CLIENT;
    out->pending += role == RC_PENDING;
  }
}

/* ---- The program -------------------------------------------------------------------- */

static const char USAGE[] =
    "usage: zauberer-relay [options]\n"
    "Forwards co-op sessions of Zauberer between a host and up to three clients\n"
    "that find it by invite code. It sees only opaque messages and keeps no logs.\n"
    "\n"
    "  --port N           listen on this TCP port (default 47821, 0 = any free port)\n"
    "  --seed N           seed of the invite code generator (default: from the clock)\n"
    "  --idle SECONDS     end sessions without traffic after this long (default 600)\n"
    "  --max-sessions N   refuse new sessions beyond this many (default 1024)\n"
    "  --max-seconds N    stop after this long (default: run until interrupted)\n"
    "  --help             show this text\n";

static uint64_t wall_clock(void *ctx) {
  (void)ctx;
  return zb_sock_clock_ns();
}

static bool parse_number(const char *s, uint64_t max, uint64_t *out) {
  char *end = NULL;
  if (!s || *s < '0' || *s > '9') {
    return false;
  }
  unsigned long long v = strtoull(s, &end, 10);
  if (*end || v > max) {
    return false;
  }
  *out = v;
  return true;
}

int zb_relay_main(int argc, char **argv, FILE *out, volatile sig_atomic_t *stop) {
  uint64_t port = ZB_RELAY_PORT, idle = 600, max_sessions = 1024, max_seconds = UINT64_MAX;
  uint64_t seed = zb_sock_clock_ns() ^ (uint64_t)(uintptr_t)&port;
  for (int i = 1; i < argc; i += 2) { /* options come with a value, except --help */
    const char *opt = argv[i], *value = i + 1 < argc ? argv[i + 1] : NULL;
    uint64_t *target = NULL, max = UINT64_MAX;
    if (strcmp(opt, "--help") == 0 || strcmp(opt, "-h") == 0) {
      fputs(USAGE, out);
      return 0;
    } else if (strcmp(opt, "--port") == 0) {
      target = &port;
      max = 65535;
    } else if (strcmp(opt, "--seed") == 0) {
      target = &seed;
    } else if (strcmp(opt, "--idle") == 0) {
      target = &idle;
      max = 86400 * 365;
    } else if (strcmp(opt, "--max-sessions") == 0) {
      target = &max_sessions;
      max = 1000000;
    } else if (strcmp(opt, "--max-seconds") == 0) {
      target = &max_seconds;
      max = 86400 * 365;
    }
    if (!target) {
      fprintf(out, "zauberer-relay: unknown option %s\n%s", opt, USAGE);
      return 2;
    }
    if (!parse_number(value, max, target)) {
      fprintf(out, "zauberer-relay: %s needs a number up to %llu\n", opt, (unsigned long long)max);
      return 2;
    }
  }
  char err[128];
  uint16_t bound = 0;
  zb_transport *t = zb_ws_server_create(zb_heap_allocator(), (uint16_t)port, &bound, err, sizeof err);
  if (!t) {
    fprintf(out, "zauberer-relay: %s\n", err);
    return 1;
  }
  zb_relay_config cfg;
  memset(&cfg, 0, sizeof cfg);
  cfg.a = zb_heap_allocator();
  cfg.seed = seed;
  cfg.clock_ns = wall_clock;
  cfg.idle_timeout_ns = (idle ? idle : 1) * 1000000000ull;
  cfg.max_sessions = (uint32_t)(max_sessions ? max_sessions : 1);
  zb_relay *r = zb_relay_create(&cfg, t);
  fprintf(out, "zauberer-relay listening on port %u\n", (unsigned)bound);
  fflush(out);
  uint64_t start = zb_sock_clock_ns();
  do {
    zb_relay_update(r);
    zb_sock_sleep_ms(2);
  } while (!*stop && (max_seconds == UINT64_MAX || zb_sock_clock_ns() - start < max_seconds * 1000000000ull));
  zb_relay_stats st;
  zb_relay_get_stats(r, &st);
  fprintf(out, "zauberer-relay stopped (%u sessions open, %llu messages forwarded)\n", st.sessions,
          (unsigned long long)st.forwarded);
  zb_relay_destroy(r);
  return 0;
}

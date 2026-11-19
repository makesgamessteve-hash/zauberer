/* SPDX-License-Identifier: MIT */
#include "net/transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/array.h"
#include "core/hash.h"
#include "net/socket.h"
#include "net/ws.h"

void zb_transport_destroy(zb_transport *t) {
  if (t) {
    t->destroy(t->ctx);
  }
}

/* ---- Event queue shared by the implementations ---------------------------- */

typedef struct queued {
  zb_tr_event_kind kind;
  uint32_t conn;
  uint8_t *data;
  size_t len;
} queued;

typedef struct event_queue {
  const zb_allocator *a;
  zb_array items; /* queued */
  size_t head;
  uint8_t *current; /* data of the event last returned */
} event_queue;

static void eq_init(event_queue *q, const zb_allocator *a) {
  memset(q, 0, sizeof *q);
  q->a = a;
  zb_array_init(&q->items, a, sizeof(queued));
}

static zb_result eq_push(event_queue *q, zb_tr_event_kind kind, uint32_t conn, const void *data, size_t len) {
  queued e = {kind, conn, NULL, len};
  if (len) {
    e.data = zb_alloc(q->a, len);
    if (!e.data) {
      return ZB_ERR_NOMEM;
    }
    memcpy(e.data, data, len);
  }
  if (!zb_array_push(&q->items, &e)) {
    zb_free(q->a, e.data);
    return ZB_ERR_NOMEM;
  }
  return ZB_OK;
}

static bool eq_next(event_queue *q, zb_tr_event *ev) {
  zb_free(q->a, q->current);
  q->current = NULL;
  if (q->head >= q->items.len) {
    zb_array_clear(&q->items);
    q->head = 0;
    return false;
  }
  queued *e = zb_array_at(&q->items, q->head++);
  q->current = e->data;
  ev->kind = e->kind;
  ev->conn = e->conn;
  ev->data = e->data;
  ev->len = e->len;
  return true;
}

static void eq_free(event_queue *q) {
  zb_free(q->a, q->current);
  for (size_t i = q->head; i < q->items.len; ++i) {
    zb_free(q->a, ((queued *)zb_array_at(&q->items, i))->data);
  }
  zb_array_free(&q->items);
}

/* ---- Loopback ------------------------------------------------------------------ */

typedef struct lb_end lb_end;

typedef struct lb_link {
  uint32_t id;
  lb_end *client;
  bool open;
  uint64_t last_up, last_down; /* delivery time of the latest event in flight per direction */
} lb_link;

typedef struct lb_flight {
  uint64_t at;
  uint32_t link;
  bool up; /* towards the listener */
  zb_tr_event_kind kind;
  uint8_t *data;
  size_t len;
} lb_flight;

struct zb_loopback {
  const zb_allocator *a;
  lb_end *listener;
  zb_array links; /* lb_link */
  uint32_t next_id;
  bool impaired;
  zb_loopback_impairment imp;
  zb_array flight; /* lb_flight, in sending order */
  uint64_t rng;
  bool checked;
  uint64_t checked_second; /* connection drops are decided once per whole second of clock time */
  uint32_t cuts;
};

struct lb_end {
  zb_transport iface;
  zb_loopback *lb;
  bool is_listener;
  uint32_t link; /* client: its link id */
  event_queue q;
};

zb_loopback *zb_loopback_create(const zb_allocator *a) {
  zb_loopback *lb = zb_calloc(a, sizeof *lb);
  if (lb) {
    lb->a = a;
    zb_array_init(&lb->links, a, sizeof(lb_link));
    zb_array_init(&lb->flight, a, sizeof(lb_flight));
  }
  return lb;
}

static void drop_flight(zb_loopback *lb, uint32_t link, bool up_only) {
  for (size_t i = 0; i < lb->flight.len;) {
    lb_flight *f = zb_array_at(&lb->flight, i);
    if ((up_only && f->up) || (!up_only && f->link == link)) {
      zb_free(lb->a, f->data);
      zb_array_remove(&lb->flight, i);
    } else {
      ++i;
    }
  }
}

void zb_loopback_destroy(zb_loopback *lb) {
  if (lb) {
    for (size_t i = 0; i < lb->flight.len; ++i) {
      zb_free(lb->a, ((lb_flight *)zb_array_at(&lb->flight, i))->data);
    }
    zb_array_free(&lb->flight);
    zb_array_free(&lb->links);
    zb_free(lb->a, lb);
  }
}

static lb_link *find_link(zb_loopback *lb, uint32_t id) {
  for (size_t i = 0; i < lb->links.len; ++i) {
    lb_link *l = zb_array_at(&lb->links, i);
    if (l->id == id) {
      return l;
    }
  }
  return NULL;
}

static uint64_t lb_now(const zb_loopback *lb) { return lb->imp.clock_ns ? lb->imp.clock_ns(lb->imp.clock_ctx) : 0; }

/* Hands an event to the other end of a link: at once, or after the impairment's delay. */
static zb_result lb_deliver(zb_loopback *lb, lb_link *l, bool up, zb_tr_event_kind kind, const void *data, size_t len) {
  if (!lb->impaired) {
    if (up) {
      return lb->listener ? eq_push(&lb->listener->q, kind, l->id, data, len) : ZB_ERR_STATE;
    }
    return eq_push(&l->client->q, kind, 1, data, len);
  }
  lb->rng = zb_mix64(lb->rng + 0x9e3779b97f4a7c15ull);
  uint64_t at = lb_now(lb) + lb->imp.latency_ns + (lb->imp.jitter_ns ? lb->rng % (lb->imp.jitter_ns + 1) : 0);
  uint64_t *last = up ? &l->last_up : &l->last_down;
  at = at < *last ? *last : at; /* TCP keeps the order */
  *last = at;
  lb_flight f = {at, l->id, up, kind, NULL, len};
  if (len) {
    f.data = zb_alloc(lb->a, len);
    if (!f.data) {
      return ZB_ERR_NOMEM;
    }
    memcpy(f.data, data, len);
  }
  if (!zb_array_push(&lb->flight, &f)) {
    zb_free(lb->a, f.data);
    return ZB_ERR_NOMEM;
  }
  return ZB_OK;
}

uint32_t zb_loopback_cuts(const zb_loopback *lb) { return lb->cuts; }

bool zb_loopback_cut(zb_loopback *lb, uint32_t conn) {
  lb_link *l = find_link(lb, conn);
  if (!l || !l->open) {
    return false;
  }
  l->open = false;
  lb->cuts++;
  drop_flight(lb, conn, false);
  if (lb->listener) {
    eq_push(&lb->listener->q, ZB_TR_CLOSE, conn, NULL, 0);
  }
  eq_push(&l->client->q, ZB_TR_CLOSE, 1, NULL, 0);
  return true;
}

void zb_loopback_impair(zb_loopback *lb, const zb_loopback_impairment *imp) {
  lb->impaired = imp != NULL;
  if (imp) {
    lb->imp = *imp;
    lb->rng = zb_mix64(imp->seed ^ 0x6c6f6f70ull);
    lb->checked = false;
  }
}

/* Cuts connections at random (impairment) and delivers the events that are due. */
static void lb_pump(zb_loopback *lb) {
  if (!lb->impaired && lb->flight.len == 0) {
    return;
  }
  uint64_t now = lb_now(lb);
  if (lb->impaired && lb->imp.drops_per_second > 0) {
    uint64_t second = now / 1000000000ull;
    if (!lb->checked) {
      lb->checked = true;
      lb->checked_second = second;
    }
    while (lb->checked_second < second) {
      lb->checked_second++;
      for (size_t i = 0; i < lb->links.len; ++i) {
        lb_link *l = zb_array_at(&lb->links, i);
        uint64_t r = zb_mix64(lb->imp.seed ^ zb_mix64(((uint64_t)l->id << 32) ^ lb->checked_second));
        if (l->open && (double)(r >> 11) * 0x1.0p-53 < lb->imp.drops_per_second) {
          zb_loopback_cut(lb, l->id);
        }
      }
    }
  }
  for (size_t i = 0; i < lb->flight.len;) {
    lb_flight *f = zb_array_at(&lb->flight, i);
    if (f->at > now) {
      ++i;
      continue;
    }
    lb_link *l = f->up ? NULL : find_link(lb, f->link);
    if (f->up && lb->listener) {
      eq_push(&lb->listener->q, f->kind, f->link, f->data, f->len);
    } else if (l) {
      eq_push(&l->client->q, f->kind, 1, f->data, f->len);
    }
    zb_free(lb->a, f->data);
    zb_array_remove(&lb->flight, i);
  }
}

static void lb_update(void *ctx) { lb_pump(((lb_end *)ctx)->lb); }

static bool lb_next(void *ctx, zb_tr_event *ev) { return eq_next(&((lb_end *)ctx)->q, ev); }

static zb_result lb_send(void *ctx, uint32_t conn, const void *data, size_t len) {
  lb_end *e = ctx;
  lb_link *l = find_link(e->lb, e->is_listener ? conn : e->link);
  if (!l || !l->open || !e->lb->listener) {
    return ZB_ERR_STATE;
  }
  return lb_deliver(e->lb, l, !e->is_listener, ZB_TR_MESSAGE, data, len);
}

static void lb_close(void *ctx, uint32_t conn) {
  lb_end *e = ctx;
  lb_link *l = find_link(e->lb, e->is_listener ? conn : e->link);
  if (!l || !l->open) {
    return;
  }
  l->open = false;
  if (e->is_listener || e->lb->listener) {
    lb_deliver(e->lb, l, !e->is_listener, ZB_TR_CLOSE, NULL, 0);
  }
}

static void lb_destroy(void *ctx) {
  lb_end *e = ctx;
  zb_loopback *lb = e->lb;
  if (e->is_listener) {
    for (size_t i = 0; i < lb->links.len; ++i) {
      lb_link *l = zb_array_at(&lb->links, i);
      if (l->open) {
        l->open = false;
        lb_deliver(lb, l, false, ZB_TR_CLOSE, NULL, 0);
      }
    }
    drop_flight(lb, 0, true);
    lb->listener = NULL;
  } else {
    lb_close(e, 0);
    for (size_t i = 0; i < lb->links.len; ++i) {
      if (((lb_link *)zb_array_at(&lb->links, i))->id == e->link) {
        zb_array_remove_swap(&lb->links, i);
        break;
      }
    }
  }
  eq_free(&e->q);
  zb_free(lb->a, e);
}

static lb_end *lb_new(zb_loopback *lb, bool listener) {
  lb_end *e = zb_calloc(lb->a, sizeof *e);
  if (!e) {
    return NULL;
  }
  e->lb = lb;
  e->is_listener = listener;
  eq_init(&e->q, lb->a);
  e->iface.ctx = e;
  e->iface.name = "loopback";
  e->iface.update = lb_update;
  e->iface.next = lb_next;
  e->iface.send = lb_send;
  e->iface.close = lb_close;
  e->iface.destroy = lb_destroy;
  return e;
}

zb_transport *zb_loopback_listen(zb_loopback *lb) {
  if (lb->listener) {
    return NULL;
  }
  lb_end *e = lb_new(lb, true);
  if (!e) {
    return NULL;
  }
  lb->listener = e;
  return &e->iface;
}

zb_transport *zb_loopback_connect(zb_loopback *lb) {
  if (!lb->listener) {
    return NULL;
  }
  lb_end *e = lb_new(lb, false);
  if (!e) {
    return NULL;
  }
  lb_link link = {++lb->next_id, e, true, 0, 0};
  lb_link *l = zb_array_push(&lb->links, &link);
  if (!l) {
    eq_free(&e->q);
    zb_free(lb->a, e);
    return NULL;
  }
  e->link = link.id;
  if (lb_deliver(lb, l, false, ZB_TR_OPEN, NULL, 0) != ZB_OK || lb_deliver(lb, l, true, ZB_TR_OPEN, NULL, 0) != ZB_OK) {
    lb_destroy(e);
    return NULL;
  }
  return &e->iface;
}

/* ---- URL ------------------------------------------------------------------------- */

bool zb_ws_parse_url(const char *url, char *host, size_t host_len, uint16_t *port, char *path, size_t path_len) {
  if (strncmp(url, "ws://", 5) != 0) {
    return false;
  }
  const char *h = url + 5;
  size_t n = strcspn(h, ":/");
  if (n == 0 || n >= host_len) {
    return false;
  }
  memcpy(host, h, n);
  host[n] = '\0';
  const char *p = h + n;
  *port = 80;
  if (*p == ':') {
    char *end = NULL;
    long v = strtol(p + 1, &end, 10);
    if (end == p + 1 || v <= 0 || v > 65535) {
      return false;
    }
    *port = (uint16_t)v;
    p = end;
  }
  if (*p && *p != '/') {
    return false;
  }
  snprintf(path, path_len, "%s", *p ? p : "/");
  return true;
}

#if !defined(__EMSCRIPTEN__)

/* ---- WebSocket over our own sockets ----------------------------------------------------- */

typedef struct ws_conn {
  uint32_t id;
  zb_socket sock;
  zb_ws ws;
  bool open;      /* handshake done, OPEN reported */
  bool connected; /* client: TCP connect finished */
  bool dead;
} ws_conn;

typedef struct ws_tr {
  zb_transport iface;
  const zb_allocator *a;
  zb_socket listener; /* server only */
  zb_array conns;     /* ws_conn * */
  uint32_t next_id;
  event_queue q;
} ws_tr;

static void conn_free(ws_tr *t, ws_conn *c) {
  zb_sock_close(c->sock);
  zb_ws_free(&c->ws);
  zb_free(t->a, c);
}

/* Moves bytes for one connection; false once it is gone. */
static bool pump(ws_tr *t, ws_conn *c) {
  uint8_t buf[16384];
  bool eof = false; /* the peer is gone, but what it sent before still counts */
  for (;;) {
    long n = zb_sock_recv(c->sock, buf, sizeof buf);
    if (n == ZB_SOCK_WOULD_BLOCK) {
      break;
    }
    if (n == ZB_SOCK_CLOSED) {
      eof = true;
      break;
    }
    if (zb_ws_feed(&c->ws, buf, (size_t)n) != ZB_OK) {
      c->dead = true;
      break;
    }
  }
  const uint8_t *msg;
  size_t len;
  int r;
  while (!c->dead && (r = zb_ws_poll(&c->ws, &msg, &len)) != 0) {
    if (r < 0) {
      c->dead = true;
      break;
    }
    if (!c->open) { /* OPEN comes before the connection's first message */
      c->open = true;
      eq_push(&t->q, ZB_TR_OPEN, c->id, NULL, 0);
    }
    eq_push(&t->q, ZB_TR_MESSAGE, c->id, msg, len);
  }
  if (!c->open && c->ws.state == ZB_WS_OPEN) {
    c->open = true;
    eq_push(&t->q, ZB_TR_OPEN, c->id, NULL, 0);
  }
  c->dead = c->dead || eof;
  size_t pending;
  const uint8_t *out = zb_ws_pending(&c->ws, &pending);
  while (pending > 0) {
    long n = zb_sock_send(c->sock, out, pending);
    if (n == ZB_SOCK_WOULD_BLOCK) {
      break;
    }
    if (n == ZB_SOCK_CLOSED) {
      c->dead = true;
      break;
    }
    zb_ws_sent(&c->ws, (size_t)n);
    out = zb_ws_pending(&c->ws, &pending);
  }
  return !(c->dead || (c->ws.state == ZB_WS_CLOSED && pending == 0));
}

static void ws_update(void *ctx) {
  ws_tr *t = ctx;
  if (t->listener != ZB_SOCKET_NONE) {
    zb_socket s;
    while ((s = zb_sock_accept(t->listener)) != ZB_SOCKET_NONE) {
      ws_conn *c = zb_calloc(t->a, sizeof *c);
      if (!c) {
        zb_sock_close(s);
        continue;
      }
      c->id = ++t->next_id;
      c->sock = s;
      c->connected = true;
      zb_ws_init_server(&c->ws, t->a);
      if (!zb_array_push(&t->conns, &c)) {
        conn_free(t, c);
      }
    }
  }
  for (size_t i = 0; i < t->conns.len;) {
    ws_conn *c = *(ws_conn **)zb_array_at(&t->conns, i);
    if (!c->connected) {
      int st = zb_sock_poll_connect(c->sock);
      if (st == 0) {
        ++i;
        continue;
      }
      c->connected = st > 0;
      c->dead = st < 0;
    }
    if (c->dead || !pump(t, c)) {
      if (c->open || t->listener == ZB_SOCKET_NONE) {
        eq_push(&t->q, ZB_TR_CLOSE, c->id, NULL, 0);
      }
      conn_free(t, c);
      zb_array_remove_swap(&t->conns, i);
      continue;
    }
    ++i;
  }
}

static bool ws_next(void *ctx, zb_tr_event *ev) { return eq_next(&((ws_tr *)ctx)->q, ev); }

static ws_conn *ws_find(ws_tr *t, uint32_t id) {
  for (size_t i = 0; i < t->conns.len; ++i) {
    ws_conn *c = *(ws_conn **)zb_array_at(&t->conns, i);
    if (c->id == id) {
      return c;
    }
  }
  return NULL;
}

static zb_result ws_send(void *ctx, uint32_t conn, const void *data, size_t len) {
  ws_conn *c = ws_find(ctx, conn);
  return c && c->open ? zb_ws_send(&c->ws, data, len) : ZB_ERR_STATE;
}

static void ws_close(void *ctx, uint32_t conn) {
  ws_conn *c = ws_find(ctx, conn);
  if (c) {
    zb_ws_close(&c->ws, 1000);
  }
}

static void ws_destroy(void *ctx) {
  ws_tr *t = ctx;
  for (size_t i = 0; i < t->conns.len; ++i) {
    ws_conn *c = *(ws_conn **)zb_array_at(&t->conns, i);
    zb_ws_close(&c->ws, 1001);
    pump(t, c);
    conn_free(t, c);
  }
  zb_sock_close(t->listener);
  zb_array_free(&t->conns);
  eq_free(&t->q);
  zb_free(t->a, t);
}

static ws_tr *ws_new(const zb_allocator *a) {
  ws_tr *t = zb_calloc(a, sizeof *t);
  if (!t) {
    return NULL;
  }
  t->a = a;
  t->listener = ZB_SOCKET_NONE;
  zb_array_init(&t->conns, a, sizeof(ws_conn *));
  eq_init(&t->q, a);
  t->iface.ctx = t;
  t->iface.name = "websocket";
  t->iface.update = ws_update;
  t->iface.next = ws_next;
  t->iface.send = ws_send;
  t->iface.close = ws_close;
  t->iface.destroy = ws_destroy;
  return t;
}

zb_transport *zb_ws_server_create(const zb_allocator *a, uint16_t port, uint16_t *bound_port, char *err,
                                  size_t err_len) {
  ws_tr *t = ws_new(a);
  if (!t) {
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  t->listener = zb_sock_listen(port, bound_port);
  if (t->listener == ZB_SOCKET_NONE) {
    snprintf(err, err_len, "cannot listen on port %u", (unsigned)port);
    ws_destroy(t);
    return NULL;
  }
  return &t->iface;
}

zb_transport *zb_ws_client_create(const zb_allocator *a, const char *url, char *err, size_t err_len) {
  char host[256], path[256];
  uint16_t port;
  if (!zb_ws_parse_url(url, host, sizeof host, &port, path, sizeof path)) {
    snprintf(err, err_len, "not a ws:// address: %s", url);
    return NULL;
  }
  ws_tr *t = ws_new(a);
  ws_conn *c = t ? zb_calloc(a, sizeof *c) : NULL;
  if (!c) {
    snprintf(err, err_len, "out of memory");
    if (t) {
      ws_destroy(t);
    }
    return NULL;
  }
  c->id = 1;
  c->sock = zb_sock_connect(host, port);
  if (c->sock == ZB_SOCKET_NONE || zb_ws_init_client(&c->ws, a, host, path, (uint64_t)(uintptr_t)c) != ZB_OK ||
      !zb_array_push(&t->conns, &c)) {
    snprintf(err, err_len, "cannot connect to %s", url);
    conn_free(t, c);
    ws_destroy(t);
    return NULL;
  }
  return &t->iface;
}

#else

/* ---- The browser's WebSocket (web/lib_net.js) ------------------------------------ */

extern int zb_js_ws_open(const char *url);
extern int zb_js_ws_state(int id); /* 0 connecting, 1 open, 2 closed */
extern int zb_js_ws_send(int id, const void *data, int len);
extern int zb_js_ws_peek(int id); /* size of the next message, or -1 */
extern void zb_js_ws_take(int id, void *dst);
extern void zb_js_ws_close(int id);

typedef struct js_tr {
  zb_transport iface;
  const zb_allocator *a;
  int id;
  bool open, closed;
  event_queue q;
  zb_array buf;
} js_tr;

static void js_update(void *ctx) {
  js_tr *t = ctx;
  if (t->closed) {
    return;
  }
  int st = zb_js_ws_state(t->id);
  if (st >= 1 && !t->open) {
    t->open = true;
    eq_push(&t->q, ZB_TR_OPEN, 1, NULL, 0);
  }
  int n;
  while ((n = zb_js_ws_peek(t->id)) >= 0) {
    if (zb_array_reserve(&t->buf, (size_t)n + 1) != ZB_OK) {
      break;
    }
    zb_js_ws_take(t->id, t->buf.data);
    eq_push(&t->q, ZB_TR_MESSAGE, 1, t->buf.data, (size_t)n);
  }
  if (st == 2) {
    t->closed = true;
    eq_push(&t->q, ZB_TR_CLOSE, 1, NULL, 0);
  }
}

static bool js_next(void *ctx, zb_tr_event *ev) { return eq_next(&((js_tr *)ctx)->q, ev); }

static zb_result js_send(void *ctx, uint32_t conn, const void *data, size_t len) {
  js_tr *t = ctx;
  (void)conn;
  return t->open && !t->closed && zb_js_ws_send(t->id, data, (int)len) == 0 ? ZB_OK : ZB_ERR_STATE;
}

static void js_close(void *ctx, uint32_t conn) {
  (void)conn;
  zb_js_ws_close(((js_tr *)ctx)->id);
}

static void js_destroy(void *ctx) {
  js_tr *t = ctx;
  zb_js_ws_close(t->id);
  eq_free(&t->q);
  zb_array_free(&t->buf);
  zb_free(t->a, t);
}

zb_transport *zb_ws_server_create(const zb_allocator *a, uint16_t port, uint16_t *bound_port, char *err,
                                  size_t err_len) {
  (void)a;
  (void)port;
  (void)bound_port;
  snprintf(err, err_len, "hosting is not available in the browser");
  return NULL;
}

zb_transport *zb_ws_client_create(const zb_allocator *a, const char *url, char *err, size_t err_len) {
  char host[256], path[256];
  uint16_t port;
  if (!zb_ws_parse_url(url, host, sizeof host, &port, path, sizeof path)) {
    snprintf(err, err_len, "not a ws:// address: %s", url);
    return NULL;
  }
  js_tr *t = zb_calloc(a, sizeof *t);
  if (!t) {
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  t->a = a;
  t->id = zb_js_ws_open(url);
  eq_init(&t->q, a);
  zb_array_init(&t->buf, a, 1);
  if (t->id < 0) {
    snprintf(err, err_len, "the browser cannot open %s", url);
    js_destroy(t);
    return NULL;
  }
  t->iface.ctx = t;
  t->iface.name = "browser-websocket";
  t->iface.update = js_update;
  t->iface.next = js_next;
  t->iface.send = js_send;
  t->iface.close = js_close;
  t->iface.destroy = js_destroy;
  return &t->iface;
}

#endif

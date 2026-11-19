/* SPDX-License-Identifier: MIT */
/* The endpoints of the relay protocol (net/relay.h): a host's listening
 * transport and a client's transport, both over a client connection to the
 * relay. They work over any transport, so also in the browser. */
#include <stdio.h>
#include <string.h>

#include "core/array.h"
#include "net/relay.h"

#define KEEPALIVE_NS 20000000000ull

static uint32_t get_cid(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put_cid(uint8_t *p, uint32_t cid) {
  for (int i = 0; i < 4; ++i) {
    p[i] = (uint8_t)(cid >> (8 * i));
  }
}

static void set_failure(char *out, size_t cap, const uint8_t *text, size_t len) {
  len = len < cap - 1 ? len : cap - 1;
  memcpy(out, text, len);
  out[len] = '\0';
}

/* ---- Host ---------------------------------------------------------------------------- */

typedef struct host_tr {
  zb_transport iface;
  const zb_allocator *a;
  zb_transport *up;
  uint64_t (*clock_ns)(void *ctx);
  void *clock_ctx;
  uint64_t last_ping;
  bool opened, closed;
  char code[ZB_RELAY_CODE_LEN + 1];
  char failure[128];
  zb_array conns; /* uint32_t: relayed clients that are open */
  zb_array buf;
} host_tr;

static bool forget(zb_array *conns, uint32_t cid) {
  for (size_t i = 0; i < conns->len; ++i) {
    if (*(uint32_t *)zb_array_at(conns, i) == cid) {
      zb_array_remove_swap(conns, i);
      return true;
    }
  }
  return false;
}

static bool known(const zb_array *conns, uint32_t cid) {
  for (size_t i = 0; i < conns->len; ++i) {
    if (*(uint32_t *)zb_array_at(conns, i) == cid) {
      return true;
    }
  }
  return false;
}

static void h_update(void *ctx) {
  host_tr *h = ctx;
  h->up->update(h->up->ctx);
  if (h->clock_ns && h->code[0] && !h->closed) {
    uint64_t t = h->clock_ns(h->clock_ctx);
    if (t - h->last_ping >= KEEPALIVE_NS) {
      static const uint8_t ping = ZB_RELAY_PING;
      h->up->send(h->up->ctx, 1, &ping, 1);
      h->last_ping = t;
    }
  }
}

/* Translates the relay's messages into connection events. */
static bool h_next(void *ctx, zb_tr_event *ev) {
  host_tr *h = ctx;
  zb_tr_event u;
  while (!h->closed && h->up->next(h->up->ctx, &u)) {
    if (u.kind == ZB_TR_OPEN) {
      static const uint8_t reg[2] = {ZB_RELAY_REGISTER, ZB_RELAY_VERSION};
      h->opened = true;
      h->up->send(h->up->ctx, 1, reg, sizeof reg);
    } else if (u.kind == ZB_TR_CLOSE) {
      h->closed = true;
      if (!h->failure[0]) {
        snprintf(h->failure, sizeof h->failure, "%s",
                 h->opened ? "lost the connection to the relay" : "could not reach the relay");
      }
    } else if (u.len == 1 + ZB_RELAY_CODE_LEN && u.data[0] == ZB_RELAY_CODE) {
      memcpy(h->code, u.data + 1, ZB_RELAY_CODE_LEN);
      h->code[ZB_RELAY_CODE_LEN] = '\0';
      h->last_ping = h->clock_ns ? h->clock_ns(h->clock_ctx) : 0;
    } else if (u.len >= 1 && u.data[0] == ZB_RELAY_REFUSED) {
      set_failure(h->failure, sizeof h->failure, u.data + 1, u.len - 1);
    } else if (u.len >= 5) {
      uint32_t cid = get_cid(u.data + 1);
      ev->conn = cid;
      ev->data = NULL;
      ev->len = 0;
      if (u.data[0] == ZB_RELAY_OPEN && !known(&h->conns, cid) && zb_array_push(&h->conns, &cid)) {
        ev->kind = ZB_TR_OPEN;
        return true;
      }
      if (u.data[0] == ZB_RELAY_FROM && known(&h->conns, cid)) {
        ev->kind = ZB_TR_MESSAGE;
        ev->data = u.data + 5;
        ev->len = u.len - 5;
        return true;
      }
      if (u.data[0] == ZB_RELAY_CLOSED && forget(&h->conns, cid)) {
        ev->kind = ZB_TR_CLOSE;
        return true;
      }
    }
  }
  if (h->closed && h->conns.len) { /* the relay is gone: so are its clients */
    ev->kind = ZB_TR_CLOSE;
    ev->conn = *(uint32_t *)zb_array_at(&h->conns, h->conns.len - 1);
    ev->data = NULL;
    ev->len = 0;
    zb_array_pop(&h->conns);
    return true;
  }
  return false;
}

static zb_result h_send(void *ctx, uint32_t conn, const void *data, size_t len) {
  host_tr *h = ctx;
  if (h->closed || !known(&h->conns, conn)) {
    return ZB_ERR_STATE;
  }
  if (zb_array_reserve(&h->buf, len + 5) != ZB_OK) {
    return ZB_ERR_NOMEM;
  }
  h->buf.data[0] = ZB_RELAY_TO;
  put_cid(h->buf.data + 1, conn);
  if (len) {
    memcpy(h->buf.data + 5, data, len);
  }
  return h->up->send(h->up->ctx, 1, h->buf.data, len + 5);
}

static void h_close(void *ctx, uint32_t conn) {
  host_tr *h = ctx;
  if (!h->closed && forget(&h->conns, conn)) {
    uint8_t kick[5] = {ZB_RELAY_KICK};
    put_cid(kick + 1, conn);
    h->up->send(h->up->ctx, 1, kick, sizeof kick);
  }
}

static void h_destroy(void *ctx) {
  host_tr *h = ctx;
  zb_transport_destroy(h->up);
  zb_array_free(&h->conns);
  zb_array_free(&h->buf);
  zb_free(h->a, h);
}

static const char *h_code(void *ctx) { return ((host_tr *)ctx)->code; }
static const char *h_failure(void *ctx) { return ((host_tr *)ctx)->failure; }

zb_transport *zb_relay_host_transport(const zb_allocator *a, zb_transport *uplink, uint64_t (*clock_ns)(void *ctx),
                                      void *clock_ctx) {
  host_tr *h = uplink ? zb_calloc(a, sizeof *h) : NULL;
  if (!h) {
    zb_transport_destroy(uplink);
    return NULL;
  }
  h->a = a;
  h->up = uplink;
  h->clock_ns = clock_ns;
  h->clock_ctx = clock_ctx;
  zb_array_init(&h->conns, a, sizeof(uint32_t));
  zb_array_init(&h->buf, a, 1);
  h->iface.ctx = h;
  h->iface.name = "relay-host";
  h->iface.update = h_update;
  h->iface.next = h_next;
  h->iface.send = h_send;
  h->iface.close = h_close;
  h->iface.destroy = h_destroy;
  h->iface.invite_code = h_code;
  h->iface.failure = h_failure;
  return &h->iface;
}

/* ---- Client ---------------------------------------------------------------------------- */

typedef struct join_tr {
  zb_transport iface;
  const zb_allocator *a;
  zb_transport *up;
  char code[ZB_RELAY_CODE_LEN + 1];
  bool opened, joined, closed;
  char failure[128];
} join_tr;

static void j_update(void *ctx) {
  join_tr *j = ctx;
  j->up->update(j->up->ctx);
}

static bool j_next(void *ctx, zb_tr_event *ev) {
  join_tr *j = ctx;
  zb_tr_event u;
  while (!j->closed && j->up->next(j->up->ctx, &u)) {
    if (u.kind == ZB_TR_OPEN) {
      uint8_t msg[2 + ZB_RELAY_CODE_LEN] = {ZB_RELAY_JOIN, ZB_RELAY_VERSION};
      memcpy(msg + 2, j->code, ZB_RELAY_CODE_LEN);
      j->opened = true;
      j->up->send(j->up->ctx, 1, msg, sizeof msg);
      continue;
    }
    if (u.kind == ZB_TR_CLOSE) {
      j->closed = true;
      if (!j->joined && !j->failure[0]) {
        snprintf(j->failure, sizeof j->failure, "%s",
                 j->opened ? "the relay closed the connection" : "could not reach the relay");
      }
      *ev = u;
      return true;
    }
    if (j->joined) {
      *ev = u;
      return true;
    }
    if (u.len == 1 && u.data[0] == ZB_RELAY_JOINED) {
      j->joined = true;
      ev->kind = ZB_TR_OPEN;
      ev->conn = 1;
      ev->data = NULL;
      ev->len = 0;
      return true;
    }
    if (u.len >= 1 && u.data[0] == ZB_RELAY_REFUSED) {
      set_failure(j->failure, sizeof j->failure, u.data + 1, u.len - 1);
    }
  }
  return false;
}

static zb_result j_send(void *ctx, uint32_t conn, const void *data, size_t len) {
  join_tr *j = ctx;
  (void)conn;
  return j->joined && !j->closed ? j->up->send(j->up->ctx, 1, data, len) : ZB_ERR_STATE;
}

static void j_close(void *ctx, uint32_t conn) {
  join_tr *j = ctx;
  (void)conn;
  j->up->close(j->up->ctx, 1);
}

static void j_destroy(void *ctx) {
  join_tr *j = ctx;
  zb_transport_destroy(j->up);
  zb_free(j->a, j);
}

static const char *j_failure(void *ctx) { return ((join_tr *)ctx)->failure; }

zb_transport *zb_relay_join_transport(const zb_allocator *a, zb_transport *uplink, const char *code) {
  char normal[ZB_RELAY_CODE_LEN + 1];
  join_tr *j = uplink && zb_relay_normalize_code(code, normal) ? zb_calloc(a, sizeof *j) : NULL;
  if (!j) {
    zb_transport_destroy(uplink);
    return NULL;
  }
  j->a = a;
  j->up = uplink;
  memcpy(j->code, normal, sizeof normal);
  j->iface.ctx = j;
  j->iface.name = "relay-client";
  j->iface.update = j_update;
  j->iface.next = j_next;
  j->iface.send = j_send;
  j->iface.close = j_close;
  j->iface.destroy = j_destroy;
  j->iface.failure = j_failure;
  return &j->iface;
}

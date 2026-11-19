/* SPDX-License-Identifier: MIT */
/* Transports carry whole messages between connections. Implementations: an
 * in-process loopback network (tests, local play), a WebSocket server
 * (desktop hosts), and a WebSocket client (desktop: our own sockets; web: the
 * browser's WebSocket). All are polled from the main thread. */
#ifndef ZB_NET_TRANSPORT_H
#define ZB_NET_TRANSPORT_H

#include "core/alloc.h"

typedef enum zb_tr_event_kind { ZB_TR_OPEN, ZB_TR_MESSAGE, ZB_TR_CLOSE } zb_tr_event_kind;

typedef struct zb_tr_event {
  zb_tr_event_kind kind;
  uint32_t conn;       /* connection id (a client transport has one: 1) */
  const uint8_t *data; /* message bytes, valid until the next call of next() */
  size_t len;
} zb_tr_event;

typedef struct zb_transport {
  void *ctx;
  const char *name;
  /* Moves bytes; afterwards events can be read with next(). */
  void (*update)(void *ctx);
  bool (*next)(void *ctx, zb_tr_event *ev);
  zb_result (*send)(void *ctx, uint32_t conn, const void *data, size_t len);
  void (*close)(void *ctx, uint32_t conn);
  void (*destroy)(void *ctx);
  /* Optional (NULL when a transport has nothing to say): */
  const char *(*invite_code)(void *ctx); /* hosting through a relay: the invite code, "" until known */
  const char *(*failure)(void *ctx);     /* why the transport failed for good, "" while it works */
} zb_transport;

void zb_transport_destroy(zb_transport *t);

/* ---- Loopback: an in-process network with one listener ---- */
typedef struct zb_loopback zb_loopback;
zb_loopback *zb_loopback_create(const zb_allocator *a);
/* Destroy after every transport created from it. */
void zb_loopback_destroy(zb_loopback *lb);
/* The listening side (at most one per loopback network). */
zb_transport *zb_loopback_listen(zb_loopback *lb);
/* A client connection to the listener. Connections are numbered from 1 in the
 * order they are made; the listener sees that number as the conn id. */
zb_transport *zb_loopback_connect(zb_loopback *lb);

/* Impairment for tests: a loopback network that behaves like a slow,
 * unreliable internet path. WebSocket runs over TCP, so messages are never
 * lost or reordered; instead they are delayed (latency plus jitter, order
 * kept per direction) and whole connections can drop. Deterministic: the
 * jitter and the drops depend only on the seed and the clock readings. */
typedef struct zb_loopback_impairment {
  uint64_t latency_ns;     /* one-way delay of every event */
  uint64_t jitter_ns;      /* extra delay, uniformly 0..jitter_ns */
  double drops_per_second; /* chance that an open connection is cut, per second of clock time */
  uint64_t seed;
  uint64_t (*clock_ns)(void *ctx); /* required */
  void *clock_ctx;
} zb_loopback_impairment;
/* Applies (or with NULL removes) impairment; events already in flight keep their times. */
void zb_loopback_impair(zb_loopback *lb, const zb_loopback_impairment *imp);
/* Cuts connection conn as a failing network would: both ends see it close at
 * once and whatever was still in flight is lost. False if it is not open. */
bool zb_loopback_cut(zb_loopback *lb, uint32_t conn);
/* Connections cut so far (by zb_loopback_cut or by impairment). */
uint32_t zb_loopback_cuts(const zb_loopback *lb);

/* ---- WebSocket ---- */
/* Listens on port (0 = any; the port in use is stored in *bound_port).
 * Not available on the web. */
zb_transport *zb_ws_server_create(const zb_allocator *a, uint16_t port, uint16_t *bound_port, char *err,
                                  size_t err_len);
/* Connects to a ws:// URL. */
zb_transport *zb_ws_client_create(const zb_allocator *a, const char *url, char *err, size_t err_len);
/* Splits "ws://host:port/path" (port defaults to 80). */
bool zb_ws_parse_url(const char *url, char *host, size_t host_len, uint16_t *port, char *path, size_t path_len);

#endif

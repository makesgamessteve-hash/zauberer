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
} zb_transport;

void zb_transport_destroy(zb_transport *t);

/* ---- Loopback: an in-process network with one listener ---- */
typedef struct zb_loopback zb_loopback;
zb_loopback *zb_loopback_create(const zb_allocator *a);
/* Destroy after every transport created from it. */
void zb_loopback_destroy(zb_loopback *lb);
/* The listening side (at most one per loopback network). */
zb_transport *zb_loopback_listen(zb_loopback *lb);
/* A client connection to the listener. */
zb_transport *zb_loopback_connect(zb_loopback *lb);

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

/* SPDX-License-Identifier: MIT */
/* The WebSocket protocol (RFC 6455) as a state machine over byte buffers:
 * the opening handshake (server or client side), framing with masking,
 * fragmented messages, ping/pong and close. No sockets here (net/transport.c
 * moves the bytes), so it is fully unit-testable. */
#ifndef ZB_NET_WS_H
#define ZB_NET_WS_H

#include "core/array.h"

#define ZB_WS_MAX_MESSAGE (1u << 20)

typedef enum zb_ws_state { ZB_WS_HANDSHAKE, ZB_WS_OPEN, ZB_WS_CLOSED } zb_ws_state;

typedef struct zb_ws {
  const zb_allocator *a;
  bool server;
  zb_ws_state state;
  zb_array in;  /* received bytes not yet parsed */
  zb_array out; /* bytes waiting to be sent */
  zb_array msg; /* message being assembled */
  bool in_message;
  char accept[32]; /* client: the Sec-WebSocket-Accept expected */
  uint64_t mask_state;
  size_t max_message;
  char error[96];
} zb_ws;

/* Server side: waits for the client's HTTP upgrade request. */
void zb_ws_init_server(zb_ws *ws, const zb_allocator *a);
/* Client side: queues the upgrade request for host/path; seed varies the key and masks. */
zb_result zb_ws_init_client(zb_ws *ws, const zb_allocator *a, const char *host, const char *path, uint64_t seed);
void zb_ws_free(zb_ws *ws);

/* Appends received bytes. */
zb_result zb_ws_feed(zb_ws *ws, const void *data, size_t len);
/* Parses buffered bytes: returns 1 with a complete data message in *msg (valid
 * until the next call), 0 when more bytes are needed, -1 when the connection
 * is closed or broken (see ws->error). Answers pings and handshakes itself. */
int zb_ws_poll(zb_ws *ws, const uint8_t **msg, size_t *len);
/* Frames one binary message for sending. */
zb_result zb_ws_send(zb_ws *ws, const void *data, size_t len);
/* Starts the closing handshake. */
void zb_ws_close(zb_ws *ws, uint16_t code);
/* Bytes to write to the socket; drop them once written. */
const uint8_t *zb_ws_pending(const zb_ws *ws, size_t *len);
void zb_ws_sent(zb_ws *ws, size_t n);

#endif

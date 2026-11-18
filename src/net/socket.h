/* SPDX-License-Identifier: MIT */
/* Minimal non-blocking TCP sockets (POSIX and Winsock). Not available on the
 * web, where the browser's WebSocket is used instead (net/transport.h). */
#ifndef ZB_NET_SOCKET_H
#define ZB_NET_SOCKET_H

#include "core/base.h"

typedef intptr_t zb_socket;
#define ZB_SOCKET_NONE ((zb_socket) - 1)

/* Result of a send or receive: bytes moved, or one of these. */
#define ZB_SOCK_WOULD_BLOCK (-1)
#define ZB_SOCK_CLOSED (-2)

bool zb_sock_supported(void);
/* Listens on all interfaces (port 0 = any free port, reported in *bound_port). */
zb_socket zb_sock_listen(uint16_t port, uint16_t *bound_port);
/* Accepts a pending connection or returns ZB_SOCKET_NONE. */
zb_socket zb_sock_accept(zb_socket listener);
/* Starts connecting to a host name or address (resolves synchronously). */
zb_socket zb_sock_connect(const char *host, uint16_t port);
/* 1 connected, 0 still connecting, -1 failed. */
int zb_sock_poll_connect(zb_socket s);
long zb_sock_send(zb_socket s, const void *data, size_t len);
long zb_sock_recv(zb_socket s, void *buf, size_t cap);
void zb_sock_close(zb_socket s);

#endif

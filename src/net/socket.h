/* SPDX-License-Identifier: MIT */
/* Minimal non-blocking TCP and UDP sockets (POSIX and Winsock). Not
 * available on the web, where the browser's WebSocket is used instead
 * (net/transport.h): there every function here fails. */
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

/* ---- UDP (LAN discovery) ---- */
/* Opens a non-blocking UDP socket on all interfaces (port 0 = any free port,
 * reported in *bound_port) that may send broadcasts. The address is reusable,
 * so several games on one machine can listen on the same port. */
zb_socket zb_udp_open(uint16_t port, uint16_t *bound_port);
/* Sends one datagram to a dotted IPv4 address ("255.255.255.255" broadcasts). */
long zb_udp_send(zb_socket s, const char *address, uint16_t port, const void *data, size_t len);
/* Receives one datagram (ZB_SOCK_WOULD_BLOCK when none is waiting); the sender's
 * dotted address goes to from (at least 16 bytes) and its port to *from_port. */
long zb_udp_recv(zb_socket s, void *buf, size_t cap, char *from, size_t from_len, uint16_t *from_port);
/* Parses a dotted IPv4 address into four bytes. */
bool zb_ipv4_parse(const char *address, uint8_t out[4]);

/* ---- Time, for programs without the platform layer (the relay) ---- */
/* A monotonic clock in nanoseconds. */
uint64_t zb_sock_clock_ns(void);
void zb_sock_sleep_ms(unsigned ms);

#endif

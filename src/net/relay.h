/* SPDX-License-Identifier: MIT */
/* The relay (zauberer-relay): reaches co-op sessions through invite codes,
 * for hosts that cannot accept connections (browsers, home routers). It
 * forwards opaque messages between a host and its clients and has no game
 * logic; anyone can run one.
 *
 * Protocol (binary WebSocket messages, first byte = kind):
 *   host -> relay:   REGISTER(version)            relay -> host:   CODE(6 chars) or REFUSED(reason)
 *                    TO(conn u32, payload)                         OPEN(conn u32), FROM(conn u32, payload),
 *                    KICK(conn u32), PING                          CLOSED(conn u32)
 *   client -> relay: JOIN(version, 6 chars)       relay -> client: JOINED or REFUSED(reason), then
 *                                                                  only the host's payloads, unchanged
 * After JOINED every client message is a payload for the host. When the host
 * leaves, its clients are disconnected and the code expires. */
#ifndef ZB_NET_RELAY_H
#define ZB_NET_RELAY_H

#include <signal.h>
#include <stdio.h>

#include "net/transport.h"

#define ZB_RELAY_PORT 47821
#define ZB_RELAY_VERSION 1
#define ZB_RELAY_CODE_LEN 6
#define ZB_RELAY_MAX_CLIENTS 3
/* Invite codes use these characters only (no 0/O, 1/I/L). */
#define ZB_RELAY_ALPHABET "23456789ABCDEFGHJKMNPQRSTUVWXYZ"

enum {
  ZB_RELAY_REGISTER = 1,
  ZB_RELAY_JOIN,
  ZB_RELAY_TO,
  ZB_RELAY_KICK,
  ZB_RELAY_PING,
  ZB_RELAY_CODE = 0x81,
  ZB_RELAY_JOINED,
  ZB_RELAY_REFUSED,
  ZB_RELAY_OPEN,
  ZB_RELAY_FROM,
  ZB_RELAY_CLOSED
};

typedef struct zb_relay_config {
  const zb_allocator *a;
  uint64_t seed; /* invite codes come from a counter-based generator seeded here */
  uint64_t (*clock_ns)(void *ctx);
  void *clock_ctx;
  uint64_t idle_timeout_ns;      /* a session without any traffic ends; default 10 minutes */
  uint64_t handshake_timeout_ns; /* a connection must register or join within this; default 10 s */
  uint32_t max_sessions;         /* default 1024 */
} zb_relay_config;

typedef struct zb_relay_stats {
  uint32_t sessions, clients, pending; /* live now */
  uint64_t forwarded, bytes;           /* payloads forwarded */
  uint64_t refused;                    /* registrations and joins refused */
} zb_relay_stats;

typedef struct zb_relay zb_relay;

/* Serves on a listening transport (taken over; destroyed on failure). */
zb_relay *zb_relay_create(const zb_relay_config *cfg, zb_transport *listener);
void zb_relay_destroy(zb_relay *r);
/* Accepts, forwards and times out; call often. */
void zb_relay_update(zb_relay *r);
void zb_relay_get_stats(const zb_relay *r, zb_relay_stats *out);
/* Normalises an invite code typed by a person (case, spaces, dashes) into
 * out (7 bytes); false if it cannot be one. */
bool zb_relay_normalize_code(const char *code, char out[ZB_RELAY_CODE_LEN + 1]);

/* The program: zauberer-relay [--port N] [--seed N] [--idle SECONDS]
 * [--max-sessions N] [--max-seconds N] [--help]. Serves until *stop is set
 * (or --max-seconds passed); returns the exit code (2 for usage errors). */
int zb_relay_main(int argc, char **argv, FILE *out, volatile sig_atomic_t *stop);

/* ---- Endpoints ---- */
/* Hosting through a relay: wraps a client transport to the relay (taken
 * over). The result is a listening transport for zb_net_host whose
 * connections are the relayed clients; its invite_code() reports the code once
 * the relay assigned it and failure() why the relay refused or was lost.
 * With a clock it sends a keepalive every 20 s. */
zb_transport *zb_relay_host_transport(const zb_allocator *a, zb_transport *uplink, uint64_t (*clock_ns)(void *ctx),
                                      void *clock_ctx);
/* Joining through a relay: wraps a client transport to the relay (taken over)
 * and asks for the session with this code. It opens once the relay accepted
 * and then behaves as a direct connection; failure() tells why a join was refused. */
zb_transport *zb_relay_join_transport(const zb_allocator *a, zb_transport *uplink, const char *code);

#endif

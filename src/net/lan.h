/* SPDX-License-Identifier: MIT */
/* LAN discovery over UDP (desktop only; in the browser zb_lan_open fails with
 * "unavailable"). A host answers queries on a fixed port with a small
 * datagram (session name, protocol version, players present and the maximum,
 * the WebSocket port to join on); a scanner sends queries to the broadcast
 * address and to this machine and collects the answers. */
#ifndef ZB_NET_LAN_H
#define ZB_NET_LAN_H

#include "core/alloc.h"

#define ZB_LAN_PORT 47822
#define ZB_LAN_VERSION 1
#define ZB_LAN_MAX_NAME 47

typedef struct zb_lan_session {
  char name[ZB_LAN_MAX_NAME + 1];
  char address[48]; /* dotted IPv4 address the answer came from */
  uint16_t port;    /* WebSocket port: join at ws://address:port/ */
  uint16_t protocol;
  uint8_t players, max_players;
  uint64_t seen_ns; /* when the last answer arrived */
} zb_lan_session;

typedef struct zb_lan zb_lan;

/* Opens a discovery socket on port (0 = any free port, for scanners; the port
 * in use goes to *bound_port when not NULL). */
zb_lan *zb_lan_open(const zb_allocator *a, uint16_t port, uint16_t *bound_port, char *err, size_t err_len);
void zb_lan_close(zb_lan *l);
/* Host side: answers every waiting query with self (NULL: reads and ignores them). */
void zb_lan_answer(zb_lan *l, const zb_lan_session *self);
/* Scanner side: sends one query to address:port. */
zb_result zb_lan_query(zb_lan *l, const char *address, uint16_t port);
/* Scanner side: reads waiting answers (one entry per address and port, the
 * latest answer wins), forgets entries older than max_age_ns and returns the count. */
size_t zb_lan_collect(zb_lan *l, uint64_t now_ns, uint64_t max_age_ns);
const zb_lan_session *zb_lan_found(const zb_lan *l, size_t i);

/* The datagrams, exposed for tests. A query is 6 bytes; an answer at most 64. */
size_t zb_lan_encode_answer(const zb_lan_session *s, uint8_t *out, size_t cap);
bool zb_lan_decode_answer(const uint8_t *data, size_t len, zb_lan_session *out);

#endif

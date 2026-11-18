/* SPDX-License-Identifier: MIT */
/* A co-op session over a transport: the host (player 0) accepts up to three
 * clients, checks that they generate the same world, assigns player indices,
 * relays messages and keeps connections alive; clients join, learn their
 * player index and exchange messages on named channels. Host-authoritative:
 * what travels on the channels is up to the game. */
#ifndef ZB_NET_SESSION_H
#define ZB_NET_SESSION_H

#include "net/transport.h"

#define ZB_NET_MAX_PLAYERS 4
#define ZB_NET_PROTOCOL 1
#define ZB_NET_TO_ALL 255
#define ZB_NET_MAX_CHANNEL 32

typedef enum zb_net_role { ZB_NET_NONE, ZB_NET_HOST, ZB_NET_JOINING, ZB_NET_CLIENT } zb_net_role;

typedef enum zb_net_event_kind {
  ZB_NET_EV_JOINED,  /* a player joined (on clients: also the players already there) */
  ZB_NET_EV_LEFT,    /* a player left */
  ZB_NET_EV_DATA,    /* a message on a channel */
  ZB_NET_EV_WELCOME, /* client: accepted as `player` */
  ZB_NET_EV_ENDED    /* client: the session ended or was refused (see zb_net_error) */
} zb_net_event_kind;

typedef struct zb_net_event {
  zb_net_event_kind kind;
  int player; /* joined/left/welcome: the player; data: the sender */
  char channel[ZB_NET_MAX_CHANNEL + 1];
  const uint8_t *data;
  size_t len;
} zb_net_event;

typedef struct zb_net_config {
  const zb_allocator *a;
  uint64_t world_seed;   /* must match between host and clients */
  uint64_t package_hash; /* the enabled package set; must match too */
  uint64_t (*clock_ns)(void *ctx);
  void *clock_ctx;
  uint64_t ping_interval_ns; /* default 1 s */
  uint64_t timeout_ns;       /* default 10 s without any message */
} zb_net_config;

typedef struct zb_net_stats {
  uint64_t bytes_sent, bytes_received, messages_sent, messages_received;
  uint32_t rtt_ms[ZB_NET_MAX_PLAYERS]; /* last measured round trip per player */
} zb_net_stats;

typedef struct zb_net zb_net;

zb_net *zb_net_create(const zb_net_config *cfg);
void zb_net_destroy(zb_net *n);
/* Starts hosting on a listening transport (the session takes ownership). */
zb_result zb_net_host(zb_net *n, zb_transport *listener);
/* Joins through a client transport (the session takes ownership). */
zb_result zb_net_join(zb_net *n, zb_transport *client);
/* Leaves the session (a host ends it for everyone). */
void zb_net_leave(zb_net *n);
/* Moves messages and handles the protocol; call once per tick. */
void zb_net_update(zb_net *n);
/* The next event (data valid until the next call). */
bool zb_net_next(zb_net *n, zb_net_event *ev);
/* Sends on a channel to a player or ZB_NET_TO_ALL (clients reach others through the host). */
zb_result zb_net_send(zb_net *n, int to, const char *channel, const void *data, size_t len);

zb_net_role zb_net_role_of(const zb_net *n);
int zb_net_player(const zb_net *n);
/* Bit i set when player i is in the session. */
unsigned zb_net_players(const zb_net *n);
const char *zb_net_error(const zb_net *n);
void zb_net_get_stats(const zb_net *n, zb_net_stats *out);

#endif

/* SPDX-License-Identifier: MIT */
/* A co-op session over a transport: the host (player 0) accepts up to three
 * clients, checks that they generate the same world, assigns player indices,
 * relays messages and keeps connections alive; clients join, learn their
 * player index and exchange messages on named channels. Host-authoritative:
 * what travels on the channels is up to the game.
 *
 * Also here: package-set checks that list the differences, reconnecting
 * within a grace period (resume tokens), a bandwidth budget per connection
 * with "latest-only" channels, large blobs (the world bundle) sent in
 * flow-controlled chunks, and answering LAN discovery queries. */
#ifndef ZB_NET_SESSION_H
#define ZB_NET_SESSION_H

#include "net/transport.h"

#define ZB_NET_MAX_PLAYERS 4
#define ZB_NET_PROTOCOL 2
#define ZB_NET_TO_ALL 255
#define ZB_NET_MAX_CHANNEL 32
#define ZB_NET_MAX_MISMATCHES 32
#define ZB_NET_HASH_HEX 40 /* blob content hashes: SHA-1 as lowercase hex */

typedef enum zb_net_role { ZB_NET_NONE, ZB_NET_HOST, ZB_NET_JOINING, ZB_NET_CLIENT } zb_net_role;

typedef enum zb_net_event_kind {
  ZB_NET_EV_JOINED,        /* a player joined (on clients: also the players already there) */
  ZB_NET_EV_LEFT,          /* a player left (or did not come back within the grace period) */
  ZB_NET_EV_DATA,          /* a message on a channel */
  ZB_NET_EV_WELCOME,       /* client: accepted as `player` */
  ZB_NET_EV_ENDED,         /* the session ended or was refused (see zb_net_error, zb_net_mismatches) */
  ZB_NET_EV_AWAY,          /* a player's connection was lost; their place is kept for the grace period */
  ZB_NET_EV_BACK,          /* that player reconnected */
  ZB_NET_EV_BLOB_PROGRESS, /* a blob is arriving from `player`: done of total bytes */
  ZB_NET_EV_BLOB,          /* a blob arrived complete and matched its hash: data/len, hash */
  ZB_NET_EV_BLOB_FAILED,   /* a blob to or from `player` failed; data/len is the reason */
  ZB_NET_EV_BLOB_SENT      /* `player` received an outgoing blob completely */
} zb_net_event_kind;

typedef struct zb_net_event {
  zb_net_event_kind kind;
  int player; /* joined/left/welcome/away/back: the player; data: the sender; blobs: the other side */
  char channel[ZB_NET_MAX_CHANNEL + 1]; /* data: the channel; blob events: the blob's name */
  const uint8_t *data;
  size_t len;
  uint64_t done, total;           /* blob events: bytes */
  char hash[ZB_NET_HASH_HEX + 1]; /* blob events: the content hash */
} zb_net_event;

typedef struct zb_net_config {
  const zb_allocator *a;
  uint64_t world_seed;   /* must match between host and clients */
  uint64_t package_hash; /* the enabled package set; must match too */
  uint64_t (*clock_ns)(void *ctx);
  void *clock_ctx;
  uint64_t ping_interval_ns; /* default 1 s */
  uint64_t timeout_ns;       /* default 10 s without any message */
  uint64_t resume_grace_ns;  /* how long a lost player's place is kept; default 30 s */
  uint32_t budget_kbps;      /* average budget per connection; default 64 kbit/s */
  uint32_t burst_kbps;       /* burst allowance: one second at this rate; default 256 kbit/s */
  uint32_t blob_window;      /* unacknowledged blob bytes per receiver; default 256 KiB */
  uint64_t max_blob;         /* largest blob accepted; default 128 MiB */
} zb_net_config;

typedef struct zb_net_stats {
  uint64_t bytes_sent, bytes_received, messages_sent, messages_received;
  uint32_t rtt_ms[ZB_NET_MAX_PLAYERS];        /* last measured round trip per player */
  uint64_t sent_to[ZB_NET_MAX_PLAYERS];       /* bytes sent to each player (on clients: [0] = the host) */
  uint64_t received_from[ZB_NET_MAX_PLAYERS]; /* bytes received from each player's connection */
  uint32_t kbps_out[ZB_NET_MAX_PLAYERS];      /* kbit/s to each player, averaged over the last second */
  uint32_t kbps_in[ZB_NET_MAX_PLAYERS];       /* kbit/s from each player, averaged over the last second */
  uint64_t coalesced;                         /* latest-only messages replaced by newer ones before sending */
} zb_net_stats;

/* One enabled package, for the package-set check. */
typedef struct zb_net_package {
  const char *id;
  const char *version;
} zb_net_package;

/* A difference between the host's and a client's package sets ("" = not enabled there). */
typedef struct zb_net_mismatch {
  char id[64];
  char host_version[32];
  char client_version[32];
} zb_net_mismatch;

/* Reliable channels deliver every message. On a latest-only channel only the
 * newest unsent message per channel and sender is kept while a connection is
 * over its bandwidth budget; older ones are replaced (snapshots, positions). */
typedef enum zb_net_channel_class { ZB_NET_RELIABLE, ZB_NET_LATEST } zb_net_channel_class;

typedef struct zb_net zb_net;

zb_net *zb_net_create(const zb_net_config *cfg);
void zb_net_destroy(zb_net *n);
/* Starts hosting on a listening transport (the session takes ownership). */
zb_result zb_net_host(zb_net *n, zb_transport *listener);
/* Joins through a client transport (the session takes ownership). */
zb_result zb_net_join(zb_net *n, zb_transport *client);
/* After a lost connection (zb_net_can_resume): joins again through a new
 * client transport with the resume token from the welcome, so the host gives
 * back the same player index if it is still within the grace period. Takes
 * ownership of the transport even when it fails (ZB_ERR_STATE: nothing to resume). */
zb_result zb_net_resume(zb_net *n, zb_transport *client);
/* True after a client lost its connection (not when it left or was refused or the host ended the session). */
bool zb_net_can_resume(const zb_net *n);
/* Leaves the session (a host ends it for everyone). */
void zb_net_leave(zb_net *n);
/* Moves messages and handles the protocol; call once per tick. */
void zb_net_update(zb_net *n);
/* The next event (data valid until the next call). */
bool zb_net_next(zb_net *n, zb_net_event *ev);
/* Sends on a channel to a player or ZB_NET_TO_ALL (clients reach others through the host). */
zb_result zb_net_send(zb_net *n, int to, const char *channel, const void *data, size_t len);

/* The enabled packages in load order (copied), sent with every join so that a
 * refused join lists the differences. */
zb_result zb_net_set_packages(zb_net *n, const zb_net_package *packages, size_t count);
/* Why the last join was refused, package by package (empty unless the package sets differed). */
size_t zb_net_mismatches(const zb_net *n, const zb_net_mismatch **out);

/* Sets a channel's class (channels are reliable unless set; the same on every peer). */
zb_result zb_net_set_channel_class(zb_net *n, const char *channel, zb_net_channel_class cls);
/* Changes the per-connection budget (0 keeps the current value). Reliable
 * messages are always sent and count against it; latest-only messages wait. */
void zb_net_set_budget(zb_net *n, uint32_t kbps, uint32_t burst_kbps);

/* Sends a blob (copied) to one player: clients can send to the host only, the
 * host to any client. Chunks are paced by acknowledgements (a window of
 * unacknowledged bytes) so other channels keep flowing; the receiver gets
 * progress events and the blob once its SHA-1 matches. Blobs to one player go
 * one after another. Blob traffic is counted in the stats but not against the budget. */
zb_result zb_net_send_blob(zb_net *n, int to, const char *name, const void *data, size_t len);
/* The content hash used for blobs (SHA-1, lowercase hex). */
void zb_net_blob_hash(const void *data, size_t len, char out[ZB_NET_HASH_HEX + 1]);

/* Hosting through a relay: the session's invite code ("" when there is none yet). */
const char *zb_net_invite_code(const zb_net *n);

/* Answers LAN discovery queries (net/lan.h) on discovery_port (0 = ZB_LAN_PORT)
 * while hosting, with this name and the WebSocket port clients join on. */
zb_result zb_net_lan_announce(zb_net *n, const char *name, uint16_t ws_port, uint16_t discovery_port, char *err,
                              size_t err_len);
void zb_net_lan_stop(zb_net *n);
bool zb_net_lan_announcing(const zb_net *n);

zb_net_role zb_net_role_of(const zb_net *n);
int zb_net_player(const zb_net *n);
/* Bit i set when player i is in the session (players who are away included). */
unsigned zb_net_players(const zb_net *n);
/* Bit i set when player i lost their connection and may still come back. */
unsigned zb_net_away(const zb_net *n);
const char *zb_net_error(const zb_net *n);
void zb_net_get_stats(const zb_net *n, zb_net_stats *out);
/* The session's clock (the configured one). */
uint64_t zb_net_now(const zb_net *n);

#endif

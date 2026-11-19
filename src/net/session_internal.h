/* SPDX-License-Identifier: MIT */
/* Internals shared by session.c and session_blob.c. */
#ifndef ZB_NET_SESSION_INTERNAL_H
#define ZB_NET_SESSION_INTERNAL_H

#include "core/array.h"
#include "net/lan.h"
#include "net/session.h"

enum {
  MSG_HELLO = 1,
  MSG_WELCOME,
  MSG_REJECT,
  MSG_DATA,
  MSG_JOINED,
  MSG_LEFT,
  MSG_PING,
  MSG_PONG,
  MSG_BYE,  /* leaving on purpose (either side) */
  MSG_AWAY, /* host: a player's connection was lost */
  MSG_BACK, /* host: that player reconnected */
  MSG_BLOB_START,
  MSG_BLOB_CHUNK,
  MSG_BLOB_ACK,
  MSG_BLOB_ABORT
};

#define ZB_NET_MAX_PEERS 8
#define ZB_NET_MAX_CLASSES 32
#define ZB_NET_BLOB_CHUNK 16384

typedef struct peer {
  bool used;
  bool welcomed;
  uint32_t conn;
  int player;
  uint64_t last_seen;
  int64_t tokens; /* bandwidth budget in bytes (may go negative after reliable messages) */
} peer;

/* Host: what is kept for each player, also while they are away. */
typedef struct slot {
  uint64_t token; /* resume token handed out in the welcome */
  uint64_t away_until;
} slot;

/* A latest-only message waiting for budget. */
typedef struct pending_msg {
  uint32_t conn;
  int from;
  char channel[ZB_NET_MAX_CHANNEL + 1];
  uint8_t *data; /* the framed message */
  size_t len;
} pending_msg;

typedef struct stored_event {
  zb_net_event ev;
  uint8_t *data;
} stored_event;

typedef struct out_blob {
  int to;
  uint32_t id;
  char name[ZB_NET_MAX_CHANNEL + 1];
  uint8_t *data;
  size_t len, sent, acked;
  bool started;
  uint8_t hash[20];
} out_blob;

typedef struct in_blob {
  uint32_t id;
  char name[ZB_NET_MAX_CHANNEL + 1];
  uint8_t *data;
  size_t total, received;
  uint8_t hash[20];
  bool dirty; /* progress to report */
} in_blob;

struct zb_net {
  zb_net_config cfg;
  zb_net_role role;
  zb_transport *t;
  int player;
  unsigned players, away;
  peer peers[ZB_NET_MAX_PEERS]; /* host: connections (welcomed or not); client: [0] = the host */
  slot slots[ZB_NET_MAX_PLAYERS];
  uint64_t token; /* client: resume token from the welcome */
  bool resumable; /* client: the connection was lost and may be resumed */
  uint64_t tokens_made;
  zb_array events; /* stored_event */
  size_t head;
  uint8_t *current;
  zb_array buf;
  uint64_t last_ping;
  char error[128];
  zb_net_stats stats;
  /* Package set */
  zb_array packages; /* encoded: per package id length, id, version length, version */
  uint16_t package_count;
  zb_net_mismatch mismatches[ZB_NET_MAX_MISMATCHES];
  size_t mismatch_count;
  /* Budget */
  struct {
    char name[ZB_NET_MAX_CHANNEL + 1];
    zb_net_channel_class cls;
  } classes[ZB_NET_MAX_CLASSES];
  size_t class_count;
  zb_array pending; /* pending_msg */
  uint64_t last_refill;
  uint64_t window_start;
  uint64_t window_out[ZB_NET_MAX_PLAYERS], window_in[ZB_NET_MAX_PLAYERS];
  /* Blobs */
  zb_array out_blobs; /* out_blob * */
  in_blob *in_blobs[ZB_NET_MAX_PLAYERS];
  uint32_t next_blob;
  /* LAN */
  zb_lan *lan;
  char lan_name[ZB_LAN_MAX_NAME + 1];
  uint16_t lan_port;
};

void zb_net_put_u32(uint8_t *p, uint32_t v);
void zb_net_put_u64(uint8_t *p, uint64_t v);
uint32_t zb_net_get_u32(const uint8_t *p);
uint64_t zb_net_get_u64(const uint8_t *p);
/* Queues an event; with own, data is taken over (freed with the event) instead of copied. */
stored_event *zb_net_push(zb_net *n, zb_net_event_kind kind, int player, const char *name, const void *data, size_t len,
                          bool own);
/* The welcomed connection of a player (client: [0] for the host), or NULL. */
peer *zb_net_peer_of(zb_net *n, int player);
/* Sends a message on a connection; charge counts it against the budget. */
zb_result zb_net_send_raw(zb_net *n, peer *p, const void *data, size_t len, bool charge);

/* session_blob.c */
void zb_net_blob_update(zb_net *n);
void zb_net_blob_message(zb_net *n, peer *p, const uint8_t *msg, size_t len);
/* Fails the blobs to and from a player who is gone. */
void zb_net_blob_drop_player(zb_net *n, int player, const char *why);
/* Frees every blob without events. */
void zb_net_blob_release(zb_net *n);

#endif

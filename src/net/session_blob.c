/* SPDX-License-Identifier: MIT */
/* Blob transfer (the world bundle): a start message with the size, SHA-1 and
 * name, then chunks in order; the receiver acknowledges each chunk and the
 * sender keeps at most a window of unacknowledged bytes in flight, so
 * messages on other channels never wait behind more than one window. */
#include <stdio.h>
#include <string.h>

#include "core/sha1.h"
#include "net/session_internal.h"

#define START_HEAD 34 /* kind, id u32, total u64, SHA-1 (20), name length; then the name */
#define CHUNK_HEAD 13 /* kind, id u32, offset u64; then the bytes */
#define ACK_LEN 13    /* kind, id u32, bytes received u64 */
#define ABORT_LEN 5   /* kind, id u32 */

static void to_hex(const uint8_t d[20], char *out) {
  static const char digits[] = "0123456789abcdef";
  for (int i = 0; i < 20; ++i) {
    out[2 * i] = digits[d[i] >> 4];
    out[2 * i + 1] = digits[d[i] & 15];
  }
  out[ZB_NET_HASH_HEX] = '\0';
}

static void blob_digest(const void *data, size_t len, uint8_t out[20]) {
  zb_sha1(len ? data : "", len, out); /* never a null pointer, even for nothing */
}

void zb_net_blob_hash(const void *data, size_t len, char out[ZB_NET_HASH_HEX + 1]) {
  uint8_t d[20];
  blob_digest(data, len, d);
  to_hex(d, out);
}

static stored_event *blob_event(zb_net *n, zb_net_event_kind kind, int player, const char *name, const uint8_t hash[20],
                                uint64_t done, uint64_t total) {
  stored_event *se = zb_net_push(n, kind, player, name, NULL, 0, false);
  if (se) {
    to_hex(hash, se->ev.hash);
    se->ev.done = done;
    se->ev.total = total;
  }
  return se;
}

static void fail_event(zb_net *n, int player, const char *name, const uint8_t hash[20], const char *why) {
  stored_event *se = zb_net_push(n, ZB_NET_EV_BLOB_FAILED, player, name, why, strlen(why), false);
  if (se) {
    to_hex(hash, se->ev.hash);
  }
}

static void free_out(zb_net *n, size_t i) {
  out_blob *b = *(out_blob **)zb_array_at(&n->out_blobs, i);
  zb_free(n->cfg.a, b->data);
  zb_free(n->cfg.a, b);
  zb_array_remove(&n->out_blobs, i);
}

static void free_in(zb_net *n, int player) {
  in_blob *b = n->in_blobs[player];
  if (b) {
    zb_free(n->cfg.a, b->data);
    zb_free(n->cfg.a, b);
    n->in_blobs[player] = NULL;
  }
}

zb_result zb_net_send_blob(zb_net *n, int to, const char *name, const void *data, size_t len) {
  size_t nl = strlen(name);
  if (n->role != ZB_NET_HOST && n->role != ZB_NET_CLIENT) {
    return ZB_ERR_STATE;
  }
  if (nl == 0 || nl > ZB_NET_MAX_CHANNEL || to < 0 || to >= ZB_NET_MAX_PLAYERS || to == n->player) {
    return ZB_ERR_INVALID;
  }
  if (!zb_net_peer_of(n, to)) {
    return ZB_ERR_NOT_FOUND;
  }
  out_blob *b = zb_calloc(n->cfg.a, sizeof *b);
  if (!b) {
    return ZB_ERR_NOMEM;
  }
  b->data = len ? zb_alloc(n->cfg.a, len) : NULL;
  if ((len && !b->data) || !zb_array_push(&n->out_blobs, &b)) {
    zb_free(n->cfg.a, b->data);
    zb_free(n->cfg.a, b);
    return ZB_ERR_NOMEM;
  }
  if (len) {
    memcpy(b->data, data, len);
  }
  b->to = to;
  b->id = ++n->next_blob;
  b->len = len;
  memcpy(b->name, name, nl + 1);
  blob_digest(data, len, b->hash);
  return ZB_OK;
}

static void send_id(zb_net *n, peer *p, uint8_t kind, uint32_t id, const uint64_t *value) {
  uint8_t m[ACK_LEN];
  m[0] = kind;
  zb_net_put_u32(m + 1, id);
  if (value) {
    zb_net_put_u64(m + 5, *value);
  }
  zb_net_send_raw(n, p, m, value ? ACK_LEN : ABORT_LEN, false);
}

void zb_net_blob_update(zb_net *n) {
  unsigned busy = 0; /* one blob at a time per receiver */
  for (size_t i = 0; i < n->out_blobs.len; ++i) {
    out_blob *b = *(out_blob **)zb_array_at(&n->out_blobs, i);
    peer *p = zb_net_peer_of(n, b->to);
    if ((busy & (1u << b->to)) || !p) {
      continue;
    }
    busy |= 1u << b->to;
    if (!b->started) {
      size_t nl = strlen(b->name);
      if (zb_array_reserve(&n->buf, START_HEAD + nl) != ZB_OK) {
        continue;
      }
      uint8_t *m = n->buf.data;
      m[0] = MSG_BLOB_START;
      zb_net_put_u32(m + 1, b->id);
      zb_net_put_u64(m + 5, b->len);
      memcpy(m + 13, b->hash, 20);
      m[33] = (uint8_t)nl;
      memcpy(m + START_HEAD, b->name, nl);
      if (zb_net_send_raw(n, p, m, START_HEAD + nl, false) != ZB_OK) {
        continue;
      }
      b->started = true;
    }
    while (b->sent < b->len && b->sent - b->acked < n->cfg.blob_window) {
      size_t c = b->len - b->sent < ZB_NET_BLOB_CHUNK ? b->len - b->sent : ZB_NET_BLOB_CHUNK;
      if (zb_array_reserve(&n->buf, CHUNK_HEAD + c) != ZB_OK) {
        break;
      }
      uint8_t *m = n->buf.data;
      m[0] = MSG_BLOB_CHUNK;
      zb_net_put_u32(m + 1, b->id);
      zb_net_put_u64(m + 5, b->sent);
      memcpy(m + CHUNK_HEAD, b->data + b->sent, c);
      if (zb_net_send_raw(n, p, m, CHUNK_HEAD + c, false) != ZB_OK) {
        break;
      }
      b->sent += c;
    }
  }
  for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
    in_blob *b = n->in_blobs[i];
    if (b && b->dirty) {
      b->dirty = false;
      blob_event(n, ZB_NET_EV_BLOB_PROGRESS, i, b->name, b->hash, b->received, b->total);
    }
  }
}

/* The last byte arrived: hand the blob over if its hash matches. */
static void complete(zb_net *n, int from) {
  in_blob *b = n->in_blobs[from];
  blob_event(n, ZB_NET_EV_BLOB_PROGRESS, from, b->name, b->hash, b->total, b->total);
  uint8_t digest[20];
  blob_digest(b->data, b->total, digest);
  if (memcmp(digest, b->hash, 20) != 0) {
    fail_event(n, from, b->name, b->hash, "the blob's content hash does not match");
    free_in(n, from);
    return;
  }
  stored_event *se = zb_net_push(n, ZB_NET_EV_BLOB, from, b->name, b->data, b->total, true);
  b->data = NULL; /* the event owns it now (or freed it) */
  if (se) {
    to_hex(b->hash, se->ev.hash);
    se->ev.done = se->ev.total = b->total;
  }
  free_in(n, from);
}

/* Refuses an incoming blob: tells the sender and reports it here. */
static void refuse(zb_net *n, peer *p, int from, uint32_t id, const char *name, const uint8_t hash[20],
                   const char *why) {
  send_id(n, p, MSG_BLOB_ABORT, id, NULL);
  fail_event(n, from, name, hash, why);
  free_in(n, from);
}

static void blob_start(zb_net *n, peer *p, int from, const uint8_t *msg, size_t len) {
  if (len < START_HEAD || msg[33] == 0 || msg[33] > ZB_NET_MAX_CHANNEL || len != (size_t)START_HEAD + msg[33]) {
    return;
  }
  char name[ZB_NET_MAX_CHANNEL + 1];
  memcpy(name, msg + START_HEAD, msg[33]);
  name[msg[33]] = '\0';
  uint32_t id = zb_net_get_u32(msg + 1);
  uint64_t total = zb_net_get_u64(msg + 5);
  free_in(n, from); /* a newer blob replaces an unfinished one */
  if (total > n->cfg.max_blob || (uint64_t)(size_t)total != total) {
    refuse(n, p, from, id, name, msg + 13, "the blob is too large");
    return;
  }
  in_blob *b = zb_calloc(n->cfg.a, sizeof *b);
  uint8_t *data = b && total ? zb_alloc(n->cfg.a, (size_t)total) : NULL;
  if (!b || (total && !data)) {
    zb_free(n->cfg.a, b);
    refuse(n, p, from, id, name, msg + 13, "out of memory");
    return;
  }
  b->id = id;
  b->data = data;
  b->total = (size_t)total;
  memcpy(b->name, name, sizeof name);
  memcpy(b->hash, msg + 13, 20);
  b->dirty = true;
  n->in_blobs[from] = b;
  if (total == 0) {
    uint64_t zero = 0;
    send_id(n, p, MSG_BLOB_ACK, id, &zero);
    complete(n, from);
  }
}

static void blob_chunk(zb_net *n, peer *p, int from, const uint8_t *msg, size_t len) {
  in_blob *b = n->in_blobs[from];
  if (len < CHUNK_HEAD || !b || zb_net_get_u32(msg + 1) != b->id) {
    return; /* a chunk of a blob that was refused or replaced */
  }
  size_t c = len - CHUNK_HEAD;
  if (zb_net_get_u64(msg + 5) != b->received || c > b->total - b->received) {
    refuse(n, p, from, b->id, b->name, b->hash, "malformed blob");
    return;
  }
  memcpy(b->data + b->received, msg + CHUNK_HEAD, c);
  b->received += c;
  b->dirty = true;
  uint64_t got = b->received;
  send_id(n, p, MSG_BLOB_ACK, b->id, &got);
  if (b->received == b->total) {
    complete(n, from);
  }
}

static out_blob *find_out(zb_net *n, int to, uint32_t id, size_t *index) {
  for (size_t i = 0; i < n->out_blobs.len; ++i) {
    out_blob *b = *(out_blob **)zb_array_at(&n->out_blobs, i);
    if (b->to == to && b->id == id) {
      *index = i;
      return b;
    }
  }
  return NULL;
}

void zb_net_blob_message(zb_net *n, peer *p, const uint8_t *msg, size_t len) {
  int from = p->player;
  size_t i;
  if (msg[0] == MSG_BLOB_START) {
    blob_start(n, p, from, msg, len);
  } else if (msg[0] == MSG_BLOB_CHUNK) {
    blob_chunk(n, p, from, msg, len);
  } else if (msg[0] == MSG_BLOB_ACK && len == ACK_LEN) {
    out_blob *b = find_out(n, from, zb_net_get_u32(msg + 1), &i);
    uint64_t acked = zb_net_get_u64(msg + 5);
    if (b && b->started && acked <= b->sent) {
      b->acked = acked > b->acked ? (size_t)acked : b->acked;
      if (b->acked == b->len) {
        blob_event(n, ZB_NET_EV_BLOB_SENT, from, b->name, b->hash, b->len, b->len);
        free_out(n, i);
      }
    }
  } else if (msg[0] == MSG_BLOB_ABORT && len == ABORT_LEN) {
    uint32_t id = zb_net_get_u32(msg + 1);
    out_blob *b = find_out(n, from, id, &i);
    if (b) {
      fail_event(n, from, b->name, b->hash, "the receiver refused the blob");
      free_out(n, i);
    }
  }
}

void zb_net_blob_drop_player(zb_net *n, int player, const char *why) {
  for (size_t i = 0; i < n->out_blobs.len;) {
    out_blob *b = *(out_blob **)zb_array_at(&n->out_blobs, i);
    if (b->to == player) {
      fail_event(n, player, b->name, b->hash, why);
      free_out(n, i);
    } else {
      ++i;
    }
  }
  in_blob *b = n->in_blobs[player];
  if (b) {
    fail_event(n, player, b->name, b->hash, why);
    free_in(n, player);
  }
}

void zb_net_blob_release(zb_net *n) {
  while (n->out_blobs.len) {
    free_out(n, n->out_blobs.len - 1);
  }
  for (int i = 0; i < ZB_NET_MAX_PLAYERS; ++i) {
    free_in(n, i);
  }
}

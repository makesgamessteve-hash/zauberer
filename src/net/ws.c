/* SPDX-License-Identifier: MIT */
#include "net/ws.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "core/hash.h"
#include "core/sha1.h"

#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

enum { OP_CONT = 0, OP_TEXT = 1, OP_BINARY = 2, OP_CLOSE = 8, OP_PING = 9, OP_PONG = 10 };

static void init(zb_ws *ws, const zb_allocator *a, bool server) {
  memset(ws, 0, sizeof *ws);
  ws->a = a;
  ws->server = server;
  ws->state = ZB_WS_HANDSHAKE;
  ws->max_message = ZB_WS_MAX_MESSAGE;
  zb_array_init(&ws->in, a, 1);
  zb_array_init(&ws->out, a, 1);
  zb_array_init(&ws->msg, a, 1);
}

static zb_result append(zb_array *arr, const void *data, size_t len) {
  if (len == 0) {
    return ZB_OK;
  }
  if (zb_array_reserve(arr, arr->len + len) != ZB_OK) {
    return ZB_ERR_NOMEM;
  }
  memcpy(arr->data + arr->len, data, len);
  arr->len += len;
  return ZB_OK;
}

static void drop_front(zb_array *arr, size_t n) {
  memmove(arr->data, arr->data + n, arr->len - n);
  arr->len -= n;
}

static void accept_key(const char *key, char out[32]) {
  /* SHA-1 of the key followed by the protocol's GUID (keys longer than 90 bytes are cut). */
  char buf[128];
  size_t klen = strlen(key);
  size_t glen = sizeof GUID - 1;
  if (klen > sizeof buf - glen) {
    klen = sizeof buf - glen;
  }
  memcpy(buf, key, klen);
  memcpy(buf + klen, GUID, glen);
  uint8_t digest[20];
  zb_sha1(buf, klen + glen, digest);
  zb_base64(digest, sizeof digest, out);
}

void zb_ws_init_server(zb_ws *ws, const zb_allocator *a) { init(ws, a, true); }

zb_result zb_ws_init_client(zb_ws *ws, const zb_allocator *a, const char *host, const char *path, uint64_t seed) {
  init(ws, a, false);
  ws->mask_state = zb_mix64(seed ^ 0x77736b6579ull);
  uint8_t raw[16];
  for (int i = 0; i < 16; ++i) {
    ws->mask_state = zb_mix64(ws->mask_state + 0x9e3779b97f4a7c15ull);
    raw[i] = (uint8_t)ws->mask_state;
  }
  char key[32];
  zb_base64(raw, sizeof raw, key);
  accept_key(key, ws->accept);
  char req[512];
  int n = snprintf(req, sizeof req,
                   "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                   "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",
                   path && path[0] ? path : "/", host, key);
  if (n <= 0 || (size_t)n >= sizeof req) {
    return ZB_ERR_INVALID;
  }
  return append(&ws->out, req, (size_t)n);
}

void zb_ws_free(zb_ws *ws) {
  if (!ws->in.a) {
    return; /* never initialised */
  }
  zb_array_free(&ws->in);
  zb_array_free(&ws->out);
  zb_array_free(&ws->msg);
}

zb_result zb_ws_feed(zb_ws *ws, const void *data, size_t len) { return append(&ws->in, data, len); }

const uint8_t *zb_ws_pending(const zb_ws *ws, size_t *len) {
  *len = ws->out.len;
  return ws->out.data;
}

void zb_ws_sent(zb_ws *ws, size_t n) { drop_front(&ws->out, n < ws->out.len ? n : ws->out.len); }

static int fail(zb_ws *ws, const char *why) {
  if (!ws->error[0]) {
    snprintf(ws->error, sizeof ws->error, "%s", why);
  }
  ws->state = ZB_WS_CLOSED;
  return -1;
}

/* Case-insensitive header lookup in an HTTP head; copies the trimmed value. */
static bool header(const char *head, const char *name, char *out, size_t cap) {
  size_t nl = strlen(name);
  for (const char *line = strstr(head, "\r\n"); line && line[2]; line = strstr(line + 2, "\r\n")) {
    const char *p = line + 2;
    size_t i = 0;
    while (i < nl && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)name[i])) {
      ++i;
    }
    if (i == nl && p[i] == ':') {
      p += i + 1;
      while (*p == ' ' || *p == '\t') {
        ++p;
      }
      size_t n = 0;
      while (p[n] && p[n] != '\r' && n + 1 < cap) {
        out[n] = p[n];
        ++n;
      }
      out[n] = '\0';
      return true;
    }
  }
  return false;
}

static bool contains_token(const char *value, const char *token) {
  size_t tl = strlen(token);
  for (const char *p = value; *p; ++p) {
    size_t i = 0;
    while (i < tl && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)token[i])) {
      ++i;
    }
    if (i == tl) {
      return true;
    }
  }
  return false;
}

static int handshake(zb_ws *ws) {
  const char *end = NULL;
  for (size_t i = 3; i < ws->in.len; ++i) {
    if (memcmp(ws->in.data + i - 3, "\r\n\r\n", 4) == 0) {
      end = (const char *)ws->in.data + i + 1;
      break;
    }
  }
  if (!end) {
    return ws->in.len > 8192 ? fail(ws, "handshake too long") : 0;
  }
  size_t head_len = (size_t)(end - (const char *)ws->in.data);
  char head[8200];
  memcpy(head, ws->in.data, head_len);
  head[head_len] = '\0';
  drop_front(&ws->in, head_len);
  char value[128];
  if (ws->server) {
    if (strncmp(head, "GET ", 4) != 0 || !header(head, "Upgrade", value, sizeof value) ||
        !contains_token(value, "websocket") || !header(head, "Sec-WebSocket-Key", value, sizeof value) || !value[0]) {
      static const char bad[] = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
      append(&ws->out, bad, sizeof bad - 1);
      return fail(ws, "not a WebSocket upgrade request");
    }
    char acc[32];
    accept_key(value, acc);
    char resp[256];
    int n = snprintf(resp, sizeof resp,
                     "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: %s\r\n\r\n",
                     acc);
    if (append(&ws->out, resp, (size_t)n) != ZB_OK) {
      return fail(ws, "out of memory");
    }
  } else {
    if (strncmp(head, "HTTP/1.1 101", 12) != 0) {
      return fail(ws, "the server refused the WebSocket upgrade");
    }
    if (!header(head, "Sec-WebSocket-Accept", value, sizeof value) || strcmp(value, ws->accept) != 0) {
      return fail(ws, "wrong Sec-WebSocket-Accept");
    }
  }
  ws->state = ZB_WS_OPEN;
  return 1;
}

static zb_result frame(zb_ws *ws, int opcode, const void *data, size_t len) {
  uint8_t head[14];
  size_t h = 0;
  head[h++] = (uint8_t)(0x80 | opcode);
  uint8_t mask_bit = ws->server ? 0 : 0x80;
  if (len < 126) {
    head[h++] = (uint8_t)(mask_bit | len);
  } else if (len <= 0xFFFF) {
    head[h++] = (uint8_t)(mask_bit | 126);
    head[h++] = (uint8_t)(len >> 8);
    head[h++] = (uint8_t)len;
  } else {
    head[h++] = (uint8_t)(mask_bit | 127);
    for (int i = 7; i >= 0; --i) {
      head[h++] = (uint8_t)((uint64_t)len >> (8 * i));
    }
  }
  uint8_t mask[4] = {0, 0, 0, 0};
  if (!ws->server) {
    ws->mask_state = zb_mix64(ws->mask_state + 0x9e3779b97f4a7c15ull);
    for (int i = 0; i < 4; ++i) {
      mask[i] = (uint8_t)(ws->mask_state >> (8 * i));
      head[h++] = mask[i];
    }
  }
  size_t start = ws->out.len;
  if (append(&ws->out, head, h) != ZB_OK || append(&ws->out, data, len) != ZB_OK) {
    return ZB_ERR_NOMEM;
  }
  if (!ws->server) {
    uint8_t *p = ws->out.data + start + h;
    for (size_t i = 0; i < len; ++i) {
      p[i] ^= mask[i & 3];
    }
  }
  return ZB_OK;
}

zb_result zb_ws_send(zb_ws *ws, const void *data, size_t len) {
  if (ws->state != ZB_WS_OPEN) {
    return ZB_ERR_STATE;
  }
  return frame(ws, OP_BINARY, data, len);
}

void zb_ws_close(zb_ws *ws, uint16_t code) {
  if (ws->state == ZB_WS_OPEN) {
    uint8_t body[2] = {(uint8_t)(code >> 8), (uint8_t)code};
    frame(ws, OP_CLOSE, body, sizeof body);
  }
  ws->state = ZB_WS_CLOSED;
}

int zb_ws_poll(zb_ws *ws, const uint8_t **msg, size_t *len) {
  if (ws->state == ZB_WS_CLOSED) {
    return -1;
  }
  if (ws->state == ZB_WS_HANDSHAKE) {
    int r = handshake(ws);
    if (r <= 0) {
      return r;
    }
  }
  if (ws->in_message == false && ws->msg.len) {
    ws->msg.len = 0; /* the previous complete message was consumed */
  }
  for (;;) {
    const uint8_t *p = ws->in.data;
    size_t avail = ws->in.len;
    if (avail < 2) {
      return 0;
    }
    bool fin = (p[0] & 0x80) != 0;
    int opcode = p[0] & 0x0F;
    bool masked = (p[1] & 0x80) != 0;
    uint64_t plen = p[1] & 0x7F;
    size_t h = 2;
    if (plen == 126) {
      if (avail < 4) {
        return 0;
      }
      plen = (uint64_t)p[2] << 8 | p[3];
      h = 4;
    } else if (plen == 127) {
      if (avail < 10) {
        return 0;
      }
      plen = 0;
      for (int i = 0; i < 8; ++i) {
        plen = plen << 8 | p[2 + i];
      }
      h = 10;
    }
    if (masked != ws->server) {
      return fail(ws, ws->server ? "client frames must be masked" : "server frames must not be masked");
    }
    if (plen > ws->max_message) {
      return fail(ws, "message too large");
    }
    size_t mh = masked ? 4 : 0;
    if (avail < h + mh + plen) {
      return 0;
    }
    uint8_t *payload = ws->in.data + h + mh;
    if (masked) {
      const uint8_t *mask = ws->in.data + h;
      for (size_t i = 0; i < plen; ++i) {
        payload[i] ^= mask[i & 3];
      }
    }
    size_t frame_len = h + mh + (size_t)plen;
    if (opcode >= OP_CLOSE) { /* control frames */
      if (opcode == OP_PING) {
        frame(ws, OP_PONG, payload, (size_t)plen);
      } else if (opcode == OP_CLOSE) {
        if (ws->state == ZB_WS_OPEN) {
          frame(ws, OP_CLOSE, payload, plen >= 2 ? 2 : 0);
        }
        drop_front(&ws->in, frame_len);
        return fail(ws, "closed by peer");
      }
      drop_front(&ws->in, frame_len);
      continue;
    }
    if (opcode == OP_CONT ? !ws->in_message : ws->in_message) {
      return fail(ws, "unexpected continuation frame");
    }
    if (opcode != OP_CONT) {
      ws->msg.len = 0;
    }
    if (ws->msg.len + plen > ws->max_message || append(&ws->msg, payload, (size_t)plen) != ZB_OK) {
      return fail(ws, "message too large");
    }
    drop_front(&ws->in, frame_len);
    ws->in_message = !fin;
    if (fin) {
      *msg = ws->msg.data;
      *len = ws->msg.len;
      return 1;
    }
  }
}

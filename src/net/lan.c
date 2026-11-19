/* SPDX-License-Identifier: MIT */
#include "net/lan.h"

#include <stdio.h>
#include <string.h>

#include "core/array.h"
#include "net/socket.h"

static const uint8_t QUERY[6] = {'Z', 'B', 'L', 'Q', ZB_LAN_VERSION, 0};
#define ANSWER_HEAD 13 /* "ZBLA", version u16, protocol u16, players, max, port u16, name length */

struct zb_lan {
  const zb_allocator *a;
  zb_socket s;
  zb_array found; /* zb_lan_session */
};

zb_lan *zb_lan_open(const zb_allocator *a, uint16_t port, uint16_t *bound_port, char *err, size_t err_len) {
  if (!zb_sock_supported()) {
    snprintf(err, err_len, "unavailable");
    return NULL;
  }
  zb_lan *l = zb_calloc(a, sizeof *l);
  if (!l) {
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  l->a = a;
  zb_array_init(&l->found, a, sizeof(zb_lan_session));
  l->s = zb_udp_open(port, bound_port);
  if (l->s == ZB_SOCKET_NONE) {
    snprintf(err, err_len, "cannot open a UDP socket on port %u", (unsigned)port);
    zb_lan_close(l);
    return NULL;
  }
  return l;
}

void zb_lan_close(zb_lan *l) {
  if (l) {
    zb_sock_close(l->s);
    zb_array_free(&l->found);
    zb_free(l->a, l);
  }
}

size_t zb_lan_encode_answer(const zb_lan_session *s, uint8_t *out, size_t cap) {
  size_t name = strlen(s->name);
  name = name > ZB_LAN_MAX_NAME ? ZB_LAN_MAX_NAME : name;
  if (cap < ANSWER_HEAD + name) {
    return 0;
  }
  memcpy(out, "ZBLA", 4);
  out[4] = ZB_LAN_VERSION;
  out[5] = 0;
  out[6] = (uint8_t)s->protocol;
  out[7] = (uint8_t)(s->protocol >> 8);
  out[8] = s->players;
  out[9] = s->max_players;
  out[10] = (uint8_t)s->port;
  out[11] = (uint8_t)(s->port >> 8);
  out[12] = (uint8_t)name;
  memcpy(out + ANSWER_HEAD, s->name, name);
  return ANSWER_HEAD + name;
}

bool zb_lan_decode_answer(const uint8_t *data, size_t len, zb_lan_session *out) {
  if (len < ANSWER_HEAD || memcmp(data, "ZBLA", 4) != 0 || data[4] != ZB_LAN_VERSION || data[12] > ZB_LAN_MAX_NAME ||
      len != (size_t)ANSWER_HEAD + data[12]) {
    return false;
  }
  memset(out, 0, sizeof *out);
  out->protocol = (uint16_t)(data[6] | data[7] << 8);
  out->players = data[8];
  out->max_players = data[9];
  out->port = (uint16_t)(data[10] | data[11] << 8);
  memcpy(out->name, data + ANSWER_HEAD, data[12]);
  return true;
}

void zb_lan_answer(zb_lan *l, const zb_lan_session *self) {
  uint8_t buf[128], reply[64];
  char from[48];
  uint16_t from_port;
  long n;
  while ((n = zb_udp_recv(l->s, buf, sizeof buf, from, sizeof from, &from_port)) >= 0) {
    if (self && n == (long)sizeof QUERY && memcmp(buf, QUERY, sizeof QUERY) == 0) {
      size_t len = zb_lan_encode_answer(self, reply, sizeof reply);
      zb_udp_send(l->s, from, from_port, reply, len);
    }
  }
}

zb_result zb_lan_query(zb_lan *l, const char *address, uint16_t port) {
  return zb_udp_send(l->s, address, port, QUERY, sizeof QUERY) == (long)sizeof QUERY ? ZB_OK : ZB_ERR_IO;
}

size_t zb_lan_collect(zb_lan *l, uint64_t now_ns, uint64_t max_age_ns) {
  uint8_t buf[128];
  char from[48];
  uint16_t from_port;
  long n;
  zb_lan_session s;
  while ((n = zb_udp_recv(l->s, buf, sizeof buf, from, sizeof from, &from_port)) >= 0) {
    if (!zb_lan_decode_answer(buf, (size_t)n, &s)) {
      continue;
    }
    snprintf(s.address, sizeof s.address, "%s", from);
    s.seen_ns = now_ns;
    size_t i = 0;
    while (i < l->found.len) {
      zb_lan_session *f = zb_array_at(&l->found, i);
      if (f->port == s.port && strcmp(f->address, s.address) == 0) {
        *f = s;
        break;
      }
      ++i;
    }
    if (i == l->found.len) {
      zb_array_push(&l->found, &s);
    }
  }
  for (size_t i = 0; i < l->found.len;) {
    if (now_ns - ((zb_lan_session *)zb_array_at(&l->found, i))->seen_ns > max_age_ns) {
      zb_array_remove(&l->found, i);
    } else {
      ++i;
    }
  }
  return l->found.len;
}

const zb_lan_session *zb_lan_found(const zb_lan *l, size_t i) {
  return i < l->found.len ? zb_array_at(&l->found, i) : NULL;
}

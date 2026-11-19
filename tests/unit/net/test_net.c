/* SPDX-License-Identifier: MIT */
#include "core/sha1.h"
#include "net/session.h"
#include "net/socket.h"
#include "net/transport.h"
#include "net/ws.h"
#include "support/test_support.h"
#include "support/test_threads.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

ZB_TEST_LEAK_CHECKED

static void test_sha1_base64(void) {
  uint8_t d[20];
  zb_sha1("abc", 3, d);
  char hex[41];
  for (int i = 0; i < 20; ++i) {
    snprintf(hex + i * 2, 3, "%02x", d[i]);
  }
  TEST_ASSERT_EQUAL_STRING("a9993e364706816aba3e25717850c26c9cd0d89d", hex);
  zb_sha1("", 0, d);
  TEST_ASSERT_EQUAL_UINT8(0xda, d[0]);
  /* 56 bytes: the padding spills into a second block. */
  zb_sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, d);
  TEST_ASSERT_EQUAL_UINT8(0x84, d[0]);
  TEST_ASSERT_EQUAL_UINT8(0xf1, d[19]);
  char b64[16];
  zb_base64((const uint8_t *)"Ma", 2, b64);
  TEST_ASSERT_EQUAL_STRING("TWE=", b64);
  zb_base64((const uint8_t *)"M", 1, b64);
  TEST_ASSERT_EQUAL_STRING("TQ==", b64);
  zb_base64((const uint8_t *)"Man", 3, b64);
  TEST_ASSERT_EQUAL_STRING("TWFu", b64);
}

/* Moves pending bytes from one side to the other. */
static void pipe_bytes(zb_ws *from, zb_ws *to) {
  size_t n;
  const uint8_t *p = zb_ws_pending(from, &n);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_ws_feed(to, p, n));
  zb_ws_sent(from, n);
}

/* spec: netcode: WebSocket protocol */
static void test_ws_protocol(void) {
  const zb_allocator *a = zb_test_alloc();
  zb_ws srv, cli;
  zb_ws_init_server(&srv, a);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_ws_init_client(&cli, a, "example.org", "/play", 42));
  const uint8_t *msg;
  size_t len;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, zb_ws_send(&cli, "x", 1)); /* not open yet */
  pipe_bytes(&cli, &srv);
  TEST_ASSERT_EQUAL_INT(0, zb_ws_poll(&srv, &msg, &len));
  TEST_ASSERT_EQUAL_INT(ZB_WS_OPEN, srv.state);
  pipe_bytes(&srv, &cli);
  TEST_ASSERT_EQUAL_INT(0, zb_ws_poll(&cli, &msg, &len));
  TEST_ASSERT_EQUAL_INT(ZB_WS_OPEN, cli.state);
  /* Messages of every length class, in both directions. */
  static uint8_t big[70000];
  for (size_t i = 0; i < sizeof big; ++i) {
    big[i] = (uint8_t)(i * 7);
  }
  const size_t sizes[] = {0, 5, 125, 126, 300, 65535, 70000};
  for (size_t k = 0; k < sizeof sizes / sizeof sizes[0]; ++k) {
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_ws_send(&cli, big, sizes[k]));
    pipe_bytes(&cli, &srv);
    TEST_ASSERT_EQUAL_INT(1, zb_ws_poll(&srv, &msg, &len));
    TEST_ASSERT_EQUAL_size_t(sizes[k], len);
    TEST_ASSERT_TRUE(len == 0 || memcmp(msg, big, len) == 0);
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_ws_send(&srv, big, sizes[k]));
    pipe_bytes(&srv, &cli);
    TEST_ASSERT_EQUAL_INT(1, zb_ws_poll(&cli, &msg, &len));
    TEST_ASSERT_EQUAL_size_t(sizes[k], len);
  }
  /* A fragmented, masked text message split across feeds, with a ping in between. */
  static const uint8_t frag[] = {0x01, 0x83, 1, 2,    3,    4, 'a' ^ 1, 'b' ^ 2, 'c' ^ 3, 0x89, 0x80, 0,
                                 0,    0,    0, 0x80, 0x82, 0, 0,       0,       0,       'd',  'e'};
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_ws_feed(&srv, frag, 5));
  TEST_ASSERT_EQUAL_INT(0, zb_ws_poll(&srv, &msg, &len));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_ws_feed(&srv, frag + 5, sizeof frag - 5));
  TEST_ASSERT_EQUAL_INT(1, zb_ws_poll(&srv, &msg, &len));
  TEST_ASSERT_EQUAL_size_t(5, len);
  TEST_ASSERT_EQUAL_MEMORY("abcde", msg, 5);
  pipe_bytes(&srv, &cli); /* the pong */
  TEST_ASSERT_EQUAL_INT(0, zb_ws_poll(&cli, &msg, &len));
  /* Closing from the client: both sides end up closed. */
  zb_ws_close(&cli, 1000);
  pipe_bytes(&cli, &srv);
  TEST_ASSERT_EQUAL_INT(-1, zb_ws_poll(&srv, &msg, &len));
  TEST_ASSERT_EQUAL_STRING("closed by peer", srv.error);
  TEST_ASSERT_EQUAL_INT(-1, zb_ws_poll(&srv, &msg, &len));
  zb_ws_free(&srv);
  zb_ws_free(&cli);
}

static int server_reply(const char *request, zb_ws *srv) {
  const uint8_t *msg;
  size_t len;
  zb_ws_init_server(srv, zb_test_alloc());
  zb_ws_feed(srv, request, strlen(request));
  return zb_ws_poll(srv, &msg, &len);
}

static void test_ws_errors(void) {
  zb_ws srv;
  TEST_ASSERT_EQUAL_INT(-1, server_reply("POST / HTTP/1.1\r\nHost: x\r\n\r\n", &srv));
  size_t n;
  TEST_ASSERT_NOT_NULL(strstr((const char *)zb_ws_pending(&srv, &n), "400"));
  zb_ws_free(&srv);
  TEST_ASSERT_EQUAL_INT(-1, server_reply("GET / HTTP/1.1\r\nupgrade: WebSocket\r\n\r\n", &srv)); /* no key */
  zb_ws_free(&srv);
  /* Header names are case-insensitive. */
  TEST_ASSERT_EQUAL_INT(0, server_reply("GET / HTTP/1.1\r\nUPGRADE: websocket\r\nsec-websocket-key: "
                                        "dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n",
                                        &srv));
  TEST_ASSERT_NOT_NULL(strstr((const char *)zb_ws_pending(&srv, &n), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
  /* Unmasked client frames, unexpected continuations and oversized messages are errors. */
  const uint8_t *msg;
  size_t len;
  static const uint8_t unmasked[] = {0x82, 0x01, 'x'};
  zb_ws_feed(&srv, unmasked, sizeof unmasked);
  TEST_ASSERT_EQUAL_INT(-1, zb_ws_poll(&srv, &msg, &len));
  zb_ws_free(&srv);
  server_reply("GET / HTTP/1.1\r\nUpgrade: websocket\r\nSec-WebSocket-Key: k\r\n\r\n", &srv);
  static const uint8_t cont[] = {0x80, 0x80, 0, 0, 0, 0};
  zb_ws_feed(&srv, cont, sizeof cont);
  TEST_ASSERT_EQUAL_INT(-1, zb_ws_poll(&srv, &msg, &len));
  zb_ws_free(&srv);
  server_reply("GET / HTTP/1.1\r\nUpgrade: websocket\r\nSec-WebSocket-Key: k\r\n\r\n", &srv);
  static const uint8_t huge[] = {0x82, 0xFF, 0, 0, 0, 1, 0, 0, 0, 0};
  zb_ws_feed(&srv, huge, sizeof huge);
  TEST_ASSERT_EQUAL_INT(-1, zb_ws_poll(&srv, &msg, &len));
  TEST_ASSERT_EQUAL_STRING("message too large", srv.error);
  zb_ws_free(&srv);
  server_reply("GET / HTTP/1.1\r\nUpgrade: websocket\r\nSec-WebSocket-Key: k\r\n\r\n", &srv);
  static const uint8_t mid[] = {0x82, 0xFE, 0x01};
  zb_ws_feed(&srv, mid, sizeof mid); /* incomplete 16-bit length */
  TEST_ASSERT_EQUAL_INT(0, zb_ws_poll(&srv, &msg, &len));
  zb_ws_free(&srv);
  /* A client rejects a wrong accept key or a refusal. */
  zb_ws cli;
  zb_ws_init_client(&cli, zb_test_alloc(), "h", NULL, 1);
  zb_ws_sent(&cli, 100000);
  const char *wrong = "HTTP/1.1 101 OK\r\nSec-WebSocket-Accept: nope\r\n\r\n";
  zb_ws_feed(&cli, wrong, strlen(wrong));
  TEST_ASSERT_EQUAL_INT(-1, zb_ws_poll(&cli, &msg, &len));
  zb_ws_free(&cli);
  zb_ws_init_client(&cli, zb_test_alloc(), "h", "/", 1);
  zb_ws_feed(&cli, "HTTP/1.1 403 No\r\n\r\n", 19);
  TEST_ASSERT_EQUAL_INT(-1, zb_ws_poll(&cli, &msg, &len));
  zb_ws_free(&cli);
  char host[16], path[16];
  uint16_t port;
  TEST_ASSERT_TRUE(zb_ws_parse_url("ws://a.b:81/x", host, sizeof host, &port, path, sizeof path));
  TEST_ASSERT_EQUAL_STRING("a.b", host);
  TEST_ASSERT_EQUAL_UINT16(81, port);
  TEST_ASSERT_EQUAL_STRING("/x", path);
  TEST_ASSERT_TRUE(zb_ws_parse_url("ws://h", host, sizeof host, &port, path, sizeof path));
  TEST_ASSERT_EQUAL_UINT16(80, port);
  TEST_ASSERT_FALSE(zb_ws_parse_url("http://h", host, sizeof host, &port, path, sizeof path));
  TEST_ASSERT_FALSE(zb_ws_parse_url("ws://h:0", host, sizeof host, &port, path, sizeof path));
  TEST_ASSERT_FALSE(zb_ws_parse_url("ws://h:80x", host, sizeof host, &port, path, sizeof path));
  TEST_ASSERT_FALSE(zb_ws_parse_url("ws://", host, sizeof host, &port, path, sizeof path));
}

static uint64_t g_now;
static uint64_t fake_clock(void *ctx) {
  (void)ctx;
  return g_now;
}

static zb_net *make_net(uint64_t seed) {
  zb_net_config c;
  memset(&c, 0, sizeof c);
  c.a = zb_test_alloc();
  c.world_seed = seed;
  c.package_hash = 77;
  c.clock_ns = fake_clock;
  return zb_net_create(&c);
}

static void pump(zb_net **nets, int count, int rounds) {
  for (int r = 0; r < rounds; ++r) {
    for (int i = 0; i < count; ++i) {
      zb_net_update(nets[i]);
    }
  }
}

/* Collects the next event of a kind (skipping others). */
static bool expect_event(zb_net *n, zb_net_event_kind kind, zb_net_event *out) {
  zb_net_event ev;
  while (zb_net_next(n, &ev)) {
    if (ev.kind == kind) {
      *out = ev;
      return true;
    }
  }
  return false;
}

/* spec: netcode: Player indices */
/* spec: netcode: Refused joins */
/* spec: netcode: Leaving and silence */
static void test_session_loopback(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_net *host = make_net(5), *c1 = make_net(5), *c2 = make_net(5), *bad = make_net(6);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_host(host, zb_loopback_listen(lb)));
  TEST_ASSERT_NULL(zb_loopback_listen(lb)); /* one listener per network */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_join(c1, zb_loopback_connect(lb)));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_join(c2, zb_loopback_connect(lb)));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_join(bad, zb_loopback_connect(lb)));
  TEST_ASSERT_EQUAL_INT(ZB_NET_JOINING, zb_net_role_of(c1));
  zb_net *all[] = {host, c1, c2, bad};
  pump(all, 4, 3);
  zb_net_event ev;
  TEST_ASSERT_TRUE(expect_event(c1, ZB_NET_EV_WELCOME, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_EQUAL_INT(ZB_NET_CLIENT, zb_net_role_of(c1));
  TEST_ASSERT_EQUAL_INT(1, zb_net_player(c1));
  TEST_ASSERT_EQUAL_INT(2, zb_net_player(c2));
  TEST_ASSERT_EQUAL_UINT(0x7, zb_net_players(host));
  /* A different world is refused with a reason. */
  TEST_ASSERT_TRUE(expect_event(bad, ZB_NET_EV_ENDED, &ev));
  TEST_ASSERT_EQUAL_STRING("the host plays a different world (seed)", zb_net_error(bad));
  TEST_ASSERT_EQUAL_INT(ZB_NET_NONE, zb_net_role_of(bad));
  TEST_ASSERT_EQUAL_INT(-1, zb_net_player(bad));
  /* Routing: client to host, client to everyone (relayed), host to one. */
  while (zb_net_next(host, &ev)) {
  }
  while (zb_net_next(c2, &ev)) {
  }
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send(c1, 0, "input", "abc", 3));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send(c1, ZB_NET_TO_ALL, "chat", "hi", 2));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send(host, 2, "state", "s", 1));
  pump(all, 3, 2);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_DATA, &ev));
  TEST_ASSERT_EQUAL_STRING("input", ev.channel);
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_EQUAL_MEMORY("abc", ev.data, 3);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_DATA, &ev));
  TEST_ASSERT_EQUAL_STRING("chat", ev.channel);
  /* The host's own message left before it relayed the chat. */
  TEST_ASSERT_TRUE(expect_event(c2, ZB_NET_EV_DATA, &ev));
  TEST_ASSERT_EQUAL_STRING("state", ev.channel);
  TEST_ASSERT_EQUAL_INT(0, ev.player);
  TEST_ASSERT_TRUE(expect_event(c2, ZB_NET_EV_DATA, &ev));
  TEST_ASSERT_EQUAL_STRING("chat", ev.channel);
  TEST_ASSERT_EQUAL_INT(1, ev.player);                      /* the host keeps the real sender */
  TEST_ASSERT_FALSE(expect_event(c1, ZB_NET_EV_DATA, &ev)); /* not addressed to c1 */
  /* Invalid sends. */
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_send(c1, 1, "self", NULL, 0));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_send(c1, 0, "", NULL, 0));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, zb_net_send(host, 3, "nobody", NULL, 0));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, zb_net_send(bad, 0, "x", NULL, 0));
  /* Pings measure round trips; silence ends the session. */
  g_now = 2000000000ull;
  pump(all, 3, 3);
  zb_net_stats st;
  zb_net_get_stats(host, &st);
  TEST_ASSERT_TRUE(st.messages_sent > 0 && st.bytes_received > 0);
  /* A client leaving: the others learn it. */
  zb_net_leave(c2);
  pump(all, 2, 2);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_LEFT, &ev));
  TEST_ASSERT_EQUAL_INT(2, ev.player);
  TEST_ASSERT_TRUE(expect_event(c1, ZB_NET_EV_LEFT, &ev));
  TEST_ASSERT_EQUAL_UINT(0x3, zb_net_players(host));
  /* The host going silent: the client times out. */
  g_now += 11000000000ull;
  zb_net_update(c1);
  TEST_ASSERT_TRUE(expect_event(c1, ZB_NET_EV_ENDED, &ev));
  TEST_ASSERT_EQUAL_STRING("the host stopped answering", zb_net_error(c1));
  zb_net_destroy(c1);
  zb_net_destroy(c2);
  zb_net_destroy(bad);
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
  zb_net_destroy(NULL);
}

/* spec: netcode: Refused joins */
/* spec: netcode: Leaving and silence */
static void test_session_full_and_host_leaving(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_net *host = make_net(1);
  zb_net_host(host, zb_loopback_listen(lb));
  zb_net *c[4];
  for (int i = 0; i < 4; ++i) {
    c[i] = make_net(1);
    zb_net_join(c[i], zb_loopback_connect(lb));
  }
  zb_net *all[] = {host, c[0], c[1], c[2], c[3]};
  pump(all, 5, 3);
  zb_net_event ev;
  TEST_ASSERT_TRUE(expect_event(c[3], ZB_NET_EV_ENDED, &ev));
  TEST_ASSERT_EQUAL_STRING("the session is full", zb_net_error(c[3]));
  TEST_ASSERT_EQUAL_INT(-1, zb_net_player(c[3]));
  TEST_ASSERT_EQUAL_UINT(0xF, zb_net_players(host));
  /* The host leaves: every client's session ends. */
  zb_net_leave(host);
  pump(all, 4, 1);
  for (int i = 0; i < 3; ++i) {
    TEST_ASSERT_TRUE(expect_event(c[i], ZB_NET_EV_ENDED, &ev));
    TEST_ASSERT_EQUAL_STRING("the host ended the session", zb_net_error(c[i]));
  }
  /* Joining nowhere fails. */
  zb_net *lone = make_net(1);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_join(lone, zb_loopback_connect(lb)));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_host(lone, NULL));
  zb_net_destroy(lone);
  for (int i = 0; i < 4; ++i) {
    zb_net_destroy(c[i]);
  }
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
}

/* Next message on a raw transport (skipping open events); NULL when none. */
static const uint8_t *raw_next(zb_transport *t, size_t *len, bool *closed) {
  zb_tr_event ev;
  while (t->next(t->ctx, &ev)) {
    if (ev.kind == ZB_TR_MESSAGE) {
      *len = ev.len;
      return ev.data;
    }
    if (ev.kind == ZB_TR_CLOSE && closed) {
      *closed = true;
    }
  }
  return NULL;
}

static void put64(uint8_t *p, uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    p[i] = (uint8_t)(v >> (8 * i));
  }
}

/* A protocol 2 hello without a resume token and without packages (31 bytes). */
static void hello(uint8_t out[31], unsigned version, uint64_t seed, uint64_t packages) {
  memset(out, 0, 31);
  out[0] = 1; /* MSG_HELLO */
  out[1] = (uint8_t)version;
  out[2] = (uint8_t)(version >> 8);
  put64(out + 3, seed);
  put64(out + 11, packages);
}

/* Sends one message from a raw client and returns the host's reply as text after its type byte. */
/* Sends one message from a new raw client and checks the host's refusal and the close. */
static void expect_reject(zb_net *host, zb_loopback *lb, const void *msg, size_t len, const char *reason) {
  zb_transport *raw = zb_loopback_connect(lb);
  zb_net_update(host);
  raw->send(raw->ctx, 1, msg, len);
  zb_net_update(host);
  size_t n = 0;
  bool closed = false;
  const uint8_t *reply = raw_next(raw, &n, &closed);
  TEST_ASSERT_NOT_NULL(reply);
  TEST_ASSERT_EQUAL_UINT8(3, reply[0]); /* MSG_REJECT */
  TEST_ASSERT_EQUAL_size_t(strlen(reason), n - 1);
  TEST_ASSERT_EQUAL_MEMORY(reason, reply + 1, n - 1);
  raw_next(raw, &n, &closed);
  TEST_ASSERT_TRUE(closed);
  zb_transport_destroy(raw);
}

/* spec: netcode: Refused joins */
static void test_session_protocol_errors(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_net *host = make_net(5);
  zb_net_host(host, zb_loopback_listen(lb));
  uint8_t h[31];
  hello(h, 2, 5, 77);
  expect_reject(host, lb, h, 1, "malformed hello");
  expect_reject(host, lb, h, 20, "malformed hello"); /* too short for protocol 2 */
  hello(h, 1, 5, 77);
  expect_reject(host, lb, h, 19, "different game version (network protocol)"); /* a protocol 1 game */
  hello(h, 2, 5, 77);
  h[29] = 1; /* one package announced, none there */
  expect_reject(host, lb, h, sizeof h, "malformed hello");
  uint8_t hp[34];
  hello(hp, 2, 5, 77);
  hp[29] = 1;
  hp[31] = 1; /* id "a", then a version longer than what is left */
  hp[32] = 'a';
  hp[33] = 9;
  expect_reject(host, lb, hp, sizeof hp, "malformed hello");
  hello(h, 2, 5, 78);
  expect_reject(host, lb, h, sizeof h, "the host has a different package set");
  static const uint8_t early[] = {4, 0, 0, 0};
  expect_reject(host, lb, early, sizeof early, "hello expected");
  zb_transport *raw = zb_loopback_connect(lb);
  zb_net_update(host);
  raw->send(raw->ctx, 1, early, 0); /* empty messages are ignored */
  hello(h, 2, 5, 77);
  raw->send(raw->ctx, 1, h, sizeof h);
  zb_net_update(host);
  size_t n;
  const uint8_t *reply = raw_next(raw, &n, NULL);
  TEST_ASSERT_NOT_NULL(reply);
  TEST_ASSERT_EQUAL_UINT8(2, reply[0]); /* MSG_WELCOME */
  TEST_ASSERT_EQUAL_UINT8(1, reply[1]);
  /* Malformed data is dropped; pings are answered. */
  static const uint8_t short_data[] = {4, 0, 0, 9, 'a'};
  uint8_t long_channel[4 + 40];
  memset(long_channel, 'c', sizeof long_channel);
  long_channel[0] = 4;
  long_channel[2] = 0;
  long_channel[3] = 40;
  uint8_t ping[9] = {7};
  put64(ping + 1, 123);
  raw->send(raw->ctx, 1, short_data, sizeof short_data);
  raw->send(raw->ctx, 1, long_channel, sizeof long_channel);
  raw->send(raw->ctx, 1, ping, sizeof ping);
  zb_net_update(host);
  zb_net_event ev;
  TEST_ASSERT_FALSE(expect_event(host, ZB_NET_EV_DATA, &ev));
  reply = raw_next(raw, &n, NULL);
  while (reply && reply[0] != 8) { /* MSG_PONG */
    reply = raw_next(raw, &n, NULL);
  }
  TEST_ASSERT_NOT_NULL(reply);
  TEST_ASSERT_EQUAL_size_t(9, n);
  /* At most eight connections may wait at once. */
  zb_transport *waiting[8];
  for (int i = 0; i < 8; ++i) {
    waiting[i] = zb_loopback_connect(lb);
  }
  zb_net_update(host);
  bool closed = false;
  raw_next(waiting[7], &n, &closed);
  TEST_ASSERT_TRUE(closed);
  /* A silent client is away after the timeout and gone after the grace period. */
  g_now = 11000000000ull;
  zb_net_update(host);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_AWAY, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_EQUAL_UINT(0x3, zb_net_players(host));
  TEST_ASSERT_EQUAL_UINT(0x2, zb_net_away(host));
  g_now = 41000000000ull;
  zb_net_update(host);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_LEFT, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_EQUAL_UINT(0x1, zb_net_players(host));
  TEST_ASSERT_EQUAL_UINT(0, zb_net_away(host));
  for (int i = 0; i < 8; ++i) {
    zb_transport_destroy(waiting[i]);
  }
  zb_transport_destroy(raw);
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
}

/* Loopback edge cases: sending on closed links and a listener that goes away. */
static void test_loopback_edges(void) {
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  TEST_ASSERT_NULL(zb_loopback_connect(lb)); /* nobody listens */
  zb_transport *l = zb_loopback_listen(lb);
  zb_transport *c = zb_loopback_connect(lb);
  c->update(c->ctx);
  TEST_ASSERT_EQUAL_INT(ZB_OK, c->send(c->ctx, 1, "x", 1));
  l->close(l->ctx, 1);
  l->close(l->ctx, 1); /* twice: nothing happens */
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, c->send(c->ctx, 1, "x", 1));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, l->send(l->ctx, 1, "x", 1));
  zb_transport *c2 = zb_loopback_connect(lb);
  zb_transport_destroy(l); /* the listener goes: open links close */
  size_t n;
  bool closed = false;
  raw_next(c2, &n, &closed);
  TEST_ASSERT_TRUE(closed);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, c2->send(c2->ctx, 1, "x", 1));
  zb_transport_destroy(c2);
  zb_transport_destroy(c);
  zb_loopback_destroy(lb);
}

/* Runs a short session on a failing allocator; nothing may leak or crash. */
static bool session_with_budget(long budget) {
  zb_failing_allocator fa;
  const zb_allocator *a = zb_failing_init(&fa, zb_test_alloc(), budget);
  zb_loopback *lb = zb_loopback_create(a);
  if (!lb) {
    return false;
  }
  zb_net_config c;
  memset(&c, 0, sizeof c);
  c.a = a;
  c.world_seed = 9;
  c.clock_ns = fake_clock;
  zb_net *host = zb_net_create(&c), *client = zb_net_create(&c);
  bool ok = false;
  if (host && client && zb_net_host(host, zb_loopback_listen(lb)) == ZB_OK &&
      zb_net_join(client, zb_loopback_connect(lb)) == ZB_OK) {
    zb_net *all[] = {host, client};
    pump(all, 2, 3);
    zb_net_send(client, ZB_NET_TO_ALL, "a", "12345", 5);
    zb_net_send(host, 1, "b", "678", 3);
    pump(all, 2, 3);
    zb_net_event ev;
    ok = expect_event(host, ZB_NET_EV_DATA, &ev) && expect_event(client, ZB_NET_EV_DATA, &ev);
  }
  zb_net_destroy(client);
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
  return ok && fa.calls <= budget;
}

static void test_out_of_memory(void) {
  g_now = 0;
  long budget = 0;
  while (!session_with_budget(budget) && budget < 400) {
    ++budget;
  }
  TEST_ASSERT_TRUE(budget > 3 && budget < 400);
  zb_ws ws;
  zb_failing_allocator fa;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_ws_init_client(&ws, zb_failing_init(&fa, zb_test_alloc(), 0), "h", "/", 1));
  zb_ws_free(&ws);
  /* The server's reply cannot be queued. */
  zb_ws_init_server(&ws, zb_failing_init(&fa, zb_test_alloc(), 1));
  const char *req = "GET / HTTP/1.1\r\nUpgrade: websocket\r\nSec-WebSocket-Key: k\r\n\r\n";
  zb_ws_feed(&ws, req, strlen(req));
  const uint8_t *msg;
  size_t len;
  TEST_ASSERT_EQUAL_INT(-1, zb_ws_poll(&ws, &msg, &len));
  zb_ws_free(&ws);
  if (zb_sock_supported()) {
    char err[64];
    uint16_t port;
    TEST_ASSERT_NULL(zb_ws_server_create(zb_failing_init(&fa, zb_test_alloc(), 0), 0, &port, err, sizeof err));
    TEST_ASSERT_EQUAL_STRING("out of memory", err);
    TEST_ASSERT_NULL(
        zb_ws_client_create(zb_failing_init(&fa, zb_test_alloc(), 0), "ws://127.0.0.1:1", err, sizeof err));
    TEST_ASSERT_EQUAL_STRING("out of memory", err);
    TEST_ASSERT_NULL(
        zb_ws_client_create(zb_failing_init(&fa, zb_test_alloc(), 1), "ws://127.0.0.1:1", err, sizeof err));
    TEST_ASSERT_NULL(
        zb_ws_client_create(zb_failing_init(&fa, zb_test_alloc(), 2), "ws://127.0.0.1:1", err, sizeof err));
  }
}

/* spec: netcode: Real sockets */
static void test_websocket_over_sockets(void) {
  if (!zb_sock_supported()) {
    TEST_IGNORE_MESSAGE("no sockets in this build");
  }
  char err[128];
  uint16_t port = 0;
  zb_transport *srv = zb_ws_server_create(zb_test_alloc(), 0, &port, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(srv, err);
  TEST_ASSERT_TRUE(port > 0);
  char url[64];
  snprintf(url, sizeof url, "ws://127.0.0.1:%u/", (unsigned)port);
  zb_transport *cli = zb_ws_client_create(zb_test_alloc(), url, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(cli, err);
  bool srv_open = false, cli_open = false, got = false, echoed = false;
  uint32_t conn = 0;
  for (int i = 0; i < 2000 && !echoed; ++i) {
    srv->update(srv->ctx);
    cli->update(cli->ctx);
    zb_tr_event ev;
    while (srv->next(srv->ctx, &ev)) {
      if (ev.kind == ZB_TR_OPEN) {
        srv_open = true;
        conn = ev.conn;
      } else if (ev.kind == ZB_TR_MESSAGE) {
        got = ev.len == 5 && memcmp(ev.data, "hello", 5) == 0;
        srv->send(srv->ctx, conn, "world", 5);
      }
    }
    while (cli->next(cli->ctx, &ev)) {
      if (ev.kind == ZB_TR_OPEN) {
        cli_open = true;
        TEST_ASSERT_EQUAL_INT(ZB_OK, cli->send(cli->ctx, 1, "hello", 5));
      } else if (ev.kind == ZB_TR_MESSAGE) {
        echoed = ev.len == 5 && memcmp(ev.data, "world", 5) == 0;
      }
    }
    if (zb_test_thread_api()) {
      zb_test_sleep_ms(1);
    }
  }
  TEST_ASSERT_TRUE(srv_open && cli_open && got && echoed);
  /* The client closes; the server reports it. */
  cli->close(cli->ctx, 1);
  bool closed = false;
  for (int i = 0; i < 2000 && !closed; ++i) {
    cli->update(cli->ctx);
    srv->update(srv->ctx);
    zb_tr_event ev;
    while (srv->next(srv->ctx, &ev)) {
      closed = closed || ev.kind == ZB_TR_CLOSE;
    }
    if (zb_test_thread_api()) {
      zb_test_sleep_ms(1);
    }
  }
  TEST_ASSERT_TRUE(closed);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, srv->send(srv->ctx, conn, "x", 1));
  zb_transport_destroy(cli);
  zb_transport_destroy(srv);
  zb_transport_destroy(NULL);
  /* Bad addresses and ports in use. */
  TEST_ASSERT_NULL(zb_ws_client_create(zb_test_alloc(), "http://x", err, sizeof err));
  TEST_ASSERT_NOT_NULL(strstr(err, "not a ws://"));
  TEST_ASSERT_NULL(zb_ws_client_create(zb_test_alloc(), "ws://nonexistent.invalid:1", err, sizeof err));
  uint16_t p1 = 0;
  zb_transport *a = zb_ws_server_create(zb_test_alloc(), 0, &p1, err, sizeof err);
  TEST_ASSERT_NULL(zb_ws_server_create(zb_test_alloc(), p1, NULL, err, sizeof err));
  zb_transport_destroy(a);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sha1_base64);
  RUN_TEST(test_ws_protocol);
  RUN_TEST(test_ws_errors);
  RUN_TEST(test_session_loopback);
  RUN_TEST(test_session_full_and_host_leaving);
  RUN_TEST(test_websocket_over_sockets);
  RUN_TEST(test_session_protocol_errors);
  RUN_TEST(test_loopback_edges);
  RUN_TEST(test_out_of_memory);
  return UNITY_END();
}

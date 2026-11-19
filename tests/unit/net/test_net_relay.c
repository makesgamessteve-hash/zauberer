/* SPDX-License-Identifier: MIT */
/* The relay (invite codes, forwarding) and its endpoints, over loopback and
 * real WebSockets, plus the zauberer-relay program's entry. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net/relay.h"
#include "net/session.h"
#include "net/socket.h"
#include "net/transport.h"
#include "support/test_support.h"
#include "support/test_threads.h"
#include "unity.h"

#include "net_support.h"

ZB_TEST_LEAK_CHECKED

#define SECOND 1000000000ull

static zb_relay *relay_on(zb_loopback *lb, uint64_t seed, uint32_t max_sessions) {
  zb_relay_config c;
  memset(&c, 0, sizeof c);
  c.a = zb_test_alloc();
  c.seed = seed;
  c.clock_ns = fake_clock;
  c.max_sessions = max_sessions;
  c.idle_timeout_ns = 60 * SECOND;
  return zb_relay_create(&c, zb_loopback_listen(lb));
}

static bool valid_code(const char *code) {
  if (strlen(code) != ZB_RELAY_CODE_LEN) {
    return false;
  }
  for (int i = 0; i < ZB_RELAY_CODE_LEN; ++i) {
    if (!strchr(ZB_RELAY_ALPHABET, code[i])) {
      return false;
    }
  }
  return true;
}

/* A raw host: registers and returns the code it got ("" if refused; *reason then holds why). */
static zb_transport *raw_register(zb_relay *r, zb_loopback *lb, char code[16], char reason[96]) {
  zb_transport *t = zb_loopback_connect(lb);
  zb_relay_update(r);
  static const uint8_t reg[2] = {ZB_RELAY_REGISTER, ZB_RELAY_VERSION};
  t->send(t->ctx, 1, reg, sizeof reg);
  zb_relay_update(r);
  size_t n = 0;
  const uint8_t *m = raw_message(t, &n, NULL);
  code[0] = reason[0] = '\0';
  TEST_ASSERT_NOT_NULL(m);
  if (m[0] == ZB_RELAY_CODE) {
    memcpy(code, m + 1, n - 1);
    code[n - 1] = '\0';
  } else {
    memcpy(reason, m + 1, n - 1);
    reason[n - 1] = '\0';
  }
  return t;
}

/* A raw client: sends a join message; returns the reason it was refused, or "" when it joined. */
static const char *raw_join_msg(zb_relay *r, zb_loopback *lb, const uint8_t *msg, size_t len, zb_transport **out) {
  static char reason[96];
  zb_transport *t = zb_loopback_connect(lb);
  zb_relay_update(r);
  t->send(t->ctx, 1, msg, len);
  zb_relay_update(r);
  size_t n = 0;
  bool closed = false;
  const uint8_t *m = raw_message(t, &n, &closed);
  TEST_ASSERT_NOT_NULL(m);
  reason[0] = '\0';
  if (m[0] == ZB_RELAY_REFUSED) {
    memcpy(reason, m + 1, n - 1);
    reason[n - 1] = '\0';
    raw_message(t, &n, &closed);
    TEST_ASSERT_TRUE(closed);
  } else {
    TEST_ASSERT_EQUAL_UINT8(ZB_RELAY_JOINED, m[0]);
  }
  if (out) {
    *out = t;
  } else {
    zb_transport_destroy(t);
  }
  return reason;
}

static const char *raw_join(zb_relay *r, zb_loopback *lb, const char *code, zb_transport **out) {
  uint8_t msg[8] = {ZB_RELAY_JOIN, ZB_RELAY_VERSION};
  memcpy(msg + 2, code, 6);
  return raw_join_msg(r, lb, msg, sizeof msg, out);
}

/* spec: coop-sessions: Relay and invite codes */
static void test_invite_codes(void) {
  char out[ZB_RELAY_CODE_LEN + 1];
  TEST_ASSERT_TRUE(zb_relay_normalize_code("k7m-2pq", out));
  TEST_ASSERT_EQUAL_STRING("K7M2PQ", out);
  TEST_ASSERT_TRUE(zb_relay_normalize_code(" abc def ", out));
  TEST_ASSERT_EQUAL_STRING("ABCDEF", out);
  TEST_ASSERT_FALSE(zb_relay_normalize_code("ABCDE", out));
  TEST_ASSERT_FALSE(zb_relay_normalize_code("ABCDEFG", out));
  TEST_ASSERT_FALSE(zb_relay_normalize_code("ABCDE0", out)); /* no 0, O, 1, I or L */
  TEST_ASSERT_FALSE(zb_relay_normalize_code("ABCDEO", out));
  TEST_ASSERT_FALSE(zb_relay_normalize_code("ABCDE1", out));
  TEST_ASSERT_FALSE(zb_relay_normalize_code("ABCDEI", out));
  TEST_ASSERT_FALSE(zb_relay_normalize_code("abcdel", out));
  /* Codes come from the seed, are unique among live sessions and use the alphabet. */
  g_now = 0;
  char first[3][16], reason[96];
  uint64_t seeds[3] = {42, 42, 43};
  for (int k = 0; k < 3; ++k) {
    zb_loopback *lb = zb_loopback_create(zb_test_alloc());
    zb_relay *r = relay_on(lb, seeds[k], 0);
    zb_transport *hosts[60];
    char codes[60][16];
    for (int i = 0; i < 60; ++i) {
      hosts[i] = raw_register(r, lb, codes[i], reason);
      TEST_ASSERT_TRUE(valid_code(codes[i]));
      for (int j = 0; j < i; ++j) {
        TEST_ASSERT_TRUE(strcmp(codes[i], codes[j]) != 0);
      }
    }
    memcpy(first[k], codes[0], sizeof codes[0]);
    zb_relay_stats st;
    zb_relay_get_stats(r, &st);
    TEST_ASSERT_EQUAL_UINT32(60, st.sessions);
    for (int i = 0; i < 60; ++i) {
      zb_transport_destroy(hosts[i]);
    }
    zb_relay_update(r);
    zb_relay_get_stats(r, &st);
    TEST_ASSERT_EQUAL_UINT32(0, st.sessions);
    zb_relay_destroy(r);
    zb_loopback_destroy(lb);
  }
  TEST_ASSERT_EQUAL_STRING(first[0], first[1]);
  TEST_ASSERT_TRUE(strcmp(first[0], first[2]) != 0);
}

/* spec: coop-sessions: Relay and invite codes */
static void test_relay_forwarding(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_relay *r = relay_on(lb, 1, 2);
  char code[16], reason[96];
  zb_transport *host = raw_register(r, lb, code, reason);
  TEST_ASSERT_TRUE(valid_code(code));
  /* Refusals carry a reason. */
  TEST_ASSERT_EQUAL_STRING("there is no session with this invite code", raw_join(r, lb, "ZZZZZZ", NULL));
  TEST_ASSERT_EQUAL_STRING("there is no session with this invite code", raw_join(r, lb, "000000", NULL));
  static const uint8_t old_join[8] = {ZB_RELAY_JOIN, 9, 'A', 'B', 'C', 'D', 'E', 'F'};
  TEST_ASSERT_EQUAL_STRING("the relay speaks a different protocol version",
                           raw_join_msg(r, lb, old_join, sizeof old_join, NULL));
  static const uint8_t old_reg[2] = {ZB_RELAY_REGISTER, 9};
  TEST_ASSERT_EQUAL_STRING("the relay speaks a different protocol version",
                           raw_join_msg(r, lb, old_reg, sizeof old_reg, NULL));
  static const uint8_t junk[1] = {0x42};
  TEST_ASSERT_EQUAL_STRING("register or join first", raw_join_msg(r, lb, junk, sizeof junk, NULL));
  /* Three clients, typed in any case; a fourth is refused. */
  char lower[8];
  for (int i = 0; i < 6; ++i) {
    lower[i] = (char)(code[i] >= 'A' && code[i] <= 'Z' ? code[i] + 32 : code[i]);
  }
  zb_transport *c[3];
  for (int i = 0; i < 3; ++i) {
    TEST_ASSERT_EQUAL_STRING("", raw_join(r, lb, i == 0 ? lower : code, &c[i]));
  }
  TEST_ASSERT_EQUAL_STRING("the session is full", raw_join(r, lb, code, NULL));
  uint32_t cids[3];
  int opened = 0;
  zb_tr_event ev;
  while (host->next(host->ctx, &ev)) {
    if (ev.kind == ZB_TR_MESSAGE && ev.data[0] == ZB_RELAY_OPEN) {
      cids[opened++] = (uint32_t)ev.data[1] | (uint32_t)ev.data[2] << 8;
    }
  }
  TEST_ASSERT_EQUAL_INT(3, opened);
  TEST_ASSERT_TRUE(cids[0] != cids[1] && cids[1] != cids[2]);
  /* Client to host: tagged with the connection id. Host to client: unchanged. */
  c[0]->send(c[0]->ctx, 1, "hi", 2);
  uint8_t to[7] = {ZB_RELAY_TO, (uint8_t)cids[1], 0, 0, 0, 'y', 'o'};
  host->send(host->ctx, 1, to, sizeof to);
  uint8_t to_nobody[6] = {ZB_RELAY_TO, 99, 0, 0, 0, 'x'};
  host->send(host->ctx, 1, to_nobody, sizeof to_nobody);
  static const uint8_t short_to[2] = {ZB_RELAY_TO, 1};
  host->send(host->ctx, 1, short_to, sizeof short_to);
  static const uint8_t ping[1] = {ZB_RELAY_PING};
  host->send(host->ctx, 1, ping, sizeof ping);
  host->send(host->ctx, 1, ping, 0);
  uint8_t kick[5] = {ZB_RELAY_KICK, (uint8_t)cids[2], 0, 0, 0};
  host->send(host->ctx, 1, kick, sizeof kick);
  zb_relay_update(r);
  size_t n;
  const uint8_t *m = raw_message(host, &n, NULL);
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_UINT8(ZB_RELAY_FROM, m[0]);
  TEST_ASSERT_EQUAL_UINT32(cids[0], m[1]);
  TEST_ASSERT_EQUAL_size_t(7, n);
  TEST_ASSERT_EQUAL_MEMORY("hi", m + 5, 2);
  m = raw_message(c[1], &n, NULL);
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_size_t(2, n);
  TEST_ASSERT_EQUAL_MEMORY("yo", m, 2);
  bool closed = false;
  raw_message(c[2], &n, &closed);
  TEST_ASSERT_TRUE(closed); /* kicked */
  /* A client that goes: the host learns its id. */
  zb_transport_destroy(c[1]);
  zb_relay_update(r);
  m = raw_message(host, &n, NULL);
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_UINT8(ZB_RELAY_CLOSED, m[0]);
  TEST_ASSERT_EQUAL_UINT32(cids[1], m[1]);
  zb_relay_stats st;
  zb_relay_get_stats(r, &st);
  TEST_ASSERT_EQUAL_UINT32(1, st.sessions);
  TEST_ASSERT_EQUAL_UINT32(1, st.clients);
  TEST_ASSERT_EQUAL_UINT64(2, st.forwarded);
  TEST_ASSERT_EQUAL_UINT64(4, st.bytes);
  TEST_ASSERT_EQUAL_UINT64(6, st.refused);
  /* The relay holds at most max_sessions. */
  char code2[16];
  zb_transport *host2 = raw_register(r, lb, code2, reason);
  TEST_ASSERT_TRUE(valid_code(code2));
  char code3[16];
  zb_transport *host3 = raw_register(r, lb, code3, reason);
  TEST_ASSERT_EQUAL_STRING("the relay is full", reason);
  zb_transport_destroy(host3);
  /* The host leaves: its clients are disconnected and the code expires. */
  zb_transport_destroy(host);
  zb_relay_update(r);
  closed = false;
  raw_message(c[0], &n, &closed);
  TEST_ASSERT_TRUE(closed);
  TEST_ASSERT_EQUAL_STRING("this invite code has expired", raw_join(r, lb, code, NULL));
  char code4[16];
  zb_transport *host4 = raw_register(r, lb, code4, reason); /* takes the ended session's place */
  TEST_ASSERT_TRUE(valid_code(code4));
  zb_transport_destroy(host4);
  zb_relay_update(r);
  /* Idle sessions and silent connections time out. */
  zb_transport *silent = zb_loopback_connect(lb);
  zb_relay_update(r);
  g_now += 11 * SECOND;
  zb_relay_update(r);
  closed = false;
  raw_message(silent, &n, &closed);
  TEST_ASSERT_TRUE(closed);
  g_now += 60 * SECOND;
  zb_relay_update(r);
  closed = false;
  raw_message(host2, &n, &closed);
  TEST_ASSERT_TRUE(closed);
  TEST_ASSERT_EQUAL_STRING("this invite code has expired", raw_join(r, lb, code2, NULL));
  zb_transport_destroy(silent);
  zb_transport_destroy(host2);
  zb_transport_destroy(c[0]);
  zb_transport_destroy(c[2]);
  zb_relay_destroy(r);
  zb_relay_destroy(NULL);
  /* Creation needs a listener and memory. */
  zb_relay_config cfg;
  memset(&cfg, 0, sizeof cfg);
  cfg.a = zb_test_alloc();
  TEST_ASSERT_NULL(zb_relay_create(&cfg, NULL));
  zb_failing_allocator fa;
  cfg.a = zb_failing_init(&fa, zb_test_alloc(), 0);
  TEST_ASSERT_NULL(zb_relay_create(&cfg, zb_loopback_listen(lb)));
  zb_loopback_destroy(lb);
}

typedef struct relayed {
  zb_loopback *lb;
  zb_relay *relay;
  zb_net *nets[12];
  int count;
} relayed;

static void step(relayed *s, int rounds) {
  for (int i = 0; i < rounds; ++i) {
    if (s->relay) {
      zb_relay_update(s->relay);
    }
    pump(s->nets, s->count, 1);
  }
}

static zb_net *relay_join(relayed *s, const char *code) {
  zb_net *n = make_net(5);
  zb_transport *t = zb_relay_join_transport(zb_test_alloc(), zb_loopback_connect(s->lb), code);
  TEST_ASSERT_NOT_NULL(t);
  zb_net_join(n, t);
  s->nets[s->count++] = n;
  return n;
}

/* spec: coop-sessions: Joining through the relay */
static void test_sessions_through_relay(void) {
  g_now = 0;
  relayed s;
  memset(&s, 0, sizeof s);
  s.lb = zb_loopback_create(zb_test_alloc());
  s.relay = relay_on(s.lb, 9, 0);
  zb_net *host = make_net(5);
  s.nets[s.count++] = host;
  TEST_ASSERT_EQUAL_STRING("", zb_net_invite_code(host));
  zb_net_host(host, zb_relay_host_transport(zb_test_alloc(), zb_loopback_connect(s.lb), fake_clock, NULL));
  step(&s, 2);
  char code[16];
  snprintf(code, sizeof code, "%s", zb_net_invite_code(host));
  TEST_ASSERT_TRUE(valid_code(code));
  char typed[16];
  snprintf(typed, sizeof typed, "%c%c%c-%c%c%c", code[0] | 32, code[1] | 32, code[2] | 32, code[3] | 32, code[4] | 32,
           code[5] | 32);
  zb_net *c1 = relay_join(&s, typed), *c2 = relay_join(&s, code), *c3 = relay_join(&s, code);
  step(&s, 4);
  TEST_ASSERT_EQUAL_INT(1, zb_net_player(c1));
  TEST_ASSERT_EQUAL_INT(2, zb_net_player(c2));
  TEST_ASSERT_EQUAL_INT(3, zb_net_player(c3));
  TEST_ASSERT_EQUAL_UINT(0xF, zb_net_players(host));
  /* The relay refuses a fourth client and unknown codes, with reasons. */
  zb_net *c4 = relay_join(&s, code);
  zb_net *lost = relay_join(&s, "QQQQQQ");
  step(&s, 3);
  TEST_ASSERT_EQUAL_STRING("the session is full", zb_net_error(c4));
  TEST_ASSERT_EQUAL_STRING("there is no session with this invite code", zb_net_error(lost));
  TEST_ASSERT_FALSE(zb_net_can_resume(lost));
  TEST_ASSERT_NULL(zb_relay_join_transport(zb_test_alloc(), zb_loopback_connect(s.lb), "not a code"));
  TEST_ASSERT_NULL(zb_relay_join_transport(zb_test_alloc(), NULL, code));
  TEST_ASSERT_NULL(zb_relay_host_transport(zb_test_alloc(), NULL, NULL, NULL));
  /* Messages travel as on a direct connection. */
  for (int i = 0; i < s.count; ++i) {
    drain(s.nets[i]);
  }
  zb_net_send(c1, ZB_NET_TO_ALL, "chat", "hello", 5);
  zb_net_send(host, 2, "state", "s", 1);
  step(&s, 3);
  zb_net_event ev;
  TEST_ASSERT_TRUE(expect_event(c3, ZB_NET_EV_DATA, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_EQUAL_MEMORY("hello", ev.data, 5);
  int both = 0; /* the host's own message takes fewer hops than the relayed chat */
  while (expect_event(c2, ZB_NET_EV_DATA, &ev)) {
    both |= strcmp(ev.channel, "state") == 0 ? 1 : strcmp(ev.channel, "chat") == 0 ? 2 : 4;
  }
  TEST_ASSERT_EQUAL_INT(3, both);
  /* A client leaves; another loses its connection and comes back through the relay. */
  zb_net_leave(c3);
  step(&s, 3);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_LEFT, &ev));
  TEST_ASSERT_EQUAL_INT(3, ev.player);
  TEST_ASSERT_TRUE(zb_loopback_cut(s.lb, 2)); /* c1's connection to the relay */
  step(&s, 3);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_AWAY, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_TRUE(zb_net_can_resume(c1));
  TEST_ASSERT_EQUAL_INT(ZB_OK,
                        zb_net_resume(c1, zb_relay_join_transport(zb_test_alloc(), zb_loopback_connect(s.lb), code)));
  step(&s, 4);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_BACK, &ev));
  TEST_ASSERT_EQUAL_INT(1, zb_net_player(c1));
  /* A lone host keeps its session alive with keepalives; one without a clock does not. */
  zb_net *alone = make_net(5), *quiet = make_net(5);
  zb_net_host(alone, zb_relay_host_transport(zb_test_alloc(), zb_loopback_connect(s.lb), fake_clock, NULL));
  zb_net_host(quiet, zb_relay_host_transport(zb_test_alloc(), zb_loopback_connect(s.lb), NULL, NULL));
  s.nets[s.count++] = alone;
  s.nets[s.count++] = quiet;
  step(&s, 2);
  TEST_ASSERT_TRUE(valid_code(zb_net_invite_code(alone)));
  for (int i = 0; i < 14; ++i) {
    g_now += 5 * SECOND;
    step(&s, 2);
  }
  TEST_ASSERT_EQUAL_INT(ZB_NET_HOST, zb_net_role_of(alone));
  TEST_ASSERT_EQUAL_INT(ZB_NET_NONE, zb_net_role_of(quiet));
  TEST_ASSERT_EQUAL_STRING("lost the connection to the relay", zb_net_error(quiet));
  TEST_ASSERT_EQUAL_STRING("", zb_net_invite_code(quiet));
  /* The host ends the session: clients learn it and the code expires. */
  zb_net_leave(host);
  step(&s, 3);
  TEST_ASSERT_EQUAL_STRING("the host ended the session", zb_net_error(c1));
  TEST_ASSERT_EQUAL_STRING("the host ended the session", zb_net_error(c2));
  zb_net *late = relay_join(&s, code);
  step(&s, 3);
  TEST_ASSERT_EQUAL_STRING("this invite code has expired", zb_net_error(late));
  /* The relay goes away: its hosts' sessions end and their clients lose the host. */
  zb_net *guest = make_net(5);
  zb_net_join(guest, zb_relay_join_transport(zb_test_alloc(), zb_loopback_connect(s.lb), zb_net_invite_code(alone)));
  s.nets[s.count++] = guest;
  step(&s, 4);
  TEST_ASSERT_EQUAL_INT(ZB_NET_CLIENT, zb_net_role_of(guest));
  zb_relay_destroy(s.relay);
  s.relay = NULL;
  step(&s, 2);
  TEST_ASSERT_EQUAL_STRING("lost the connection to the relay", zb_net_error(alone));
  TEST_ASSERT_EQUAL_STRING("the connection to the host was lost", zb_net_error(guest));
  for (int i = 0; i < s.count; ++i) {
    zb_net_destroy(s.nets[i]);
  }
  zb_loopback_destroy(s.lb);
}

/* spec: coop-sessions: Relay and invite codes */
static void test_relay_full_and_failing(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_relay *r = relay_on(lb, 3, 1);
  zb_net *a = make_net(5), *b = make_net(5);
  zb_net_host(a, zb_relay_host_transport(zb_test_alloc(), zb_loopback_connect(lb), NULL, NULL));
  zb_net_host(b, zb_relay_host_transport(zb_test_alloc(), zb_loopback_connect(lb), NULL, NULL));
  for (int i = 0; i < 3; ++i) {
    zb_relay_update(r);
    zb_net_update(a);
    zb_net_update(b);
  }
  TEST_ASSERT_EQUAL_INT(ZB_NET_HOST, zb_net_role_of(a));
  TEST_ASSERT_EQUAL_INT(ZB_NET_NONE, zb_net_role_of(b));
  TEST_ASSERT_EQUAL_STRING("the relay is full", zb_net_error(b));
  zb_net_event ev;
  TEST_ASSERT_TRUE(expect_event(b, ZB_NET_EV_ENDED, &ev));
  /* Out of memory: the uplink is not leaked. */
  zb_failing_allocator fa;
  TEST_ASSERT_NULL(
      zb_relay_host_transport(zb_failing_init(&fa, zb_test_alloc(), 0), zb_loopback_connect(lb), NULL, NULL));
  TEST_ASSERT_NULL(
      zb_relay_join_transport(zb_failing_init(&fa, zb_test_alloc(), 0), zb_loopback_connect(lb), "ABCDEF"));
  /* A host transport refuses to send to unknown connections and after the relay is gone. */
  zb_transport *t = zb_relay_host_transport(zb_test_alloc(), zb_loopback_connect(lb), NULL, NULL);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, t->send(t->ctx, 5, "x", 1));
  t->close(t->ctx, 5);
  zb_transport *j = zb_relay_join_transport(zb_test_alloc(), zb_loopback_connect(lb), "ABCDEF");
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, j->send(j->ctx, 1, "x", 1)); /* not joined yet */
  zb_relay_destroy(r);
  t->update(t->ctx);
  j->update(j->ctx);
  zb_tr_event tev;
  while (t->next(t->ctx, &tev)) {
  }
  while (j->next(j->ctx, &tev)) {
  }
  TEST_ASSERT_EQUAL_STRING("lost the connection to the relay", t->failure(t->ctx));
  TEST_ASSERT_EQUAL_STRING("the relay closed the connection", j->failure(j->ctx));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, t->send(t->ctx, 5, "x", 1));
  zb_transport_destroy(t);
  zb_transport_destroy(j);
  zb_net_destroy(a);
  zb_net_destroy(b);
  zb_loopback_destroy(lb);
  /* The connection to the relay fails before it opens. */
  lb = zb_loopback_create(zb_test_alloc());
  zb_loopback_impairment imp = {SECOND, 0, 0, 1, fake_clock, NULL};
  zb_loopback_impair(lb, &imp);
  zb_transport *listener = zb_loopback_listen(lb);
  zb_net *h = make_net(5), *cl = make_net(5);
  zb_net_host(h, zb_relay_host_transport(zb_test_alloc(), zb_loopback_connect(lb), NULL, NULL));
  zb_net_join(cl, zb_relay_join_transport(zb_test_alloc(), zb_loopback_connect(lb), "ABCDEF"));
  zb_loopback_cut(lb, 1);
  zb_loopback_cut(lb, 2);
  zb_net_update(h);
  zb_net_update(cl);
  TEST_ASSERT_EQUAL_STRING("could not reach the relay", zb_net_error(h));
  TEST_ASSERT_EQUAL_STRING("could not reach the relay", zb_net_error(cl));
  zb_net_destroy(h);
  zb_net_destroy(cl);
  zb_transport_destroy(listener);
  zb_loopback_destroy(lb);
}

static uint64_t real_clock(void *ctx) {
  (void)ctx;
  return zb_sock_clock_ns();
}

static char *run_main(int argc, const char **argv, int *code, volatile sig_atomic_t *stop) {
  char path[512];
  snprintf(path, sizeof path, "%s/relay-out.txt", zb_test_tmp_dir());
  FILE *f = fopen(path, "w+");
  TEST_ASSERT_NOT_NULL(f);
  *code = zb_relay_main(argc, (char **)argv, f, stop);
  fclose(f);
  return zb_test_read_file(path, NULL);
}

/* spec: coop-sessions: Relay and invite codes */
/* spec: coop-sessions: Joining through the relay */
static void test_relay_over_websockets(void) {
  if (!zb_sock_supported()) {
    TEST_IGNORE_MESSAGE("no sockets in this build");
  }
  char err[128], url[64];
  uint16_t port = 0;
  zb_relay_config cfg;
  memset(&cfg, 0, sizeof cfg);
  cfg.a = zb_test_alloc();
  cfg.seed = 5;
  cfg.clock_ns = real_clock;
  zb_relay *r = zb_relay_create(&cfg, zb_ws_server_create(zb_test_alloc(), 0, &port, err, sizeof err));
  TEST_ASSERT_NOT_NULL_MESSAGE(r, err);
  snprintf(url, sizeof url, "ws://127.0.0.1:%u/", (unsigned)port);
  zb_net_config c = net_config(5);
  c.clock_ns = real_clock;
  zb_net *host = zb_net_create(&c), *client = zb_net_create(&c);
  zb_net_host(host, zb_relay_host_transport(zb_test_alloc(), zb_ws_client_create(zb_test_alloc(), url, err, sizeof err),
                                            real_clock, NULL));
  for (int i = 0; i < 3000 && !zb_net_invite_code(host)[0]; ++i) {
    zb_relay_update(r);
    zb_net_update(host);
    zb_test_sleep_ms(1);
  }
  TEST_ASSERT_TRUE_MESSAGE(valid_code(zb_net_invite_code(host)), zb_net_error(host));
  zb_net_join(client,
              zb_relay_join_transport(zb_test_alloc(), zb_ws_client_create(zb_test_alloc(), url, err, sizeof err),
                                      zb_net_invite_code(host)));
  for (int i = 0; i < 3000 && zb_net_role_of(client) != ZB_NET_CLIENT; ++i) {
    zb_relay_update(r);
    zb_net_update(host);
    zb_net_update(client);
    zb_test_sleep_ms(1);
  }
  TEST_ASSERT_EQUAL_INT(1, zb_net_player(client));
  zb_net_send(client, 0, "hi", "x", 1);
  zb_net_event ev;
  bool got = false;
  for (int i = 0; i < 3000 && !got; ++i) {
    zb_relay_update(r);
    zb_net_update(client);
    zb_net_update(host);
    got = expect_event(host, ZB_NET_EV_DATA, &ev);
    zb_test_sleep_ms(1);
  }
  TEST_ASSERT_TRUE(got);
  /* A refused join: the relay closes the connection and later hears it closed. */
  zb_net *stranger = zb_net_create(&c);
  zb_net_join(stranger, zb_relay_join_transport(zb_test_alloc(),
                                                zb_ws_client_create(zb_test_alloc(), url, err, sizeof err), "QQQQQQ"));
  for (int i = 0; i < 3000 && zb_net_role_of(stranger) != ZB_NET_NONE; ++i) {
    zb_relay_update(r);
    zb_net_update(stranger);
    zb_test_sleep_ms(1);
  }
  TEST_ASSERT_EQUAL_STRING("there is no session with this invite code", zb_net_error(stranger));
  for (int i = 0; i < 20; ++i) {
    zb_relay_update(r);
    zb_test_sleep_ms(1);
  }
  zb_net_destroy(stranger);
  zb_net_destroy(client);
  zb_net_destroy(host);
  zb_relay_destroy(r);
  /* Nobody listens any more: the relay cannot be reached. */
  /* (The connection may also fail at once, which zb_ws_client_create reports itself.) */
  zb_net *h2 = zb_net_create(&c);
  zb_transport *up = zb_ws_client_create(zb_test_alloc(), url, err, sizeof err);
  if (up) {
    zb_net_host(h2, zb_relay_host_transport(zb_test_alloc(), up, NULL, NULL));
    for (int i = 0; i < 3000 && zb_net_role_of(h2) != ZB_NET_NONE; ++i) {
      zb_net_update(h2);
      zb_test_sleep_ms(1);
    }
    TEST_ASSERT_EQUAL_STRING("could not reach the relay", zb_net_error(h2));
  }
  zb_net_destroy(h2);
}

/* spec: coop-sessions: Relay and invite codes */
static void test_relay_program(void) {
  volatile sig_atomic_t stop = 0;
  int code = -1;
  const char *help[] = {"zauberer-relay", "--help"};
  char *out = run_main(2, help, &code, &stop);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_NOT_NULL(strstr(out, "usage: zauberer-relay"));
  free(out);
  const char *bad[][3] = {{"r", "--bogus", ""}, {"r", "--port", ""}, {"r", "--port", "70000"}, {"r", "--seed", "x"}};
  for (int i = 0; i < 4; ++i) {
    out = run_main(bad[i][2][0] ? 3 : 2, bad[i], &code, &stop);
    TEST_ASSERT_EQUAL_INT(2, code);
    TEST_ASSERT_NOT_NULL(strstr(out, "zauberer-relay: "));
    free(out);
  }
  if (!zb_sock_supported()) {
    const char *run[] = {"r", "--port", "0"};
    out = run_main(3, run, &code, &stop);
    TEST_ASSERT_EQUAL_INT(1, code);
    free(out);
    TEST_IGNORE_MESSAGE("no sockets in this build");
  }
  const char *run[] = {"r", "--port", "0", "--max-seconds", "0", "--seed", "5", "--idle", "0", "--max-sessions", "0"};
  out = run_main(11, run, &code, &stop);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_NOT_NULL(strstr(out, "zauberer-relay listening on port "));
  TEST_ASSERT_NOT_NULL(strstr(out, "zauberer-relay stopped"));
  free(out);
  /* Stopped from outside (a signal in the program). */
  stop = 1;
  const char *stopped[] = {"r", "--port", "0"};
  out = run_main(3, stopped, &code, &stop);
  TEST_ASSERT_EQUAL_INT(0, code);
  free(out);
  /* A port in use. */
  char err[64], port_text[16];
  uint16_t port = 0;
  zb_transport *busy = zb_ws_server_create(zb_test_alloc(), 0, &port, err, sizeof err);
  snprintf(port_text, sizeof port_text, "%u", (unsigned)port);
  const char *taken[] = {"r", "--port", port_text};
  out = run_main(3, taken, &code, &stop);
  TEST_ASSERT_EQUAL_INT(1, code);
  TEST_ASSERT_NOT_NULL(strstr(out, "cannot listen"));
  free(out);
  zb_transport_destroy(busy);
}

/* spec: coop-sessions: Relay and invite codes */
static void test_relay_late_messages(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_loopback_impairment imp = {10000000ull, 0, 0, 1, fake_clock, NULL};
  zb_loopback_impair(lb, &imp);
  zb_relay *r = relay_on(lb, 1, 0);
  zb_transport *host = zb_loopback_connect(lb), *client = zb_loopback_connect(lb);
  static const uint8_t reg[2] = {ZB_RELAY_REGISTER, ZB_RELAY_VERSION};
  g_now = 20000000ull;
  host->update(host->ctx);
  host->send(host->ctx, 1, reg, sizeof reg);
  char code[16] = "";
  size_t n;
  for (int i = 0; i < 10 && !code[0]; ++i) {
    g_now += 10000000ull;
    zb_relay_update(r);
    host->update(host->ctx);
    const uint8_t *m = raw_message(host, &n, NULL);
    if (m && m[0] == ZB_RELAY_CODE) {
      memcpy(code, m + 1, 6);
      code[6] = '\0';
    }
  }
  TEST_ASSERT_TRUE(valid_code(code));
  uint8_t join[8] = {ZB_RELAY_JOIN, ZB_RELAY_VERSION};
  memcpy(join + 2, code, 6);
  client->update(client->ctx);
  client->send(client->ctx, 1, join, sizeof join);
  for (int i = 0; i < 4; ++i) {
    g_now += 10000000ull;
    zb_relay_update(r);
  }
  /* The host kicks the client while the client's message is still on its way. */
  static const uint8_t kick[5] = {ZB_RELAY_KICK, 1, 0, 0, 0};
  host->send(host->ctx, 1, kick, sizeof kick);
  client->send(client->ctx, 1, "late", 4);
  g_now += 20000000ull;
  zb_relay_update(r);
  zb_relay_stats st;
  zb_relay_get_stats(r, &st);
  TEST_ASSERT_EQUAL_UINT32(0, st.clients);
  TEST_ASSERT_EQUAL_UINT64(0, st.forwarded);
  zb_transport_destroy(client);
  zb_transport_destroy(host);
  zb_relay_destroy(r);
  zb_loopback_destroy(lb);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_invite_codes);
  RUN_TEST(test_relay_forwarding);
  RUN_TEST(test_sessions_through_relay);
  RUN_TEST(test_relay_full_and_failing);
  RUN_TEST(test_relay_late_messages);
  RUN_TEST(test_relay_over_websockets);
  RUN_TEST(test_relay_program);
  return UNITY_END();
}

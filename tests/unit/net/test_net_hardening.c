/* SPDX-License-Identifier: MIT */
/* Network hardening: impaired loopback connections, reconnecting with a
 * resume token, and the per-connection bandwidth budget. */
#include <stdio.h>
#include <string.h>

#include "net/session.h"
#include "net/transport.h"
#include "support/test_support.h"
#include "unity.h"

#include "net_support.h"

ZB_TEST_LEAK_CHECKED

#define MS 1000000ull
#define SECOND 1000000000ull

typedef struct trace {
  uint64_t welcome_at;
  uint64_t arrival[30];
} trace;

/* A client joins over an impaired network and sends 30 numbered messages, one per 10 ms. */
static void impaired_run(uint64_t seed, trace *out) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_loopback_impairment imp = {100 * MS, 50 * MS, 0, seed, fake_clock, NULL};
  zb_loopback_impair(lb, &imp);
  zb_net *host = make_net(1), *client = make_net(1);
  zb_net_host(host, zb_loopback_listen(lb));
  zb_net_join(client, zb_loopback_connect(lb));
  memset(out, 0, sizeof *out);
  int sent = 0, got = 0;
  for (int step = 0; step < 200 && got < 30; ++step) {
    g_now = (uint64_t)step * 10 * MS;
    zb_net_update(host);
    zb_net_update(client);
    zb_net_event ev;
    while (zb_net_next(client, &ev)) {
      if (ev.kind == ZB_NET_EV_WELCOME) {
        out->welcome_at = g_now;
      }
    }
    if (zb_net_role_of(client) == ZB_NET_CLIENT && sent < 30) {
      uint8_t seq = (uint8_t)sent++;
      zb_net_send(client, 0, "seq", &seq, 1);
    }
    while (zb_net_next(host, &ev)) {
      if (ev.kind == ZB_NET_EV_DATA) {
        TEST_ASSERT_EQUAL_UINT8(got, ev.data[0]); /* in order, none lost */
        out->arrival[got++] = g_now;
      }
    }
  }
  TEST_ASSERT_EQUAL_INT(30, got);
  zb_net_destroy(client);
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
}

/* spec: network-hardening: Impaired connections in tests */
static void test_impaired_latency(void) {
  trace a, b, c;
  impaired_run(7, &a);
  impaired_run(7, &b);
  impaired_run(8, &c);
  /* The connection opens, the hello goes up and the welcome comes down: three one-way delays. */
  TEST_ASSERT_TRUE(a.welcome_at >= 300 * MS && a.welcome_at <= 470 * MS);
  for (int i = 0; i < 30; ++i) {
    uint64_t sent_at = a.welcome_at + (uint64_t)i * 10 * MS;
    uint64_t delay = a.arrival[i] - sent_at;
    TEST_ASSERT_TRUE(delay >= 100 * MS && delay <= 160 * MS);
  }
  /* Deterministic for a seed, different for another. */
  TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof a);
  TEST_ASSERT_TRUE(memcmp(&a, &c, sizeof a) != 0);
}

static uint64_t first_drop(uint64_t seed, double rate) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_loopback_impairment imp = {20 * MS, 0, rate, seed, fake_clock, NULL};
  zb_loopback_impair(lb, &imp);
  zb_net *host = make_net(1), *client = make_net(1);
  zb_net_host(host, zb_loopback_listen(lb));
  zb_net_join(client, zb_loopback_connect(lb));
  uint64_t dropped = 0;
  for (int step = 0; step < 6000 && !dropped; ++step) {
    g_now = (uint64_t)step * 50 * MS;
    zb_net_update(host);
    zb_net_update(client);
    if (zb_loopback_cuts(lb) > 0) {
      dropped = g_now;
      zb_net_event ev;
      TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_AWAY, &ev));
      TEST_ASSERT_TRUE(expect_event(client, ZB_NET_EV_ENDED, &ev));
      TEST_ASSERT_EQUAL_STRING("the connection to the host was lost", zb_net_error(client));
      TEST_ASSERT_TRUE(zb_net_can_resume(client));
    }
  }
  zb_net_destroy(client);
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
  return dropped;
}

/* spec: network-hardening: Impaired connections in tests */
static void test_impaired_drops(void) {
  uint64_t a = first_drop(3, 0.05), b = first_drop(3, 0.05);
  TEST_ASSERT_TRUE(a > 0);
  TEST_ASSERT_EQUAL_UINT64(a, b);                       /* the same second for the same seed */
  TEST_ASSERT_EQUAL_UINT64(SECOND, first_drop(4, 1.0)); /* certain: at the first second */
  /* Removing impairment delivers what is in flight; cutting an unknown connection does nothing. */
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_loopback_impairment imp = {30 * MS, 0, 0, 1, fake_clock, NULL};
  zb_loopback_impair(lb, &imp);
  zb_transport *l = zb_loopback_listen(lb);
  zb_transport *c = zb_loopback_connect(lb);
  zb_transport *gone = zb_loopback_connect(lb);
  TEST_ASSERT_EQUAL_INT(ZB_OK, l->send(l->ctx, 2, "late", 4)); /* to a client that goes away */
  zb_transport_destroy(gone);
  zb_loopback_impair(lb, NULL);
  TEST_ASSERT_EQUAL_INT(ZB_OK, c->send(c->ctx, 1, "now", 3)); /* without impairment: at once */
  size_t n = 0;
  TEST_ASSERT_NOT_NULL(raw_message(l, &n, NULL));
  TEST_ASSERT_EQUAL_size_t(3, n);
  c->update(c->ctx);
  TEST_ASSERT_NULL(raw_message(c, &n, NULL)); /* not due yet */
  g_now = 40 * MS;
  c->update(c->ctx);
  zb_tr_event ev;
  bool open = false;
  while (c->next(c->ctx, &ev)) {
    open = open || ev.kind == ZB_TR_OPEN;
  }
  TEST_ASSERT_TRUE(open);
  TEST_ASSERT_FALSE(zb_loopback_cut(lb, 99));
  TEST_ASSERT_TRUE(zb_loopback_cut(lb, 1));
  TEST_ASSERT_FALSE(zb_loopback_cut(lb, 1));
  /* Messages towards a listener that goes away are dropped. */
  zb_loopback_impair(lb, &imp);
  zb_transport *d = zb_loopback_connect(lb);
  d->send(d->ctx, 1, "x", 1);
  zb_transport_destroy(l);
  g_now = SECOND;
  d->update(d->ctx);
  bool closed = false;
  raw_message(d, &n, &closed);
  TEST_ASSERT_TRUE(closed);
  zb_transport_destroy(d);
  zb_transport_destroy(c);
  /* Events still in flight when the network goes away are freed. */
  zb_transport *l2 = zb_loopback_listen(lb);
  zb_transport *e = zb_loopback_connect(lb);
  l2->send(l2->ctx, 4, "bye", 3);
  zb_transport_destroy(e);
  zb_transport_destroy(l2);
  zb_loopback_destroy(lb);
}

/* A raw protocol-2 hello carrying a resume token. */
static void raw_hello(zb_transport *t, uint64_t token) {
  uint8_t h[31];
  memset(h, 0, sizeof h);
  h[0] = 1;
  h[1] = 2;
  h[3] = 5; /* world seed 5 */
  h[11] = 77;
  for (int i = 0; i < 8; ++i) {
    h[19 + i] = (uint8_t)(token >> (8 * i));
  }
  t->send(t->ctx, 1, h, sizeof h);
}

static uint64_t welcome_token(zb_transport *t, int *player) {
  size_t n;
  const uint8_t *m;
  while ((m = raw_message(t, &n, NULL)) != NULL) {
    if (m[0] == 2 && n >= 19) {
      *player = m[1];
      uint64_t k = 0;
      for (int i = 7; i >= 0; --i) {
        k = k << 8 | m[11 + i];
      }
      return k;
    }
  }
  return 0;
}

/* spec: network-hardening: Reconnecting keeps the heir */
static void test_reconnect(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_net *host = make_net(5), *c1 = make_net(5), *c2 = make_net(5);
  zb_net_host(host, zb_loopback_listen(lb));
  zb_net_join(c1, zb_loopback_connect(lb));
  zb_net_join(c2, zb_loopback_connect(lb));
  zb_net *all[] = {host, c1, c2};
  pump(all, 3, 3);
  drain(host);
  drain(c2);
  TEST_ASSERT_FALSE(zb_net_can_resume(c1));
  /* The network fails under player 1: the host keeps the place. */
  TEST_ASSERT_TRUE(zb_loopback_cut(lb, 1));
  pump(all, 3, 1);
  zb_net_event ev;
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_AWAY, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_TRUE(expect_event(c2, ZB_NET_EV_AWAY, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_EQUAL_UINT(0x7, zb_net_players(host));
  TEST_ASSERT_EQUAL_UINT(0x2, zb_net_away(host));
  TEST_ASSERT_EQUAL_UINT(0x2, zb_net_away(c2));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, zb_net_send(host, 1, "x", NULL, 0));
  TEST_ASSERT_TRUE(expect_event(c1, ZB_NET_EV_ENDED, &ev));
  TEST_ASSERT_EQUAL_STRING("the connection to the host was lost", zb_net_error(c1));
  TEST_ASSERT_TRUE(zb_net_can_resume(c1));
  /* Within the grace period it comes back as the same player. */
  for (int i = 0; i < 20; ++i) {
    g_now += SECOND;
    pump(all, 3, 1);
  }
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_resume(c1, zb_loopback_connect(lb)));
  pump(all, 3, 3);
  TEST_ASSERT_TRUE(expect_event(c1, ZB_NET_EV_WELCOME, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_BACK, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_TRUE(expect_event(c2, ZB_NET_EV_BACK, &ev));
  TEST_ASSERT_EQUAL_UINT(0, zb_net_away(host));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send(c1, ZB_NET_TO_ALL, "hi", "x", 1));
  pump(all, 3, 2);
  TEST_ASSERT_TRUE(expect_event(c2, ZB_NET_EV_DATA, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  /* A new connection with the token replaces one the host still thinks alive. */
  zb_transport *raw1 = zb_loopback_connect(lb);
  zb_net_update(host);
  raw_hello(raw1, 0);
  zb_net_update(host);
  int p1 = -1, p2 = -1;
  uint64_t token = welcome_token(raw1, &p1);
  TEST_ASSERT_EQUAL_INT(3, p1);
  TEST_ASSERT_TRUE(token != 0);
  zb_transport *raw2 = zb_loopback_connect(lb);
  zb_net_update(host);
  raw_hello(raw2, token);
  zb_net_update(host);
  TEST_ASSERT_EQUAL_UINT64(token, welcome_token(raw2, &p2));
  TEST_ASSERT_EQUAL_INT(3, p2);
  bool closed = false;
  size_t n;
  raw_message(raw1, &n, &closed);
  TEST_ASSERT_TRUE(closed);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_BACK, &ev));
  TEST_ASSERT_EQUAL_INT(3, ev.player);
  /* After the grace period the place is gone. */
  zb_transport_destroy(raw1);
  zb_transport_destroy(raw2);
  zb_loopback_cut(lb, 3); /* c1's second connection */
  uint64_t lost_at = g_now;
  bool left = false;
  while (!left) {
    g_now += SECOND;
    pump(all, 3, 1);
    while (zb_net_next(host, &ev)) {
      if (ev.kind == ZB_NET_EV_LEFT && ev.player == 1) {
        left = true;
      }
    }
  }
  TEST_ASSERT_TRUE(g_now - lost_at >= 30 * SECOND && g_now - lost_at <= 31 * SECOND);
  zb_net *c3 = make_net(5);
  zb_net_join(c3, zb_loopback_connect(lb));
  zb_net *all4[] = {host, c1, c2, c3};
  pump(all4, 4, 3);
  TEST_ASSERT_EQUAL_INT(1, zb_net_player(c3));
  TEST_ASSERT_TRUE(zb_net_can_resume(c1));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_resume(c1, zb_loopback_connect(lb)));
  pump(all4, 4, 3);
  TEST_ASSERT_EQUAL_INT(ZB_NET_CLIENT, zb_net_role_of(c1));
  TEST_ASSERT_EQUAL_INT(3, zb_net_player(c1)); /* a new place: the old token expired */
  /* Leaving on purpose cannot be resumed; neither can a host that ended the session. */
  zb_net_leave(c3);
  TEST_ASSERT_FALSE(zb_net_can_resume(c3));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, zb_net_resume(c3, zb_loopback_connect(lb)));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_resume(c3, NULL));
  pump(all4, 3, 1);
  TEST_ASSERT_TRUE(expect_event(host, ZB_NET_EV_LEFT, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  /* Someone joining while a player is away learns it. */
  TEST_ASSERT_TRUE(zb_loopback_cut(lb, 7)); /* c1's third connection */
  pump(all4, 3, 1);
  zb_net *c5 = make_net(5);
  zb_net_join(c5, zb_loopback_connect(lb));
  zb_net *all5[] = {host, c2, c5};
  pump(all5, 3, 3);
  TEST_ASSERT_EQUAL_INT(1, zb_net_player(c5));
  TEST_ASSERT_EQUAL_UINT(0x8, zb_net_away(c5));
  TEST_ASSERT_TRUE(expect_event(c5, ZB_NET_EV_AWAY, &ev));
  TEST_ASSERT_EQUAL_INT(3, ev.player);
  zb_net_destroy(c5);
  zb_net_leave(host);
  pump(all4, 3, 1);
  TEST_ASSERT_EQUAL_STRING("the host ended the session", zb_net_error(c2));
  TEST_ASSERT_FALSE(zb_net_can_resume(c2));
  zb_net_destroy(c3);
  zb_net_destroy(c1);
  zb_net_destroy(c2);
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
}

/* spec: network-hardening: Reconnecting keeps the heir */
static void test_resume_failures(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_net *host = make_net(5), *client = make_net(5);
  zb_net_host(host, zb_loopback_listen(lb));
  zb_net_join(client, zb_loopback_connect(lb));
  zb_net *all[] = {host, client};
  pump(all, 2, 3);
  /* A host that stops answering: the client may try again. */
  g_now = 11 * SECOND;
  zb_net_update(client);
  TEST_ASSERT_EQUAL_STRING("the host stopped answering", zb_net_error(client));
  TEST_ASSERT_TRUE(zb_net_can_resume(client));
  zb_net_destroy(host);
  /* The new connection fails before the welcome: still resumable. */
  zb_transport *raw = zb_loopback_listen(lb);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_resume(client, zb_loopback_connect(lb)));
  zb_tr_event ev;
  zb_net_update(client);
  while (raw->next(raw->ctx, &ev)) {
  }
  raw->close(raw->ctx, 2);
  zb_net_update(client);
  TEST_ASSERT_EQUAL_STRING("could not reach the host", zb_net_error(client));
  TEST_ASSERT_TRUE(zb_net_can_resume(client));
  /* Leaving clears it. */
  zb_net_leave(client);
  TEST_ASSERT_FALSE(zb_net_can_resume(client));
  zb_net_destroy(client);
  zb_transport_destroy(raw);
  zb_loopback_destroy(lb);
}

static int count_state(zb_net *n, int *last) {
  int count = 0;
  zb_net_event ev;
  while (zb_net_next(n, &ev)) {
    if (ev.kind == ZB_NET_EV_DATA && strcmp(ev.channel, "state") == 0) {
      count++;
      *last = ev.data[0];
    }
  }
  return count;
}

/* spec: network-hardening: Bandwidth budget */
static void test_bandwidth_budget(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_net *host = make_net(5), *client = make_net(5);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_set_channel_class(host, "state", ZB_NET_LATEST));
  zb_net_host(host, zb_loopback_listen(lb));
  zb_net_join(client, zb_loopback_connect(lb));
  zb_net *all[] = {host, client};
  pump(all, 2, 3);
  drain(client);
  /* A burst: the budget (one second at 256 kbit/s) lets some through, the rest
   * waits and is coalesced; reliable messages all go. */
  uint8_t msg[1000];
  memset(msg, 0, sizeof msg);
  for (int i = 0; i < 100; ++i) {
    msg[0] = (uint8_t)i;
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send(host, 1, "state", msg, sizeof msg));
    uint8_t c = (uint8_t)i;
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send(host, ZB_NET_TO_ALL, "chat", &c, 1));
  }
  pump(all, 2, 1);
  int last = -1;
  zb_net_event ev;
  int state = 0, chat = 0;
  while (zb_net_next(client, &ev)) {
    if (ev.kind == ZB_NET_EV_DATA) {
      state += strcmp(ev.channel, "state") == 0;
      chat += strcmp(ev.channel, "chat") == 0;
      last = strcmp(ev.channel, "state") == 0 ? ev.data[0] : last;
    }
  }
  TEST_ASSERT_EQUAL_INT(100, chat);
  TEST_ASSERT_TRUE(state >= 25 && state <= 33);
  TEST_ASSERT_TRUE(last < 99);
  zb_net_stats st;
  zb_net_get_stats(host, &st);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)(100 - state - 1), st.coalesced);
  /* Once budget is back, the newest one is sent. */
  g_now += SECOND;
  pump(all, 2, 2);
  TEST_ASSERT_EQUAL_INT(1, count_state(client, &last));
  TEST_ASSERT_EQUAL_INT(99, last);
  /* Sustained: 320 kbit/s offered for 10 s, about 64 kbit/s delivered (plus the burst). */
  uint64_t start = g_now;
  uint8_t big[2000];
  memset(big, 0, sizeof big);
  int received = 0;
  for (int step = 1; step <= 200; ++step) {
    g_now = start + (uint64_t)step * 50 * MS;
    big[0] = (uint8_t)step;
    zb_net_send(host, 1, "state", big, sizeof big);
    pump(all, 2, 1);
    received += count_state(client, &last);
  }
  double kbps = (double)received * 2004.0 * 8.0 / 1000.0 / 10.0;
  TEST_ASSERT_TRUE_MESSAGE(kbps > 55.0 && kbps < 100.0, "average close to the budget");
  TEST_ASSERT_TRUE(last >= 190); /* what arrives is recent */
  zb_net_get_stats(host, &st);
  TEST_ASSERT_TRUE(st.kbps_out[1] >= 50 && st.kbps_out[1] <= 90);
  TEST_ASSERT_TRUE(st.sent_to[1] > 0 && st.received_from[1] > 0);
  zb_net_stats cs;
  zb_net_get_stats(client, &cs);
  TEST_ASSERT_TRUE(cs.kbps_in[0] >= 50 && cs.kbps_in[0] <= 90);
  TEST_ASSERT_TRUE(cs.received_from[0] > 0);
  /* A bigger budget lets more through; zero keeps the value. */
  zb_net_set_budget(host, 0, 0);
  zb_net_set_budget(host, 512, 1024);
  g_now += 2 * SECOND;
  pump(all, 2, 1);
  count_state(client, &last);
  for (int i = 0; i < 50; ++i) {
    zb_net_send(host, 1, "state", big, sizeof big);
  }
  pump(all, 2, 1);
  TEST_ASSERT_TRUE(count_state(client, &last) >= 50);
  /* The host forwards latest-only messages of one client to another under the same rules. */
  zb_net *c2 = make_net(5);
  zb_net_set_channel_class(c2, "state", ZB_NET_LATEST);
  zb_net_join(c2, zb_loopback_connect(lb));
  zb_net *all3[] = {host, client, c2};
  pump(all3, 3, 3);
  drain(client);
  for (int i = 0; i < 60; ++i) {
    big[0] = (uint8_t)i;
    zb_net_send(c2, ZB_NET_TO_ALL, "state", big, sizeof big);
  }
  pump(all3, 3, 2);
  g_now += 5 * SECOND;
  pump(all3, 3, 3);
  int forwarded = count_state(client, &last);
  TEST_ASSERT_TRUE(forwarded > 0 && forwarded < 60);
  TEST_ASSERT_EQUAL_INT(59, last);
  /* Waiting messages go with their connection, and with the session. */
  for (int i = 0; i < 200; ++i) {
    zb_net_send(host, ZB_NET_TO_ALL, "state", big, sizeof big);
    zb_net_send(c2, 0, "state", big, sizeof big);
  }
  zb_net_leave(c2);
  pump(all3, 2, 1);
  zb_net_send(host, 1, "state", big, sizeof big);
  zb_net_destroy(c2);
  zb_net_destroy(client);
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
}

/* spec: network-hardening: Bandwidth budget */
static void test_channel_classes(void) {
  zb_net *n = make_net(1);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_set_channel_class(n, "", ZB_NET_LATEST));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID,
                        zb_net_set_channel_class(n, "a_channel_name_longer_than_32_bytes", ZB_NET_LATEST));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_set_channel_class(n, "x", (zb_net_channel_class)7));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_set_channel_class(n, "x", ZB_NET_LATEST));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_set_channel_class(n, "x", ZB_NET_RELIABLE));
  char name[8];
  for (int i = 1; i < 32; ++i) {
    snprintf(name, sizeof name, "c%d", i);
    TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_set_channel_class(n, name, ZB_NET_LATEST));
  }
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM, zb_net_set_channel_class(n, "one_more", ZB_NET_LATEST));
  zb_net_destroy(n);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_impaired_latency);
  RUN_TEST(test_impaired_drops);
  RUN_TEST(test_reconnect);
  RUN_TEST(test_resume_failures);
  RUN_TEST(test_bandwidth_budget);
  RUN_TEST(test_channel_classes);
  return UNITY_END();
}

/* SPDX-License-Identifier: MIT */
/* LAN discovery over UDP, tested on this machine (127.0.0.1 stands in for
 * the broadcast address). Skipped only where UDP sockets cannot be created. */
#include <stdio.h>
#include <string.h>

#include "net/lan.h"
#include "net/session.h"
#include "net/socket.h"
#include "net/transport.h"
#include "support/test_support.h"
#include "support/test_threads.h"
#include "unity.h"

#include "net_support.h"

ZB_TEST_LEAK_CHECKED

#define SECOND 1000000000ull

/* spec: coop-sessions: LAN discovery */
static void test_lan_datagrams(void) {
  uint8_t ip[4];
  TEST_ASSERT_TRUE(zb_ipv4_parse("192.168.1.20", ip));
  TEST_ASSERT_EQUAL_UINT8(192, ip[0]);
  TEST_ASSERT_EQUAL_UINT8(20, ip[3]);
  TEST_ASSERT_TRUE(zb_ipv4_parse("255.255.255.255", ip));
  TEST_ASSERT_FALSE(zb_ipv4_parse("256.1.1.1", ip));
  TEST_ASSERT_FALSE(zb_ipv4_parse("1.2.3", ip));
  TEST_ASSERT_FALSE(zb_ipv4_parse("1.2.3.4.5", ip));
  TEST_ASSERT_FALSE(zb_ipv4_parse("a.b.c.d", ip));
  TEST_ASSERT_FALSE(zb_ipv4_parse("1.2.3.0004", ip));
  zb_lan_session s, d;
  memset(&s, 0, sizeof s);
  snprintf(s.name, sizeof s.name, "Ada's world");
  s.port = 47000;
  s.protocol = ZB_NET_PROTOCOL;
  s.players = 2;
  s.max_players = 4;
  uint8_t buf[64];
  size_t n = zb_lan_encode_answer(&s, buf, sizeof buf);
  TEST_ASSERT_EQUAL_size_t(13 + strlen("Ada's world"), n);
  TEST_ASSERT_TRUE(zb_lan_decode_answer(buf, n, &d));
  TEST_ASSERT_EQUAL_STRING("Ada's world", d.name);
  TEST_ASSERT_EQUAL_UINT16(47000, d.port);
  TEST_ASSERT_EQUAL_UINT16(ZB_NET_PROTOCOL, d.protocol);
  TEST_ASSERT_EQUAL_UINT8(2, d.players);
  TEST_ASSERT_EQUAL_UINT8(4, d.max_players);
  TEST_ASSERT_EQUAL_size_t(0, zb_lan_encode_answer(&s, buf, 10)); /* does not fit */
  TEST_ASSERT_FALSE(zb_lan_decode_answer(buf, n - 1, &d));
  TEST_ASSERT_FALSE(zb_lan_decode_answer(buf, 5, &d));
  buf[4] = 9;
  TEST_ASSERT_FALSE(zb_lan_decode_answer(buf, n, &d)); /* another version */
  buf[4] = ZB_LAN_VERSION;
  buf[0] = 'X';
  TEST_ASSERT_FALSE(zb_lan_decode_answer(buf, n, &d));
  memset(s.name, 'n', ZB_LAN_MAX_NAME);
  s.name[ZB_LAN_MAX_NAME] = '\0';
  TEST_ASSERT_EQUAL_size_t(13 + ZB_LAN_MAX_NAME, zb_lan_encode_answer(&s, buf, sizeof buf));
  buf[12] = ZB_LAN_MAX_NAME + 1;
  TEST_ASSERT_FALSE(zb_lan_decode_answer(buf, 14 + ZB_LAN_MAX_NAME, &d));
}

static void wait_a_little(void) {
  if (zb_test_thread_api()) {
    zb_test_sleep_ms(2);
  }
}

/* spec: coop-sessions: LAN discovery */
static void test_lan_discovery(void) {
  char err[96];
  if (!zb_sock_supported()) {
    TEST_ASSERT_NULL(zb_lan_open(zb_test_alloc(), 0, NULL, err, sizeof err));
    TEST_ASSERT_EQUAL_STRING("unavailable", err);
    TEST_IGNORE_MESSAGE("no sockets in this build");
  }
  uint16_t port = 0;
  zb_lan *probe = zb_lan_open(zb_test_alloc(), 0, &port, err, sizeof err);
  if (!probe) {
    TEST_IGNORE_MESSAGE("UDP sockets cannot be created here");
  }
  zb_lan_close(probe); /* a free port for the host to announce on */
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_net *host = make_net(1);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_lan_announce(host, "Lan party", 47001, port, err, sizeof err));
  TEST_ASSERT_TRUE(zb_net_lan_announcing(host));
  zb_net_host(host, zb_loopback_listen(lb));
  zb_net *client = make_net(1);
  zb_net_join(client, zb_loopback_connect(lb));
  zb_net *all[] = {host, client};
  pump(all, 2, 3);
  uint16_t scan_port = 0;
  zb_lan *scan = zb_lan_open(zb_test_alloc(), 0, &scan_port, err, sizeof err);
  TEST_ASSERT_NOT_NULL_MESSAGE(scan, err);
  /* Stray datagrams are ignored by both sides. */
  static const uint8_t junk[] = {'Z', 'B', 'L', 'Q', 9, 0};
  zb_socket stray = zb_udp_open(0, NULL);
  TEST_ASSERT_TRUE(stray != ZB_SOCKET_NONE);
  TEST_ASSERT_EQUAL_INT(6, (int)zb_udp_send(stray, "127.0.0.1", port, junk, sizeof junk));
  TEST_ASSERT_EQUAL_INT(6, (int)zb_udp_send(stray, "127.0.0.1", scan_port, junk, sizeof junk));
  zb_sock_close(stray);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_lan_query(scan, "127.0.0.1", port));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_IO, zb_lan_query(scan, "not an address", port));
  size_t found = 0;
  for (int i = 0; i < 2000 && found == 0; ++i) {
    zb_net_update(host);
    found = zb_lan_collect(scan, g_now, 3 * SECOND);
    wait_a_little();
  }
  TEST_ASSERT_EQUAL_size_t(1, found);
  const zb_lan_session *s = zb_lan_found(scan, 0);
  TEST_ASSERT_EQUAL_STRING("Lan party", s->name);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", s->address);
  TEST_ASSERT_EQUAL_UINT16(47001, s->port);
  TEST_ASSERT_EQUAL_UINT16(ZB_NET_PROTOCOL, s->protocol);
  TEST_ASSERT_EQUAL_UINT8(2, s->players);
  TEST_ASSERT_EQUAL_UINT8(4, s->max_players);
  TEST_ASSERT_NULL(zb_lan_found(scan, 1));
  /* A second answer updates the entry; old entries are forgotten. */
  g_now = SECOND;
  zb_lan_query(scan, "127.0.0.1", port);
  const zb_lan_session *again = NULL;
  for (int i = 0; i < 2000; ++i) {
    zb_net_update(host);
    zb_lan_collect(scan, g_now, 3 * SECOND);
    again = zb_lan_found(scan, 0);
    if (again && again->seen_ns == SECOND) {
      break;
    }
    wait_a_little();
  }
  TEST_ASSERT_EQUAL_UINT64(SECOND, again->seen_ns);
  TEST_ASSERT_EQUAL_size_t(1, zb_lan_collect(scan, 3 * SECOND, 3 * SECOND));
  TEST_ASSERT_EQUAL_size_t(0, zb_lan_collect(scan, 10 * SECOND, 3 * SECOND));
  /* Only a host answers: after leaving, queries are read and ignored. */
  zb_net_leave(host);
  zb_lan_query(scan, "127.0.0.1", port);
  for (int i = 0; i < 50; ++i) {
    zb_net_update(host);
    TEST_ASSERT_EQUAL_size_t(0, zb_lan_collect(scan, 10 * SECOND, 3 * SECOND));
    wait_a_little();
  }
  /* A host that announces answers anyone on the port (an announcer seen directly). */
  uint16_t direct_port = 0;
  zb_lan *ann = zb_lan_open(zb_test_alloc(), 0, &direct_port, err, sizeof err);
  TEST_ASSERT_NOT_NULL(ann);
  zb_lan_session me;
  memset(&me, 0, sizeof me);
  snprintf(me.name, sizeof me.name, "direct");
  zb_lan_query(scan, "127.0.0.1", direct_port);
  found = 0;
  for (int i = 0; i < 2000 && found == 0; ++i) {
    zb_lan_answer(ann, &me);
    found = zb_lan_collect(scan, 10 * SECOND, 3 * SECOND);
    wait_a_little();
  }
  TEST_ASSERT_EQUAL_size_t(1, found);
  TEST_ASSERT_EQUAL_STRING("direct", zb_lan_found(scan, 0)->name);
  zb_lan_close(ann);
  zb_lan_close(scan);
  zb_lan_close(NULL);
  zb_net_lan_stop(host);
  TEST_ASSERT_FALSE(zb_net_lan_announcing(host));
  zb_net_destroy(client);
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
}

/* spec: coop-sessions: LAN discovery */
static void test_lan_errors(void) {
  char err[96];
  zb_failing_allocator fa;
  if (zb_sock_supported()) {
    TEST_ASSERT_NULL(zb_lan_open(zb_failing_init(&fa, zb_test_alloc(), 0), 0, NULL, err, sizeof err));
    TEST_ASSERT_EQUAL_STRING("out of memory", err);
    zb_net_config c = net_config(1);
    c.a = zb_failing_init(&fa, zb_test_alloc(), 1);
    zb_net *n = zb_net_create(&c);
    TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT_EQUAL_INT(ZB_ERR_IO, zb_net_lan_announce(n, "x", 1, 0, err, sizeof err));
    TEST_ASSERT_FALSE(zb_net_lan_announcing(n));
    zb_net_destroy(n);
  }
  TEST_ASSERT_EQUAL_INT(ZB_SOCK_CLOSED, (int)zb_udp_send(ZB_SOCKET_NONE, "bad", 1, "x", 1));
  /* The relay's clock is monotonic; sleeping waits (the web has no relay: there it returns at once). */
  uint64_t a = zb_sock_clock_ns();
  zb_sock_sleep_ms(2);
  TEST_ASSERT_TRUE(zb_sock_clock_ns() >= a + (zb_sock_supported() ? 1000000u : 0u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_lan_datagrams);
  RUN_TEST(test_lan_discovery);
  RUN_TEST(test_lan_errors);
  return UNITY_END();
}

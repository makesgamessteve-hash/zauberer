/* SPDX-License-Identifier: MIT */
/* Co-op session features of M9: package mismatch lists, blob transfer (the
 * world bundle) and the blob cache. */
#include <stdio.h>
#include <string.h>

#include "core/memfs.h"
#include "net/cache.h"
#include "net/session.h"
#include "net/transport.h"
#include "support/test_support.h"
#include "unity.h"

#include "net_support.h"

ZB_TEST_LEAK_CHECKED

static void set_packages(zb_net *n, const char *const *pairs, size_t count) {
  zb_net_package p[48];
  for (size_t i = 0; i < count; ++i) {
    p[i].id = pairs[2 * i];
    p[i].version = pairs[2 * i + 1];
  }
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_set_packages(n, p, count));
}

/* spec: coop-sessions: Package mismatches are listed */
static void test_package_mismatches(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_net *host = make_net(5);
  zb_net_config cc = net_config(5);
  cc.package_hash = 78;
  zb_net *client = zb_net_create(&cc);
  static const char *const host_pkgs[] = {"base", "1.0.0", "mod_a", "2.0.0", "mod_b", "1.0.0"};
  static const char *const client_pkgs[] = {"base", "1.0.0", "mod_a", "2.1.0", "mod_c", "0.5.0"};
  set_packages(host, host_pkgs, 3);
  set_packages(client, client_pkgs, 3);
  zb_net_host(host, zb_loopback_listen(lb));
  zb_net_join(client, zb_loopback_connect(lb));
  zb_net *all[] = {host, client};
  pump(all, 2, 3);
  zb_net_event ev;
  TEST_ASSERT_TRUE(expect_event(client, ZB_NET_EV_ENDED, &ev));
  TEST_ASSERT_EQUAL_STRING("the host has a different package set", zb_net_error(client));
  TEST_ASSERT_FALSE(zb_net_can_resume(client));
  const zb_net_mismatch *mm;
  TEST_ASSERT_EQUAL_size_t(3, zb_net_mismatches(client, &mm));
  TEST_ASSERT_EQUAL_STRING("mod_a", mm[0].id);
  TEST_ASSERT_EQUAL_STRING("2.0.0", mm[0].host_version);
  TEST_ASSERT_EQUAL_STRING("2.1.0", mm[0].client_version);
  TEST_ASSERT_EQUAL_STRING("mod_b", mm[1].id);
  TEST_ASSERT_EQUAL_STRING("1.0.0", mm[1].host_version);
  TEST_ASSERT_EQUAL_STRING("", mm[1].client_version);
  TEST_ASSERT_EQUAL_STRING("mod_c", mm[2].id);
  TEST_ASSERT_EQUAL_STRING("", mm[2].host_version);
  TEST_ASSERT_EQUAL_STRING("0.5.0", mm[2].client_version);
  TEST_ASSERT_EQUAL_UINT(0x1, zb_net_players(host)); /* nobody joined */
  /* A long list is capped; long names are cut. */
  static char ids[40][80];
  const char *many[80];
  for (int i = 0; i < 40; ++i) {
    snprintf(ids[i], sizeof ids[i], "a_package_with_a_very_long_identifier_that_goes_on_and_on_%02d_xxxxxxxx", i);
    many[2 * i] = ids[i];
    many[2 * i + 1] = "1.0";
  }
  set_packages(client, many, 40);
  zb_net_join(client, zb_loopback_connect(lb));
  pump(all, 2, 3);
  TEST_ASSERT_EQUAL_size_t(ZB_NET_MAX_MISMATCHES, zb_net_mismatches(client, &mm));
  TEST_ASSERT_EQUAL_size_t(63, strlen(mm[3].id)); /* host packages first, then the client's */
  /* Joining again resets the list; equal sets join. */
  cc.package_hash = 77;
  zb_net *same = zb_net_create(&cc);
  set_packages(same, host_pkgs, 3);
  zb_net_join(same, zb_loopback_connect(lb));
  zb_net *all2[] = {host, same};
  pump(all2, 2, 3);
  TEST_ASSERT_TRUE(expect_event(same, ZB_NET_EV_WELCOME, &ev));
  TEST_ASSERT_EQUAL_size_t(0, zb_net_mismatches(same, &mm));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_set_packages(same, NULL, 70000));
  zb_net_destroy(same);
  zb_net_destroy(client);
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
}

/* spec: coop-sessions: Package mismatches are listed */
static void test_mismatch_list_malformed(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_transport *raw_host = zb_loopback_listen(lb);
  zb_net *client = make_net(5);
  static const uint8_t replies[][16] = {
      {3, 'n', 'o', 0, 1, 1, 'a', 0, 0},      /* one entry: package a, on neither side */
      {3, 'n', 'o', 0, 1, 1, 'a', 9, 'x', 0}, /* a field longer than the message */
  };
  static const size_t lens[] = {9, 10};
  for (int k = 0; k < 2; ++k) {
    zb_net_join(client, zb_loopback_connect(lb));
    zb_net_update(client);
    zb_tr_event ev;
    while (raw_host->next(raw_host->ctx, &ev)) {
    }
    raw_host->send(raw_host->ctx, (uint32_t)(k + 1), replies[k], lens[k]);
    zb_net_update(client);
    TEST_ASSERT_EQUAL_STRING("no", zb_net_error(client));
    const zb_net_mismatch *mm;
    TEST_ASSERT_EQUAL_size_t(k == 0 ? 1 : 0, zb_net_mismatches(client, &mm));
  }
  zb_net_destroy(client);
  zb_transport_destroy(raw_host);
  zb_loopback_destroy(lb);
}

static uint8_t *make_blob(size_t len, uint64_t seed) {
  uint8_t *b = zb_alloc(zb_test_alloc(), len ? len : 1);
  uint64_t x = seed;
  for (size_t i = 0; i < len; ++i) {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    b[i] = (uint8_t)(x >> 56);
  }
  return b;
}

typedef struct session3 {
  zb_loopback *lb;
  zb_net *host, *c1, *c2;
} session3;

static void start3(session3 *s, const zb_net_config *client_cfg) {
  g_now = 0;
  s->lb = zb_loopback_create(zb_test_alloc());
  s->host = make_net(5);
  s->c1 = client_cfg ? zb_net_create(client_cfg) : make_net(5);
  s->c2 = make_net(5);
  zb_net_host(s->host, zb_loopback_listen(s->lb));
  zb_net_join(s->c1, zb_loopback_connect(s->lb));
  zb_net_join(s->c2, zb_loopback_connect(s->lb));
  zb_net *all[] = {s->host, s->c1, s->c2};
  pump(all, 3, 3);
  TEST_ASSERT_EQUAL_UINT(0x7, zb_net_players(s->host));
  drain(s->host);
  drain(s->c1);
  drain(s->c2);
}

static void stop3(session3 *s) {
  zb_net_destroy(s->c1);
  zb_net_destroy(s->c2);
  zb_net_destroy(s->host);
  zb_loopback_destroy(s->lb);
}

/* spec: coop-sessions: World bundle transfer and cache */
static void test_blob_transfer(void) {
  session3 s;
  start3(&s, NULL);
  const size_t len = 3u * 1024u * 1024u + 123u;
  uint8_t *bundle = make_blob(len, 1);
  char hash[ZB_NET_HASH_HEX + 1];
  zb_net_blob_hash(bundle, len, hash);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send_blob(s.host, 1, "world", bundle, len));
  zb_net *all[] = {s.host, s.c1, s.c2};
  uint64_t last_done = 0;
  int progress = 0, rounds = 0;
  bool got = false, sent = false;
  for (; rounds < 200 && !(got && sent); ++rounds) {
    /* Other channels keep flowing while the bundle travels. */
    uint8_t tick = (uint8_t)rounds;
    zb_net_send(s.host, 1, "state", &tick, 1);
    zb_net_stats before, after;
    zb_net_get_stats(s.host, &before);
    pump(all, 3, 1);
    zb_net_get_stats(s.host, &after);
    TEST_ASSERT_TRUE(after.sent_to[1] - before.sent_to[1] <= 256u * 1024u + 4096u); /* at most one window */
    zb_net_event ev;
    bool state_now = false;
    while (zb_net_next(s.c1, &ev)) {
      if (ev.kind == ZB_NET_EV_DATA) {
        TEST_ASSERT_EQUAL_UINT8(tick, ev.data[0]);
        state_now = true;
      } else if (ev.kind == ZB_NET_EV_BLOB_PROGRESS) {
        TEST_ASSERT_EQUAL_STRING("world", ev.channel);
        TEST_ASSERT_EQUAL_UINT64(len, ev.total);
        TEST_ASSERT_TRUE(ev.done >= last_done);
        TEST_ASSERT_EQUAL_STRING(hash, ev.hash);
        last_done = ev.done;
        progress++;
      } else if (ev.kind == ZB_NET_EV_BLOB) {
        TEST_ASSERT_EQUAL_INT(0, ev.player);
        TEST_ASSERT_EQUAL_size_t(len, ev.len);
        TEST_ASSERT_EQUAL_MEMORY(bundle, ev.data, len);
        TEST_ASSERT_EQUAL_STRING(hash, ev.hash);
        got = true;
      }
    }
    TEST_ASSERT_TRUE_MESSAGE(state_now, "a state message waited behind the bundle");
    while (zb_net_next(s.host, &ev)) {
      if (ev.kind == ZB_NET_EV_BLOB_SENT) {
        TEST_ASSERT_EQUAL_INT(1, ev.player);
        TEST_ASSERT_EQUAL_STRING("world", ev.channel);
        sent = true;
      }
    }
    TEST_ASSERT_FALSE(expect_event(s.c2, ZB_NET_EV_BLOB_PROGRESS, &ev)); /* only player 1 gets it */
  }
  TEST_ASSERT_TRUE(got && sent);
  TEST_ASSERT_TRUE(progress > 5);
  TEST_ASSERT_EQUAL_UINT64(len, last_done);
  TEST_ASSERT_TRUE(rounds > 10); /* paced by the window, not sent at once */
  zb_net_stats st;
  zb_net_get_stats(s.host, &st);
  TEST_ASSERT_TRUE(st.sent_to[1] > len && st.sent_to[2] < 1000);
  /* Two blobs to one player go one after the other; a client sends to the host; empty blobs work. */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send_blob(s.host, 2, "first", bundle, 100000));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send_blob(s.host, 2, "second", bundle + 7, 5));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send_blob(s.c1, 0, "save", bundle, 20000));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send_blob(s.c1, 0, "empty", NULL, 0));
  int order = 0;
  bool host_got = false, host_empty = false;
  for (int i = 0; i < 50; ++i) {
    pump(all, 3, 1);
    zb_net_event ev;
    while (zb_net_next(s.c2, &ev)) {
      if (ev.kind == ZB_NET_EV_BLOB) {
        TEST_ASSERT_EQUAL_STRING(order == 0 ? "first" : "second", ev.channel);
        order++;
      }
    }
    while (zb_net_next(s.host, &ev)) {
      if (ev.kind == ZB_NET_EV_BLOB && strcmp(ev.channel, "save") == 0) {
        TEST_ASSERT_EQUAL_INT(1, ev.player);
        TEST_ASSERT_EQUAL_MEMORY(bundle, ev.data, 20000);
        host_got = true;
      } else if (ev.kind == ZB_NET_EV_BLOB && strcmp(ev.channel, "empty") == 0) {
        TEST_ASSERT_EQUAL_size_t(0, ev.len);
        host_empty = true;
      }
    }
  }
  TEST_ASSERT_EQUAL_INT(2, order);
  TEST_ASSERT_TRUE(host_got && host_empty);
  /* Refused sends. */
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_send_blob(s.host, 1, "", bundle, 1));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID,
                        zb_net_send_blob(s.host, 1, "a_name_that_is_much_too_long_for_a_blob", bundle, 1));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_send_blob(s.host, 0, "self", bundle, 1));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_send_blob(s.host, 9, "x", bundle, 1));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, zb_net_send_blob(s.host, 3, "nobody", bundle, 1));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, zb_net_send_blob(s.c1, 2, "not via host", bundle, 1));
  /* A receiver that leaves fails the transfer on both sides of the host. */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send_blob(s.host, 2, "world", bundle, len));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send_blob(s.c1, 0, "upload", bundle, len));
  pump(all, 3, 2);
  zb_net_leave(s.c2);
  zb_net_leave(s.c1);
  pump(all, 1, 1);
  zb_net_event ev;
  int failed = 0;
  while (zb_net_next(s.host, &ev)) {
    if (ev.kind == ZB_NET_EV_BLOB_FAILED) {
      TEST_ASSERT_EQUAL_size_t(strlen("the player left"), ev.len);
      TEST_ASSERT_EQUAL_MEMORY("the player left", ev.data, ev.len);
      failed++;
    }
  }
  TEST_ASSERT_EQUAL_INT(2, failed);
  TEST_ASSERT_EQUAL_INT(ZB_ERR_STATE, zb_net_send_blob(s.c1, 0, "x", bundle, 1));
  zb_free(zb_test_alloc(), bundle);
  stop3(&s);
}

/* spec: coop-sessions: World bundle transfer and cache */
static void test_blob_refused(void) {
  zb_net_config cc = net_config(5);
  cc.max_blob = 1000;
  session3 s;
  start3(&s, &cc);
  uint8_t *data = make_blob(5000, 2);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send_blob(s.host, 1, "big", data, 5000));
  zb_net *all[] = {s.host, s.c1, s.c2};
  pump(all, 3, 3);
  zb_net_event ev;
  TEST_ASSERT_TRUE(expect_event(s.c1, ZB_NET_EV_BLOB_FAILED, &ev));
  TEST_ASSERT_EQUAL_STRING("big", ev.channel);
  TEST_ASSERT_EQUAL_MEMORY("the blob is too large", ev.data, ev.len);
  TEST_ASSERT_TRUE(expect_event(s.host, ZB_NET_EV_BLOB_FAILED, &ev));
  TEST_ASSERT_EQUAL_INT(1, ev.player);
  TEST_ASSERT_EQUAL_MEMORY("the receiver refused the blob", ev.data, ev.len);
  /* A connection that is lost fails the transfer as well. */
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send_blob(s.host, 1, "small", data, 100));
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_send_blob(s.host, 2, "w", data, 5000));
  zb_loopback_cut(s.lb, 2);
  pump(all, 3, 1);
  TEST_ASSERT_TRUE(expect_event(s.host, ZB_NET_EV_BLOB_FAILED, &ev));
  TEST_ASSERT_EQUAL_MEMORY("the connection was lost", ev.data, ev.len);
  zb_free(zb_test_alloc(), data);
  stop3(&s);
}

/* Builds a blob message by hand: start (hash, name), chunk (offset, bytes). */
static size_t blob_start_msg(uint8_t *m, uint32_t id, uint64_t total, uint8_t hash_byte, const char *name) {
  memset(m, 0, 34);
  m[0] = 12; /* MSG_BLOB_START */
  for (int i = 0; i < 4; ++i) {
    m[1 + i] = (uint8_t)(id >> (8 * i));
  }
  for (int i = 0; i < 8; ++i) {
    m[5 + i] = (uint8_t)(total >> (8 * i));
  }
  memset(m + 13, hash_byte, 20);
  m[33] = (uint8_t)strlen(name);
  memcpy(m + 34, name, strlen(name));
  return 34 + strlen(name);
}

static size_t blob_chunk_msg(uint8_t *m, uint32_t id, uint64_t offset, const void *data, size_t len) {
  m[0] = 13; /* MSG_BLOB_CHUNK */
  for (int i = 0; i < 4; ++i) {
    m[1 + i] = (uint8_t)(id >> (8 * i));
  }
  for (int i = 0; i < 8; ++i) {
    m[5 + i] = (uint8_t)(offset >> (8 * i));
  }
  memcpy(m + 13, data, len);
  return 13 + len;
}

/* spec: coop-sessions: World bundle transfer and cache */
static void test_blob_protocol_errors(void) {
  g_now = 0;
  zb_loopback *lb = zb_loopback_create(zb_test_alloc());
  zb_transport *raw = zb_loopback_listen(lb);
  zb_net *client = make_net(5);
  zb_net_join(client, zb_loopback_connect(lb));
  zb_net_update(client);
  zb_tr_event tev;
  while (raw->next(raw->ctx, &tev)) {
  }
  uint8_t welcome[19] = {2, 1, 3};
  for (int i = 0; i < 8; ++i) {
    welcome[3 + i] = (uint8_t)(5ull >> (8 * i));
  }
  welcome[11] = 42; /* resume token */
  raw->send(raw->ctx, 1, welcome, sizeof welcome);
  zb_net_update(client);
  TEST_ASSERT_EQUAL_INT(ZB_NET_CLIENT, zb_net_role_of(client));
  uint8_t m[128];
  /* A wrong hash: the blob arrives but is refused. */
  raw->send(raw->ctx, 1, m, blob_start_msg(m, 1, 4, 0xAB, "bad"));
  raw->send(raw->ctx, 1, m, blob_chunk_msg(m, 1, 0, "abcd", 4));
  /* Malformed starts are ignored; a chunk out of order fails the blob; stale chunks are ignored. */
  raw->send(raw->ctx, 1, m, blob_start_msg(m, 2, 4, 0, "") - 0);
  raw->send(raw->ctx, 1, m, 20);
  raw->send(raw->ctx, 1, m, blob_start_msg(m, 3, 8, 0, "gap"));
  raw->send(raw->ctx, 1, m, blob_chunk_msg(m, 3, 4, "abcd", 4));
  raw->send(raw->ctx, 1, m, blob_chunk_msg(m, 3, 0, "abcd", 4));
  raw->send(raw->ctx, 1, m, blob_chunk_msg(m, 77, 0, "abcd", 4));
  /* A newer blob replaces an unfinished one; acks and aborts for unknown blobs are ignored. */
  raw->send(raw->ctx, 1, m, blob_start_msg(m, 4, 8, 0, "old"));
  raw->send(raw->ctx, 1, m, blob_start_msg(m, 5, 2, 0, "new"));
  static const uint8_t ack[13] = {14, 9, 0, 0, 0, 1};
  static const uint8_t abort_msg[5] = {15, 9, 0, 0, 0};
  raw->send(raw->ctx, 1, ack, sizeof ack);
  raw->send(raw->ctx, 1, abort_msg, sizeof abort_msg);
  zb_net_update(client);
  zb_net_event ev;
  TEST_ASSERT_TRUE(expect_event(client, ZB_NET_EV_BLOB_FAILED, &ev));
  TEST_ASSERT_EQUAL_STRING("bad", ev.channel);
  TEST_ASSERT_EQUAL_MEMORY("the blob's content hash does not match", ev.data, ev.len);
  TEST_ASSERT_EQUAL_STRING("abababababababababababababababababababab", ev.hash);
  TEST_ASSERT_TRUE(expect_event(client, ZB_NET_EV_BLOB_FAILED, &ev));
  TEST_ASSERT_EQUAL_STRING("gap", ev.channel);
  TEST_ASSERT_EQUAL_MEMORY("malformed blob", ev.data, ev.len);
  TEST_ASSERT_FALSE(expect_event(client, ZB_NET_EV_BLOB_FAILED, &ev));
  /* The client answered with acknowledgements and refusals. */
  size_t n;
  int acks = 0, aborts = 0;
  const uint8_t *reply;
  while ((reply = raw_message(raw, &n, NULL)) != NULL) {
    acks += reply[0] == 14;
    aborts += reply[0] == 15;
  }
  TEST_ASSERT_EQUAL_INT(1, acks); /* for the bad blob's only chunk */
  TEST_ASSERT_EQUAL_INT(1, aborts);
  /* The session ending frees an unfinished blob silently. */
  zb_net_leave(client);
  zb_net_destroy(client);
  zb_transport_destroy(raw);
  zb_loopback_destroy(lb);
}

/* spec: coop-sessions: World bundle transfer and cache */
static void test_blob_cache(void) {
  zb_memfs *mfs = zb_memfs_create(zb_test_alloc());
  const zb_fs_api *fs = zb_memfs_api(mfs);
  static const char data[] = "the world bundle";
  char hash[ZB_NET_HASH_HEX + 1];
  zb_net_blob_hash("abc", 3, hash);
  TEST_ASSERT_EQUAL_STRING("a9993e364706816aba3e25717850c26c9cd0d89d", hash);
  zb_net_blob_hash(data, sizeof data, hash);
  TEST_ASSERT_FALSE(zb_net_cache_has(fs, "user/", hash)); /* first join: not there */
  char stored[ZB_NET_HASH_HEX + 1];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_cache_store(fs, "user/", data, sizeof data, stored));
  TEST_ASSERT_EQUAL_STRING(hash, stored);
  TEST_ASSERT_TRUE(zb_net_cache_has(fs, "user/", hash)); /* second join: skip the transfer */
  char *back = NULL;
  size_t len = 0;
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_cache_load(fs, "user/", hash, zb_test_alloc(), &back, &len));
  TEST_ASSERT_EQUAL_size_t(sizeof data, len);
  TEST_ASSERT_EQUAL_MEMORY(data, back, len);
  zb_free(zb_test_alloc(), back);
  char path[256];
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_cache_path("user/", hash, path, sizeof path));
  TEST_ASSERT_EQUAL_STRING_LEN("user/net-cache/", path, 15);
  TEST_ASSERT_EQUAL_INT(ZB_OK, zb_net_cache_path(NULL, hash, path, sizeof path));
  TEST_ASSERT_EQUAL_STRING_LEN("net-cache/", path, 10);
  /* A damaged file is not used and is removed. */
  zb_memfs_put(mfs, "user/net-cache/0000000000000000000000000000000000000000.blob", "x", 1);
  TEST_ASSERT_TRUE(zb_net_cache_has(fs, "user/", "0000000000000000000000000000000000000000"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, zb_net_cache_load(fs, "user/", "0000000000000000000000000000000000000000",
                                                            zb_test_alloc(), &back, &len));
  TEST_ASSERT_NULL(back);
  TEST_ASSERT_FALSE(zb_net_cache_has(fs, "user/", "0000000000000000000000000000000000000000"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOT_FOUND, zb_net_cache_load(fs, "user/", "1111111111111111111111111111111111111111",
                                                            zb_test_alloc(), &back, &len));
  /* Only hashes name files; paths must fit. */
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_cache_path("user/", "../../etc/passwd", path, sizeof path));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID,
                        zb_net_cache_path("user/", "ABCDEF0000000000000000000000000000000000", path, sizeof path));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_cache_path("user/", hash, path, 20));
  TEST_ASSERT_FALSE(zb_net_cache_has(fs, "user/", "nope"));
  TEST_ASSERT_EQUAL_INT(ZB_ERR_INVALID, zb_net_cache_load(fs, "user/", "nope", zb_test_alloc(), &back, &len));
  zb_failing_allocator fa;
  TEST_ASSERT_EQUAL_INT(ZB_ERR_NOMEM,
                        zb_net_cache_load(fs, "user/", hash, zb_failing_init(&fa, zb_test_alloc(), 0), &back, &len));
  zb_memfs_destroy(mfs);
}

/* Runs a session with packages, a latest-only channel and blobs on a failing
 * allocator; nothing may leak or crash. */
static bool features_with_budget(long budget) {
  zb_failing_allocator fa;
  const zb_allocator *a = zb_failing_init(&fa, zb_test_alloc(), budget);
  zb_loopback *lb = zb_loopback_create(a);
  if (!lb) {
    return false;
  }
  zb_loopback_impairment imp = {0, 0, 0, 1, fake_clock, NULL}; /* events wait for the next update */
  zb_loopback_impair(lb, &imp);
  zb_net_config c = net_config(9);
  c.a = a;
  zb_net *host = zb_net_create(&c), *client = zb_net_create(&c);
  static const zb_net_package pk[] = {{"base", "1"}, {"mod", "2"}};
  bool ok = false;
  if (host && client && zb_net_set_packages(host, pk, 2) == ZB_OK && zb_net_set_packages(client, pk, 2) == ZB_OK &&
      zb_net_set_channel_class(host, "s", ZB_NET_LATEST) == ZB_OK &&
      zb_net_host(host, zb_loopback_listen(lb)) == ZB_OK && zb_net_join(client, zb_loopback_connect(lb)) == ZB_OK) {
    zb_net *all[] = {host, client};
    pump(all, 2, 4);
    static uint8_t big[40000];
    zb_net_send_blob(host, 1, "b", big, sizeof big);
    zb_net_send_blob(client, 0, "c", big, 100);
    for (int i = 0; i < 40; ++i) {
      zb_net_send(host, 1, "s", big, 1000);
    }
    g_now += 2000000000ull;
    pump(all, 2, 8);
    zb_net_event ev;
    ok = expect_event(client, ZB_NET_EV_BLOB, &ev) && expect_event(host, ZB_NET_EV_BLOB, &ev);
  }
  zb_net_destroy(client);
  zb_net_destroy(host);
  zb_loopback_destroy(lb);
  return ok && fa.calls <= budget;
}

static void test_out_of_memory(void) {
  g_now = 0;
  long budget = 0;
  while (!features_with_budget(budget) && budget < 2000) {
    ++budget;
  }
  TEST_ASSERT_TRUE(budget > 10 && budget < 2000);
  zb_failing_allocator fa;
  zb_net_config c = net_config(1);
  c.a = zb_failing_init(&fa, zb_test_alloc(), 0);
  TEST_ASSERT_NULL(zb_net_create(&c));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_package_mismatches);
  RUN_TEST(test_mismatch_list_malformed);
  RUN_TEST(test_blob_transfer);
  RUN_TEST(test_blob_refused);
  RUN_TEST(test_blob_protocol_errors);
  RUN_TEST(test_blob_cache);
  RUN_TEST(test_out_of_memory);
  return UNITY_END();
}

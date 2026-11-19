/* SPDX-License-Identifier: MIT */
/* Helpers shared by the network tests: a fake clock, sessions and pumping. */
#ifndef ZB_TEST_NET_SUPPORT_H
#define ZB_TEST_NET_SUPPORT_H

#include <string.h>

#include "net/session.h"
#include "support/test_support.h"
#include "unity.h"

static uint64_t g_now;

static inline uint64_t fake_clock(void *ctx) {
  (void)ctx;
  return g_now;
}

static inline zb_net_config net_config(uint64_t seed) {
  zb_net_config c;
  memset(&c, 0, sizeof c);
  c.a = zb_test_alloc();
  c.world_seed = seed;
  c.package_hash = 77;
  c.clock_ns = fake_clock;
  return c;
}

static inline zb_net *make_net(uint64_t seed) {
  zb_net_config c = net_config(seed);
  return zb_net_create(&c);
}

static inline void pump(zb_net **nets, int count, int rounds) {
  for (int r = 0; r < rounds; ++r) {
    for (int i = 0; i < count; ++i) {
      zb_net_update(nets[i]);
    }
  }
}

/* Collects the next event of a kind (skipping others). */
static inline bool expect_event(zb_net *n, zb_net_event_kind kind, zb_net_event *out) {
  zb_net_event ev;
  while (zb_net_next(n, &ev)) {
    if (ev.kind == kind) {
      *out = ev;
      return true;
    }
  }
  return false;
}

static inline void drain(zb_net *n) {
  zb_net_event ev;
  while (zb_net_next(n, &ev)) {
  }
}

/* Next message on a raw transport (skipping other events); NULL when none. */
static inline const uint8_t *raw_message(zb_transport *t, size_t *len, bool *closed) {
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

#endif

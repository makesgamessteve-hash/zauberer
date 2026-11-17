/* SPDX-License-Identifier: MIT */
#include "core/loop.h"

#include "core/log.h"

void zb_loop_init(zb_loop *l) {
  l->acc = 0;
  l->last_ns = 0;
  l->started = false;
  l->paused = false;
  l->time_scale = 1.0;
  l->tick = 0;
  l->dropped = 0;
  l->stalls = 0;
}

unsigned zb_loop_advance(zb_loop *l, uint64_t now_ns) {
  if (!l->started) {
    l->started = true;
    l->last_ns = now_ns;
    return 0;
  }
  uint64_t dt = now_ns > l->last_ns ? now_ns - l->last_ns : 0;
  l->last_ns = now_ns;
  if (l->paused) {
    return 0;
  }
  if (l->time_scale != 1.0) {
    dt = (uint64_t)((double)dt * l->time_scale + 0.5);
  }
  l->acc += dt * ZB_TICK_HZ;
  uint64_t due = l->acc / ZB_NS_PER_SEC;
  if (due > ZB_MAX_CATCHUP_TICKS) {
    uint64_t dropped = due - ZB_MAX_CATCHUP_TICKS;
    l->dropped += dropped;
    l->stalls++;
    zb_warn("simulation fell behind by %llu ticks; running %u and skipping the rest", (unsigned long long)due,
            ZB_MAX_CATCHUP_TICKS);
    due = ZB_MAX_CATCHUP_TICKS;
    l->acc %= ZB_NS_PER_SEC;
  } else {
    l->acc -= due * ZB_NS_PER_SEC;
  }
  l->tick += due;
  return (unsigned)due;
}

double zb_loop_alpha(const zb_loop *l) { return (double)l->acc / (double)ZB_NS_PER_SEC; }

double zb_loop_tick_seconds(void) { return 1.0 / (double)ZB_TICK_HZ; }

void zb_loop_set_paused(zb_loop *l, bool paused) { l->paused = paused; }

void zb_loop_set_time_scale(zb_loop *l, double scale) {
  if (scale < 0.1) {
    scale = 0.1;
  } else if (scale > 10.0) {
    scale = 10.0;
  }
  l->time_scale = scale;
}

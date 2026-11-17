/* SPDX-License-Identifier: MIT */
/* Fixed-step game loop timing: 60 ticks per second regardless of frame rate,
 * bounded catch-up, interpolation factor, pause and time scale. Time is
 * accumulated in integer units of 1/60 ns so that exactly 60 ticks happen
 * per second of real time. */
#ifndef ZB_CORE_LOOP_H
#define ZB_CORE_LOOP_H

#include "core/base.h"

#define ZB_TICK_HZ 60u
#define ZB_NS_PER_SEC 1000000000ull
#define ZB_MAX_CATCHUP_TICKS 8u

typedef struct zb_loop {
  uint64_t acc;          /* elapsed time * ZB_TICK_HZ, in ns */
  uint64_t last_ns;
  bool started;
  bool paused;
  double time_scale;
  uint64_t tick;         /* total ticks issued */
  uint64_t dropped;      /* ticks discarded by the catch-up clamp */
  unsigned stalls;       /* number of frames that hit the clamp */
} zb_loop;

void zb_loop_init(zb_loop *l);
/* Accounts for real time up to now_ns and returns how many ticks to run now
 * (at most ZB_MAX_CATCHUP_TICKS). Logs one warning per stall. */
unsigned zb_loop_advance(zb_loop *l, uint64_t now_ns);
/* Position between the last two ticks, in [0, 1). */
double zb_loop_alpha(const zb_loop *l);
/* Duration of one tick in seconds. */
double zb_loop_tick_seconds(void);
void zb_loop_set_paused(zb_loop *l, bool paused);
/* Development time scale, clamped to [0.1, 10]. */
void zb_loop_set_time_scale(zb_loop *l, double scale);

#endif

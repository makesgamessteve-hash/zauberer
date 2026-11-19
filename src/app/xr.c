/* SPDX-License-Identifier: MIT */
#include "app/xr.h"

#include "app/engine.h"
#include "core/log.h"

#include <string.h>

static void reset_frame(zb_engine *e) {
  memset(&e->xr.state.frame, 0, sizeof e->xr.state.frame);
  e->xr.state.frame.head.orientation = zb_quat_identity();
  for (int h = 0; h < 2; ++h) {
    e->xr.state.frame.controllers[h] = zb_xr_controller_idle();
  }
}

/* The emulated headset's frame: its views and head, and the emulated controllers. */
static void emulated_frame(zb_engine *e, zb_xr_frame *f) {
  zb_xr_emulate(e->xr.emulated_head, e->drawable_w, e->drawable_h, ZB_XR_DEFAULT_IPD, f);
  f->controllers[0] = e->xr.emulated_controllers[0];
  f->controllers[1] = e->xr.emulated_controllers[1];
}

static void apply_frame(zb_engine *e, const zb_xr_frame *f) {
  e->xr.state.frame = *f;
  zb_render_set_views(e->render, f->views, f->view_count);
  zb_render_set_target(e->render, f->framebuffer, f->width, f->height);
}

static void start_session(zb_engine *e, bool emulated) {
  e->xr.state.active = true;
  e->xr.state.available = true;
  e->xr.state.emulated = emulated;
  e->xr.state.frames = 0;
  e->xr.pending_event = "started";
  zb_info("WebXR: session started%s", emulated ? " (emulated headset)" : "");
}

static void end_session(zb_engine *e) {
  if (!e->xr.state.active) {
    return;
  }
  zb_info("WebXR: session ended after %llu frame(s)", (unsigned long long)e->xr.state.frames);
  e->xr.state.active = false;
  e->xr.state.emulated = false;
  e->xr.state.available = e->xr.device_available;
  reset_frame(e);
  zb_render_set_views(e->render, NULL, 0);
  zb_render_set_target(e->render, 0, 0, 0);
  e->xr.pending_event = "ended";
}

/* A session change from the device (emulated false) or from the test API. */
static void change_status(zb_engine *e, zb_xr_status status, bool emulated) {
  if (!emulated) {
    e->xr.device_available = status != ZB_XR_UNSUPPORTED;
  }
  bool running = status == ZB_XR_RUNNING;
  if (running && e->xr.state.active && e->xr.state.emulated && !emulated) {
    end_session(e); /* a real headset replaces the emulated one */
  }
  if (running && !e->xr.state.active) {
    start_session(e, emulated);
  } else if (!running && e->xr.state.active && e->xr.state.emulated == emulated) {
    end_session(e);
  }
  e->xr.state.available = e->xr.device_available || e->xr.state.active;
}

void zb_engine_xr_status(zb_engine *e, zb_xr_status status) { change_status(e, status, false); }

bool zb_engine_xr_frame(zb_engine *e, const zb_xr_frame *f) {
  if (!e->xr.state.active || e->xr.in_frame || e->quit) {
    return false;
  }
  apply_frame(e, f);
  e->xr.state.frames++;
  e->xr.in_frame = true;
  zb_engine_iterate(e);
  e->xr.in_frame = false;
  return !e->quit;
}

bool zb_engine_xr_takes_over(zb_engine *e) {
  if (e->xr.pending_event && e->scripts_ready) {
    const char *ev = e->xr.pending_event;
    e->xr.pending_event = NULL;
    zb_script_event(e->script, "xr_session", 0, 0, ev);
  }
  if (!e->xr.state.active || e->xr.in_frame) {
    return false;
  }
  if (e->xr.state.emulated) {
    zb_xr_frame f;
    emulated_frame(e, &f);
    zb_engine_xr_frame(e, &f);
    if (!e->headless) {
      zb_platform_swap(e->platform);
    }
  }
  return true;
}

void zb_engine_xr_emulate(zb_engine *e, const zb_xr_pose *head) {
  if (e->xr.state.active && !e->xr.state.emulated) {
    return; /* a headset presents */
  }
  change_status(e, head ? ZB_XR_RUNNING : ZB_XR_SUPPORTED, true);
  if (!head) {
    return;
  }
  e->xr.emulated_head = *head;
  /* The rest of this iteration already renders the emulated views. */
  zb_xr_frame f;
  emulated_frame(e, &f);
  apply_frame(e, &f);
}

void zb_engine_xr_emulate_controller(zb_engine *e, int hand, const zb_xr_controller *c) {
  if (hand != ZB_XR_HAND_LEFT && hand != ZB_XR_HAND_RIGHT) {
    return;
  }
  zb_xr_controller ctl = *c;
  ctl.pose.orientation = zb_quat_normalize(ctl.pose.orientation); /* identity for a zero quaternion */
  e->xr.emulated_controllers[hand - 1] = ctl;
  if (e->xr.state.active && e->xr.state.emulated) {
    e->xr.state.frame.controllers[hand - 1] = ctl;
  }
}

/* ---- Platform handler and script host callbacks ------------------------------- */

static bool on_frame(void *user, const zb_xr_frame *f) { return zb_engine_xr_frame(user, f); }

static void on_event(void *user, const zb_event *ev) {
  zb_engine *e = user;
  if (ev->type == ZB_EV_XR_STATUS) {
    zb_engine_xr_status(e, (zb_xr_status)ev->u.xr_status.status);
  } else {
    zb_engine_event(e, ev);
  }
}

static void host_xr_state(void *ctx, zb_xr_state *out) { *out = ((zb_engine *)ctx)->xr.state; }

static void host_xr_emulate(void *ctx, const zb_xr_pose *head) { zb_engine_xr_emulate(ctx, head); }

static void host_xr_controller(void *ctx, int hand, const zb_xr_controller *c) {
  zb_engine_xr_emulate_controller(ctx, hand, c);
}

static void host_xr_button(void *ctx, int hand, int button, bool down) {
  zb_engine *e = ctx;
  zb_event ev;
  memset(&ev, 0, sizeof ev);
  ev.type = ZB_EV_XR_BUTTON;
  ev.time_ns = zb_platform_now_ns(e->platform);
  ev.u.xr_button.hand = (uint8_t)hand;
  ev.u.xr_button.button = (uint8_t)button;
  ev.u.xr_button.down = down;
  on_event(e, &ev);
}

void zb_engine_xr_attach(zb_engine *e, zb_script_host *host) {
  reset_frame(e);
  e->xr.emulated_controllers[0] = e->xr.emulated_controllers[1] = zb_xr_controller_idle();
  host->xr_state = host_xr_state;
  host->xr_emulate = host_xr_emulate;
  host->xr_controller = host_xr_controller;
  host->xr_button = host_xr_button;
  zb_xr_handler h = {e, on_frame, on_event};
  zb_platform_set_xr_handler(e->platform, &h);
}

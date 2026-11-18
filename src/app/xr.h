/* SPDX-License-Identifier: MIT */
/* WebXR sessions in the engine: the platform's XR frames run the engine step
 * with the headset's views, session changes reach the renderer and the
 * scripts, and scenarios can emulate a headset through the same path. */
#ifndef ZB_APP_XR_H
#define ZB_APP_XR_H

#include "core/xr.h"
#include "script/script.h"

struct zb_engine;

typedef struct zb_engine_xr {
  zb_xr_state state;         /* what scripts see */
  bool device_available;     /* the browser can start a session */
  bool in_frame;             /* an XR frame is running the engine step */
  zb_xr_pose emulated_head;  /* head pose of an emulated session */
  const char *pending_event; /* "started" or "ended", for the scripts */
} zb_engine_xr;

/* Registers the engine with the platform's WebXR bridge and sets the
 * scripts' XR host callbacks. */
void zb_engine_xr_attach(struct zb_engine *e, zb_script_host *host);
/* Called first by zb_engine_iterate: delivers a pending xr_session event,
 * runs one frame of an emulated session, and returns true while a session's
 * frames drive the engine step (the iterate step then does nothing else). */
bool zb_engine_xr_takes_over(struct zb_engine *e);
/* One XR frame: applies its views, target and head pose and runs the engine
 * step into the XR framebuffer. Returns false when the session should end. */
bool zb_engine_xr_frame(struct zb_engine *e, const zb_xr_frame *f);
/* Availability or session change reported by the platform. */
void zb_engine_xr_status(struct zb_engine *e, zb_xr_status status);
/* Test API: starts or moves an emulated headset session; NULL ends it. No
 * effect while a real headset presents. */
void zb_engine_xr_emulate(struct zb_engine *e, const zb_xr_pose *head);

#endif

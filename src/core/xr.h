/* SPDX-License-Identifier: MIT */
/* Platform-independent WebXR data: the views and viewer pose of one XR frame
 * as the page's bridge (web/lib_zauberer.js) reports them, the flat float
 * layout the bridge writes, the session state scripts see, and an emulated
 * headset for tests and tools. */
#ifndef ZB_CORE_XR_H
#define ZB_CORE_XR_H

#include "core/math.h"

#define ZB_XR_MAX_VIEWS 2
/* Distance between the emulated headset's eyes (metres). */
#define ZB_XR_DEFAULT_IPD 0.064f

typedef enum zb_xr_status {
  ZB_XR_UNSUPPORTED, /* no immersive VR session can be started */
  ZB_XR_SUPPORTED,   /* a session can be started */
  ZB_XR_RUNNING      /* a session is running */
} zb_xr_status;

typedef enum zb_xr_hand { ZB_XR_HAND_NONE, ZB_XR_HAND_LEFT, ZB_XR_HAND_RIGHT, ZB_XR_HAND_COUNT } zb_xr_hand;

typedef enum zb_xr_button { ZB_XR_SELECT, ZB_XR_SQUEEZE, ZB_XR_BUTTON_COUNT } zb_xr_button;

typedef struct zb_xr_pose {
  zb_vec3 position; /* metres, in the session's reference space */
  zb_quat orientation;
} zb_xr_pose;

typedef struct zb_xr_view {
  int x, y, width, height; /* viewport in the XR framebuffer (pixels, origin bottom-left) */
  zb_mat4 view;            /* reference space -> eye (the inverse of the eye's pose) */
  zb_mat4 proj;            /* projection (OpenGL clip space) */
} zb_xr_view;

typedef struct zb_xr_frame {
  uint32_t framebuffer; /* GL framebuffer name of the XR layer (0 = default framebuffer) */
  int width, height;    /* framebuffer size in pixels */
  int view_count;
  zb_xr_view views[ZB_XR_MAX_VIEWS];
  zb_xr_pose head; /* viewer pose */
} zb_xr_frame;

/* What scripts see of the session (xr.state()). */
typedef struct zb_xr_state {
  bool available; /* a session can be started */
  bool active;    /* a session is running */
  bool emulated;  /* the running session is emulated (test API) */
  uint64_t frames;
  zb_xr_frame frame; /* views and head pose of the last XR frame */
} zb_xr_state;

/* Flat layout written by the bridge: view count, framebuffer width and height,
 * head position (3) and orientation (x, y, z, w), then per view the viewport
 * (x, y, width, height), the view matrix (16, column-major) and the projection
 * (16). */
#define ZB_XR_HEADER_FLOATS 10
#define ZB_XR_VIEW_FLOATS 36
#define ZB_XR_MAX_FLOATS (ZB_XR_HEADER_FLOATS + ZB_XR_MAX_VIEWS * ZB_XR_VIEW_FLOATS)

/* Decodes n floats of the layout above. Returns false (leaving out unchanged)
 * for an unsupported view count, a length that does not match it, a
 * non-positive framebuffer or viewport size, a viewport outside the
 * framebuffer or a non-finite number. */
bool zb_xr_frame_decode(const float *data, size_t n, uint32_t framebuffer, zb_xr_frame *out);

/* An emulated headset: two views side by side in a width x height
 * framebuffer, eyes ipd apart along the head's right axis, each seeing 50
 * degrees outwards and 40 degrees inwards (like real lenses) with a symmetric
 * vertical field of view that matches its viewport's aspect ratio; near 0.1,
 * far 1000. */
void zb_xr_emulate(zb_xr_pose head, int width, int height, float ipd, zb_xr_frame *out);

#endif

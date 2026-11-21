/* SPDX-License-Identifier: MIT */
/* Platform-independent input and window events. The platform layer
 * translates native events into these; input and the app consume them. */
#ifndef ZB_CORE_EVENT_H
#define ZB_CORE_EVENT_H

#include "core/base.h"

typedef enum zb_event_type {
  ZB_EV_NONE,
  ZB_EV_QUIT,
  ZB_EV_RESIZE,
  ZB_EV_KEY,
  ZB_EV_MOUSE_BUTTON,
  ZB_EV_MOUSE_MOTION,
  ZB_EV_MOUSE_WHEEL,
  ZB_EV_GAMEPAD_ADDED,
  ZB_EV_GAMEPAD_REMOVED,
  ZB_EV_GAMEPAD_BUTTON,
  ZB_EV_GAMEPAD_AXIS,
  ZB_EV_TOUCH_DOWN,
  ZB_EV_TOUCH_UP,
  ZB_EV_TOUCH_MOTION,
  ZB_EV_AUDIO_UNLOCKED,
  ZB_EV_FOCUS,     /* the window gained or lost focus, or was shown or hidden */
  ZB_EV_XR_STATUS, /* WebXR availability or session change */
  ZB_EV_XR_BUTTON  /* select or squeeze of an XR input source */
} zb_event_type;

typedef struct zb_event {
  zb_event_type type;
  union {
    struct {
      int width, height;
    } resize; /* drawable pixels */
    struct {
      uint16_t key;
      bool down;
      bool repeat;
    } key; /* zb_key */
    struct {
      uint8_t button;
      bool down;
      float x, y;
      bool synthetic;
    } mouse_button;
    struct {
      float x, y, dx, dy;
      bool synthetic;
    } mouse_motion;
    struct {
      float dx, dy;
    } wheel;
    struct {
      uint32_t id;
    } gamepad;
    struct {
      uint32_t id;
      uint8_t button;
      bool down;
    } gamepad_button;
    struct {
      uint32_t id;
      uint8_t axis;
      float value;
    } gamepad_axis;
    struct {
      uint64_t finger;
      float x, y;
    } touch; /* normalized 0..1 */
    struct {
      uint8_t status; /* zb_xr_status (core/xr.h) */
    } xr_status;
    struct {
      bool on; /* focused (or shown) */
    } focus;
    struct {
      uint8_t hand;   /* zb_xr_hand */
      uint8_t button; /* zb_xr_button */
      bool down;
    } xr_button;
  } u;
  uint64_t time_ns;
} zb_event;

#endif

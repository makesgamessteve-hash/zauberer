/* SPDX-License-Identifier: MIT */
#include "core/keys.h"

#include <string.h>

#define NAME(id, name) name,
static const char *const k_key_names[] = {ZB_KEY_LIST(NAME)};
static const char *const k_pad_button_names[] = {ZB_PAD_BUTTON_LIST(NAME)};
static const char *const k_pad_axis_names[] = {ZB_PAD_AXIS_LIST(NAME)};
#undef NAME

static const char *const k_mouse_names[] = {"none", "left", "middle", "right", "x1", "x2"};

static int find(const char *const *names, int count, const char *name) {
  for (int i = 0; i < count; ++i) {
    if (strcmp(names[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

const char *zb_key_name(zb_key key) { return (unsigned)key < (unsigned)ZB_KEY_COUNT ? k_key_names[key] : "unknown"; }

zb_key zb_key_from_name(const char *name) {
  int i = find(k_key_names, ZB_KEY_COUNT, name);
  return i < 0 ? ZB_KEY_UNKNOWN : (zb_key)i;
}

const char *zb_mouse_button_name(zb_mouse_button button) {
  return (unsigned)button < (unsigned)ZB_MOUSE_BUTTON_COUNT ? k_mouse_names[button] : "none";
}

int zb_mouse_button_from_name(const char *name) {
  int i = find(k_mouse_names, ZB_MOUSE_BUTTON_COUNT, name);
  return i < 0 ? 0 : i;
}

const char *zb_pad_button_name(zb_pad_button button) {
  return (unsigned)button < (unsigned)ZB_PAD_BUTTON_COUNT ? k_pad_button_names[button] : "?";
}

int zb_pad_button_from_name(const char *name) { return find(k_pad_button_names, ZB_PAD_BUTTON_COUNT, name); }

const char *zb_pad_axis_name(zb_pad_axis axis) {
  return (unsigned)axis < (unsigned)ZB_PAD_AXIS_COUNT ? k_pad_axis_names[axis] : "?";
}

int zb_pad_axis_from_name(const char *name) { return find(k_pad_axis_names, ZB_PAD_AXIS_COUNT, name); }

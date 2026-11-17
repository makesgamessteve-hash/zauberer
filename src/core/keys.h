/* SPDX-License-Identifier: MIT */
/* Platform-independent identifiers for keys, mouse buttons and gamepad
 * controls, with the names used in binding data ("key:space"). */
#ifndef ZB_CORE_KEYS_H
#define ZB_CORE_KEYS_H

#include "core/base.h"

#define ZB_KEY_LIST(ITEM) \
  ITEM(UNKNOWN, "unknown") \
  ITEM(A, "a") ITEM(B, "b") ITEM(C, "c") ITEM(D, "d") ITEM(E, "e") ITEM(F, "f") ITEM(G, "g") ITEM(H, "h") \
  ITEM(I, "i") ITEM(J, "j") ITEM(K, "k") ITEM(L, "l") ITEM(M, "m") ITEM(N, "n") ITEM(O, "o") ITEM(P, "p") \
  ITEM(Q, "q") ITEM(R, "r") ITEM(S, "s") ITEM(T, "t") ITEM(U, "u") ITEM(V, "v") ITEM(W, "w") ITEM(X, "x") \
  ITEM(Y, "y") ITEM(Z, "z") \
  ITEM(N0, "0") ITEM(N1, "1") ITEM(N2, "2") ITEM(N3, "3") ITEM(N4, "4") ITEM(N5, "5") ITEM(N6, "6") \
  ITEM(N7, "7") ITEM(N8, "8") ITEM(N9, "9") \
  ITEM(F1, "f1") ITEM(F2, "f2") ITEM(F3, "f3") ITEM(F4, "f4") ITEM(F5, "f5") ITEM(F6, "f6") \
  ITEM(F7, "f7") ITEM(F8, "f8") ITEM(F9, "f9") ITEM(F10, "f10") ITEM(F11, "f11") ITEM(F12, "f12") \
  ITEM(SPACE, "space") ITEM(RETURN, "return") ITEM(ESCAPE, "escape") ITEM(TAB, "tab") \
  ITEM(BACKSPACE, "backspace") ITEM(DELETE, "delete") ITEM(INSERT, "insert") ITEM(HOME, "home") \
  ITEM(END, "end") ITEM(PAGEUP, "pageup") ITEM(PAGEDOWN, "pagedown") \
  ITEM(UP, "up") ITEM(DOWN, "down") ITEM(LEFT, "left") ITEM(RIGHT, "right") \
  ITEM(LSHIFT, "left_shift") ITEM(RSHIFT, "right_shift") ITEM(LCTRL, "left_ctrl") \
  ITEM(RCTRL, "right_ctrl") ITEM(LALT, "left_alt") ITEM(RALT, "right_alt") \
  ITEM(MINUS, "minus") ITEM(EQUALS, "equals") ITEM(COMMA, "comma") ITEM(PERIOD, "period") \
  ITEM(SLASH, "slash") ITEM(SEMICOLON, "semicolon") ITEM(APOSTROPHE, "apostrophe") \
  ITEM(GRAVE, "grave") ITEM(LBRACKET, "left_bracket") ITEM(RBRACKET, "right_bracket") \
  ITEM(BACKSLASH, "backslash")

#define ZB_KEY_ENUM(id, name) ZB_KEY_##id,
typedef enum zb_key { ZB_KEY_LIST(ZB_KEY_ENUM) ZB_KEY_COUNT } zb_key;
#undef ZB_KEY_ENUM

typedef enum zb_mouse_button {
  ZB_MOUSE_LEFT = 1,
  ZB_MOUSE_MIDDLE = 2,
  ZB_MOUSE_RIGHT = 3,
  ZB_MOUSE_X1 = 4,
  ZB_MOUSE_X2 = 5,
  ZB_MOUSE_BUTTON_COUNT = 6
} zb_mouse_button;

#define ZB_PAD_BUTTON_LIST(ITEM) \
  ITEM(A, "a") ITEM(B, "b") ITEM(X, "x") ITEM(Y, "y") ITEM(BACK, "back") ITEM(GUIDE, "guide") \
  ITEM(START, "start") ITEM(LEFT_STICK, "left_stick") ITEM(RIGHT_STICK, "right_stick") \
  ITEM(LEFT_SHOULDER, "left_shoulder") ITEM(RIGHT_SHOULDER, "right_shoulder") \
  ITEM(DPAD_UP, "dpad_up") ITEM(DPAD_DOWN, "dpad_down") ITEM(DPAD_LEFT, "dpad_left") \
  ITEM(DPAD_RIGHT, "dpad_right")

#define ZB_PAD_BUTTON_ENUM(id, name) ZB_PAD_##id,
typedef enum zb_pad_button { ZB_PAD_BUTTON_LIST(ZB_PAD_BUTTON_ENUM) ZB_PAD_BUTTON_COUNT } zb_pad_button;
#undef ZB_PAD_BUTTON_ENUM

#define ZB_PAD_AXIS_LIST(ITEM) \
  ITEM(LEFT_X, "left_x") ITEM(LEFT_Y, "left_y") ITEM(RIGHT_X, "right_x") ITEM(RIGHT_Y, "right_y") \
  ITEM(LEFT_TRIGGER, "left_trigger") ITEM(RIGHT_TRIGGER, "right_trigger")

#define ZB_PAD_AXIS_ENUM(id, name) ZB_AXIS_##id,
typedef enum zb_pad_axis { ZB_PAD_AXIS_LIST(ZB_PAD_AXIS_ENUM) ZB_PAD_AXIS_COUNT } zb_pad_axis;
#undef ZB_PAD_AXIS_ENUM

const char *zb_key_name(zb_key key);
/* Returns ZB_KEY_UNKNOWN for unknown names. */
zb_key zb_key_from_name(const char *name);
const char *zb_mouse_button_name(zb_mouse_button button);
/* Returns 0 for unknown names. */
int zb_mouse_button_from_name(const char *name);
const char *zb_pad_button_name(zb_pad_button button);
/* Returns -1 for unknown names. */
int zb_pad_button_from_name(const char *name);
const char *zb_pad_axis_name(zb_pad_axis axis);
/* Returns -1 for unknown names. */
int zb_pad_axis_from_name(const char *name);

#endif

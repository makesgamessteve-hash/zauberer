/* SPDX-License-Identifier: MIT */
/* Device-independent input: named actions, bindings, gamepads, automatic
 * touch mode, virtual touch controls, and per-tick recording/replay. */
#ifndef ZB_INPUT_INPUT_H
#define ZB_INPUT_INPUT_H

#include "core/alloc.h"
#include "core/event.h"
#include "core/luon.h"

typedef enum zb_action_type { ZB_ACTION_BUTTON, ZB_ACTION_AXIS1, ZB_ACTION_AXIS2 } zb_action_type;

typedef enum zb_input_mode { ZB_INPUT_DESKTOP, ZB_INPUT_TOUCH } zb_input_mode;

typedef struct zb_action_state {
  bool pressed;  /* went down since the previous tick */
  bool released; /* went up since the previous tick */
  bool held;
  float x; /* button: 0/1, axis1: -1..1, axis2: x */
  float y; /* axis2: y */
} zb_action_state;

typedef struct zb_input zb_input;

#define ZB_INPUT_MAX_ACTIONS 128
#define ZB_INPUT_MAX_BINDINGS 8
#define ZB_INPUT_MAX_CONTROLS 16
#define ZB_INPUT_TOUCH_GRACE_NS 300000000ull

zb_input *zb_input_create(const zb_allocator *a);
void zb_input_destroy(zb_input *in);

/* Defines an action (or returns the existing id if the name and type match).
 * Returns -1 on a type conflict or when the action table is full. */
int zb_input_define(zb_input *in, const char *name, zb_action_type type);
int zb_input_find(const zb_input *in, const char *name);
const char *zb_input_action_name(const zb_input *in, int action);
zb_action_type zb_input_action_type(const zb_input *in, int action);
size_t zb_input_action_count(const zb_input *in);

/* Adds a binding such as "key:space", "mouse:left", "mouse:motion",
 * "gamepad:a", "gamepad:left_x", "gamepad:left_stick", "keys:a,d"
 * (axis1: negative, positive), "keys:w,s,a,d" (axis2: up, down, left,
 * right), "touch:stick:<name>", "touch:button:<name>", "xr:select",
 * "xr:squeeze" (any hand), "xr:left_select", "xr:right_squeeze" etc. */
zb_result zb_input_bind(zb_input *in, int action, const char *binding);
void zb_input_clear_bindings(zb_input *in, int action);
size_t zb_input_binding_count(const zb_input *in, int action);
const char *zb_input_binding(const zb_input *in, int action, size_t i);

/* Applies binding data: { action = { type = "button", bind = { ... } }, ... }.
 * Later calls override earlier ones per action (package overlay order). */
zb_result zb_input_apply_defaults(zb_input *in, const zb_value *table, char *err, size_t err_len);
/* Applies user overrides: { action = { "key:j", ... } }. Unknown actions and
 * invalid bindings are skipped with one warning each. Returns the number of
 * skipped entries. */
size_t zb_input_apply_user(zb_input *in, const zb_value *table);
/* Records a user override in memory (persist with zb_input_write_user). */
zb_result zb_input_set_user_binding(zb_input *in, int action, const char *const *bindings, size_t count);
/* Serialises user overrides as a luon table expression into buf. */
size_t zb_input_write_user(const zb_input *in, char *buf, size_t len);

void zb_input_handle_event(zb_input *in, const zb_event *ev);
/* Samples devices into the per-tick snapshot; call once before each tick. */
void zb_input_sample(zb_input *in, uint64_t now_ns);
const zb_action_state *zb_input_state(const zb_input *in, int action);
uint8_t zb_input_player(const zb_input *in);

zb_input_mode zb_input_get_mode(const zb_input *in);
void zb_input_set_mode(zb_input *in, zb_input_mode mode);
/* True once after the mode changed (cleared when read). */
bool zb_input_take_mode_change(zb_input *in);

/* Virtual touch controls in normalised screen coordinates (0..1). */
int zb_input_add_stick(zb_input *in, const char *name, float cx, float cy, float radius);
int zb_input_add_button(zb_input *in, const char *name, float x, float y, float w, float h);

/* Test and replay injection: overrides an action's value for n ticks. */
void zb_input_inject(zb_input *in, int action, float x, float y, unsigned ticks);

/* ---- Recording ------------------------------------------------------------- */

typedef struct zb_input_recorder zb_input_recorder;

/* Serialises snapshots to memory; the caller writes the bytes to a file. */
zb_input_recorder *zb_input_recorder_create(const zb_allocator *a, const zb_input *in, uint64_t seed,
                                            uint64_t package_hash);
void zb_input_recorder_add(zb_input_recorder *rec, const zb_input *in);
const unsigned char *zb_input_recorder_bytes(zb_input_recorder *rec, size_t *len);
void zb_input_recorder_destroy(zb_input_recorder *rec);

typedef struct zb_input_replay zb_input_replay;

/* Parses a recording; actions are matched by name. */
zb_input_replay *zb_input_replay_open(const zb_allocator *a, zb_input *in, const unsigned char *data, size_t len,
                                      char *err, size_t err_len);
uint64_t zb_input_replay_seed(const zb_input_replay *rp);
uint64_t zb_input_replay_package_hash(const zb_input_replay *rp);
uint64_t zb_input_replay_tick_count(const zb_input_replay *rp);
/* Replaces the snapshot with the next recorded tick; false at the end. */
bool zb_input_replay_next(zb_input_replay *rp, zb_input *in);
void zb_input_replay_close(zb_input_replay *rp);

#endif

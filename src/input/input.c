/* SPDX-License-Identifier: MIT */
#include "input/input.h"

#include "core/keys.h"
#include "core/log.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PADS 8
#define MAX_TOUCHES 10
#define NAME_LEN 48
#define BIND_LEN 64
#define STICK_DEADZONE 0.15f

typedef enum src_kind {
  SRC_KEY,
  SRC_MOUSE_BUTTON,
  SRC_MOUSE_MOTION,
  SRC_PAD_BUTTON,
  SRC_PAD_AXIS,
  SRC_PAD_STICK,
  SRC_KEYS_1D,
  SRC_KEYS_2D,
  SRC_TOUCH_STICK,
  SRC_TOUCH_BUTTON
} src_kind;

typedef struct binding {
  src_kind kind;
  int a, b, c, d;
  bool invert; /* gamepad axes written with a leading '-' */
  char control[NAME_LEN];
  char text[BIND_LEN];
} binding;

typedef struct action {
  char name[NAME_LEN];
  zb_action_type type;
  binding binds[ZB_INPUT_MAX_BINDINGS];
  int nbinds;
  zb_action_state state;
  bool user_set;
  char user[ZB_INPUT_MAX_BINDINGS][BIND_LEN];
  int nuser;
  float inject_x, inject_y;
  unsigned inject_ticks;
} action;

typedef struct button_raw {
  bool down;
  bool pressed_since;
} button_raw;

typedef struct pad {
  bool connected;
  uint32_t id;
  button_raw buttons[ZB_PAD_BUTTON_COUNT];
  float axes[ZB_PAD_AXIS_COUNT];
} pad;

typedef struct touch {
  bool active;
  uint64_t finger;
  float x, y;
  int control; /* owning control or -1 */
} touch;

typedef struct control {
  char name[NAME_LEN];
  bool is_stick;
  float x, y, w, h; /* button rect, or stick centre (x, y) and radius (w) */
  bool held;
  bool pressed_since;
  float vx, vy;
} control;

struct zb_input {
  const zb_allocator *a;
  action actions[ZB_INPUT_MAX_ACTIONS];
  size_t count;
  button_raw keys[ZB_KEY_COUNT];
  button_raw mouse[ZB_MOUSE_BUTTON_COUNT];
  float mouse_dx, mouse_dy;
  pad pads[MAX_PADS];
  touch touches[MAX_TOUCHES];
  control controls[ZB_INPUT_MAX_CONTROLS];
  size_t ncontrols;
  zb_input_mode mode;
  bool mode_changed;
  uint64_t last_touch_ns;
  bool any_touch_seen;
  uint8_t player;
};

zb_input *zb_input_create(const zb_allocator *a) {
  zb_input *in = zb_calloc(a, sizeof(*in));
  if (in) {
    in->a = a;
  }
  return in;
}

void zb_input_destroy(zb_input *in) {
  if (in) {
    zb_free(in->a, in);
  }
}

int zb_input_find(const zb_input *in, const char *name) {
  for (size_t i = 0; i < in->count; ++i) {
    if (strcmp(in->actions[i].name, name) == 0) {
      return (int)i;
    }
  }
  return -1;
}

int zb_input_define(zb_input *in, const char *name, zb_action_type type) {
  int existing = zb_input_find(in, name);
  if (existing >= 0) {
    return in->actions[existing].type == type ? existing : -1;
  }
  if (in->count >= ZB_INPUT_MAX_ACTIONS || strlen(name) >= NAME_LEN || !*name) {
    return -1;
  }
  action *ac = &in->actions[in->count];
  memset(ac, 0, sizeof(*ac));
  snprintf(ac->name, sizeof ac->name, "%s", name);
  ac->type = type;
  return (int)in->count++;
}

const char *zb_input_action_name(const zb_input *in, int id) {
  return (id >= 0 && (size_t)id < in->count) ? in->actions[id].name : NULL;
}

zb_action_type zb_input_action_type(const zb_input *in, int id) { return in->actions[id].type; }

size_t zb_input_action_count(const zb_input *in) { return in->count; }

/* ---- Binding parsing ------------------------------------------------------- */

static bool parse_keys(const char *list, int *out, int n) {
  char buf[BIND_LEN];
  snprintf(buf, sizeof buf, "%s", list);
  int i = 0;
  char *p = buf;
  for (;;) {
    char *comma = strchr(p, ',');
    if (comma) {
      *comma = '\0';
    }
    zb_key k = zb_key_from_name(p);
    if (i >= n || k == ZB_KEY_UNKNOWN) {
      return false;
    }
    out[i++] = (int)k;
    if (!comma) {
      break;
    }
    p = comma + 1;
  }
  return i == n;
}

static bool parse_binding(const char *text, zb_action_type type, binding *b) {
  memset(b, 0, sizeof(*b));
  if (strlen(text) >= BIND_LEN) {
    return false;
  }
  snprintf(b->text, sizeof b->text, "%s", text);
  const char *colon = strchr(text, ':');
  if (!colon) {
    return false;
  }
  size_t dev_len = (size_t)(colon - text);
  const char *rest = colon + 1;
  if (dev_len == 3 && strncmp(text, "key", 3) == 0) {
    zb_key k = zb_key_from_name(rest);
    b->kind = SRC_KEY;
    b->a = (int)k;
    return k != ZB_KEY_UNKNOWN && type != ZB_ACTION_AXIS2;
  }
  if (dev_len == 4 && strncmp(text, "keys", 4) == 0) {
    int keys[4];
    if (type == ZB_ACTION_AXIS1 && parse_keys(rest, keys, 2)) {
      b->kind = SRC_KEYS_1D;
      b->a = keys[0];
      b->b = keys[1];
      return true;
    }
    if (type == ZB_ACTION_AXIS2 && parse_keys(rest, keys, 4)) {
      b->kind = SRC_KEYS_2D;
      b->a = keys[0];
      b->b = keys[1];
      b->c = keys[2];
      b->d = keys[3];
      return true;
    }
    return false;
  }
  if (dev_len == 5 && strncmp(text, "mouse", 5) == 0) {
    if (strcmp(rest, "motion") == 0) {
      b->kind = SRC_MOUSE_MOTION;
      return type == ZB_ACTION_AXIS2;
    }
    int btn = zb_mouse_button_from_name(rest);
    b->kind = SRC_MOUSE_BUTTON;
    b->a = btn;
    return btn > 0 && type != ZB_ACTION_AXIS2;
  }
  if (dev_len == 7 && strncmp(text, "gamepad", 7) == 0) {
    if (type == ZB_ACTION_AXIS2 && (strcmp(rest, "left_stick") == 0 || strcmp(rest, "right_stick") == 0)) {
      bool left = rest[0] == 'l';
      b->kind = SRC_PAD_STICK;
      b->a = left ? ZB_AXIS_LEFT_X : ZB_AXIS_RIGHT_X;
      b->b = left ? ZB_AXIS_LEFT_Y : ZB_AXIS_RIGHT_Y;
      return true;
    }
    int btn = zb_pad_button_from_name(rest);
    if (btn >= 0) {
      b->kind = SRC_PAD_BUTTON;
      b->a = btn;
      return type != ZB_ACTION_AXIS2;
    }
    bool invert = rest[0] == '-';
    int axis = zb_pad_axis_from_name(invert ? rest + 1 : rest);
    if (axis >= 0) {
      b->kind = SRC_PAD_AXIS;
      b->a = axis;
      b->invert = invert;
      return type != ZB_ACTION_AXIS2;
    }
    return false;
  }
  if (dev_len == 5 && strncmp(text, "touch", 5) == 0) {
    if (strncmp(rest, "stick:", 6) == 0 && rest[6] && strlen(rest + 6) < NAME_LEN) {
      b->kind = SRC_TOUCH_STICK;
      snprintf(b->control, sizeof b->control, "%s", rest + 6);
      return type == ZB_ACTION_AXIS2;
    }
    if (strncmp(rest, "button:", 7) == 0 && rest[7] && strlen(rest + 7) < NAME_LEN) {
      b->kind = SRC_TOUCH_BUTTON;
      snprintf(b->control, sizeof b->control, "%s", rest + 7);
      return type == ZB_ACTION_BUTTON;
    }
    return false;
  }
  return false;
}

zb_result zb_input_bind(zb_input *in, int action_id, const char *text) {
  if (action_id < 0 || (size_t)action_id >= in->count) {
    return ZB_ERR_INVALID;
  }
  action *ac = &in->actions[action_id];
  binding b;
  if (!parse_binding(text, ac->type, &b)) {
    return ZB_ERR_INVALID;
  }
  if (ac->nbinds >= ZB_INPUT_MAX_BINDINGS) {
    return ZB_ERR_NOMEM;
  }
  ac->binds[ac->nbinds++] = b;
  return ZB_OK;
}

void zb_input_clear_bindings(zb_input *in, int action_id) {
  if (action_id >= 0 && (size_t)action_id < in->count) {
    in->actions[action_id].nbinds = 0;
  }
}

size_t zb_input_binding_count(const zb_input *in, int action_id) { return (size_t)in->actions[action_id].nbinds; }

const char *zb_input_binding(const zb_input *in, int action_id, size_t i) {
  const action *ac = &in->actions[action_id];
  return i < (size_t)ac->nbinds ? ac->binds[i].text : NULL;
}

static bool type_from_name(const char *s, zb_action_type *out) {
  if (strcmp(s, "button") == 0) {
    *out = ZB_ACTION_BUTTON;
  } else if (strcmp(s, "axis1") == 0) {
    *out = ZB_ACTION_AXIS1;
  } else if (strcmp(s, "axis2") == 0) {
    *out = ZB_ACTION_AXIS2;
  } else {
    return false;
  }
  return true;
}

zb_result zb_input_apply_defaults(zb_input *in, const zb_value *table, char *err, size_t err_len) {
  size_t n = zb_table_count(table);
  for (size_t i = 0; i < n; ++i) {
    const zb_table_entry *e = zb_table_entry_at(table, i);
    if (e->key.type != ZB_VAL_STRING || e->value.type != ZB_VAL_TABLE) {
      snprintf(err, err_len, "input data: entries must be 'name = { type = ..., bind = { ... } }'");
      return ZB_ERR_INVALID;
    }
    const char *name = e->key.as.str.ptr;
    zb_action_type type;
    if (!type_from_name(zb_value_str(zb_table_get(&e->value, "type"), ""), &type)) {
      snprintf(err, err_len, "input data: action '%s' has no valid type (button, axis1, axis2)", name);
      return ZB_ERR_INVALID;
    }
    int id = zb_input_define(in, name, type);
    if (id < 0) {
      snprintf(err, err_len, "input data: cannot define action '%s'", name);
      return ZB_ERR_INVALID;
    }
    if (in->actions[id].user_set) {
      continue; /* user overrides always win */
    }
    zb_input_clear_bindings(in, id);
    const zb_value *binds = zb_table_get(&e->value, "bind");
    for (size_t k = 1; k <= zb_table_len(binds); ++k) {
      const char *text = zb_value_str(zb_table_index(binds, k), "");
      if (zb_input_bind(in, id, text) != ZB_OK) {
        snprintf(err, err_len, "input data: invalid binding '%s' for action '%s'", text, name);
        return ZB_ERR_INVALID;
      }
    }
  }
  return ZB_OK;
}

zb_result zb_input_set_user_binding(zb_input *in, int action_id, const char *const *bindings, size_t count) {
  if (action_id < 0 || (size_t)action_id >= in->count || count > ZB_INPUT_MAX_BINDINGS) {
    return ZB_ERR_INVALID;
  }
  action *ac = &in->actions[action_id];
  binding parsed[ZB_INPUT_MAX_BINDINGS];
  for (size_t i = 0; i < count; ++i) {
    if (!parse_binding(bindings[i], ac->type, &parsed[i])) {
      return ZB_ERR_INVALID;
    }
  }
  ac->nbinds = (int)count;
  ac->nuser = (int)count;
  ac->user_set = true;
  for (size_t i = 0; i < count; ++i) {
    ac->binds[i] = parsed[i];
    snprintf(ac->user[i], BIND_LEN, "%s", bindings[i]);
  }
  return ZB_OK;
}

size_t zb_input_apply_user(zb_input *in, const zb_value *table) {
  size_t skipped = 0;
  size_t n = zb_table_count(table);
  for (size_t i = 0; i < n; ++i) {
    const zb_table_entry *e = zb_table_entry_at(table, i);
    const char *name = e->key.type == ZB_VAL_STRING ? e->key.as.str.ptr : "?";
    int id = e->key.type == ZB_VAL_STRING ? zb_input_find(in, name) : -1;
    char key[96];
    snprintf(key, sizeof key, "user-binding:%s", name);
    if (id < 0) {
      zb_log_once(ZB_LOG_WARN, key, "user configuration: ignoring bindings for unknown action '%s'", name);
      skipped++;
      continue;
    }
    const char *list[ZB_INPUT_MAX_BINDINGS];
    size_t count = zb_table_len(&e->value);
    bool ok = count <= ZB_INPUT_MAX_BINDINGS && e->value.type == ZB_VAL_TABLE;
    for (size_t k = 0; ok && k < count; ++k) {
      list[k] = zb_value_str(zb_table_index(&e->value, k + 1), "");
    }
    if (!ok || zb_input_set_user_binding(in, id, list, count) != ZB_OK) {
      zb_log_once(ZB_LOG_WARN, key, "user configuration: ignoring invalid bindings for action '%s'", name);
      skipped++;
    }
  }
  return skipped;
}

size_t zb_input_write_user(const zb_input *in, char *buf, size_t len) {
  size_t used = 0;
#define EMIT(...) \
  do { \
    int n_ = snprintf(buf + used, used < len ? len - used : 0, __VA_ARGS__); \
    if (n_ > 0) \
      used += (size_t)n_; \
  } while (0)
  EMIT("{\n");
  for (size_t i = 0; i < in->count; ++i) {
    const action *ac = &in->actions[i];
    if (!ac->user_set) {
      continue;
    }
    EMIT("    [\"%s\"] = {", ac->name);
    for (int k = 0; k < ac->nuser; ++k) {
      EMIT("%s\"%s\"", k ? ", " : " ", ac->user[k]);
    }
    EMIT(" },\n");
  }
  EMIT("  }");
#undef EMIT
  return used;
}

/* ---- Events ----------------------------------------------------------------- */

static void set_mode(zb_input *in, zb_input_mode mode) {
  if (in->mode != mode) {
    in->mode = mode;
    in->mode_changed = true;
  }
}

static void desktop_input(zb_input *in, uint64_t t) {
  if (in->any_touch_seen && t < in->last_touch_ns + ZB_INPUT_TOUCH_GRACE_NS) {
    return;
  }
  set_mode(in, ZB_INPUT_DESKTOP);
}

static void press(button_raw *b, bool down) {
  if (down && !b->down) {
    b->pressed_since = true;
  }
  b->down = down;
}

static pad *find_pad(zb_input *in, uint32_t id, bool create) {
  for (int i = 0; i < MAX_PADS; ++i) {
    if (in->pads[i].connected && in->pads[i].id == id) {
      return &in->pads[i];
    }
  }
  if (create) {
    for (int i = 0; i < MAX_PADS; ++i) {
      if (!in->pads[i].connected) {
        memset(&in->pads[i], 0, sizeof(pad));
        in->pads[i].connected = true;
        in->pads[i].id = id;
        return &in->pads[i];
      }
    }
  }
  return NULL;
}

static bool in_control(const control *c, float x, float y) {
  if (c->is_stick) {
    float dx = x - c->x, dy = y - c->y;
    return dx * dx + dy * dy <= c->w * c->w;
  }
  return x >= c->x && x <= c->x + c->w && y >= c->y && y <= c->y + c->h;
}

static void update_control(zb_input *in, touch *t) {
  if (t->control < 0) {
    return;
  }
  control *c = &in->controls[t->control];
  if (c->is_stick) {
    float vx = (t->x - c->x) / c->w;
    float vy = -(t->y - c->y) / c->w;
    float len = sqrtf(vx * vx + vy * vy);
    if (len > 1.0f) {
      vx /= len;
      vy /= len;
    }
    c->vx = vx;
    c->vy = vy;
  }
}

static void touch_down(zb_input *in, const zb_event *ev) {
  touch *slot = NULL;
  for (int i = 0; i < MAX_TOUCHES; ++i) {
    if (!in->touches[i].active) {
      slot = &in->touches[i];
      break;
    }
  }
  if (!slot) {
    return;
  }
  slot->active = true;
  slot->finger = ev->u.touch.finger;
  slot->x = ev->u.touch.x;
  slot->y = ev->u.touch.y;
  slot->control = -1;
  for (size_t i = 0; i < in->ncontrols; ++i) {
    control *c = &in->controls[i];
    if (!c->held && in_control(c, slot->x, slot->y)) {
      slot->control = (int)i;
      c->held = true;
      c->pressed_since = true;
      update_control(in, slot);
      break;
    }
  }
}

static touch *find_touch(zb_input *in, uint64_t finger) {
  for (int i = 0; i < MAX_TOUCHES; ++i) {
    if (in->touches[i].active && in->touches[i].finger == finger) {
      return &in->touches[i];
    }
  }
  return NULL;
}

void zb_input_handle_event(zb_input *in, const zb_event *ev) {
  switch (ev->type) {
    case ZB_EV_KEY:
      if (ev->u.key.key < ZB_KEY_COUNT && !ev->u.key.repeat) {
        press(&in->keys[ev->u.key.key], ev->u.key.down);
        desktop_input(in, ev->time_ns);
      }
      break;
    case ZB_EV_MOUSE_BUTTON:
      if (!ev->u.mouse_button.synthetic && ev->u.mouse_button.button < ZB_MOUSE_BUTTON_COUNT) {
        press(&in->mouse[ev->u.mouse_button.button], ev->u.mouse_button.down);
        desktop_input(in, ev->time_ns);
      }
      break;
    case ZB_EV_MOUSE_MOTION:
      if (!ev->u.mouse_motion.synthetic) {
        in->mouse_dx += ev->u.mouse_motion.dx;
        in->mouse_dy += ev->u.mouse_motion.dy;
        if (ev->u.mouse_motion.dx != 0.0f || ev->u.mouse_motion.dy != 0.0f) {
          desktop_input(in, ev->time_ns);
        }
      }
      break;
    case ZB_EV_GAMEPAD_ADDED: find_pad(in, ev->u.gamepad.id, true); break;
    case ZB_EV_GAMEPAD_REMOVED: {
      pad *p = find_pad(in, ev->u.gamepad.id, false);
      if (p) {
        memset(p, 0, sizeof(*p)); /* releases everything it held */
      }
      break;
    }
    case ZB_EV_GAMEPAD_BUTTON: {
      pad *p = find_pad(in, ev->u.gamepad_button.id, true);
      if (p && ev->u.gamepad_button.button < ZB_PAD_BUTTON_COUNT) {
        press(&p->buttons[ev->u.gamepad_button.button], ev->u.gamepad_button.down);
        desktop_input(in, ev->time_ns);
      }
      break;
    }
    case ZB_EV_GAMEPAD_AXIS: {
      pad *p = find_pad(in, ev->u.gamepad_axis.id, true);
      if (p && ev->u.gamepad_axis.axis < ZB_PAD_AXIS_COUNT) {
        p->axes[ev->u.gamepad_axis.axis] = ev->u.gamepad_axis.value;
      }
      break;
    }
    case ZB_EV_TOUCH_DOWN:
      in->any_touch_seen = true;
      in->last_touch_ns = ev->time_ns;
      set_mode(in, ZB_INPUT_TOUCH);
      touch_down(in, ev);
      break;
    case ZB_EV_TOUCH_MOTION:
    case ZB_EV_TOUCH_UP: {
      in->last_touch_ns = ev->time_ns;
      touch *t = find_touch(in, ev->u.touch.finger);
      if (!t) {
        break;
      }
      t->x = ev->u.touch.x;
      t->y = ev->u.touch.y;
      update_control(in, t);
      if (ev->type == ZB_EV_TOUCH_UP) {
        if (t->control >= 0) {
          control *c = &in->controls[t->control];
          c->held = false;
          c->vx = 0.0f;
          c->vy = 0.0f;
        }
        t->active = false;
      }
      break;
    }
    default: break;
  }
}

/* ---- Sampling --------------------------------------------------------------- */

static float key_value(const zb_input *in, int k) {
  return (in->keys[k].down || in->keys[k].pressed_since) ? 1.0f : 0.0f;
}

static const control *find_control(const zb_input *in, const char *name) {
  for (size_t i = 0; i < in->ncontrols; ++i) {
    if (strcmp(in->controls[i].name, name) == 0) {
      return &in->controls[i];
    }
  }
  return NULL;
}

static float pad_axis(const zb_input *in, int axis) {
  float best = 0.0f;
  for (int i = 0; i < MAX_PADS; ++i) {
    if (in->pads[i].connected && fabsf(in->pads[i].axes[axis]) > fabsf(best)) {
      best = in->pads[i].axes[axis];
    }
  }
  return best;
}

static bool pad_button(const zb_input *in, int button) {
  for (int i = 0; i < MAX_PADS; ++i) {
    const button_raw *b = &in->pads[i].buttons[button];
    if (in->pads[i].connected && (b->down || b->pressed_since)) {
      return true;
    }
  }
  return false;
}

/* Evaluates one binding into (x, y); returns its magnitude. */
static float eval_binding(const zb_input *in, const binding *b, float *x, float *y) {
  *x = 0.0f;
  *y = 0.0f;
  switch (b->kind) {
    case SRC_KEY: *x = key_value(in, b->a); break;
    case SRC_MOUSE_BUTTON: *x = (in->mouse[b->a].down || in->mouse[b->a].pressed_since) ? 1.0f : 0.0f; break;
    case SRC_MOUSE_MOTION:
      *x = in->mouse_dx;
      *y = -in->mouse_dy;
      break;
    case SRC_PAD_BUTTON: *x = pad_button(in, b->a) ? 1.0f : 0.0f; break;
    case SRC_PAD_AXIS: {
      float v = pad_axis(in, b->a);
      *x = fabsf(v) < STICK_DEADZONE ? 0.0f : (b->invert ? -v : v);
      break;
    }
    case SRC_PAD_STICK: {
      float sx = pad_axis(in, b->a), sy = -pad_axis(in, b->b);
      if (sqrtf(sx * sx + sy * sy) >= STICK_DEADZONE) {
        *x = sx;
        *y = sy;
      }
      break;
    }
    case SRC_KEYS_1D: *x = key_value(in, b->b) - key_value(in, b->a); break;
    case SRC_KEYS_2D:
      *y = key_value(in, b->a) - key_value(in, b->b);
      *x = key_value(in, b->d) - key_value(in, b->c);
      break;
    case SRC_TOUCH_STICK: {
      const control *c = find_control(in, b->control);
      if (c && c->is_stick && c->held) {
        *x = c->vx;
        *y = c->vy;
      }
      break;
    }
    case SRC_TOUCH_BUTTON: {
      const control *c = find_control(in, b->control);
      *x = (c && !c->is_stick && (c->held || c->pressed_since)) ? 1.0f : 0.0f;
      break;
    }
  }
  return sqrtf(*x * *x + *y * *y);
}

void zb_input_sample(zb_input *in, uint64_t now_ns) {
  (void)now_ns;
  for (size_t i = 0; i < in->count; ++i) {
    action *ac = &in->actions[i];
    float x = 0.0f, y = 0.0f, best = 0.0f;
    for (int k = 0; k < ac->nbinds; ++k) {
      float bx, by;
      float mag = eval_binding(in, &ac->binds[k], &bx, &by);
      if (mag > best) {
        best = mag;
        x = bx;
        y = by;
      }
    }
    if (ac->inject_ticks > 0) {
      x = ac->inject_x;
      y = ac->inject_y;
      ac->inject_ticks--;
    }
    bool prev = ac->state.held;
    bool now;
    if (ac->type == ZB_ACTION_BUTTON) {
      now = x > 0.5f;
      x = now ? 1.0f : 0.0f;
      y = 0.0f;
    } else {
      now = fabsf(x) > 0.0f || fabsf(y) > 0.0f;
    }
    if (ac->type == ZB_ACTION_AXIS1) {
      x = x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
      y = 0.0f;
    }
    ac->state.pressed = now && !prev;
    ac->state.released = !now && prev;
    ac->state.held = now;
    ac->state.x = x;
    ac->state.y = y;
  }
  for (int k = 0; k < ZB_KEY_COUNT; ++k) {
    in->keys[k].pressed_since = false;
  }
  for (int k = 0; k < ZB_MOUSE_BUTTON_COUNT; ++k) {
    in->mouse[k].pressed_since = false;
  }
  for (int p = 0; p < MAX_PADS; ++p) {
    for (int k = 0; k < ZB_PAD_BUTTON_COUNT; ++k) {
      in->pads[p].buttons[k].pressed_since = false;
    }
  }
  for (size_t c = 0; c < in->ncontrols; ++c) {
    in->controls[c].pressed_since = false;
  }
  in->mouse_dx = 0.0f;
  in->mouse_dy = 0.0f;
}

const zb_action_state *zb_input_state(const zb_input *in, int action_id) {
  if (action_id < 0 || (size_t)action_id >= in->count) {
    return NULL;
  }
  return &in->actions[action_id].state;
}

uint8_t zb_input_player(const zb_input *in) { return in->player; }

zb_input_mode zb_input_get_mode(const zb_input *in) { return in->mode; }

void zb_input_set_mode(zb_input *in, zb_input_mode mode) { set_mode(in, mode); }

bool zb_input_take_mode_change(zb_input *in) {
  bool c = in->mode_changed;
  in->mode_changed = false;
  return c;
}

static int add_control(zb_input *in, const char *name, bool stick, float x, float y, float w, float h) {
  if (in->ncontrols >= ZB_INPUT_MAX_CONTROLS || strlen(name) >= NAME_LEN || find_control(in, name)) {
    return -1;
  }
  control *c = &in->controls[in->ncontrols];
  memset(c, 0, sizeof(*c));
  snprintf(c->name, sizeof c->name, "%s", name);
  c->is_stick = stick;
  c->x = x;
  c->y = y;
  c->w = w;
  c->h = h;
  return (int)in->ncontrols++;
}

int zb_input_add_stick(zb_input *in, const char *name, float cx, float cy, float radius) {
  return radius > 0.0f ? add_control(in, name, true, cx, cy, radius, radius) : -1;
}

int zb_input_add_button(zb_input *in, const char *name, float x, float y, float w, float h) {
  return add_control(in, name, false, x, y, w, h);
}

void zb_input_inject(zb_input *in, int action_id, float x, float y, unsigned ticks) {
  if (action_id >= 0 && (size_t)action_id < in->count) {
    action *ac = &in->actions[action_id];
    ac->inject_x = x;
    ac->inject_y = y;
    ac->inject_ticks = ticks;
  }
}

/* ---- Recording -------------------------------------------------------------- */

#define REC_MAGIC "ZBREC\0\1\0"
#define REC_VERSION 1u

typedef struct bytes {
  const zb_allocator *a;
  unsigned char *data;
  size_t len, cap;
  bool oom;
} bytes;

static void put(bytes *b, const void *src, size_t n) {
  if (b->len + n > b->cap) {
    size_t cap = b->cap ? b->cap * 2 : 256;
    while (cap < b->len + n) {
      cap *= 2;
    }
    unsigned char *d = zb_realloc(b->a, b->data, cap);
    if (!d) {
      b->oom = true;
      return;
    }
    b->data = d;
    b->cap = cap;
  }
  memcpy(b->data + b->len, src, n);
  b->len += n;
}

static void put_u32(bytes *b, uint32_t v) {
  unsigned char c[4] = {(unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24)};
  put(b, c, 4);
}

static void put_u64(bytes *b, uint64_t v) {
  put_u32(b, (uint32_t)v);
  put_u32(b, (uint32_t)(v >> 32));
}

static void put_f32(bytes *b, float f) {
  uint32_t v;
  memcpy(&v, &f, 4);
  put_u32(b, v);
}

typedef struct rec_state {
  uint8_t flags;
  float x, y;
} rec_state;

struct zb_input_recorder {
  bytes out;
  size_t actions;
  rec_state *last;
  rec_state *cur;
  uint32_t repeat;
  bool flushed;
};

zb_input_recorder *zb_input_recorder_create(const zb_allocator *a, const zb_input *in, uint64_t seed,
                                            uint64_t package_hash) {
  zb_input_recorder *rec = zb_calloc(a, sizeof(*rec));
  if (!rec) {
    return NULL;
  }
  rec->out.a = a;
  rec->actions = in->count;
  rec->last = zb_calloc(a, (in->count + 1) * sizeof(rec_state));
  rec->cur = zb_calloc(a, (in->count + 1) * sizeof(rec_state));
  if (!rec->last || !rec->cur) {
    zb_input_recorder_destroy(rec);
    return NULL;
  }
  put(&rec->out, REC_MAGIC, 8);
  put_u32(&rec->out, REC_VERSION);
  put_u64(&rec->out, seed);
  put_u64(&rec->out, package_hash);
  put_u32(&rec->out, (uint32_t)in->count);
  for (size_t i = 0; i < in->count; ++i) {
    unsigned char n = (unsigned char)strlen(in->actions[i].name);
    put(&rec->out, &n, 1);
    put(&rec->out, in->actions[i].name, n);
  }
  return rec;
}

static void rec_flush(zb_input_recorder *rec) {
  if (rec->repeat == 0) {
    return;
  }
  put_u32(&rec->out, rec->repeat);
  for (size_t i = 0; i < rec->actions; ++i) {
    put(&rec->out, &rec->last[i].flags, 1);
    put_f32(&rec->out, rec->last[i].x);
    put_f32(&rec->out, rec->last[i].y);
  }
  rec->repeat = 0;
}

void zb_input_recorder_add(zb_input_recorder *rec, const zb_input *in) {
  for (size_t i = 0; i < rec->actions; ++i) {
    const zb_action_state *s = &in->actions[i].state;
    rec->cur[i].flags = (uint8_t)((s->pressed ? 1 : 0) | (s->released ? 2 : 0) | (s->held ? 4 : 0));
    rec->cur[i].x = s->x;
    rec->cur[i].y = s->y;
  }
  if (rec->repeat > 0 && memcmp(rec->cur, rec->last, rec->actions * sizeof(rec_state)) == 0) {
    rec->repeat++;
    return;
  }
  rec_flush(rec);
  memcpy(rec->last, rec->cur, rec->actions * sizeof(rec_state));
  rec->repeat = 1;
}

const unsigned char *zb_input_recorder_bytes(zb_input_recorder *rec, size_t *len) {
  rec_flush(rec);
  *len = rec->out.oom ? 0 : rec->out.len;
  return rec->out.oom ? NULL : rec->out.data;
}

void zb_input_recorder_destroy(zb_input_recorder *rec) {
  if (!rec) {
    return;
  }
  const zb_allocator *a = rec->out.a;
  zb_free(a, rec->out.data);
  zb_free(a, rec->last);
  zb_free(a, rec->cur);
  zb_free(a, rec);
}

struct zb_input_replay {
  const zb_allocator *a;
  const unsigned char *data;
  size_t len, pos;
  uint64_t seed, package_hash;
  uint32_t actions;
  int *map;           /* recorded index -> action id (-1 if unknown) */
  uint32_t remaining; /* repeats left of the current record */
  size_t record_pos;
  uint64_t ticks;
};

static bool get(zb_input_replay *rp, void *dst, size_t n) {
  if (rp->pos + n > rp->len) {
    return false;
  }
  memcpy(dst, rp->data + rp->pos, n);
  rp->pos += n;
  return true;
}

static bool get_u32(zb_input_replay *rp, uint32_t *v) {
  unsigned char c[4];
  if (!get(rp, c, 4)) {
    return false;
  }
  *v = (uint32_t)c[0] | ((uint32_t)c[1] << 8) | ((uint32_t)c[2] << 16) | ((uint32_t)c[3] << 24);
  return true;
}

static bool get_u64(zb_input_replay *rp, uint64_t *v) {
  uint32_t lo, hi;
  if (!get_u32(rp, &lo) || !get_u32(rp, &hi)) {
    return false;
  }
  *v = (uint64_t)lo | ((uint64_t)hi << 32);
  return true;
}

zb_input_replay *zb_input_replay_open(const zb_allocator *a, zb_input *in, const unsigned char *data, size_t len,
                                      char *err, size_t err_len) {
  zb_input_replay *rp = zb_calloc(a, sizeof(*rp));
  if (!rp) {
    snprintf(err, err_len, "out of memory");
    return NULL;
  }
  rp->a = a;
  rp->data = data;
  rp->len = len;
  char magic[8];
  uint32_t version;
  if (!get(rp, magic, 8) || memcmp(magic, REC_MAGIC, 8) != 0) {
    snprintf(err, err_len, "not an input recording");
    zb_input_replay_close(rp);
    return NULL;
  }
  if (!get_u32(rp, &version) || version != REC_VERSION || !get_u64(rp, &rp->seed) || !get_u64(rp, &rp->package_hash) ||
      !get_u32(rp, &rp->actions) || rp->actions > ZB_INPUT_MAX_ACTIONS) {
    snprintf(err, err_len, "unsupported or truncated recording header");
    zb_input_replay_close(rp);
    return NULL;
  }
  rp->map = zb_calloc(a, (rp->actions + 1) * sizeof(int));
  if (!rp->map) {
    snprintf(err, err_len, "out of memory");
    zb_input_replay_close(rp);
    return NULL;
  }
  for (uint32_t i = 0; i < rp->actions; ++i) {
    unsigned char n;
    char name[256];
    if (!get(rp, &n, 1) || !get(rp, name, n)) {
      snprintf(err, err_len, "truncated recording header");
      zb_input_replay_close(rp);
      return NULL;
    }
    name[n] = '\0';
    rp->map[i] = zb_input_find(in, name);
  }
  /* Count ticks. */
  size_t start = rp->pos;
  size_t record = 4 + (size_t)rp->actions * 9;
  while (rp->pos + record <= rp->len) {
    uint32_t rep = 0;
    get_u32(rp, &rep);
    rp->ticks += rep;
    rp->pos += record - 4;
  }
  rp->pos = start;
  return rp;
}

uint64_t zb_input_replay_seed(const zb_input_replay *rp) { return rp->seed; }
uint64_t zb_input_replay_package_hash(const zb_input_replay *rp) { return rp->package_hash; }
uint64_t zb_input_replay_tick_count(const zb_input_replay *rp) { return rp->ticks; }

bool zb_input_replay_next(zb_input_replay *rp, zb_input *in) {
  if (rp->remaining == 0) {
    if (!get_u32(rp, &rp->remaining) || rp->remaining == 0) {
      rp->remaining = 0;
      return false;
    }
    rp->record_pos = rp->pos;
    rp->pos += (size_t)rp->actions * 9;
    if (rp->pos > rp->len) {
      rp->remaining = 0;
      return false;
    }
  }
  const unsigned char *p = rp->data + rp->record_pos;
  for (uint32_t i = 0; i < rp->actions; ++i, p += 9) {
    int id = rp->map[i];
    if (id < 0) {
      continue;
    }
    uint32_t xb = (uint32_t)p[1] | ((uint32_t)p[2] << 8) | ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);
    uint32_t yb = (uint32_t)p[5] | ((uint32_t)p[6] << 8) | ((uint32_t)p[7] << 16) | ((uint32_t)p[8] << 24);
    zb_action_state *s = &in->actions[id].state;
    s->pressed = (p[0] & 1) != 0;
    s->released = (p[0] & 2) != 0;
    s->held = (p[0] & 4) != 0;
    memcpy(&s->x, &xb, 4);
    memcpy(&s->y, &yb, 4);
  }
  rp->remaining--;
  return true;
}

void zb_input_replay_close(zb_input_replay *rp) {
  if (!rp) {
    return;
  }
  zb_free(rp->a, rp->map);
  zb_free(rp->a, rp);
}

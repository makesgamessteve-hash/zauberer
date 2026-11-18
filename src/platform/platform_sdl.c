/* SPDX-License-Identifier: MIT */
/* SDL3 implementation of the platform layer. */
#include "platform/platform.h"

#include "core/keys.h"
#include "core/log.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define ZB_MAX_PADS 8
#define ZB_AUDIO_CHUNK 1024

struct zb_platform {
  const zb_allocator *a;
  SDL_Window *window;
  SDL_GLContext gl;
  bool gles;
  bool fullscreen;
  bool video_init;
  zb_fs_api fs;
  SDL_AudioStream *audio;
  zb_audio_fill_fn audio_fill;
  void *audio_user;
  float audio_buf[ZB_AUDIO_CHUNK * 2];
  SDL_Gamepad *pads[ZB_MAX_PADS];
  zb_state_fn state_fn;
  void *state_user;
  zb_xr_handler xr;
  int xr_status; /* last zb_xr_status the page reported, -1 = none yet */
};

/* SDL user event that carries the browser's audio unlock (first gesture). */
static Uint32 g_audio_unlock_event;

/* ---- Names ------------------------------------------------------------------ */

const char *zb_platform_name(void) {
#if defined(__EMSCRIPTEN__)
  return "web";
#elif defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#else
  return "linux";
#endif
}

/* ---- File system ----------------------------------------------------------- */

static zb_result sdl_fs_read(void *ctx, const char *path, const zb_allocator *a, char **data, size_t *len) {
  (void)ctx;
  size_t size = 0;
  void *raw = SDL_LoadFile(path, &size);
  if (!raw) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) ? ZB_ERR_IO : ZB_ERR_NOT_FOUND;
  }
  char *copy = zb_alloc(a, size + 1);
  if (!copy) {
    SDL_free(raw);
    return ZB_ERR_NOMEM;
  }
  memcpy(copy, raw, size);
  copy[size] = '\0';
  SDL_free(raw);
  *data = copy;
  *len = size;
  return ZB_OK;
}

static zb_result sdl_fs_mkdirs(void *ctx, const char *path) {
  (void)ctx;
  return SDL_CreateDirectory(path) ? ZB_OK : ZB_ERR_IO;
}

static zb_result sdl_fs_write(void *ctx, const char *path, const void *data, size_t len) {
  char dir[1024];
  snprintf(dir, sizeof dir, "%s", path);
  char *slash = strrchr(dir, '/');
  if (slash && slash != dir) {
    *slash = '\0';
    sdl_fs_mkdirs(ctx, dir);
  }
  char tmp[1100];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  if (!SDL_SaveFile(tmp, data, len)) {
    return ZB_ERR_IO;
  }
  if (!SDL_RenamePath(tmp, path)) {
    SDL_RemovePath(tmp);
    return ZB_ERR_IO;
  }
  return ZB_OK;
}

typedef struct list_ctx {
  const char *dir;
  zb_fs_list_fn fn;
  void *user;
} list_ctx;

static SDL_EnumerationResult SDLCALL list_cb(void *userdata, const char *dirname, const char *fname) {
  list_ctx *lc = userdata;
  char full[1024];
  snprintf(full, sizeof full, "%s%s", dirname, fname);
  SDL_PathInfo info;
  bool is_dir = SDL_GetPathInfo(full, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
  return lc->fn(fname, is_dir, lc->user) ? SDL_ENUM_CONTINUE : SDL_ENUM_SUCCESS;
}

static zb_result sdl_fs_list(void *ctx, const char *dir, zb_fs_list_fn fn, void *user) {
  (void)ctx;
  SDL_PathInfo info;
  if (!SDL_GetPathInfo(dir, &info) || info.type != SDL_PATHTYPE_DIRECTORY) {
    return ZB_ERR_NOT_FOUND;
  }
  list_ctx lc = {dir, fn, user};
  return SDL_EnumerateDirectory(dir, list_cb, &lc) ? ZB_OK : ZB_ERR_IO;
}

static zb_result sdl_fs_stat(void *ctx, const char *path, zb_fs_stat *st) {
  (void)ctx;
  SDL_PathInfo info;
  if (!SDL_GetPathInfo(path, &info) || info.type == SDL_PATHTYPE_NONE) {
    return ZB_ERR_NOT_FOUND;
  }
  st->is_dir = info.type == SDL_PATHTYPE_DIRECTORY;
  st->size = info.size;
  st->mtime_ns = (uint64_t)info.modify_time;
  return ZB_OK;
}

static zb_result sdl_fs_remove(void *ctx, const char *path) {
  (void)ctx;
  SDL_PathInfo info;
  if (!SDL_GetPathInfo(path, &info)) {
    return ZB_ERR_NOT_FOUND; /* SDL_RemovePath reports success for missing paths */
  }
  return SDL_RemovePath(path) ? ZB_OK : ZB_ERR_IO;
}

/* ---- Threads --------------------------------------------------------------- */

#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
static void *th_create(int (*fn)(void *), void *arg, const char *name) { return SDL_CreateThread(fn, name, arg); }
static void th_join(void *t) { SDL_WaitThread(t, NULL); }
static void *mx_create(void) { return SDL_CreateMutex(); }
static void mx_destroy(void *m) { SDL_DestroyMutex(m); }
static void mx_lock(void *m) { SDL_LockMutex(m); }
static void mx_unlock(void *m) { SDL_UnlockMutex(m); }
static void *cv_create(void) { return SDL_CreateCondition(); }
static void cv_destroy(void *c) { SDL_DestroyCondition(c); }
static void cv_wait(void *c, void *m) { SDL_WaitCondition(c, m); }
static void cv_broadcast(void *c) { SDL_BroadcastCondition(c); }

static const zb_thread_api k_threads = {th_create, th_join,   mx_create,  mx_destroy, mx_lock,
                                        mx_unlock, cv_create, cv_destroy, cv_wait,    cv_broadcast};
#endif

/* ---- Lifecycle ------------------------------------------------------------- */

#ifdef __EMSCRIPTEN__
static zb_platform *g_web_platform; /* the web build has exactly one platform */
/* Implemented in web/lib_zauberer.js. */
extern void zb_js_xr_init(void);
static void xr_report_status(zb_platform *p);
#endif

zb_platform *zb_platform_create(const zb_allocator *a) {
  zb_platform *p = zb_calloc(a, sizeof(*p));
  if (!p) {
    return NULL;
  }
  p->a = a;
  p->fs.ctx = p;
  p->fs.read = sdl_fs_read;
  p->fs.write = sdl_fs_write;
  p->fs.list = sdl_fs_list;
  p->fs.stat = sdl_fs_stat;
  p->fs.mkdirs = sdl_fs_mkdirs;
  p->fs.remove = sdl_fs_remove;
  p->xr_status = -1;
  /* Touches must not turn into mouse events and vice versa (input-actions). */
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
#ifdef __EMSCRIPTEN__
  g_web_platform = p;
#endif
  return p;
}

void zb_platform_destroy(zb_platform *p) {
  if (!p) {
    return;
  }
  zb_platform_audio_close(p);
  for (int i = 0; i < ZB_MAX_PADS; ++i) {
    if (p->pads[i]) {
      SDL_CloseGamepad(p->pads[i]);
    }
  }
  if (p->gl) {
    SDL_GL_DestroyContext(p->gl);
  }
  if (p->window) {
    SDL_DestroyWindow(p->window);
  }
  if (p->video_init) {
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
  }
#ifdef __EMSCRIPTEN__
  if (g_web_platform == p) {
    g_web_platform = NULL;
  }
#endif
  zb_free(p->a, p);
}

const zb_fs_api *zb_platform_fs(zb_platform *p) { return &p->fs; }

const zb_thread_api *zb_platform_threads(zb_platform *p) {
  (void)p;
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  return NULL;
#else
  return &k_threads;
#endif
}

unsigned zb_platform_cpu_count(zb_platform *p) {
  (void)p;
  int n = SDL_GetNumLogicalCPUCores();
  return n > 0 ? (unsigned)n : 1u;
}

uint64_t zb_platform_now_ns(zb_platform *p) {
  (void)p;
  return SDL_GetTicksNS();
}

const char *zb_platform_base_path(zb_platform *p) {
  (void)p;
#ifdef __EMSCRIPTEN__
  return "";
#else
  const char *base = SDL_GetBasePath();
  return base ? base : "./";
#endif
}

zb_result zb_platform_user_dir(zb_platform *p, char *out, size_t len) {
  (void)p;
#ifdef __EMSCRIPTEN__
  snprintf(out, len, "/user/");
  SDL_CreateDirectory("/user");
  return ZB_OK;
#else
#if defined(_WIN32) || defined(__APPLE__)
  char *pref = SDL_GetPrefPath("", "Zauberer");
#else
  char *pref = SDL_GetPrefPath("", "zauberer");
#endif
  if (!pref) {
    return ZB_ERR_IO;
  }
  snprintf(out, len, "%s", pref);
  SDL_free(pref);
  return ZB_OK;
#endif
}

/* ---- Window and GL --------------------------------------------------------- */

/* After the required context failed: what does the driver offer by default? */
static void detect_gl_version(SDL_Window *window, char *out, size_t len) {
  char reason[160];
  snprintf(reason, sizeof reason, "%s", SDL_GetError());
  SDL_GL_ResetAttributes();
  SDL_GLContext ctx = SDL_GL_CreateContext(window);
  const char *version = NULL;
  if (ctx) {
    typedef const unsigned char *(*get_string_fn)(unsigned int);
    get_string_fn get_string = (get_string_fn)SDL_GL_GetProcAddress("glGetString");
    version = get_string ? (const char *)get_string(0x1F02 /* GL_VERSION */) : NULL;
  }
  if (version) {
    snprintf(out, len, "OpenGL %s", version);
  } else {
    snprintf(out, len, "no usable OpenGL context (%s)", reason);
  }
  if (ctx) {
    SDL_GL_DestroyContext(ctx);
  }
}

zb_result zb_platform_open_window(zb_platform *p, const zb_window_desc *desc, char *err, size_t err_len) {
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    snprintf(err, err_len, "cannot initialise video: %s", SDL_GetError());
    return ZB_ERR_UNSUPPORTED;
  }
  p->video_init = true;
#ifdef __EMSCRIPTEN__
  p->gles = true;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  const char *required = "WebGL 2 (OpenGL ES 3.0)";
#else
  p->gles = false;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#ifdef __APPLE__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
  const char *required = "OpenGL 3.3 core profile";
#endif
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (desc->fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
  }
  p->window = SDL_CreateWindow(desc->title, desc->width, desc->height, flags);
  if (!p->window) {
    snprintf(err, err_len, "%s is required, but no window could be created (detected: %s)", required, SDL_GetError());
    return ZB_ERR_UNSUPPORTED;
  }
  p->gl = SDL_GL_CreateContext(p->window);
  if (!p->gl) {
    char detected[256];
    detect_gl_version(p->window, detected, sizeof detected);
    snprintf(err, err_len, "%s is required, but the graphics driver only provides %s", required, detected);
    SDL_DestroyWindow(p->window);
    p->window = NULL;
    return ZB_ERR_UNSUPPORTED;
  }
  SDL_GL_MakeCurrent(p->window, p->gl);
  SDL_GL_SetSwapInterval(1);
  p->fullscreen = desc->fullscreen;
#ifdef __EMSCRIPTEN__
  zb_js_xr_init(); /* the WebXR bridge looks for an immersive VR device */
#endif
  return ZB_OK;
}

bool zb_platform_has_window(zb_platform *p) { return p->window != NULL; }

void zb_platform_drawable_size(zb_platform *p, int *w, int *h) {
  *w = 0;
  *h = 0;
  if (p->window) {
    SDL_GetWindowSizeInPixels(p->window, w, h);
  }
}

void zb_platform_swap(zb_platform *p) {
  if (p->window) {
    SDL_GL_SwapWindow(p->window);
  }
}

void zb_platform_set_fullscreen(zb_platform *p, bool on) {
  if (p->window && SDL_SetWindowFullscreen(p->window, on)) {
    SDL_SyncWindow(p->window);
    p->fullscreen = on;
  }
}

bool zb_platform_is_fullscreen(zb_platform *p) { return p->fullscreen; }

void zb_platform_set_window_size(zb_platform *p, int w, int h) {
  if (p->window && SDL_SetWindowSize(p->window, w, h)) {
    SDL_SyncWindow(p->window);
  }
}

zb_gl_proc zb_platform_gl_proc(zb_platform *p, const char *name) {
  (void)p;
  return (zb_gl_proc)SDL_GL_GetProcAddress(name);
}

bool zb_platform_is_gles(zb_platform *p) { return p->gles; }

/* ---- Events ---------------------------------------------------------------- */

typedef struct scancode_map {
  SDL_Scancode sc;
  zb_key key;
} scancode_map;

static const scancode_map k_scancodes[] = {
    {SDL_SCANCODE_A, ZB_KEY_A},
    {SDL_SCANCODE_B, ZB_KEY_B},
    {SDL_SCANCODE_C, ZB_KEY_C},
    {SDL_SCANCODE_D, ZB_KEY_D},
    {SDL_SCANCODE_E, ZB_KEY_E},
    {SDL_SCANCODE_F, ZB_KEY_F},
    {SDL_SCANCODE_G, ZB_KEY_G},
    {SDL_SCANCODE_H, ZB_KEY_H},
    {SDL_SCANCODE_I, ZB_KEY_I},
    {SDL_SCANCODE_J, ZB_KEY_J},
    {SDL_SCANCODE_K, ZB_KEY_K},
    {SDL_SCANCODE_L, ZB_KEY_L},
    {SDL_SCANCODE_M, ZB_KEY_M},
    {SDL_SCANCODE_N, ZB_KEY_N},
    {SDL_SCANCODE_O, ZB_KEY_O},
    {SDL_SCANCODE_P, ZB_KEY_P},
    {SDL_SCANCODE_Q, ZB_KEY_Q},
    {SDL_SCANCODE_R, ZB_KEY_R},
    {SDL_SCANCODE_S, ZB_KEY_S},
    {SDL_SCANCODE_T, ZB_KEY_T},
    {SDL_SCANCODE_U, ZB_KEY_U},
    {SDL_SCANCODE_V, ZB_KEY_V},
    {SDL_SCANCODE_W, ZB_KEY_W},
    {SDL_SCANCODE_X, ZB_KEY_X},
    {SDL_SCANCODE_Y, ZB_KEY_Y},
    {SDL_SCANCODE_Z, ZB_KEY_Z},
    {SDL_SCANCODE_0, ZB_KEY_N0},
    {SDL_SCANCODE_1, ZB_KEY_N1},
    {SDL_SCANCODE_2, ZB_KEY_N2},
    {SDL_SCANCODE_3, ZB_KEY_N3},
    {SDL_SCANCODE_4, ZB_KEY_N4},
    {SDL_SCANCODE_5, ZB_KEY_N5},
    {SDL_SCANCODE_6, ZB_KEY_N6},
    {SDL_SCANCODE_7, ZB_KEY_N7},
    {SDL_SCANCODE_8, ZB_KEY_N8},
    {SDL_SCANCODE_9, ZB_KEY_N9},
    {SDL_SCANCODE_F1, ZB_KEY_F1},
    {SDL_SCANCODE_F2, ZB_KEY_F2},
    {SDL_SCANCODE_F3, ZB_KEY_F3},
    {SDL_SCANCODE_F4, ZB_KEY_F4},
    {SDL_SCANCODE_F5, ZB_KEY_F5},
    {SDL_SCANCODE_F6, ZB_KEY_F6},
    {SDL_SCANCODE_F7, ZB_KEY_F7},
    {SDL_SCANCODE_F8, ZB_KEY_F8},
    {SDL_SCANCODE_F9, ZB_KEY_F9},
    {SDL_SCANCODE_F10, ZB_KEY_F10},
    {SDL_SCANCODE_F11, ZB_KEY_F11},
    {SDL_SCANCODE_F12, ZB_KEY_F12},
    {SDL_SCANCODE_SPACE, ZB_KEY_SPACE},
    {SDL_SCANCODE_RETURN, ZB_KEY_RETURN},
    {SDL_SCANCODE_ESCAPE, ZB_KEY_ESCAPE},
    {SDL_SCANCODE_TAB, ZB_KEY_TAB},
    {SDL_SCANCODE_BACKSPACE, ZB_KEY_BACKSPACE},
    {SDL_SCANCODE_DELETE, ZB_KEY_DELETE},
    {SDL_SCANCODE_INSERT, ZB_KEY_INSERT},
    {SDL_SCANCODE_HOME, ZB_KEY_HOME},
    {SDL_SCANCODE_END, ZB_KEY_END},
    {SDL_SCANCODE_PAGEUP, ZB_KEY_PAGEUP},
    {SDL_SCANCODE_PAGEDOWN, ZB_KEY_PAGEDOWN},
    {SDL_SCANCODE_UP, ZB_KEY_UP},
    {SDL_SCANCODE_DOWN, ZB_KEY_DOWN},
    {SDL_SCANCODE_LEFT, ZB_KEY_LEFT},
    {SDL_SCANCODE_RIGHT, ZB_KEY_RIGHT},
    {SDL_SCANCODE_LSHIFT, ZB_KEY_LSHIFT},
    {SDL_SCANCODE_RSHIFT, ZB_KEY_RSHIFT},
    {SDL_SCANCODE_LCTRL, ZB_KEY_LCTRL},
    {SDL_SCANCODE_RCTRL, ZB_KEY_RCTRL},
    {SDL_SCANCODE_LALT, ZB_KEY_LALT},
    {SDL_SCANCODE_RALT, ZB_KEY_RALT},
    {SDL_SCANCODE_MINUS, ZB_KEY_MINUS},
    {SDL_SCANCODE_EQUALS, ZB_KEY_EQUALS},
    {SDL_SCANCODE_COMMA, ZB_KEY_COMMA},
    {SDL_SCANCODE_PERIOD, ZB_KEY_PERIOD},
    {SDL_SCANCODE_SLASH, ZB_KEY_SLASH},
    {SDL_SCANCODE_SEMICOLON, ZB_KEY_SEMICOLON},
    {SDL_SCANCODE_APOSTROPHE, ZB_KEY_APOSTROPHE},
    {SDL_SCANCODE_GRAVE, ZB_KEY_GRAVE},
    {SDL_SCANCODE_LEFTBRACKET, ZB_KEY_LBRACKET},
    {SDL_SCANCODE_RIGHTBRACKET, ZB_KEY_RBRACKET},
    {SDL_SCANCODE_BACKSLASH, ZB_KEY_BACKSLASH},
};

static zb_key map_scancode(SDL_Scancode sc) {
  for (size_t i = 0; i < sizeof k_scancodes / sizeof k_scancodes[0]; ++i) {
    if (k_scancodes[i].sc == sc) {
      return k_scancodes[i].key;
    }
  }
  return ZB_KEY_UNKNOWN;
}

static void pad_added(zb_platform *p, SDL_JoystickID id) {
  for (int i = 0; i < ZB_MAX_PADS; ++i) {
    if (!p->pads[i]) {
      p->pads[i] = SDL_OpenGamepad(id);
      return;
    }
  }
}

static void pad_removed(zb_platform *p, SDL_JoystickID id) {
  for (int i = 0; i < ZB_MAX_PADS; ++i) {
    if (p->pads[i] && SDL_GetGamepadID(p->pads[i]) == id) {
      SDL_CloseGamepad(p->pads[i]);
      p->pads[i] = NULL;
    }
  }
}

static float axis_value(Sint16 v) {
  float f = (float)v / 32767.0f;
  return f < -1.0f ? -1.0f : f;
}

bool zb_platform_translate_event(zb_platform *p, const void *native, zb_event *out) {
  const SDL_Event *e = native;
  memset(out, 0, sizeof(*out));
  out->time_ns = e->common.timestamp;
  switch (e->type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: out->type = ZB_EV_QUIT; return true;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      out->type = ZB_EV_RESIZE;
      out->u.resize.width = e->window.data1;
      out->u.resize.height = e->window.data2;
      return true;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      out->type = ZB_EV_KEY;
      out->u.key.key = (uint16_t)map_scancode(e->key.scancode);
      out->u.key.down = e->type == SDL_EVENT_KEY_DOWN;
      out->u.key.repeat = e->key.repeat;
      return out->u.key.key != ZB_KEY_UNKNOWN;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      out->type = ZB_EV_MOUSE_BUTTON;
      out->u.mouse_button.button = e->button.button;
      out->u.mouse_button.down = e->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
      out->u.mouse_button.x = e->button.x;
      out->u.mouse_button.y = e->button.y;
      out->u.mouse_button.synthetic = e->button.which == SDL_TOUCH_MOUSEID || e->button.which == SDL_PEN_MOUSEID;
      return true;
    case SDL_EVENT_MOUSE_MOTION:
      out->type = ZB_EV_MOUSE_MOTION;
      out->u.mouse_motion.x = e->motion.x;
      out->u.mouse_motion.y = e->motion.y;
      out->u.mouse_motion.dx = e->motion.xrel;
      out->u.mouse_motion.dy = e->motion.yrel;
      out->u.mouse_motion.synthetic = e->motion.which == SDL_TOUCH_MOUSEID || e->motion.which == SDL_PEN_MOUSEID;
      return true;
    case SDL_EVENT_MOUSE_WHEEL:
      out->type = ZB_EV_MOUSE_WHEEL;
      out->u.wheel.dx = e->wheel.x;
      out->u.wheel.dy = e->wheel.y;
      return true;
    case SDL_EVENT_GAMEPAD_ADDED:
      pad_added(p, e->gdevice.which);
      out->type = ZB_EV_GAMEPAD_ADDED;
      out->u.gamepad.id = (uint32_t)e->gdevice.which;
      return true;
    case SDL_EVENT_GAMEPAD_REMOVED:
      pad_removed(p, e->gdevice.which);
      out->type = ZB_EV_GAMEPAD_REMOVED;
      out->u.gamepad.id = (uint32_t)e->gdevice.which;
      return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
      if (e->gbutton.button >= ZB_PAD_BUTTON_COUNT) {
        return false;
      }
      out->type = ZB_EV_GAMEPAD_BUTTON;
      out->u.gamepad_button.id = (uint32_t)e->gbutton.which;
      out->u.gamepad_button.button = e->gbutton.button;
      out->u.gamepad_button.down = e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
      return true;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      if (e->gaxis.axis >= ZB_PAD_AXIS_COUNT) {
        return false;
      }
      out->type = ZB_EV_GAMEPAD_AXIS;
      out->u.gamepad_axis.id = (uint32_t)e->gaxis.which;
      out->u.gamepad_axis.axis = e->gaxis.axis;
      out->u.gamepad_axis.value = axis_value(e->gaxis.value);
      return true;
    case SDL_EVENT_USER:
      if (g_audio_unlock_event == 0 || e->user.code != (Sint32)g_audio_unlock_event) {
        return false;
      }
      out->type = ZB_EV_AUDIO_UNLOCKED;
      return true;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
      out->type = e->type == SDL_EVENT_FINGER_DOWN ? ZB_EV_TOUCH_DOWN
                  : e->type == SDL_EVENT_FINGER_UP ? ZB_EV_TOUCH_UP
                                                   : ZB_EV_TOUCH_MOTION;
      out->u.touch.finger = (uint64_t)e->tfinger.fingerID;
      out->u.touch.x = e->tfinger.x;
      out->u.touch.y = e->tfinger.y;
      return true;
    default: return false;
  }
}

void zb_platform_request_quit(zb_platform *p) {
  (void)p;
  SDL_Event e;
  SDL_zero(e);
  e.type = SDL_EVENT_QUIT;
  SDL_PushEvent(&e);
}

/* ---- Messages --------------------------------------------------------------- */

#ifdef __EMSCRIPTEN__
/* Implemented in web/lib_zauberer.js. */
extern void zb_js_show_error(const char *title, const char *message);
extern int zb_js_touch_primary(void);
extern void zb_js_sync_user_data(void);
#endif

void zb_platform_show_error(zb_platform *p, const char *title, const char *message) {
  (void)p;
  fprintf(stderr, "%s: %s\n", title, message);
#ifdef __EMSCRIPTEN__
  zb_js_show_error(title, message);
#else
  if (!getenv("ZB_NO_DIALOGS")) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
  }
#endif
}

bool zb_platform_touch_primary(zb_platform *p) {
  (void)p;
#ifdef __EMSCRIPTEN__
  return zb_js_touch_primary() != 0;
#else
  return getenv("ZB_TOUCH_PRIMARY") != NULL;
#endif
}

/* ---- Audio ------------------------------------------------------------------ */

static void SDLCALL audio_cb(void *userdata, SDL_AudioStream *stream, int additional, int total) {
  (void)total;
  zb_platform *p = userdata;
  int frames = additional / (int)(sizeof(float) * 2);
  while (frames > 0) {
    int n = frames < ZB_AUDIO_CHUNK ? frames : ZB_AUDIO_CHUNK;
    p->audio_fill(p->audio_user, p->audio_buf, n);
    SDL_PutAudioStreamData(stream, p->audio_buf, n * (int)(sizeof(float) * 2));
    frames -= n;
  }
}

zb_result zb_platform_audio_open(zb_platform *p, zb_audio_fill_fn fill, void *user, int *sample_rate) {
  if (p->audio) {
    return ZB_ERR_STATE;
  }
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    return ZB_ERR_UNSUPPORTED;
  }
  SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, 48000};
  p->audio_fill = fill;
  p->audio_user = user;
  p->audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_cb, p);
  if (!p->audio) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return ZB_ERR_UNSUPPORTED;
  }
  SDL_ResumeAudioStreamDevice(p->audio);
  *sample_rate = spec.freq;
  return ZB_OK;
}

void zb_platform_audio_close(zb_platform *p) {
  if (p->audio) {
    SDL_DestroyAudioStream(p->audio);
    p->audio = NULL;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
}

bool zb_platform_audio_is_open(zb_platform *p) { return p->audio != NULL; }

void zb_platform_user_data_sync(zb_platform *p) {
  (void)p;
#ifdef __EMSCRIPTEN__
  zb_js_sync_user_data();
#endif
}

/* ---- Web bridge ------------------------------------------------------------- */

/* Queues the audio-unlock event (called from JavaScript on the first gesture,
 * and by tests). */
void zb_platform_push_audio_unlock(void) {
  g_audio_unlock_event = 0x7a62; /* "zb": SDL_EVENT_USER code */
  SDL_Event e;
  SDL_zero(e);
  e.type = SDL_EVENT_USER;
  e.user.code = (Sint32)g_audio_unlock_event;
  SDL_PushEvent(&e);
}

void zb_platform_set_state_provider(zb_platform *p, zb_state_fn fn, void *user) {
  p->state_fn = fn;
  p->state_user = user;
}

void zb_platform_set_xr_handler(zb_platform *p, const zb_xr_handler *h) {
  memset(&p->xr, 0, sizeof p->xr);
  if (h) {
    p->xr = *h;
  }
#ifdef __EMSCRIPTEN__
  xr_report_status(p);
#endif
}

#ifdef __EMSCRIPTEN__
/* ---- WebXR bridge (called from web/lib_zauberer.js) --------------------------- */

static float g_xr_frame_data[ZB_XR_MAX_FLOATS];

static void xr_report_status(zb_platform *p) {
  if (p->xr.event && p->xr_status >= 0) {
    zb_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = ZB_EV_XR_STATUS;
    ev.u.xr_status.status = (uint8_t)p->xr_status;
    p->xr.event(p->xr.user, &ev);
  }
}

/* The buffer the bridge writes each frame into (layout: core/xr.h). */
EMSCRIPTEN_KEEPALIVE float *zb_web_xr_buffer(void);
EMSCRIPTEN_KEEPALIVE float *zb_web_xr_buffer(void) { return g_xr_frame_data; }

/* status: zb_xr_status (unsupported, supported, running). */
EMSCRIPTEN_KEEPALIVE void zb_web_xr_status(int status);
EMSCRIPTEN_KEEPALIVE void zb_web_xr_status(int status) {
  if (g_web_platform && status >= ZB_XR_UNSUPPORTED && status <= ZB_XR_RUNNING) {
    g_web_platform->xr_status = status;
    xr_report_status(g_web_platform);
  }
}

/* One XR frame of `floats` values in the buffer, presented into the layer's
 * framebuffer. Returns 0 when the session should end. */
EMSCRIPTEN_KEEPALIVE int zb_web_xr_frame(int framebuffer, int floats);
EMSCRIPTEN_KEEPALIVE int zb_web_xr_frame(int framebuffer, int floats) {
  zb_platform *p = g_web_platform;
  if (!p || !p->xr.frame) {
    return 0;
  }
  zb_xr_frame f;
  if (framebuffer < 0 || floats < 0 || floats > ZB_XR_MAX_FLOATS ||
      !zb_xr_frame_decode(g_xr_frame_data, (size_t)floats, (uint32_t)framebuffer, &f)) {
    zb_log_once(ZB_LOG_WARN, "xr-frame", "WebXR: invalid frame data from the page; frame skipped");
    return 1;
  }
  return p->xr.frame(p->xr.user, &f) ? 1 : 0;
}

/* hand: zb_xr_hand, button: zb_xr_button. */
EMSCRIPTEN_KEEPALIVE void zb_web_xr_button(int hand, int button, int down);
EMSCRIPTEN_KEEPALIVE void zb_web_xr_button(int hand, int button, int down) {
  zb_platform *p = g_web_platform;
  if (p && p->xr.event && hand >= 0 && hand < ZB_XR_HAND_COUNT && button >= 0 && button < ZB_XR_BUTTON_COUNT) {
    zb_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = ZB_EV_XR_BUTTON;
    ev.time_ns = SDL_GetTicksNS();
    ev.u.xr_button.hand = (uint8_t)hand;
    ev.u.xr_button.button = (uint8_t)button;
    ev.u.xr_button.down = down != 0;
    p->xr.event(p->xr.user, &ev);
  }
}
#endif

typedef struct fetch_box {
  zb_fetch_done_fn done;
  void *user;
} fetch_box;

#ifdef __EMSCRIPTEN__
/* Implemented in web/lib_zauberer.js. */
extern void zb_js_fetch(const char *path, void *box);

EMSCRIPTEN_KEEPALIVE void zb_web_fetch_done(void *box, int status);
EMSCRIPTEN_KEEPALIVE void zb_web_fetch_done(void *box, int status) {
  fetch_box *b = box;
  zb_fetch_done_fn done = b->done;
  void *user = b->user;
  SDL_free(b);
  done(user, status == 0 ? ZB_OK : (status == 404 ? ZB_ERR_NOT_FOUND : ZB_ERR_IO));
}

EMSCRIPTEN_KEEPALIVE void zb_web_audio_unlocked(void);
EMSCRIPTEN_KEEPALIVE void zb_web_audio_unlocked(void) { zb_platform_push_audio_unlock(); }

EMSCRIPTEN_KEEPALIVE const char *zb_web_state(void);
EMSCRIPTEN_KEEPALIVE const char *zb_web_state(void) {
  static char buf[1024];
  snprintf(buf, sizeof buf, "{}");
  if (g_web_platform && g_web_platform->state_fn) {
    g_web_platform->state_fn(g_web_platform->state_user, buf, sizeof buf);
  }
  return buf;
}

bool zb_platform_has_fetch(zb_platform *p) {
  (void)p;
  return true;
}

zb_result zb_platform_fetch(zb_platform *p, const char *path, zb_fetch_done_fn done, void *user) {
  (void)p;
  fetch_box *b = SDL_malloc(sizeof *b);
  if (!b) {
    return ZB_ERR_NOMEM;
  }
  b->done = done;
  b->user = user;
  zb_js_fetch(path, b);
  return ZB_OK;
}
#else
bool zb_platform_has_fetch(zb_platform *p) {
  (void)p;
  return false;
}

zb_result zb_platform_fetch(zb_platform *p, const char *path, zb_fetch_done_fn done, void *user) {
  (void)p;
  (void)path;
  (void)done;
  (void)user;
  return ZB_ERR_UNSUPPORTED;
}
#endif

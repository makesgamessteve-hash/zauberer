/* SPDX-License-Identifier: MIT */
/* Platform layer: the only module that talks to SDL and Emscripten. It owns
 * the window, the GL context, native events, paths, file access, threads and
 * the audio device. A headless implementation without SDL exists for the
 * web-node test build. */
#ifndef ZB_PLATFORM_PLATFORM_H
#define ZB_PLATFORM_PLATFORM_H

#include "core/alloc.h"
#include "core/event.h"
#include "core/fs.h"
#include "core/thread.h"

typedef struct zb_platform zb_platform;

typedef struct zb_window_desc {
  const char *title;
  int width;
  int height;
  bool fullscreen;
} zb_window_desc;

typedef void (*zb_gl_proc)(void);
/* Fills frames of interleaved stereo float samples. */
typedef void (*zb_audio_fill_fn)(void *user, float *out, int frames);

zb_platform *zb_platform_create(const zb_allocator *a);
void zb_platform_destroy(zb_platform *p);

/* "windows", "linux", "macos", "web" or "web-node". */
const char *zb_platform_name(void);
const zb_fs_api *zb_platform_fs(zb_platform *p);
/* NULL when threads are unavailable (single-threaded web build). */
const zb_thread_api *zb_platform_threads(zb_platform *p);
unsigned zb_platform_cpu_count(zb_platform *p);
uint64_t zb_platform_now_ns(zb_platform *p);
/* Directory containing the executable (with trailing '/'), or "" on the web. */
const char *zb_platform_base_path(zb_platform *p);
/* Writes the per-user data directory into out and creates it. */
zb_result zb_platform_user_dir(zb_platform *p, char *out, size_t len);

/* Creates the window and a GL 3.3 core (desktop) or ES 3.0 (web) context. */
zb_result zb_platform_open_window(zb_platform *p, const zb_window_desc *desc, char *err, size_t err_len);
bool zb_platform_has_window(zb_platform *p);
void zb_platform_drawable_size(zb_platform *p, int *w, int *h);
void zb_platform_swap(zb_platform *p);
void zb_platform_set_fullscreen(zb_platform *p, bool on);
/* Resizes the window (in window coordinates); the drawable-size change
 * arrives as a resize event. */
void zb_platform_set_window_size(zb_platform *p, int w, int h);
bool zb_platform_is_fullscreen(zb_platform *p);
zb_gl_proc zb_platform_gl_proc(zb_platform *p, const char *name);
bool zb_platform_is_gles(zb_platform *p);

/* Converts a native event into an engine event; false if irrelevant. */
bool zb_platform_translate_event(zb_platform *p, const void *native, zb_event *out);
/* Pushes a quit request into the native event queue. */
void zb_platform_request_quit(zb_platform *p);

/* Prints to stderr and, unless ZB_NO_DIALOGS is set, shows a dialog
 * (desktop) or an on-page message (web). */
void zb_platform_show_error(zb_platform *p, const char *title, const char *message);
/* Web: the device's primary pointer is coarse and it has touch points. */
bool zb_platform_touch_primary(zb_platform *p);

zb_result zb_platform_audio_open(zb_platform *p, zb_audio_fill_fn fill, void *user, int *sample_rate);
void zb_platform_audio_close(zb_platform *p);
bool zb_platform_audio_is_open(zb_platform *p);

/* Web: persist the user data directory (IndexedDB). No-op elsewhere. */
void zb_platform_user_data_sync(zb_platform *p);

/* Web: makes a file of the asset index available in the local file system,
 * fetching it over HTTP through Cache Storage when needed. done is called on
 * the main thread in a later frame. Unsupported where files are local. */
typedef void (*zb_fetch_done_fn)(void *user, zb_result result);
bool zb_platform_has_fetch(zb_platform *p);
zb_result zb_platform_fetch(zb_platform *p, const char *path, zb_fetch_done_fn done, void *user);

/* Web: a one-line JSON summary of the engine state for test drivers
 * (exported as zb_web_state). */
typedef void (*zb_state_fn)(void *user, char *out, size_t len);
/* Queues the audio-unlock event (web: the first user gesture; also tests). */
void zb_platform_push_audio_unlock(void);
void zb_platform_set_state_provider(zb_platform *p, zb_state_fn fn, void *user);

#endif

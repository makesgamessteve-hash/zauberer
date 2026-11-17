/* SPDX-License-Identifier: MIT */
#include "core/log.h"

#include "core/hash.h"

#include <stdio.h>

typedef struct log_sink {
  zb_log_sink_fn fn;
  void *user;
} log_sink;

static log_sink g_sinks[ZB_LOG_MAX_SINKS];
static zb_log_level g_level = ZB_LOG_INFO;
static bool g_stderr = true;
static uint64_t g_once[ZB_LOG_MAX_ONCE];
static size_t g_once_count;

static const char *const k_level_names[] = {"trace", "debug", "info", "warn", "error", "off"};

const char *zb_log_level_name(zb_log_level level) {
  if ((unsigned)level > (unsigned)ZB_LOG_OFF) {
    return "?";
  }
  return k_level_names[level];
}

void zb_log_set_level(zb_log_level level) { g_level = level; }

zb_log_level zb_log_get_level(void) { return g_level; }

bool zb_log_add_sink(zb_log_sink_fn fn, void *user) {
  for (size_t i = 0; i < ZB_LOG_MAX_SINKS; ++i) {
    if (!g_sinks[i].fn) {
      g_sinks[i].fn = fn;
      g_sinks[i].user = user;
      return true;
    }
  }
  return false;
}

void zb_log_remove_sink(zb_log_sink_fn fn, void *user) {
  for (size_t i = 0; i < ZB_LOG_MAX_SINKS; ++i) {
    if (g_sinks[i].fn == fn && g_sinks[i].user == user) {
      g_sinks[i].fn = NULL;
      g_sinks[i].user = NULL;
    }
  }
}

void zb_log_set_stderr(bool enabled) { g_stderr = enabled; }

void zb_log_reset(void) {
  for (size_t i = 0; i < ZB_LOG_MAX_SINKS; ++i) {
    g_sinks[i].fn = NULL;
    g_sinks[i].user = NULL;
  }
  g_once_count = 0;
  g_level = ZB_LOG_INFO;
  g_stderr = true;
}

void zb_logv(zb_log_level level, const char *fmt, va_list ap) {
  if (level < g_level || level >= ZB_LOG_OFF) {
    return;
  }
  char buf[2048];
  vsnprintf(buf, sizeof buf, fmt, ap);
  if (g_stderr) {
    fprintf(stderr, "[%s] %s\n", zb_log_level_name(level), buf);
  }
  for (size_t i = 0; i < ZB_LOG_MAX_SINKS; ++i) {
    if (g_sinks[i].fn) {
      g_sinks[i].fn(level, buf, g_sinks[i].user);
    }
  }
}

void zb_log(zb_log_level level, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  zb_logv(level, fmt, ap);
  va_end(ap);
}

bool zb_log_once(zb_log_level level, const char *key, const char *fmt, ...) {
  uint64_t h = zb_hash_str(key);
  for (size_t i = 0; i < g_once_count; ++i) {
    if (g_once[i] == h) {
      return false;
    }
  }
  if (g_once_count < ZB_LOG_MAX_ONCE) {
    g_once[g_once_count++] = h;
  }
  va_list ap;
  va_start(ap, fmt);
  zb_logv(level, fmt, ap);
  va_end(ap);
  return true;
}

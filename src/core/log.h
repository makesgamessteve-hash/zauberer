/* SPDX-License-Identifier: MIT */
/* Logging with levels, pluggable sinks and once-per-key warnings. */
#ifndef ZB_CORE_LOG_H
#define ZB_CORE_LOG_H

#include "core/base.h"

#include <stdarg.h>

typedef enum zb_log_level {
  ZB_LOG_TRACE,
  ZB_LOG_DEBUG,
  ZB_LOG_INFO,
  ZB_LOG_WARN,
  ZB_LOG_ERROR,
  ZB_LOG_OFF
} zb_log_level;

typedef void (*zb_log_sink_fn)(zb_log_level level, const char *message, void *user);

#define ZB_LOG_MAX_SINKS 8
#define ZB_LOG_MAX_ONCE 1024

const char *zb_log_level_name(zb_log_level level);
void zb_log_set_level(zb_log_level level);
zb_log_level zb_log_get_level(void);
/* Adds a sink; returns false when all slots are used. */
bool zb_log_add_sink(zb_log_sink_fn fn, void *user);
void zb_log_remove_sink(zb_log_sink_fn fn, void *user);
/* Enable or disable the built-in stderr sink (enabled by default). */
void zb_log_set_stderr(bool enabled);
/* Clears sinks, once-keys and the level (used between tests). */
void zb_log_reset(void);

void zb_log(zb_log_level level, const char *fmt, ...) ZB_PRINTF(2, 3);
void zb_logv(zb_log_level level, const char *fmt, va_list ap);
/* Logs only the first time key is seen; returns true if it logged. */
bool zb_log_once(zb_log_level level, const char *key, const char *fmt, ...) ZB_PRINTF(3, 4);

#define zb_trace(...) zb_log(ZB_LOG_TRACE, __VA_ARGS__)
#define zb_debug(...) zb_log(ZB_LOG_DEBUG, __VA_ARGS__)
#define zb_info(...) zb_log(ZB_LOG_INFO, __VA_ARGS__)
#define zb_warn(...) zb_log(ZB_LOG_WARN, __VA_ARGS__)
#define zb_error(...) zb_log(ZB_LOG_ERROR, __VA_ARGS__)

#endif

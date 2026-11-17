/* SPDX-License-Identifier: MIT */
/* Basic types, result codes and assertions shared by every module. */
#ifndef ZB_CORE_BASE_H
#define ZB_CORE_BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum zb_result {
  ZB_OK = 0,
  ZB_ERR_NOMEM,
  ZB_ERR_IO,
  ZB_ERR_PARSE,
  ZB_ERR_INVALID,
  ZB_ERR_NOT_FOUND,
  ZB_ERR_UNSUPPORTED,
  ZB_ERR_STATE,
  ZB_ERR_CANCELLED,
  ZB_RESULT_COUNT
} zb_result;

/* Human-readable name of a result code. */
const char *zb_result_str(zb_result r);

#define ZB_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define ZB_MIN(a, b) ((a) < (b) ? (a) : (b))
#define ZB_MAX(a, b) ((a) > (b) ? (a) : (b))

#if defined(__GNUC__) || defined(__clang__)
#define ZB_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define ZB_PRINTF(fmt, args)
#endif

typedef void (*zb_assert_handler)(const char *expr, const char *file, int line);

/* Called when an assertion fails. The default handler prints and aborts. */
void zb_assert_fail(const char *expr, const char *file, int line);

/* Replace the assertion handler (tests use this); NULL restores the default. */
void zb_set_assert_handler(zb_assert_handler handler);

#ifdef NDEBUG
#define ZB_ASSERT(e) ((void)0)
#else
#define ZB_ASSERT(e) ((e) ? (void)0 : zb_assert_fail(#e, __FILE__, __LINE__))
#endif

#endif

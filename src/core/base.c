/* SPDX-License-Identifier: MIT */
#include "core/base.h"

#include <stdio.h>
#include <stdlib.h>

static const char *const k_result_names[ZB_RESULT_COUNT] = {
  "ok", "out of memory", "i/o error", "parse error", "invalid argument",
  "not found", "unsupported", "invalid state", "cancelled",
};

const char *zb_result_str(zb_result r) {
  if ((unsigned)r >= (unsigned)ZB_RESULT_COUNT) {
    return "unknown error";
  }
  return k_result_names[r];
}

static void default_assert_handler(const char *expr, const char *file, int line) {
  fprintf(stderr, "assertion failed: %s (%s:%d)\n", expr, file, line);
  abort();
}

static zb_assert_handler g_assert_handler = default_assert_handler;

void zb_set_assert_handler(zb_assert_handler handler) {
  g_assert_handler = handler ? handler : default_assert_handler;
}

void zb_assert_fail(const char *expr, const char *file, int line) {
  g_assert_handler(expr, file, line);
}

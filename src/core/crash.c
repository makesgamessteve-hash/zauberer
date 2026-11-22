/* SPDX-License-Identifier: MIT */
#include "core/crash.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/base.h"
#include "core/log.h"

#define LINE_LEN 256

static char g_lines[ZB_CRASH_LINES][LINE_LEN];
static int g_next, g_count;
static char g_path[600];
static bool g_installed, g_signals, g_writing;

static void keep(zb_log_level level, const char *message, void *user) {
  (void)user;
  snprintf(g_lines[g_next], LINE_LEN, "[%s] %s", zb_log_level_name(level), message);
  g_next = (g_next + 1) % ZB_CRASH_LINES;
  if (g_count < ZB_CRASH_LINES) {
    g_count++;
  }
}

bool zb_crash_write(const char *reason) {
  if (!g_installed || g_writing) {
    return false;
  }
  g_writing = true;
  FILE *f = fopen(g_path, "w");
  if (f) {
    fprintf(f, "Zauberer stopped: %s\n\nThe last lines logged:\n", reason ? reason : "unknown");
    for (int i = 0; i < g_count; ++i) {
      fprintf(f, "%s\n", g_lines[(g_next - g_count + i + ZB_CRASH_LINES) % ZB_CRASH_LINES]);
    }
    fclose(f);
    fprintf(stderr, "Zauberer stopped: %s. The crash log is at %s\n", reason ? reason : "unknown", g_path);
  }
  g_writing = false;
  return f != NULL;
}

static void on_assert(const char *expr, const char *file, int line) {
  char why[LINE_LEN];
  snprintf(why, sizeof why, "assertion failed: %s (%s:%d)", expr, file, line);
  zb_crash_write(why);
  zb_crash_uninstall(); /* (abort's own signal is not reported again) */
  abort();
}

#ifndef __EMSCRIPTEN__
static const int k_signals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL};

static void on_signal(int sig) {
  char why[64];
  snprintf(why, sizeof why, "fatal signal %d", sig);
  zb_crash_write(why);
  signal(sig, SIG_DFL);
  raise(sig);
}
#endif

bool zb_crash_install(const char *dir, bool signals) {
  if (!dir) {
    return false;
  }
  snprintf(g_path, sizeof g_path, "%scrash.log", dir);
  if (!g_installed) {
    g_next = g_count = 0;
    if (!zb_log_add_sink(keep, NULL)) {
      return false;
    }
    g_installed = true;
    zb_set_assert_handler(on_assert);
  }
#ifndef __EMSCRIPTEN__
  if (signals && !g_signals) {
    for (size_t i = 0; i < sizeof k_signals / sizeof k_signals[0]; ++i) {
      signal(k_signals[i], on_signal);
    }
    g_signals = true;
  }
#else
  (void)signals;
#endif
  return true;
}

void zb_crash_uninstall(void) {
  if (!g_installed) {
    return;
  }
  zb_log_remove_sink(keep, NULL);
  zb_set_assert_handler(NULL);
#ifndef __EMSCRIPTEN__
  if (g_signals) {
    for (size_t i = 0; i < sizeof k_signals / sizeof k_signals[0]; ++i) {
      signal(k_signals[i], SIG_DFL);
    }
    g_signals = false;
  }
#endif
  g_installed = false;
}

const char *zb_crash_path(void) { return g_installed ? g_path : ""; }

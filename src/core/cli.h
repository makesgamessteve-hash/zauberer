/* SPDX-License-Identifier: MIT */
/* Command-line options of the engine executable. */
#ifndef ZB_CORE_CLI_H
#define ZB_CORE_CLI_H

#include "core/base.h"

typedef struct zb_options {
  const char *assets_dir;        /* --assets <dir> */
  const char *user_dir;          /* --user-dir <dir> */
  bool headless;                 /* --headless */
  int64_t ticks;                 /* --ticks <n>; -1 = until quit */
  const char *test_path;         /* --test <file|dir> */
  const char *scenario_path;     /* --scenario <file> */
  const char *junit_path;        /* --junit <file> */
  const char *record_path;       /* --record <file> */
  const char *replay_path;       /* --replay <file> */
  int debug_views;               /* --debug-views <1|2> */
  const char *lua_coverage_path; /* --lua-coverage <file> */
  uint64_t seed;                 /* --seed <n> */
  bool seed_set;
  bool help;    /* --help */
  bool version; /* --version */
} zb_options;

void zb_options_defaults(zb_options *o);
/* Parses argv (argv[0] is skipped). On error writes a message into err. */
zb_result zb_options_parse(zb_options *o, int argc, char **argv, char *err, size_t err_len);
/* Usage text for --help. */
const char *zb_options_usage(void);

#endif

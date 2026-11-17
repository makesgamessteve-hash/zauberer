/* SPDX-License-Identifier: MIT */
#include "core/cli.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void zb_options_defaults(zb_options *o) {
  memset(o, 0, sizeof(*o));
  o->ticks = -1;
  o->debug_views = 1;
}

static bool parse_i64(const char *s, int64_t *out) {
  if (!s || !*s) {
    return false;
  }
  char *end;
  errno = 0;
  long long v = strtoll(s, &end, 10);
  if (errno || *end != '\0') {
    return false;
  }
  *out = (int64_t)v;
  return true;
}

static bool parse_u64(const char *s, uint64_t *out) {
  if (!s || !*s || *s == '-') {
    return false;
  }
  char *end;
  errno = 0;
  unsigned long long v = strtoull(s, &end, 0);
  if (errno || *end != '\0') {
    return false;
  }
  *out = (uint64_t)v;
  return true;
}

typedef enum opt_kind { OPT_FLAG, OPT_PATH, OPT_TICKS, OPT_VIEWS, OPT_SEED } opt_kind;

typedef struct opt_def {
  const char *name;
  opt_kind kind;
  size_t offset; /* into zb_options */
} opt_def;

#define OFF(field) offsetof(zb_options, field)

static const opt_def k_opts[] = {
  {"--assets", OPT_PATH, OFF(assets_dir)},
  {"--user-dir", OPT_PATH, OFF(user_dir)},
  {"--headless", OPT_FLAG, OFF(headless)},
  {"--ticks", OPT_TICKS, OFF(ticks)},
  {"--test", OPT_PATH, OFF(test_path)},
  {"--scenario", OPT_PATH, OFF(scenario_path)},
  {"--junit", OPT_PATH, OFF(junit_path)},
  {"--record", OPT_PATH, OFF(record_path)},
  {"--replay", OPT_PATH, OFF(replay_path)},
  {"--debug-views", OPT_VIEWS, OFF(debug_views)},
  {"--lua-coverage", OPT_PATH, OFF(lua_coverage_path)},
  {"--seed", OPT_SEED, OFF(seed)},
  {"--help", OPT_FLAG, OFF(help)},
  {"--version", OPT_FLAG, OFF(version)},
};

zb_result zb_options_parse(zb_options *o, int argc, char **argv, char *err, size_t err_len) {
  zb_options_defaults(o);
  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    const opt_def *def = NULL;
    for (size_t k = 0; k < ZB_ARRAY_LEN(k_opts); ++k) {
      if (strcmp(arg, k_opts[k].name) == 0) {
        def = &k_opts[k];
        break;
      }
    }
    if (!def) {
      snprintf(err, err_len, "unknown option '%s' (see --help)", arg);
      return ZB_ERR_INVALID;
    }
    unsigned char *field = (unsigned char *)o + def->offset;
    if (def->kind == OPT_FLAG) {
      *(bool *)field = true;
      continue;
    }
    if (i + 1 >= argc) {
      snprintf(err, err_len, "option '%s' needs a value", arg);
      return ZB_ERR_INVALID;
    }
    const char *value = argv[++i];
    switch (def->kind) {
      case OPT_PATH:
        *(const char **)field = value;
        break;
      case OPT_TICKS: {
        int64_t v;
        if (!parse_i64(value, &v) || v < 0) {
          snprintf(err, err_len, "invalid tick count '%s'", value);
          return ZB_ERR_INVALID;
        }
        o->ticks = v;
        break;
      }
      case OPT_VIEWS: {
        int64_t v;
        if (!parse_i64(value, &v) || v < 1 || v > 2) {
          snprintf(err, err_len, "invalid view count '%s' (1 or 2)", value);
          return ZB_ERR_INVALID;
        }
        o->debug_views = (int)v;
        break;
      }
      case OPT_SEED:
        if (!parse_u64(value, &o->seed)) {
          snprintf(err, err_len, "invalid seed '%s'", value);
          return ZB_ERR_INVALID;
        }
        o->seed_set = true;
        break;
      case OPT_FLAG:
        break;
    }
  }
  return ZB_OK;
}

const char *zb_options_usage(void) {
  return "usage: zauberer [options]\n"
         "  --assets <dir>        load packages from <dir> instead of ./assets\n"
         "  --user-dir <dir>      store configuration and saves below <dir>\n"
         "  --headless            run without window, graphics or audio\n"
         "  --ticks <n>           stop after exactly n simulation ticks\n"
         "  --test <path>         run Lua test files (file or directory)\n"
         "  --scenario <file>     run a scripted game scenario\n"
         "  --junit <file>        write test results as JUnit XML\n"
         "  --record <file>       record per-tick input to a file\n"
         "  --replay <file>       replay recorded input instead of devices\n"
         "  --debug-views <n>     render 1 or 2 (side-by-side stereo) views\n"
         "  --lua-coverage <file> collect Lua line coverage into <file>\n"
         "  --seed <n>            world and script random seed\n"
         "  --version             print the version and exit\n"
         "  --help                print this help and exit\n";
}

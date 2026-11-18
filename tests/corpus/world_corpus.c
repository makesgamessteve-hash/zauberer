/* SPDX-License-Identifier: MIT */
/* The nightly corpus: creates worlds for a range of seeds with the game's
 * default options and checks each against the Earth-likeness targets
 * (deep-time-geology: Earth-likeness). One line per world, then a summary;
 * the exit status is 1 when any world misses its targets.
 *
 *   world_corpus [first-seed] [count] [quick|normal|high]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/alloc.h"
#include "world/creation.h"

static double seconds(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
  unsigned long long first = argc > 1 ? strtoull(argv[1], NULL, 0) : 1;
  int count = argc > 2 ? atoi(argv[2]) : 16;
  zb_world_options o;
  zb_world_default_options(&o);
  int q = (int)o.quality;
  if (argc > 3) {
    const char *names[] = {"quick", "normal", "high"};
    for (q = 0; q < 3 && strcmp(argv[3], names[q]) != 0; ++q) {
    }
  }
  if (q == 3 || count < 1) {
    fprintf(stderr, "usage: world_corpus [first-seed] [count] [quick|normal|high]\n");
    return 2;
  }
  o.quality = (zb_world_quality)q;
  int failed = 0;
  double total = 0.0;
  for (int i = 0; i < count; ++i) {
    o.seed = first + (unsigned long long)i;
    char err[160];
    double t0 = seconds();
    zb_world_creation *c = zb_world_creation_create(zb_heap_allocator(), &o, err, sizeof err);
    if (!c) {
      printf("seed %llu: could not start: %s\n", (unsigned long long)o.seed, err);
      failed++;
      continue;
    }
    while (!zb_world_creation_advance(c)) {
    }
    double dt = seconds() - t0;
    total += dt;
    const zb_geology_metrics *m = zb_geology_get_metrics(zb_world_creation_geology(c));
    printf(
        "seed %llu: %s  land %.3f  continents %d (%d with mountains)  young %d old %d  ice %.3f  bands %u  "
        "attempts %d  %.1f s\n",
        (unsigned long long)o.seed, m->passed ? "ok    " : "MISSED", m->land_fraction, m->continents,
        m->continents_with_mountains, m->young_ranges, m->old_ranges, m->ice_fraction, m->climate_bands, m->attempts,
        dt);
    fflush(stdout);
    failed += m->passed ? 0 : 1;
    zb_world_creation_destroy(c);
  }
  printf("%d of %d world(s) met the Earth-likeness targets (%.1f s per world)\n", count - failed, count, total / count);
  return failed ? 1 : 0;
}

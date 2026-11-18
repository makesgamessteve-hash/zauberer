# SPDX-License-Identifier: MIT
# Performance budgets. They are measured only in release builds on Linux,
# where CI runs them in the linux-render job (ctest -L perf).
zb_add_unit_test(perf_terrain SOURCES perf/perf_terrain.c LIBS zb_world)
set_tests_properties(perf_terrain PROPERTIES LABELS perf RUN_SERIAL TRUE)

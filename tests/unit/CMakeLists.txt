# SPDX-License-Identifier: MIT
find_package(Threads REQUIRED)
target_sources(zb_test_support PRIVATE "${PROJECT_SOURCE_DIR}/tests/support/test_threads.c")
target_link_libraries(zb_test_support PUBLIC Threads::Threads)

foreach(t base alloc array hash log math jobs luon cli loop support)
  zb_add_unit_test(test_core_${t} SOURCES core/test_${t}.c LIBS zb_core)
endforeach()

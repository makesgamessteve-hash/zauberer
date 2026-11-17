# SPDX-License-Identifier: MIT
add_library(zb_test_support STATIC support/test_support.c)
zb_target_defaults(zb_test_support)
target_include_directories(zb_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(zb_test_support PUBLIC zb_core zb_unity)

add_subdirectory(unit)

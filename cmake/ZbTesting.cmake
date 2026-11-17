# SPDX-License-Identifier: MIT
# zb_add_unit_test(<name> SOURCES <files...> LIBS <targets...>)
#
# Creates a Unity test executable that links the shared test support library
# (leak checking through the tracking allocator, temp directories, fakes) and
# registers it with CTest.
function(zb_add_unit_test name)
  cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
  add_executable(${name} ${ARG_SOURCES})
  zb_target_defaults(${name})
  target_link_libraries(${name} PRIVATE zb_test_support zb_unity ${ARG_LIBS})
  target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tests")
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES
    ENVIRONMENT "ZB_SOURCE_DIR=${PROJECT_SOURCE_DIR};ZB_BINARY_DIR=${PROJECT_BINARY_DIR}"
    LABELS unit)
endfunction()

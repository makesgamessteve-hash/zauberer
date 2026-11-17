# SPDX-License-Identifier: MIT
add_library(zb_test_support STATIC support/test_support.c)
zb_target_defaults(zb_test_support)
target_include_directories(zb_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(zb_test_support PUBLIC zb_core zb_unity)

add_subdirectory(unit)

# A deliberately failing unit test, run by tests/engine to check failure reports.
add_executable(fixture_failing_unit fixtures/failing_unit/test_failing.c)
zb_target_defaults(fixture_failing_unit)
target_link_libraries(fixture_failing_unit PRIVATE zb_test_support zb_unity)

# Engine-level tests drive the zauberer binary from Python.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  set(ZB_ENGINE_TEST_ENV
    "ZB_BIN=$<TARGET_FILE:zauberer>"
    "ZB_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "ZB_FAILING_UNIT_FIXTURE=$<TARGET_FILE:fixture_failing_unit>"
    "ZB_NO_DIALOGS=1"
    "SDL_AUDIO_DRIVER=dummy")
  add_test(NAME engine_headless
    COMMAND "${Python3_EXECUTABLE}" -m unittest -v tests.engine.test_headless
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  set_tests_properties(engine_headless PROPERTIES ENVIRONMENT "${ZB_ENGINE_TEST_ENV}" LABELS engine TIMEOUT 900)
  if(NOT ZB_HEADLESS_ONLY)
    # Rendered tests need a GL 3.3 driver: on Linux the SDL offscreen driver with Mesa llvmpipe.
    if(UNIX AND NOT APPLE)
      set(ZB_RENDER_TESTS_DEFAULT ON)
    else()
      set(ZB_RENDER_TESTS_DEFAULT OFF)
    endif()
    option(ZB_RENDER_TESTS "Run the rendered engine tests" ${ZB_RENDER_TESTS_DEFAULT})
    if(ZB_RENDER_TESTS)
      add_test(NAME engine_render
        COMMAND "${Python3_EXECUTABLE}" -m unittest -v tests.engine.test_render
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
      set_tests_properties(engine_render PROPERTIES
        ENVIRONMENT "${ZB_ENGINE_TEST_ENV};ZB_RENDER_TESTS=1;SDL_VIDEO_DRIVER=offscreen"
        LABELS "engine;render" TIMEOUT 900 RUN_SERIAL TRUE)
    endif()
  endif()
  add_test(NAME python_tools
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tests/tools -t .
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  set_tests_properties(python_tools PROPERTIES LABELS tools)
endif()

# SPDX-License-Identifier: MIT
add_library(zb_test_support STATIC support/test_support.c)
zb_target_defaults(zb_test_support)
target_include_directories(zb_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(zb_test_support PUBLIC zb_core zb_unity)

add_subdirectory(unit)
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND UNIX AND NOT EMSCRIPTEN)
  include(perf/CMakeLists.txt)
  # The nightly corpus of worlds (.github/workflows/nightly.yml), not a CTest test.
  add_executable(world_corpus corpus/world_corpus.c)
  zb_target_defaults(world_corpus)
  target_link_libraries(world_corpus PRIVATE zb_world)
endif()

# A deliberately failing unit test, run by tests/engine to check failure reports.
add_executable(fixture_failing_unit fixtures/failing_unit/test_failing.c)
zb_target_defaults(fixture_failing_unit)
target_link_libraries(fixture_failing_unit PRIVATE zb_test_support zb_unity)

# Engine-level tests drive the zauberer binary from Python.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  # Cross builds (web-node) run executables through the emulator (Node.js).
  set(ZB_RUNNER "")
  if(CMAKE_CROSSCOMPILING_EMULATOR)
    list(GET CMAKE_CROSSCOMPILING_EMULATOR 0 ZB_RUNNER)
  endif()
  set(ZB_ENGINE_TEST_ENV
    "ZB_RUNNER=${ZB_RUNNER}"
    "ZB_BIN=$<TARGET_FILE:zauberer>"
    "ZB_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "ZB_FAILING_UNIT_FIXTURE=$<TARGET_FILE:fixture_failing_unit>"
    "ZB_NO_DIALOGS=1"
    "SDL_AUDIO_DRIVER=dummy")
  add_test(NAME engine_headless
    COMMAND "${Python3_EXECUTABLE}" -m unittest -v tests.engine.test_headless
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  set_tests_properties(engine_headless PROPERTIES ENVIRONMENT "${ZB_ENGINE_TEST_ENV}" LABELS engine TIMEOUT 6000)
  if(TARGET zauberer-relay)
    # The relay as a program: invite codes, forwarding and stopping on a signal.
    add_test(NAME relay_process
      COMMAND "${Python3_EXECUTABLE}" -m unittest -v tests.engine.test_relay
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(relay_process PROPERTIES
      ENVIRONMENT "ZB_RELAY_BIN=$<TARGET_FILE:zauberer-relay>" LABELS "engine;relay" TIMEOUT 120)
    # The M9 exit demo: a relay, a host and three clients over two days.
    add_test(NAME together_process
      COMMAND "${Python3_EXECUTABLE}" -m unittest -v tests.engine.test_together
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(together_process PROPERTIES
      ENVIRONMENT "${ZB_ENGINE_TEST_ENV};ZB_RELAY_BIN=$<TARGET_FILE:zauberer-relay>" LABELS "engine;relay"
      TIMEOUT 1800)
  endif()
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

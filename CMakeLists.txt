# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.25)

# The release version has one source: ZB_VERSION in src/core/version.h
# ("MAJOR.MINOR.PATCH" with an optional pre-release suffix such as "-rc.1").
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/version.h" ZB_VERSION_LINE REGEX "^#define ZB_VERSION \"")
if(NOT ZB_VERSION_LINE MATCHES "\"(([0-9]+)\\.([0-9]+)\\.([0-9]+))(-[0-9A-Za-z.-]+)?\"")
  message(FATAL_ERROR "src/core/version.h: ZB_VERSION is not a semantic version: ${ZB_VERSION_LINE}")
endif()
set(ZB_VERSION_FULL "${CMAKE_MATCH_1}${CMAKE_MATCH_5}")
set(ZB_VERSION_NUMBER "${CMAKE_MATCH_1}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/version.h")

project(zauberer VERSION ${ZB_VERSION_NUMBER} LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# EM_ASM/EM_JS (in SDL and the web platform layer) need the GNU dialect.
if(EMSCRIPTEN)
  set(CMAKE_C_EXTENSIONS ON)
else()
  set(CMAKE_C_EXTENSIONS OFF)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ZB_DEV "Development features: hot reload, on-screen errors, instruction budget" OFF)
option(ZB_TESTS "Build the unit tests" ON)
option(ZB_COVERAGE "Build with coverage instrumentation (GCC/Clang)" OFF)
option(ZB_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ZB_WERROR "Treat warnings as errors" ON)
option(ZB_USE_SYSTEM_SDL3 "Use an installed SDL3 instead of the pinned source" OFF)
option(ZB_HEADLESS_ONLY "Build only the headless engine (no SDL, no window); used by web-node" OFF)
option(ZB_WEB_THREADS "Web build with pthreads (requires cross-origin isolation)" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ZbCompiler)
include(ZbDependencies)
include(ZbTesting)

add_subdirectory(src)

# Store-agnostic desktop packages (install rules and CPack; docs/release.md).
if(NOT EMSCRIPTEN)
  include(ZbPackaging)
endif()

if(ZB_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

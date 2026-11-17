# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.25)
project(zauberer VERSION 0.1.0 LANGUAGES C)

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

if(ZB_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

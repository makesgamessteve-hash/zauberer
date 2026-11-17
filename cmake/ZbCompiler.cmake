# SPDX-License-Identifier: MIT
# Compiler settings shared by all first-party targets.

set(ZB_IS_MSVC OFF)
if(MSVC)
  set(ZB_IS_MSVC ON)
endif()

# Floating-point determinism: no FMA contraction, no fast-math (design §4).
if(ZB_IS_MSVC)
  add_compile_options(/fp:precise)
else()
  add_compile_options(-ffp-contract=off -fno-fast-math)
endif()

if(ZB_COVERAGE)
  if(ZB_IS_MSVC)
    message(FATAL_ERROR "ZB_COVERAGE requires GCC or Clang")
  endif()
  add_compile_options(--coverage -O0 -fno-inline)
  add_link_options(--coverage)
endif()

if(ZB_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

if(EMSCRIPTEN AND ZB_HEADLESS_ONLY)
  # web-node: every executable (engine and tests) runs under Node.js with the
  # real file system and environment.
  add_link_options(-sENVIRONMENT=node -sNODERAWFS=1 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1
    -sSTACK_SIZE=1048576 "SHELL:--pre-js ${PROJECT_SOURCE_DIR}/cmake/node-env.js")
endif()

if(EMSCRIPTEN AND ZB_WEB_THREADS AND NOT ZB_HEADLESS_ONLY)
  add_compile_options(-pthread)
  add_link_options(-pthread)
endif()

# Apply warnings and common definitions to a first-party target.
function(zb_target_defaults target)
  if(ZB_IS_MSVC)
    # Matches the GCC/Clang set: no unused-parameter warnings (C4100); C99
    # aggregate initialisers (C4204, C4221) and constant loop conditions (C4127) are fine.
    target_compile_options(${target} PRIVATE /W4 /wd4100 /wd4127 /wd4204 /wd4221)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    if(ZB_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
      -Wstrict-prototypes -Wmissing-prototypes -Wno-unused-parameter)
    if(ZB_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
  if(ZB_DEV)
    target_compile_definitions(${target} PRIVATE ZB_DEV=1)
  endif()
  if(ZB_COVERAGE)
    target_compile_definitions(${target} PRIVATE ZB_COVERAGE=1)
  endif()
  if(EMSCRIPTEN)
    # Relink when the JavaScript linked into the executables changes.
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
      "${PROJECT_SOURCE_DIR}/cmake/node-env.js" "${PROJECT_SOURCE_DIR}/web/lib_zauberer.js")
  endif()
endfunction()

# Third-party code: build quietly, never with -Werror.
function(zb_thirdparty_defaults target)
  if(ZB_IS_MSVC)
    target_compile_options(${target} PRIVATE /W0)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${target} PRIVATE -w)
  endif()
endfunction()

# SPDX-License-Identifier: MIT
# Third-party dependencies. Small libraries are vendored in third_party/ (see
# third_party/VENDORED.md). SDL3 is fetched from its official repository at a
# pinned tag and verified against the pinned commit.

set(ZB_TP "${PROJECT_SOURCE_DIR}/third_party")

# --- Lua 5.4 (vendored) -------------------------------------------------------
file(GLOB ZB_LUA_SOURCES "${ZB_TP}/lua/*.c")
list(REMOVE_ITEM ZB_LUA_SOURCES "${ZB_TP}/lua/lua.c")
add_library(zb_lua STATIC ${ZB_LUA_SOURCES})
target_include_directories(zb_lua SYSTEM PUBLIC "${ZB_TP}/lua")
zb_thirdparty_defaults(zb_lua)
if(UNIX AND NOT EMSCRIPTEN)
  target_compile_definitions(zb_lua PRIVATE LUA_USE_POSIX)
endif()
if(UNIX)
  target_link_libraries(zb_lua PUBLIC m)
endif()

# Standalone interpreter, used only by tools (LuaCov reports).
if(NOT EMSCRIPTEN)
  add_executable(zb_lua_interpreter "${ZB_TP}/lua/lua.c")
  target_link_libraries(zb_lua_interpreter PRIVATE zb_lua)
  zb_thirdparty_defaults(zb_lua_interpreter)
  set_target_properties(zb_lua_interpreter PROPERTIES OUTPUT_NAME zb_lua)
endif()

# --- stb_image, stb_vorbis, cgltf (vendored, single-file) --------------------
add_library(zb_thirdparty_impl STATIC
  "${ZB_TP}/impl/stb_image_impl.c"
  "${ZB_TP}/impl/cgltf_impl.c"
  "${ZB_TP}/impl/stb_image_write_impl.c"
  "${ZB_TP}/stb/stb_vorbis.c")
target_include_directories(zb_thirdparty_impl SYSTEM PUBLIC "${ZB_TP}/stb" "${ZB_TP}/cgltf")
zb_thirdparty_defaults(zb_thirdparty_impl)
if(UNIX)
  target_link_libraries(zb_thirdparty_impl PUBLIC m)
endif()

# --- Unity test framework (vendored) ------------------------------------------
add_library(zb_unity STATIC "${ZB_TP}/unity/unity.c")
target_include_directories(zb_unity SYSTEM PUBLIC "${ZB_TP}/unity")
target_compile_definitions(zb_unity PUBLIC UNITY_INCLUDE_DOUBLE UNITY_SUPPORT_64)
zb_thirdparty_defaults(zb_unity)

# --- SDL3 ---------------------------------------------------------------------
set(ZB_SDL3_TAG "release-3.4.20")
set(ZB_SDL3_COMMIT "f00dc919ca35b9756316e86c4777392de442f9b2")

if(NOT ZB_HEADLESS_ONLY)
  if(ZB_USE_SYSTEM_SDL3)
    find_package(SDL3 REQUIRED CONFIG)
  else()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
    set(SDL_TRAY OFF CACHE BOOL "" FORCE)
    if(EMSCRIPTEN AND ZB_WEB_THREADS)
      # SDL defaults to no threads on the web; the threaded build needs them.
      set(SDL_PTHREADS ON CACHE BOOL "" FORCE)
    endif()
    if(ZB_SANITIZE OR ZB_COVERAGE)
      # SDL itself is third-party: keep it uninstrumented.
      set(SDL_ASAN OFF CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG ${ZB_SDL3_TAG}
      GIT_SHALLOW TRUE
      SYSTEM)
    FetchContent_MakeAvailable(SDL3)
    if(EXISTS "${sdl3_SOURCE_DIR}/.git")
      execute_process(COMMAND git rev-parse HEAD
        WORKING_DIRECTORY "${sdl3_SOURCE_DIR}"
        OUTPUT_VARIABLE ZB_SDL3_ACTUAL OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(NOT ZB_SDL3_ACTUAL STREQUAL ZB_SDL3_COMMIT)
        message(FATAL_ERROR "SDL3 source at ${ZB_SDL3_ACTUAL} does not match pinned ${ZB_SDL3_COMMIT}")
      endif()
    else()
      message(WARNING "SDL3 source override without git metadata; pin not verified")
    endif()
  endif()
endif()

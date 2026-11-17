# SPDX-License-Identifier: MIT
# Engine modules as static libraries. Allowed dependencies follow
# docs/implementation-plan.md §1.1 and are checked by tools/check_includes.py.

function(zb_module name)
  cmake_parse_arguments(ARG "" "" "SOURCES;DEPS;PRIVATE_DEPS" ${ARGN})
  add_library(${name} STATIC ${ARG_SOURCES})
  zb_target_defaults(${name})
  target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  if(ARG_DEPS)
    target_link_libraries(${name} PUBLIC ${ARG_DEPS})
  endif()
  if(ARG_PRIVATE_DEPS)
    target_link_libraries(${name} PRIVATE ${ARG_PRIVATE_DEPS})
  endif()
endfunction()

zb_module(zb_core SOURCES
  core/alloc.c core/array.c core/base.c core/cli.c core/hash.c core/jobs.c core/keys.c
  core/log.c core/loop.c core/luon.c core/math.c core/memfs.c)
if(UNIX)
  target_link_libraries(zb_core PUBLIC m)
endif()

zb_module(zb_vfs SOURCES vfs/vfs.c DEPS zb_core)
zb_module(zb_input SOURCES input/input.c DEPS zb_core)
zb_module(zb_render SOURCES render/render.c render/image.c render/mesh.c render/backend_null.c
  render/backend_gl.c DEPS zb_core zb_vfs PRIVATE_DEPS zb_thirdparty_impl)
target_include_directories(zb_render SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb"
  "${PROJECT_SOURCE_DIR}/third_party/cgltf")
zb_module(zb_audio SOURCES audio/audio.c DEPS zb_core zb_vfs PRIVATE_DEPS zb_thirdparty_impl)
target_include_directories(zb_audio SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")
zb_module(zb_script SOURCES script/script.c script/script_api.c script/script_test.c
  DEPS zb_core zb_vfs zb_input zb_render zb_audio zb_lua)

if(ZB_HEADLESS_ONLY)
  zb_module(zb_platform SOURCES platform/platform_headless.c DEPS zb_core)
  set(ZB_ENTRY platform/main_headless.c)
else()
  zb_module(zb_platform SOURCES platform/platform_sdl.c DEPS zb_core SDL3::SDL3)
  set(ZB_ENTRY platform/sdl_main.c)
  if(UNIX AND ZB_TESTS)
    # Built alongside the SDL platform so its unit test runs in every POSIX build.
    zb_module(zb_platform_headless SOURCES platform/platform_headless.c DEPS zb_core)
  endif()
endif()

zb_module(zb_app SOURCES app/engine.c app/host.c
  DEPS zb_core zb_platform zb_vfs zb_input zb_render zb_audio zb_script)

add_executable(zauberer ${ZB_ENTRY})
zb_target_defaults(zauberer)
target_link_libraries(zauberer PRIVATE zb_app)
set_target_properties(zauberer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

if(EMSCRIPTEN)
  set(ZB_WEB_LINK -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sFORCE_FILESYSTEM=1)
  if(ZB_HEADLESS_ONLY)
    list(APPEND ZB_WEB_LINK -sENVIRONMENT=node -sNODERAWFS=1 -sEXIT_RUNTIME=1)
  else()
    list(APPEND ZB_WEB_LINK -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1
      -lidbfs.js "-sEXPORTED_RUNTIME_METHODS=['FS','IDBFS','UTF8ToString','ccall']"
      "-sEXPORTED_FUNCTIONS=['_main','_zb_web_fetch_done','_zb_web_audio_unlocked','_zb_web_state']"
      --js-library "${PROJECT_SOURCE_DIR}/web/lib_zauberer.js" -sMODULARIZE=0)
    if(ZB_WEB_THREADS)
      list(APPEND ZB_WEB_LINK -sPTHREAD_POOL_SIZE=4)
      set_target_properties(zauberer PROPERTIES OUTPUT_NAME zauberer)
    else()
      set_target_properties(zauberer PROPERTIES OUTPUT_NAME zauberer-st)
    endif()
  endif()
  target_link_options(zauberer PRIVATE ${ZB_WEB_LINK})
endif()

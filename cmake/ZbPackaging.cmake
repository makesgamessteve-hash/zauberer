# SPDX-License-Identifier: MIT
# Store-agnostic desktop packages (docs/release.md, concept §9.2): install rules for the
# shipped layout and CPack archives named zauberer-<version>-<os>-<arch>.
#
#   zauberer[.exe]  zauberer-relay[.exe]  assets/base/  mods/  LICENSES/  third_party/
#   LICENSE  README.md  CHANGELOG.md  CREDITS.md  THIRD_PARTY_NOTICES.md
#
# `cpack --preset release` (or tools/release.py package) writes the archive to
# build/release/packages/. Executables are signed between staging and archiving when the
# signing variables are set (cmake/ZbSign.cmake, tools/release.py sign-binaries).

find_package(Python3 COMPONENTS Interpreter)

# --- Platform tag (tools/release.py platform_tag() names the same) ------------
if(WIN32)
  set(ZB_PACKAGE_OS windows)
elseif(APPLE)
  set(ZB_PACKAGE_OS macos)
else()
  string(TOLOWER "${CMAKE_SYSTEM_NAME}" ZB_PACKAGE_OS)
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ZB_PACKAGE_ARCH)
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
  list(LENGTH CMAKE_OSX_ARCHITECTURES ZB_OSX_ARCH_COUNT)
  if(ZB_OSX_ARCH_COUNT GREATER 1)
    set(ZB_PACKAGE_ARCH universal)
  else()
    string(TOLOWER "${CMAKE_OSX_ARCHITECTURES}" ZB_PACKAGE_ARCH)
  endif()
endif()
if(ZB_PACKAGE_ARCH MATCHES "^(amd64|x64|x86_64)$")
  set(ZB_PACKAGE_ARCH x86_64)
elseif(ZB_PACKAGE_ARCH MATCHES "^(aarch64|arm64)$")
  set(ZB_PACKAGE_ARCH arm64)
endif()
set(ZB_PACKAGE_PLATFORM "${ZB_PACKAGE_OS}-${ZB_PACKAGE_ARCH}" CACHE STRING
  "Platform part of the package name (default: <os>-<arch> of the target)")

# --- Install rules: the shipped layout ---------------------------------------
install(TARGETS zauberer RUNTIME DESTINATION .)
if(TARGET zauberer-relay)
  install(TARGETS zauberer-relay RUNTIME DESTINATION .)
endif()
install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets/" DESTINATION assets PATTERN ".*" EXCLUDE)

# mods/: the repository's mods (an example mod) when there are any, and a README.
if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/mods")
  install(DIRECTORY "${PROJECT_SOURCE_DIR}/mods/" DESTINATION mods PATTERN ".*" EXCLUDE)
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/mods/README.md")
  install(FILES "${PROJECT_SOURCE_DIR}/packaging/mods-README.md" DESTINATION mods RENAME README.md)
endif()

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/README.md"
  "${PROJECT_SOURCE_DIR}/CHANGELOG.md" "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" DESTINATION .)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/LICENSES/" DESTINATION LICENSES)

# License texts of the third-party code compiled into the executables, at the paths
# THIRD_PARTY_NOTICES.md names (stb's licenses are in LICENSES/).
install(FILES "${PROJECT_SOURCE_DIR}/third_party/lua/LICENSE" DESTINATION third_party/lua)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/cgltf/LICENSE" DESTINATION third_party/cgltf)
if(NOT ZB_HEADLESS_ONLY AND DEFINED sdl3_SOURCE_DIR AND EXISTS "${sdl3_SOURCE_DIR}/LICENSE.txt")
  install(FILES "${sdl3_SOURCE_DIR}/LICENSE.txt" DESTINATION third_party/SDL3)
endif()

# CREDITS.md: every package's credits.lua and the third-party code, as one readable file.
if(Python3_FOUND)
  file(GLOB ZB_CREDIT_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/*/credits.lua"
    "${PROJECT_SOURCE_DIR}/assets/*/package.lua" "${PROJECT_SOURCE_DIR}/assets/*/data/third_party.lua"
    "${PROJECT_SOURCE_DIR}/mods/*/credits.lua" "${PROJECT_SOURCE_DIR}/mods/*/package.lua")
  set(ZB_CREDITS_FILE "${PROJECT_BINARY_DIR}/package/CREDITS.md")
  add_custom_command(OUTPUT "${ZB_CREDITS_FILE}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/release.py" credits --out "${ZB_CREDITS_FILE}"
    DEPENDS ${ZB_CREDIT_SOURCES} "${PROJECT_SOURCE_DIR}/tools/release.py"
    COMMENT "Writing CREDITS.md for the package")
  add_custom_target(zb_credits ALL DEPENDS "${ZB_CREDITS_FILE}")
  install(FILES "${ZB_CREDITS_FILE}" DESTINATION .)
else()
  message(WARNING "Python 3 not found: the package will have no CREDITS.md and no signing step")
endif()

# --- CPack --------------------------------------------------------------------
set(CPACK_PACKAGE_NAME zauberer)
set(CPACK_PACKAGE_VENDOR "Zauberer contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Zauberer: a simulation-driven flying-carpet mage game")
set(CPACK_PACKAGE_VERSION "${ZB_VERSION_FULL}")
set(CPACK_PACKAGE_FILE_NAME "zauberer-${ZB_VERSION_FULL}-${ZB_PACKAGE_PLATFORM}")
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/packages")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
# One plain folder named like the archive: no installer, no store metadata.
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
if(WIN32 OR APPLE)
  set(CPACK_GENERATOR ZIP)
else()
  set(CPACK_GENERATOR TGZ)
endif()
if(NOT MSVC)
  set(CPACK_STRIP_FILES ON)
endif()
set(CPACK_VERBATIM_VARIABLES ON)
if(Python3_FOUND)
  set(CPACK_ZB_PYTHON "${Python3_EXECUTABLE}")
  set(CPACK_ZB_RELEASE_TOOL "${PROJECT_SOURCE_DIR}/tools/release.py")
  set(CPACK_PRE_BUILD_SCRIPTS "${PROJECT_SOURCE_DIR}/cmake/ZbSign.cmake")
endif()
include(CPack)

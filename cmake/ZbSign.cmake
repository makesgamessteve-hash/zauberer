# SPDX-License-Identifier: MIT
# CPack pre-build script: signs the staged executables before they are archived.
# Signing is optional and driven by environment variables (docs/release.md):
#   ZB_SIGN_WINDOWS_CERT (Windows, signtool), ZB_SIGN_MACOS_IDENTITY (macOS, codesign).
# Without them tools/release.py logs that signing is skipped, and the package is unsigned.

if(NOT CPACK_ZB_PYTHON OR NOT CPACK_ZB_RELEASE_TOOL)
  message(STATUS "release: signing skipped: no Python interpreter")
  return()
endif()
execute_process(
  COMMAND "${CPACK_ZB_PYTHON}" "${CPACK_ZB_RELEASE_TOOL}" sign-binaries "${CPACK_TEMPORARY_INSTALL_DIRECTORY}"
  RESULT_VARIABLE ZB_SIGN_RESULT)
if(NOT ZB_SIGN_RESULT EQUAL 0)
  message(FATAL_ERROR "release: signing the executables failed (${ZB_SIGN_RESULT})")
endif()

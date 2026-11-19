/* SPDX-License-Identifier: MIT */
/* Engine and scripting API versions.
 * ZB_VERSION is the one source of the release version (semantic versioning, with an
 * optional pre-release suffix): CMakeLists.txt reads it for project() and the package
 * names, and tools/release.py checks that assets/base/package.lua matches it. */
#ifndef ZB_CORE_VERSION_H
#define ZB_CORE_VERSION_H

#define ZB_VERSION "1.0.0-rc.1"
#define ZB_API_VERSION_MAJOR 1
#define ZB_API_VERSION_MINOR 0
#define ZB_API_VERSION "1.0"

#endif

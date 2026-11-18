/* SPDX-License-Identifier: MIT */
/* Force-included into the Lua sources (see cmake/ZbDependencies.cmake): the
 * x ^ y operator uses the engine's deterministic pow, so scripts compute the
 * same values on every platform. */
#ifndef ZB_CORE_LUA_NUMPOW_H
#define ZB_CORE_LUA_NUMPOW_H

double zb_dpow(double x, double y);
#define luai_numpow(L, a, b) ((void)(L), (b) == 2 ? (a) * (a) : zb_dpow((a), (b)))

#endif

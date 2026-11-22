/* SPDX-License-Identifier: MIT */
/* The crash log: the last lines logged are kept, and on a fatal error (a
 * failed assertion, a fatal signal) they are written with the error to
 * crash.log in the user folder, and stderr says where. */
#ifndef ZB_CORE_CRASH_H
#define ZB_CORE_CRASH_H

#include <stdbool.h>

#define ZB_CRASH_LINES 200 /* lines of the log kept for the crash log */

/* Keeps the log's last lines and writes them to <dir>crash.log on a fatal
 * error; signals (on desktop) too when signals is true. Again: a new folder. */
bool zb_crash_install(const char *dir, bool signals);
/* Stops keeping lines and catching fatal errors. */
void zb_crash_uninstall(void);
/* Writes the crash log now with a reason; false when it could not be written. */
bool zb_crash_write(const char *reason);
/* The crash log's path ("" before install). */
const char *zb_crash_path(void);

#endif

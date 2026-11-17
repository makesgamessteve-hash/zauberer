/* SPDX-License-Identifier: MIT */
/* A zb_thread_api built on pthreads (POSIX) or Win32 threads, for tests of
 * core code without the SDL platform layer. */
#ifndef ZB_TEST_THREADS_H
#define ZB_TEST_THREADS_H

#include "core/thread.h"

const zb_thread_api *zb_test_thread_api(void);
/* Sleeps the calling thread. */
void zb_test_sleep_ms(unsigned ms);

#endif

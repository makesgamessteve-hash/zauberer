/* SPDX-License-Identifier: MIT */
/* Thread primitives as an injected interface, so core stays free of OS and
 * SDL dependencies. The platform layer provides the real implementation. */
#ifndef ZB_CORE_THREAD_H
#define ZB_CORE_THREAD_H

#include "core/base.h"

typedef struct zb_thread_api {
  void *(*thread_create)(int (*fn)(void *arg), void *arg, const char *name);
  void (*thread_join)(void *thread);
  void *(*mutex_create)(void);
  void (*mutex_destroy)(void *mutex);
  void (*mutex_lock)(void *mutex);
  void (*mutex_unlock)(void *mutex);
  void *(*cond_create)(void);
  void (*cond_destroy)(void *cond);
  void (*cond_wait)(void *cond, void *mutex);
  void (*cond_broadcast)(void *cond);
} zb_thread_api;

#endif

/* SPDX-License-Identifier: MIT */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "support/test_threads.h"

#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>

typedef struct thread_box {
  int (*fn)(void *);
  void *arg;
  HANDLE h;
} thread_box;

static DWORD WINAPI thread_entry(LPVOID p) {
  thread_box *b = p;
  return (DWORD)b->fn(b->arg);
}

static void *t_create(int (*fn)(void *), void *arg, const char *name) {
  (void)name;
  thread_box *b = malloc(sizeof *b);
  b->fn = fn;
  b->arg = arg;
  b->h = CreateThread(NULL, 0, thread_entry, b, 0, NULL);
  return b;
}

static void t_join(void *t) {
  thread_box *b = t;
  WaitForSingleObject(b->h, INFINITE);
  CloseHandle(b->h);
  free(b);
}

static void *m_create(void) {
  CRITICAL_SECTION *m = malloc(sizeof *m);
  InitializeCriticalSection(m);
  return m;
}
static void m_destroy(void *m) { DeleteCriticalSection(m); free(m); }
static void m_lock(void *m) { EnterCriticalSection(m); }
static void m_unlock(void *m) { LeaveCriticalSection(m); }
static void *c_create(void) {
  CONDITION_VARIABLE *c = malloc(sizeof *c);
  InitializeConditionVariable(c);
  return c;
}
static void c_destroy(void *c) { free(c); }
static void c_wait(void *c, void *m) { SleepConditionVariableCS(c, m, INFINITE); }
static void c_broadcast(void *c) { WakeAllConditionVariable(c); }

#else
#include <pthread.h>

typedef struct thread_box {
  int (*fn)(void *);
  void *arg;
  pthread_t t;
} thread_box;

static void *thread_entry(void *p) {
  thread_box *b = p;
  b->fn(b->arg);
  return NULL;
}

static void *t_create(int (*fn)(void *), void *arg, const char *name) {
  (void)name;
  thread_box *b = malloc(sizeof *b);
  b->fn = fn;
  b->arg = arg;
  if (pthread_create(&b->t, NULL, thread_entry, b) != 0) {
    free(b);
    return NULL;
  }
  return b;
}

static void t_join(void *t) {
  thread_box *b = t;
  pthread_join(b->t, NULL);
  free(b);
}

static void *m_create(void) {
  pthread_mutex_t *m = malloc(sizeof *m);
  pthread_mutex_init(m, NULL);
  return m;
}
static void m_destroy(void *m) { pthread_mutex_destroy(m); free(m); }
static void m_lock(void *m) { pthread_mutex_lock(m); }
static void m_unlock(void *m) { pthread_mutex_unlock(m); }
static void *c_create(void) {
  pthread_cond_t *c = malloc(sizeof *c);
  pthread_cond_init(c, NULL);
  return c;
}
static void c_destroy(void *c) { pthread_cond_destroy(c); free(c); }
static void c_wait(void *c, void *m) { pthread_cond_wait(c, m); }
static void c_broadcast(void *c) { pthread_cond_broadcast(c); }
#endif

static const zb_thread_api k_api = {t_create, t_join, m_create, m_destroy, m_lock, m_unlock,
                                    c_create, c_destroy, c_wait, c_broadcast};

const zb_thread_api *zb_test_thread_api(void) { return &k_api; }

void zb_test_sleep_ms(unsigned ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  struct timespec ts = {(time_t)(ms / 1000u), (long)(ms % 1000u) * 1000000L};
  nanosleep(&ts, NULL);
#endif
}

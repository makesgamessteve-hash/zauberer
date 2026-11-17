/* SPDX-License-Identifier: MIT */
/* Job system: a fixed worker pool with priorities and cancellation, or an
 * inline mode without threads (single-threaded web build, tests). */
#ifndef ZB_CORE_JOBS_H
#define ZB_CORE_JOBS_H

#include "core/alloc.h"
#include "core/thread.h"

typedef enum zb_job_priority {
  ZB_JOB_HIGH = 0,
  ZB_JOB_NORMAL = 1,
  ZB_JOB_LOW = 2,
  ZB_JOB_PRIORITY_COUNT
} zb_job_priority;

typedef enum zb_job_state {
  ZB_JOB_PENDING,
  ZB_JOB_RUNNING,
  ZB_JOB_DONE,
  ZB_JOB_CANCELLED
} zb_job_state;

typedef struct zb_jobs zb_jobs;
typedef struct zb_job zb_job;
typedef void (*zb_job_fn)(void *arg);

/* threads == NULL or workers == 0 selects inline mode. */
zb_jobs *zb_jobs_create(const zb_allocator *a, const zb_thread_api *threads, unsigned workers);
/* Stops the workers. Jobs still queued are freed without running, so every
 * handle must be released (or be pending) before calling this. */
void zb_jobs_destroy(zb_jobs *js);
bool zb_jobs_is_inline(const zb_jobs *js);
unsigned zb_jobs_worker_count(const zb_jobs *js);

/* Queues a job; returns NULL when out of memory. The caller owns the handle
 * and must release it with zb_job_release after it is no longer needed. */
zb_job *zb_job_submit(zb_jobs *js, zb_job_fn fn, void *arg, zb_job_priority priority);
/* Cancels a pending job; returns true if it will not run. */
bool zb_job_cancel(zb_jobs *js, zb_job *job);
zb_job_state zb_job_get_state(zb_jobs *js, const zb_job *job);
/* Blocks until the job is done or cancelled. In inline mode, runs it now. */
zb_job_state zb_job_wait(zb_jobs *js, zb_job *job);
void zb_job_release(zb_jobs *js, zb_job *job);
/* Inline mode time slicing: runs up to max pending jobs, returns how many ran.
 * In threaded mode this returns 0. */
size_t zb_jobs_run_pending(zb_jobs *js, size_t max);

#endif

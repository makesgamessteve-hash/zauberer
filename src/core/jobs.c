/* SPDX-License-Identifier: MIT */
#include "core/jobs.h"

struct zb_job {
  zb_job_fn fn;
  void *arg;
  zb_job_state state;
  zb_job_priority priority;
  bool released;
  zb_job *next; /* queue link */
};

struct zb_jobs {
  const zb_allocator *a;
  const zb_thread_api *t;
  void *mutex;
  void *cond_work;
  void *cond_done;
  zb_job *head[ZB_JOB_PRIORITY_COUNT];
  zb_job *tail[ZB_JOB_PRIORITY_COUNT];
  void **threads;
  unsigned workers;
  bool stopping;
};

static void lock(zb_jobs *js) {
  if (js->t) {
    js->t->mutex_lock(js->mutex);
  }
}

static void unlock(zb_jobs *js) {
  if (js->t) {
    js->t->mutex_unlock(js->mutex);
  }
}

static zb_job *pop_locked(zb_jobs *js) {
  for (int p = 0; p < ZB_JOB_PRIORITY_COUNT; ++p) {
    zb_job *j = js->head[p];
    if (j) {
      js->head[p] = j->next;
      if (!js->head[p]) {
        js->tail[p] = NULL;
      }
      j->next = NULL;
      return j;
    }
  }
  return NULL;
}

static bool unlink_locked(zb_jobs *js, zb_job *job) {
  zb_job **pp = &js->head[job->priority];
  zb_job *prev = NULL;
  while (*pp) {
    if (*pp == job) {
      *pp = job->next;
      if (js->tail[job->priority] == job) {
        js->tail[job->priority] = prev;
      }
      job->next = NULL;
      return true;
    }
    prev = *pp;
    pp = &(*pp)->next;
  }
  return false;
}

/* Runs a popped job; frees it if its owner already released it. */
static void run_job(zb_jobs *js, zb_job *j) {
  j->fn(j->arg);
  lock(js);
  j->state = ZB_JOB_DONE;
  bool free_it = j->released;
  if (js->t) {
    js->t->cond_broadcast(js->cond_done);
  }
  unlock(js);
  if (free_it) {
    zb_free(js->a, j);
  }
}

static int worker_main(void *arg) {
  zb_jobs *js = arg;
  for (;;) {
    lock(js);
    zb_job *j = NULL;
    while (!js->stopping && !(j = pop_locked(js))) {
      js->t->cond_wait(js->cond_work, js->mutex);
    }
    if (!j) {
      unlock(js);
      return 0;
    }
    j->state = ZB_JOB_RUNNING;
    unlock(js);
    run_job(js, j);
  }
}

zb_jobs *zb_jobs_create(const zb_allocator *a, const zb_thread_api *threads, unsigned workers) {
  zb_jobs *js = zb_calloc(a, sizeof(*js));
  if (!js) {
    return NULL;
  }
  js->a = a;
  if (!threads || workers == 0) {
    return js;
  }
  js->t = threads;
  js->mutex = threads->mutex_create();
  js->cond_work = threads->cond_create();
  js->cond_done = threads->cond_create();
  js->threads = zb_calloc(a, workers * sizeof(void *));
  if (!js->mutex || !js->cond_work || !js->cond_done || !js->threads) {
    zb_jobs_destroy(js);
    return NULL;
  }
  for (unsigned i = 0; i < workers; ++i) {
    js->threads[i] = threads->thread_create(worker_main, js, "zb-worker");
    if (!js->threads[i]) {
      zb_jobs_destroy(js);
      return NULL;
    }
    js->workers++;
  }
  return js;
}

void zb_jobs_destroy(zb_jobs *js) {
  if (!js) {
    return;
  }
  if (js->t) {
    if (js->mutex) {
      lock(js);
      js->stopping = true;
      if (js->cond_work) {
        js->t->cond_broadcast(js->cond_work);
      }
      unlock(js);
    }
    for (unsigned i = 0; i < js->workers; ++i) {
      js->t->thread_join(js->threads[i]);
    }
  }
  /* Jobs that never ran are freed; their handles become invalid. */
  zb_job *j;
  while ((j = pop_locked(js)) != NULL) {
    zb_free(js->a, j);
  }
  if (js->t) {
    if (js->cond_done) {
      js->t->cond_destroy(js->cond_done);
    }
    if (js->cond_work) {
      js->t->cond_destroy(js->cond_work);
    }
    if (js->mutex) {
      js->t->mutex_destroy(js->mutex);
    }
  }
  zb_free(js->a, js->threads);
  zb_free(js->a, js);
}

bool zb_jobs_is_inline(const zb_jobs *js) { return js->t == NULL; }

unsigned zb_jobs_worker_count(const zb_jobs *js) { return js->workers; }

zb_job *zb_job_submit(zb_jobs *js, zb_job_fn fn, void *arg, zb_job_priority priority) {
  ZB_ASSERT((unsigned)priority < ZB_JOB_PRIORITY_COUNT);
  zb_job *j = zb_calloc(js->a, sizeof(*j));
  if (!j) {
    return NULL;
  }
  j->fn = fn;
  j->arg = arg;
  j->priority = priority;
  j->state = ZB_JOB_PENDING;
  lock(js);
  if (js->tail[priority]) {
    js->tail[priority]->next = j;
  } else {
    js->head[priority] = j;
  }
  js->tail[priority] = j;
  if (js->t) {
    js->t->cond_broadcast(js->cond_work);
  }
  unlock(js);
  return j;
}

bool zb_job_cancel(zb_jobs *js, zb_job *job) {
  lock(js);
  bool cancelled = false;
  if (job->state == ZB_JOB_PENDING && unlink_locked(js, job)) {
    job->state = ZB_JOB_CANCELLED;
    cancelled = true;
    if (js->t) {
      js->t->cond_broadcast(js->cond_done);
    }
  }
  unlock(js);
  return cancelled;
}

zb_job_state zb_job_get_state(zb_jobs *js, const zb_job *job) {
  lock(js);
  zb_job_state s = job->state;
  unlock(js);
  return s;
}

zb_job_state zb_job_wait(zb_jobs *js, zb_job *job) {
  if (!js->t) {
    if (job->state == ZB_JOB_PENDING && unlink_locked(js, job)) {
      job->state = ZB_JOB_RUNNING;
      run_job(js, job);
    }
    return job->state;
  }
  lock(js);
  while (job->state == ZB_JOB_PENDING || job->state == ZB_JOB_RUNNING) {
    js->t->cond_wait(js->cond_done, js->mutex);
  }
  zb_job_state s = job->state;
  unlock(js);
  return s;
}

void zb_job_release(zb_jobs *js, zb_job *job) {
  lock(js);
  bool free_now = job->state == ZB_JOB_DONE || job->state == ZB_JOB_CANCELLED;
  job->released = true;
  unlock(js);
  if (free_now) {
    zb_free(js->a, job);
  }
}

size_t zb_jobs_run_pending(zb_jobs *js, size_t max) {
  if (js->t) {
    return 0;
  }
  size_t ran = 0;
  while (ran < max) {
    zb_job *j = pop_locked(js);
    if (!j) {
      break;
    }
    j->state = ZB_JOB_RUNNING;
    run_job(js, j);
    ran++;
  }
  return ran;
}

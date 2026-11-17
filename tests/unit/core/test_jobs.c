/* SPDX-License-Identifier: MIT */
#include "core/jobs.h"
#include "support/test_support.h"
#include "support/test_threads.h"
#include "unity.h"

ZB_TEST_LEAK_CHECKED

typedef struct work {
  int value;
  int *order;
  int *order_len;
} work;

static void add_one(void *arg) {
  work *w = arg;
  w->value++;
  if (w->order) {
    w->order[(*w->order_len)++] = w->value;
  }
}

static void test_inline_runs_in_priority_order(void) {
  zb_jobs *js = zb_jobs_create(zb_test_alloc(), NULL, 0);
  TEST_ASSERT_TRUE(zb_jobs_is_inline(js));
  TEST_ASSERT_EQUAL_UINT(0, zb_jobs_worker_count(js));
  int order[3], len = 0;
  work low = {100, order, &len}, high = {0, order, &len}, normal = {10, order, &len};
  zb_job *jl = zb_job_submit(js, add_one, &low, ZB_JOB_LOW);
  zb_job *jh = zb_job_submit(js, add_one, &high, ZB_JOB_HIGH);
  zb_job *jn = zb_job_submit(js, add_one, &normal, ZB_JOB_NORMAL);
  TEST_ASSERT_EQUAL_INT(ZB_JOB_PENDING, zb_job_get_state(js, jh));
  TEST_ASSERT_EQUAL_size_t(2, zb_jobs_run_pending(js, 2));
  TEST_ASSERT_EQUAL_INT(1, order[0]);
  TEST_ASSERT_EQUAL_INT(11, order[1]);
  TEST_ASSERT_EQUAL_INT(ZB_JOB_PENDING, zb_job_get_state(js, jl));
  TEST_ASSERT_EQUAL_size_t(1, zb_jobs_run_pending(js, 10));
  TEST_ASSERT_EQUAL_size_t(0, zb_jobs_run_pending(js, 10));
  TEST_ASSERT_EQUAL_INT(ZB_JOB_DONE, zb_job_wait(js, jl));
  zb_job_release(js, jl);
  zb_job_release(js, jh);
  zb_job_release(js, jn);
  zb_jobs_destroy(js);
}

static void test_inline_wait_runs_now_and_cancel(void) {
  zb_jobs *js = zb_jobs_create(zb_test_alloc(), NULL, 4);
  work a = {0, NULL, NULL}, b = {0, NULL, NULL}, c = {0, NULL, NULL};
  zb_job *ja = zb_job_submit(js, add_one, &a, ZB_JOB_NORMAL);
  zb_job *jb = zb_job_submit(js, add_one, &b, ZB_JOB_NORMAL);
  zb_job *jc = zb_job_submit(js, add_one, &c, ZB_JOB_NORMAL);
  TEST_ASSERT_EQUAL_INT(ZB_JOB_DONE, zb_job_wait(js, jb)); /* middle of the queue */
  TEST_ASSERT_EQUAL_INT(1, b.value);
  TEST_ASSERT_TRUE(zb_job_cancel(js, jc)); /* tail of the queue */
  TEST_ASSERT_FALSE(zb_job_cancel(js, jc));
  TEST_ASSERT_FALSE(zb_job_cancel(js, jb));
  TEST_ASSERT_EQUAL_INT(ZB_JOB_CANCELLED, zb_job_wait(js, jc));
  TEST_ASSERT_EQUAL_INT(0, c.value);
  zb_job_release(js, jb);
  zb_job_release(js, jc);
  zb_job_release(js, ja); /* released while pending: freed by destroy */
  zb_jobs_destroy(js);
  TEST_ASSERT_EQUAL_INT(0, a.value);
  zb_jobs_destroy(NULL);
}

static void test_inline_release_then_run(void) {
  zb_jobs *js = zb_jobs_create(zb_test_alloc(), NULL, 0);
  work a = {0, NULL, NULL};
  zb_job *ja = zb_job_submit(js, add_one, &a, ZB_JOB_HIGH);
  zb_job_release(js, ja); /* fire and forget */
  TEST_ASSERT_EQUAL_size_t(1, zb_jobs_run_pending(js, 1));
  TEST_ASSERT_EQUAL_INT(1, a.value);
  zb_jobs_destroy(js);
}

static void test_threaded_many_jobs(void) {
  if (!zb_test_thread_api()) {
    TEST_IGNORE_MESSAGE("no threads in this build");
  }
  zb_jobs *js = zb_jobs_create(zb_test_alloc(), zb_test_thread_api(), 3);
  TEST_ASSERT_NOT_NULL(js);
  TEST_ASSERT_FALSE(zb_jobs_is_inline(js));
  TEST_ASSERT_EQUAL_UINT(3, zb_jobs_worker_count(js));
  work w[64];
  zb_job *j[64];
  for (int i = 0; i < 64; ++i) {
    w[i].value = i;
    w[i].order = NULL;
    j[i] = zb_job_submit(js, add_one, &w[i], (zb_job_priority)(i % 3));
  }
  for (int i = 0; i < 64; ++i) {
    zb_job_state s = zb_job_wait(js, j[i]);
    TEST_ASSERT_TRUE(s == ZB_JOB_DONE);
    TEST_ASSERT_EQUAL_INT(i + 1, w[i].value);
    zb_job_release(js, j[i]);
  }
  TEST_ASSERT_EQUAL_size_t(0, zb_jobs_run_pending(js, 4));
  zb_jobs_destroy(js);
}

static void test_threaded_release_before_done(void) {
  if (!zb_test_thread_api()) {
    TEST_IGNORE_MESSAGE("no threads in this build");
  }
  zb_jobs *js = zb_jobs_create(zb_test_alloc(), zb_test_thread_api(), 2);
  work w[16];
  for (int i = 0; i < 16; ++i) {
    w[i].value = 0;
    w[i].order = NULL;
    zb_job *jj = zb_job_submit(js, add_one, &w[i], ZB_JOB_LOW);
    zb_job_release(js, jj); /* the worker frees it after running */
  }
  zb_jobs_destroy(js); /* joins workers; any not yet run are freed */
}

static void test_threaded_cancel_pending(void) {
  if (!zb_test_thread_api()) {
    TEST_IGNORE_MESSAGE("no threads in this build");
  }
  zb_jobs *js = zb_jobs_create(zb_test_alloc(), zb_test_thread_api(), 1);
  work w = {0, NULL, NULL};
  zb_job *jj = zb_job_submit(js, add_one, &w, ZB_JOB_NORMAL);
  bool cancelled = zb_job_cancel(js, jj); /* may already be running */
  zb_job_state s = zb_job_wait(js, jj);
  TEST_ASSERT_TRUE(cancelled ? s == ZB_JOB_CANCELLED : s == ZB_JOB_DONE);
  TEST_ASSERT_EQUAL_INT(cancelled ? 0 : 1, w.value);
  zb_job_release(js, jj);
  zb_jobs_destroy(js);
}

static void test_create_failures(void) {
  zb_failing_allocator f;
  TEST_ASSERT_NULL(zb_jobs_create(zb_failing_init(&f, zb_test_alloc(), 0), NULL, 0));
  if (zb_test_thread_api()) {
    TEST_ASSERT_NULL(zb_jobs_create(zb_failing_init(&f, zb_test_alloc(), 1), zb_test_thread_api(), 2));
  }
  zb_jobs *js = zb_jobs_create(zb_failing_init(&f, zb_test_alloc(), 1), NULL, 0);
  work w = {0, NULL, NULL};
  TEST_ASSERT_NULL(zb_job_submit(js, add_one, &w, ZB_JOB_NORMAL));
  zb_jobs_destroy(js);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_inline_runs_in_priority_order);
  RUN_TEST(test_inline_wait_runs_now_and_cancel);
  RUN_TEST(test_inline_release_then_run);
  RUN_TEST(test_threaded_many_jobs);
  RUN_TEST(test_threaded_release_before_done);
  RUN_TEST(test_threaded_cancel_pending);
  RUN_TEST(test_create_failures);
  return UNITY_END();
}

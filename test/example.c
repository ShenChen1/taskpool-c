#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include "taskpool.h"

#define WORKERS (5)
#define JOBS    (20)
static void *j_handles[JOBS] = {};

const char *status_str[] = {
    "TODO",
    "DOING",
    "DONE",
};

static int func(void *arg)
{
    printf("job[%p]: start\n", j_handles[(unsigned long)arg]);
    sleep((unsigned long)arg % 3);
    printf("job[%p]: end\n", j_handles[(unsigned long)arg]);
    return 0;
}

int main()
{
    int workers = WORKERS;
    int jobs = JOBS;
    unsigned long i;
    int ret = 0;
    taskpool_t *pObj = taskpool_init();
    assert(pObj);

    printf("Add %d workers\n", WORKERS);
    for (i = 0; i < workers; i++) {
        taskpool_worker_attr_t attr = {};
        attr.type = TASKPOOL_WORKER_TYPE_THREAD;
        ret = pObj->add_worker(pObj, &attr);
        assert(ret == 0);
    }

    printf("Add %d jobs\n", JOBS);
    for (i = 0; i < jobs; i++) {
        taskpool_job_attr_t attr = {};
        attr.type = TASKPOOL_WORKER_TYPE_THREAD;
        attr.func = func;
        attr.arg = (void *)i;
        ret = pObj->add_job(pObj, &attr, &j_handles[i]);
        assert(ret == 0);
    }

    printf("Get %d jobs status\n", JOBS);
    for (i = 0; i < jobs; i++) {
        taskpool_job_status_t status;
        ret = pObj->get_job_status(pObj, j_handles[i], &status);
        assert(ret == 0);
        printf("job[%p]: %s\n", j_handles[i], status_str[status.status]);
    }

    printf("Wait %d ~ %d jobs done\n", 0, JOBS / 2 - 1);
    for (i = 0; i < jobs / 2; i++) {
        ret = pObj->wait_job_done(pObj, j_handles[i]);
        assert(ret == 0);
    }

    printf("Get %d jobs status\n", JOBS);
    for (i = 0; i < jobs; i++) {
        taskpool_job_status_t status;
        ret = pObj->get_job_status(pObj, j_handles[i], &status);
        assert(ret == 0);
        printf("job[%p]: %s\n", j_handles[i], status_str[status.status]);
    }

    printf("Wait %d ~ %d jobs done\n", JOBS / 2, JOBS - 1);
    for (i = jobs / 2; i < jobs; i++) {
        ret = pObj->wait_job_done(pObj, j_handles[i]);
        assert(ret == 0);
    }

    printf("Wait all jobs done\n");
    ret = pObj->wait_all_jobs_done(pObj);
    assert(ret == 0);

    printf("Get %d jobs status\n", JOBS);
    for (i = 0; i < jobs; i++) {
        taskpool_job_status_t status;
        ret = pObj->get_job_status(pObj, j_handles[i], &status);
        assert(ret == 0);
        printf("job[%p]: %s\n", j_handles[i], status_str[status.status]);
    }

    printf("Add %d jobs\n", JOBS);
    for (i = 0; i < jobs; i++) {
        taskpool_job_attr_t attr = {};
        attr.type = TASKPOOL_WORKER_TYPE_THREAD;
        attr.func = func;
        attr.arg = (void *)i;
        ret = pObj->add_job(pObj, &attr, &j_handles[i]);
        assert(ret == 0);
    }

    printf("Delete all jobs\n");
    for (i = 0; i < jobs; i++) {
        ret = pObj->del_job(pObj, j_handles[i]);
        assert(ret == 0);
        sleep(i%3);
    }

    printf("Destroy taskpool\n");
    ret = pObj->deinit(pObj);
    assert(ret == 0);

    return 0;
}
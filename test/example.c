#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include "taskpool.h"

#define WORKERS (5)
#define JOBS    (200)
static void *j_handles[JOBS] = {};

const char *status_str[] = {
    "TODO",
    "DOING",
    "DONE",
};

static int func(void *arg)
{
    printf("job[%p]: start\n", j_handles[(unsigned long)arg]);
    sleep((unsigned long)arg % 5);
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

    for (i = 0; i < workers; i++)
    {
        taskpool_worker_attr_t attr = {};
        attr.type = TASKPOOL_WORKER_TYPE_THREAD;
        ret = pObj->add_worker(pObj, &attr);
        assert(ret == 0);
    }

    for (i = 0; i < jobs; i++)
    {
        taskpool_job_attr_t attr = {};
        attr.type = TASKPOOL_WORKER_TYPE_THREAD;
        attr.func = func;
        attr.arg = (void *)i;
        ret = pObj->add_job(pObj, &attr, &j_handles[i]);
        assert(ret == 0);
    }

    for (i = 0; i < jobs; i++)
    {
        taskpool_job_status_e status;
        ret = pObj->get_job_status(pObj, j_handles[i], &status);
        assert(ret == 0);
        printf("job[%p]: %s\n", j_handles[i], status_str[status]);
    }

    for (i = 0; i < jobs / 2; i++)
    {
        ret = pObj->wait_job_done(pObj, j_handles[i]);
        assert(ret == 0);
    }

    for (i = 0; i < jobs; i++)
    {
        taskpool_job_status_e status;
        ret = pObj->get_job_status(pObj, j_handles[i], &status);
        assert(ret == 0);
        printf("job[%p]: %s\n", j_handles[i], status_str[status]);
    }

    for (i = jobs / 2; i < jobs; i++)
    {
        ret = pObj->wait_job_done(pObj, j_handles[i]);
        assert(ret == 0);
    }

    for (i = 0; i < jobs; i++)
    {
        taskpool_job_status_e status;
        ret = pObj->get_job_status(pObj, j_handles[i], &status);
        assert(ret == 0);
        printf("job[%p]: %s\n", j_handles[i], status_str[status]);
    }

    for (i = 0; i < jobs; i++)
    {
        taskpool_job_attr_t attr = {};
        attr.type = TASKPOOL_WORKER_TYPE_THREAD;
        attr.func = func;
        attr.arg = (void *)i;
        ret = pObj->add_job(pObj, &attr, &j_handles[i]);
        assert(ret == 0);
    }

    for (i = 0; i < jobs; i++)
    {
        ret = pObj->del_job(pObj, j_handles[i]);
        assert(ret == 0);
    }

    ret = pObj->deinit(pObj);
    assert(ret == 0);

    return 0;
}
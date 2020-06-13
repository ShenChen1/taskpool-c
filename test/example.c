#include <stdio.h>
#include <assert.h>
#include "taskpool.h"

#define WORKERS (5)
#define JOBS    (20)

static int func(void *arg)
{
    printf("task:%d start\n", (int)arg);
    sleep((int)arg);
    printf("task:%d end\n", (int)arg);
    return 0;
}

int main()
{
    int workers = WORKERS;
    int jobs = JOBS;
    int i, ret = 0;
    void *w_handles[WORKERS] = {};
    void *j_handles[JOBS] = {};

    taskpool_t *pObj = taskpool_init();
    assert(pObj);

    for (i = 0; i < workers; i++)
    {
        taskpool_worker_attr_t attr = {};
        attr.type = TASKPOOL_WORKER_TYPE_THREAD;
        ret = pObj->add_worker(pObj, &attr, &w_handles[i]);
        assert(ret == 0);
    }

    for (i = 0; i < jobs; i++)
    {
        taskpool_job_attr_t attr = {};
        attr.func = func;
        attr.arg = (int)i;
        ret = pObj->add_job(pObj, &attr, &j_handles[i]);
        assert(ret == 0);
    }

    ret = pObj->wait_jobs(pObj, jobs, j_handles);
    assert(ret == 0);

    ret = pObj->deinit(pObj);
    assert(ret == 0);

    return 0;
}
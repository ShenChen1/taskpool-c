#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include "taskpool.h"

#define WORKERS (5)
#define JOBS    (20)

static int func(void *arg)
{
    printf("task:%lu start\n", (unsigned long)arg);
    sleep((unsigned long)arg%5);
    printf("task:%lu end\n", (unsigned long)arg);
    return 0;
}

int main()
{
    int workers = WORKERS;
    int jobs = JOBS;
    unsigned long i;
    int ret = 0;
    void *j_handles[JOBS] = {};

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
        ret = pObj->wait_job_done(pObj, j_handles[i]);
        assert(ret == 0);
    }

    ret = pObj->deinit(pObj);
    assert(ret == 0);

    return 0;
}
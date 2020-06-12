#include <stdio.h>
#include "taskpool.h"

typedef struct
{
    int key;
    taskpool_worker_attr_t attr;

    void *handle;

} taskpool_worker_t;

typedef struct
{
    int key;
    taskpool_job_attr_t attr;

    int isrunning;
    int retcode;

} taskpool_job_t;

typedef struct
{
    //struct list_head workers;
    //struct list_head jobs;
} taskpool_priv_t;

static taskpool_priv_t __taskpool_priv = {};
static taskpool_t s_taskpool_obj = {};

static int taskpool_deinit(taskpool_t *self)
{
    return 0;
}

static int taskpool_add_worker(taskpool_t *self, taskpool_worker_attr_t *attr, int *key)
{
    return 0;
}

static int taskpool_del_workers(taskpool_t *self, int num, int *keys)
{
    return 0;
}

static int taskpool_add_job(taskpool_t *self, taskpool_job_attr_t *attr, int *key)
{
    return 0;
}

static int taskpool_wait_jobs(taskpool_t *self, int num, int *keys)
{
    return 0;
}

taskpool_t *taskpool_init()
{
    taskpool_t *pObj = &s_taskpool_obj;

    return pObj;
}
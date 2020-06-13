#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "inner/list.h"
#include "taskpool.h"

#define TASKPOOL_MAGIC (0xdeadbeef)

typedef struct taskpool_worker_attr
{
    unsigned long magic;

    list_t worker;
    taskpool_worker_attr_t attr;

    taskpool_job_t *job;

} taskpool_worker_t;

typedef struct taskpool_job_attr
{
    unsigned long magic;

    list_t job;
    taskpool_job_attr_t attr;

    taskpool_worker_t *worker;

} taskpool_job_t;

typedef struct
{
    unsigned long magic;

    unsigned long num_of_workers;
    pthread_mutex_t mutex_for_workers;
    list_t workers_alive;
    list_t workers_working;

    unsigned long num_of_jobs;
    pthread_mutex_t mutex_for_jobs;
    list_t jobs_done;
    list_t jobs_done;
} taskpool_priv_t;

static inline taskpool_priv_t *__get_priv(taskpool_t *obj)
{
    assert(obj);
    assert(obj->priv);
    assert(((taskpool_priv_t *)(obj->priv))->magic == TASKPOOL_MAGIC);

    return obj->priv;
}

static void* __do_task(taskpool_t *obj)
{


}

static int taskpool_deinit(taskpool_t *self)
{
    return 0;
}

static int taskpool_add_worker(taskpool_t *self, const taskpool_worker_attr_t *attr, void **handle)
{
    const taskpool_worker_attr_t attr_default = {
        .type = TASKPOOL_WORKER_TYPE_THREAD,
    };

    taskpool_priv_t *pPriv = __get_priv(self);
    taskpool_worker_t *new = malloc(sizeof(taskpool_worker_t));
    if (new == NULL)
    {
        goto err;
    }

    memset(new, 0, sizeof(taskpool_worker_t));
    new->magic = TASKPOOL_MAGIC;
    attr = attr == NULL ? &attr_default : attr;
    memcpy(&new->attr, attr, sizeof(taskpool_worker_attr_t));
    new->job = NULL;
    list_add(&new->worker, &pPriv->workers);

    if (handle) {
        *handle = new;
    }

    return 0;

err:
    if (new)
    {
        free(new);
    }

    return -1;
}

static int taskpool_del_workers(taskpool_t *self, int num, void *handles)
{
    return 0;
}

static int taskpool_add_job(taskpool_t *self, const taskpool_job_attr_t *attr, void **handle)
{
    return 0;
}

static int taskpool_wait_jobs(taskpool_t *self, int num, void *handles)
{
    return 0;
}

taskpool_t *taskpool_init()
{
    taskpool_priv_t *pPriv = (taskpool_priv_t *)malloc(sizeof(taskpool_priv_t));
    if (pPriv == NULL)
    {
        goto err;
    }

    memset(pPriv, 0, sizeof(taskpool_priv_t));
    pPriv->magic = TASKPOOL_MAGIC;
    INIT_LIST_HEAD(&pPriv->workers);
    INIT_LIST_HEAD(&pPriv->jobs);

    taskpool_t *pObj = (taskpool_t *)malloc(sizeof(taskpool_t));
    if (pObj == NULL)
    {
        goto err;
    }

    memset(pObj, 0, sizeof(taskpool_t));
    pObj->priv = pPriv;
    pObj->deinit = taskpool_deinit;
    pObj->add_worker = taskpool_add_worker;
    pObj->del_workers = taskpool_del_workers;
    pObj->add_job = taskpool_add_job;
    pObj->wait_jobs = taskpool_wait_jobs;

    return pObj;

err:
    if (pObj)
    {
        free(pObj);
    }

    if (pPriv)
    {
        free(pPriv);
    }

    return NULL;
}
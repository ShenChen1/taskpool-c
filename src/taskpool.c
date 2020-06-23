#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include "mem.h"
#include "log.h"
#include "que.h"
#include "taskpool.h"

#define TASKPOOL_MAGIC (0xdeadbeef)

typedef struct
{
    size_t magic;
    pthread_mutex_t lock;

    handle_t workers;

    size_t num_of_jobs;
    handle_t jobs_todo;
    handle_t jobs_done;

    pthread_cond_t event;

} taskpool_priv_t;

typedef struct
{
    size_t magic;
    taskpool_job_attr_t attr;

    pthread_mutex_t lock;
    taskpool_job_status_e status;
    int retcode;

} taskpool_job_t;

typedef struct
{
    taskpool_worker_attr_t attr;
    taskpool_priv_t *info;
    taskpool_job_t *job;

    pthread_t task;
    int keep_alive;
} taskpool_worker_t;

static inline taskpool_priv_t *__get_priv(handle_t handle)
{
    taskpool_priv_t *priv;

    assert(handle);
    priv = ((taskpool_t *)handle)->priv;
    assert(priv);
    assert(priv->magic == TASKPOOL_MAGIC);

    return priv;
}

static inline taskpool_job_t *__get_job(handle_t handle)
{
    taskpool_job_t *job = handle;

    assert(job);
    assert(job->magic == TASKPOOL_MAGIC);

    return job;
}

static void *__do_task(void *arg)
{
    int status;
    taskpool_worker_t *worker = arg;

    pthread_detach(worker->task);
    status = que_put(worker->info->workers, worker);
    assert(!status);
    tracef("worker %p start\n", worker);

    worker->keep_alive = 1;
    while (worker->keep_alive)
    {
        status = que_get(worker->info->jobs_todo, (handle_t *)&worker->job, 1);
        if (status || worker->job == (handle_t)TASKPOOL_MAGIC)
        {
            worker->keep_alive = 0;
            goto end;
        }
#if 1
        int i;
        cpu_set_t cpuset;
        size_t cpumask = worker->job->attr.cpu_mask;
        CPU_ZERO(&cpuset);
        for (i = 0; i < get_nprocs_conf(); i++)
        {
            CPU_SET((cpumask >> i) & 0x1, &cpuset);
        }
        status = pthread_setaffinity_np(worker->task, sizeof(cpuset), &cpuset);
        assert(!status);
        struct sched_param schedprm;
        schedprm.sched_priority = worker->job->attr.sched_prio;
        status = pthread_setschedparam(worker->task, worker->job->attr.sched_policy, &schedprm);
        assert(!status);
#endif
        pthread_mutex_lock(&worker->job->lock);
        worker->job->status = TASKPOOL_JOB_STATUS_DOING;
        pthread_mutex_unlock(&worker->job->lock);

        tracef("worker %p is doing job %p ...\n", worker, worker->job);
        status = worker->job->attr.func(worker->job->attr.arg);
        tracef("worker %p finish job %p\n", worker, worker->job);

        pthread_mutex_lock(&worker->job->lock);
        worker->job->retcode = status;
        worker->job->status = TASKPOOL_JOB_STATUS_DONE;
        pthread_mutex_unlock(&worker->job->lock);

        que_put(worker->info->jobs_done, worker->job);
        worker->job = NULL;
    end:
        pthread_cond_broadcast(&worker->info->event);
    }

    tracef("worker %p end\n", worker);
    status = que_remove(worker->info->workers, worker);
    assert(!status);

    mem_free(worker);

    return NULL;
}

static int taskpool_deinit(taskpool_t *self)
{
    tracef("\n");

    int status;
    taskpool_priv_t *pPriv = __get_priv(self);
    taskpool_job_t *job = NULL;

    pthread_mutex_lock(&pPriv->lock);
    while (que_len(pPriv->jobs_todo))
    {
        pthread_cond_wait(&pPriv->event, &pPriv->lock);
    }
    pthread_mutex_unlock(&pPriv->lock);

    while (que_len(pPriv->workers))
    {
        status = self->del_worker(self);
        assert(!status);
    }

    while (que_len(pPriv->jobs_done))
    {
        status = que_peek(pPriv->jobs_done, (handle_t *)&job);
        assert(!status);
        status = self->del_job(self, job);
        assert(!status);
    }

    que_delete(pPriv->workers);
    que_delete(pPriv->jobs_todo);
    que_delete(pPriv->jobs_done);
    pthread_cond_destroy(&pPriv->event);
    pthread_mutex_destroy(&pPriv->lock);
    mem_free(pPriv);
    mem_free(self);

    return 0;
}

static int taskpool_add_worker(taskpool_t *self, const taskpool_worker_attr_t *attr)
{
    tracef("\n");

    const taskpool_worker_attr_t attr_default = {
        .type = TASKPOOL_WORKER_TYPE_THREAD,
    };

    int status;
    taskpool_priv_t *pPriv = __get_priv(self);
    taskpool_worker_t *new = mem_alloc(sizeof(taskpool_worker_t));
    if (new == NULL)
    {
        errorf("mem_alloc err\n");
        goto err;
    }

    memset(new, 0, sizeof(taskpool_worker_t));
    attr = attr == NULL ? &attr_default : attr;
    memcpy(&new->attr, attr, sizeof(taskpool_worker_attr_t));
    new->job = NULL;
    new->info = pPriv;
    new->keep_alive = 0;
    status = pthread_create(&new->task, NULL, __do_task, new);
    if (status)
    {
        errorf("pthread_create err\n");
        goto err;
    }

    return 0;

err:
    if (new)
    {
        mem_free(new);
    }

    return -1;
}

static int taskpool_del_worker(taskpool_t *self)
{
    tracef("\n");

    int status;
    taskpool_priv_t *pPriv = __get_priv(self);

    if (que_len(pPriv->workers) == 0)
    {
        errorf("no worker to delete\n");
        return -1;
    }

    pthread_mutex_lock(&pPriv->lock);
    status = que_put_to_head(pPriv->jobs_todo, (handle_t)TASKPOOL_MAGIC);
    assert(!status);
    pthread_cond_wait(&pPriv->event, &pPriv->lock);
    pthread_mutex_unlock(&pPriv->lock);

    return 0;
}

static int taskpool_add_job(taskpool_t *self, const taskpool_job_attr_t *attr, handle_t *handle)
{
    tracef("\n");

    const taskpool_job_attr_t attr_default = {
        .type = TASKPOOL_WORKER_TYPE_THREAD,
        .sched_policy = SCHED_RR,
        .sched_prio = 0,
        .cpu_mask = (size_t)(-1),
    };

    int status;
    taskpool_priv_t *pPriv = __get_priv(self);
    taskpool_job_t *new = mem_alloc(sizeof(taskpool_job_t));
    if (new == NULL)
    {
        errorf("mem_alloc err\n");
        goto err;
    }

    memset(new, 0, sizeof(taskpool_job_t));
    new->magic = TASKPOOL_MAGIC;
    status = pthread_mutex_init(&new->lock, NULL);
    assert(!status);
    attr = attr == NULL ? &attr_default : attr;
    memcpy(&new->attr, attr, sizeof(taskpool_job_attr_t));
    new->status = TASKPOOL_JOB_STATUS_TODO;
    new->retcode = 0;

    status = que_put(pPriv->jobs_todo, new);
    if (status)
    {
        errorf("que_put err\n");
        goto err;
    }

    pthread_mutex_lock(&pPriv->lock);
    pPriv->num_of_jobs++;
    pthread_mutex_unlock(&pPriv->lock);

    if (handle)
    {
        *handle = new;
        tracef("%p\n", *handle);
    }

    return 0;

err:
    if (new)
    {
        mem_free(new);
    }

    return -1;
}

static int taskpool_del_job(struct taskpool *self, handle_t handle)
{
    tracef("%p\n", handle);

    int status;
    taskpool_priv_t *pPriv = __get_priv(self);
    taskpool_job_t *job = __get_job(handle);
    handle_t que = NULL;

    if (job == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }

    pthread_mutex_lock(&job->lock);

    if (job->status == TASKPOOL_JOB_STATUS_TODO)
    {
        que = pPriv->jobs_todo;
    }

    if (job->status == TASKPOOL_JOB_STATUS_DOING)
    {
        while (job->status != TASKPOOL_JOB_STATUS_DONE)
        {
            pthread_cond_wait(&pPriv->event, &job->lock);
        }
    }

    if (job->status == TASKPOOL_JOB_STATUS_DONE)
    {
        que = pPriv->jobs_done;
    }

    status = que_remove(que, job);
    assert(!status);

    pthread_mutex_unlock(&job->lock);

    pthread_mutex_lock(&pPriv->lock);
    pPriv->num_of_jobs--;
    pthread_mutex_unlock(&pPriv->lock);

    pthread_mutex_destroy(&job->lock);
    mem_free(job);

    return 0;
}

static int taskpool_get_job_status(taskpool_t *self, handle_t handle, taskpool_job_status_e *status)
{
    tracef("%p\n", handle);

    taskpool_job_t *job = __get_job(handle);
    if (status == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }

    pthread_mutex_lock(&job->lock);
    *status = job->status;
    pthread_mutex_unlock(&job->lock);

    return 0;
}

static int taskpool_wait_job_done(taskpool_t *self, handle_t handle)
{
    tracef("%p\n", handle);

    taskpool_priv_t *pPriv = __get_priv(self);
    taskpool_job_t *job = __get_job(handle);

    pthread_mutex_lock(&job->lock);
    while (job->status != TASKPOOL_JOB_STATUS_DONE)
    {
        pthread_cond_wait(&pPriv->event, &job->lock);
    }
    pthread_mutex_unlock(&job->lock);

    tracef("%p done\n", handle);
    return 0;
}

static int taskpool_wait_all_jobs_done(struct taskpool *self)
{
    tracef("\n");

    taskpool_priv_t *pPriv = __get_priv(self);

    pthread_mutex_lock(&pPriv->lock);
    while (que_len(pPriv->jobs_todo))
    {
        pthread_cond_wait(&pPriv->event, &pPriv->lock);
    }
    pthread_mutex_unlock(&pPriv->lock);

    return 0;
}

taskpool_t *taskpool_init()
{
    tracef("\n");

    int status;
    taskpool_priv_t *pPriv = (taskpool_priv_t *)mem_alloc(sizeof(taskpool_priv_t));
    if (pPriv == NULL)
    {
        errorf("mem_alloc err\n");
        goto err;
    }

    memset(pPriv, 0, sizeof(taskpool_priv_t));
    pPriv->magic = TASKPOOL_MAGIC;
    status = pthread_mutex_init(&pPriv->lock, NULL);
    if (status)
    {
        errorf("pthread_mutex_init err\n");
        goto err;
    }
    status = pthread_cond_init(&pPriv->event, NULL);
    if (status)
    {
        errorf("pthread_cond_init err\n");
        goto err;
    }
    status |= que_create(&pPriv->workers);
    status |= que_create(&pPriv->jobs_todo);
    status |= que_create(&pPriv->jobs_done);
    if (status)
    {
        errorf("que_create err\n");
        goto err;
    }

    taskpool_t *pObj = (taskpool_t *)mem_alloc(sizeof(taskpool_t));
    if (pObj == NULL)
    {
        errorf("mem_alloc err\n");
        goto err;
    }

    memset(pObj, 0, sizeof(taskpool_t));
    pObj->priv = pPriv;
    pObj->deinit = taskpool_deinit;
    pObj->add_worker = taskpool_add_worker;
    pObj->del_worker = taskpool_del_worker;
    pObj->add_job = taskpool_add_job;
    pObj->del_job = taskpool_del_job;
    pObj->get_job_status = taskpool_get_job_status;
    pObj->wait_job_done = taskpool_wait_job_done;
    pObj->wait_all_jobs_done = taskpool_wait_all_jobs_done;

    return pObj;

err:
    if (pObj)
    {
        mem_free(pObj);
    }

    if (pPriv)
    {
        que_delete(pPriv->workers);
        que_delete(pPriv->jobs_todo);
        que_delete(pPriv->jobs_done);
        pthread_cond_destroy(&pPriv->event);
        pthread_mutex_destroy(&pPriv->lock);
        mem_free(pPriv);
    }

    return NULL;
}
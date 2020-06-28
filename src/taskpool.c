#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "mem.h"
#include "log.h"
#include "que.h"
#include "task.h"
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

    handle_t task;
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
            status = que_remove(worker->info->workers, worker);
            assert(!status);
            goto end;
        }

        pthread_mutex_lock(&worker->job->lock);
        worker->job->status = TASKPOOL_JOB_STATUS_DOING;
        pthread_mutex_unlock(&worker->job->lock);

        status |= task_set_affinity(worker->task, worker->job->attr.cpu_mask);
        status |= task_set_schedpolicy(worker->task, worker->job->attr.sched_policy);
        status |= task_set_schedpriority(worker->task, worker->job->attr.sched_prio);
        assert(!status);

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
    task_delete(worker->task);
    mem_free(worker);
    return NULL;
}

static int taskpool_deinit(taskpool_t *self)
{
    tracef("\n");

    int status;
    taskpool_priv_t *priv = __get_priv(self);
    taskpool_job_t *job = NULL;

    pthread_mutex_lock(&priv->lock);
    while (que_len(priv->jobs_todo))
    {
        pthread_cond_wait(&priv->event, &priv->lock);
    }
    pthread_mutex_unlock(&priv->lock);

    while (que_len(priv->workers))
    {
        status = self->del_worker(self);
        assert(!status);
    }

    while (que_len(priv->jobs_done))
    {
        status = que_peek(priv->jobs_done, (handle_t *)&job);
        assert(!status);
        status = self->del_job(self, job);
        assert(!status);
    }

    que_delete(priv->workers);
    que_delete(priv->jobs_todo);
    que_delete(priv->jobs_done);
    pthread_cond_destroy(&priv->event);
    pthread_mutex_destroy(&priv->lock);
    mem_free(priv);
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
    taskpool_priv_t *priv = __get_priv(self);
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
    new->info = priv;
    new->keep_alive = 0;

    task_attr_t task_attr = {};
    task_attr.type = attr->type;
    task_attr.routine = __do_task;
    task_attr.arg = new;
    status = task_create(&task_attr, &new->task);
    if (status)
    {
        errorf("task_create err\n");
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
    taskpool_priv_t *priv = __get_priv(self);

    if (que_len(priv->workers) == 0)
    {
        errorf("no worker to delete\n");
        return -1;
    }

    pthread_mutex_lock(&priv->lock);
    status = que_put_to_head(priv->jobs_todo, (handle_t)TASKPOOL_MAGIC);
    assert(!status);
    pthread_cond_wait(&priv->event, &priv->lock);
    pthread_mutex_unlock(&priv->lock);

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
    taskpool_priv_t *priv = __get_priv(self);
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

    status = que_put(priv->jobs_todo, new);
    if (status)
    {
        errorf("que_put err\n");
        goto err;
    }

    pthread_mutex_lock(&priv->lock);
    priv->num_of_jobs++;
    pthread_mutex_unlock(&priv->lock);

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

    taskpool_priv_t *priv = __get_priv(self);
    taskpool_job_t *job = __get_job(handle);

    if (job == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }

    pthread_mutex_lock(&job->lock);
    que_remove(priv->jobs_todo, job);
    if (job->status == TASKPOOL_JOB_STATUS_DOING)
    {
        while (job->status != TASKPOOL_JOB_STATUS_DONE)
        {
            pthread_cond_wait(&priv->event, &job->lock);
        }
    }
    que_remove(priv->jobs_done, job);
    pthread_mutex_unlock(&job->lock);

    pthread_mutex_lock(&priv->lock);
    priv->num_of_jobs--;
    pthread_mutex_unlock(&priv->lock);

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

    taskpool_priv_t *priv = __get_priv(self);
    taskpool_job_t *job = __get_job(handle);

    pthread_mutex_lock(&job->lock);
    while (job->status != TASKPOOL_JOB_STATUS_DONE)
    {
        pthread_cond_wait(&priv->event, &job->lock);
    }
    pthread_mutex_unlock(&job->lock);

    tracef("%p done\n", handle);
    return 0;
}

static int taskpool_wait_all_jobs_done(struct taskpool *self)
{
    tracef("\n");

    taskpool_priv_t *priv = __get_priv(self);

    pthread_mutex_lock(&priv->lock);
    while (que_len(priv->jobs_todo))
    {
        pthread_cond_wait(&priv->event, &priv->lock);
    }
    pthread_mutex_unlock(&priv->lock);

    return 0;
}

taskpool_t *taskpool_init()
{
    tracef("\n");

    int status;
    taskpool_priv_t *priv = (taskpool_priv_t *)mem_alloc(sizeof(taskpool_priv_t));
    if (priv == NULL)
    {
        errorf("mem_alloc err\n");
        goto err;
    }

    memset(priv, 0, sizeof(taskpool_priv_t));
    priv->magic = TASKPOOL_MAGIC;
    status = pthread_mutex_init(&priv->lock, NULL);
    if (status)
    {
        errorf("pthread_mutex_init err\n");
        goto err;
    }
    status = pthread_cond_init(&priv->event, NULL);
    if (status)
    {
        errorf("pthread_cond_init err\n");
        goto err;
    }
    status |= que_create(&priv->workers);
    status |= que_create(&priv->jobs_todo);
    status |= que_create(&priv->jobs_done);
    if (status)
    {
        errorf("que_create err\n");
        goto err;
    }

    taskpool_t *obj = (taskpool_t *)mem_alloc(sizeof(taskpool_t));
    if (obj == NULL)
    {
        errorf("mem_alloc err\n");
        goto err;
    }

    memset(obj, 0, sizeof(taskpool_t));
    obj->priv = priv;
    obj->deinit = taskpool_deinit;
    obj->add_worker = taskpool_add_worker;
    obj->del_worker = taskpool_del_worker;
    obj->add_job = taskpool_add_job;
    obj->del_job = taskpool_del_job;
    obj->get_job_status = taskpool_get_job_status;
    obj->wait_job_done = taskpool_wait_job_done;
    obj->wait_all_jobs_done = taskpool_wait_all_jobs_done;

    return obj;

err:
    if (obj)
    {
        mem_free(obj);
    }

    if (priv)
    {
        que_delete(priv->workers);
        que_delete(priv->jobs_todo);
        que_delete(priv->jobs_done);
        pthread_cond_destroy(&priv->event);
        pthread_mutex_destroy(&priv->lock);
        mem_free(priv);
    }

    return NULL;
}
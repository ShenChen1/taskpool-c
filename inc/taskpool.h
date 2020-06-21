#ifndef __TASKPOOL_H__
#define __TASKPOOL_H__

typedef enum
{
    TASKPOOL_WORKER_TYPE_THREAD = 0,
    TASKPOOL_WORKER_TYPE_COROUTINE, //TODO

    TASKPOOL_WORKER_TYPE_NONE,
} taskpool_worker_type_e;

typedef enum
{
    TASKPOOL_JOB_STATUS_TODO = 0,
    TASKPOOL_JOB_STATUS_DOING,
    TASKPOOL_JOB_STATUS_DONE,

    TASKPOOL_JOB_STATUS_NONE,
} taskpool_job_status_e;

typedef struct
{
    taskpool_worker_type_e type;

} taskpool_worker_attr_t;

typedef struct
{
    taskpool_worker_type_e type;

    unsigned long cpu_mask;
    unsigned long sched_policy;
    unsigned long sched_prio;

    int (*func)(void *);
    void *arg;

} taskpool_job_attr_t;

typedef struct taskpool
{
    /* Private date */
    void *priv;

    int (*deinit)(struct taskpool *self);

    int (*add_worker)(struct taskpool *self, const taskpool_worker_attr_t *attr);
    int (*del_worker)(struct taskpool *self);

    int (*add_job)(struct taskpool *self, const taskpool_job_attr_t *attr, void **handle);
    int (*del_job)(struct taskpool *self, void *handle);
    int (*wait_job_done)(struct taskpool *self, void *handle);
    int (*get_job_status)(struct taskpool *self, void *handle, taskpool_job_status_e *status);

} taskpool_t;

taskpool_t *taskpool_init();

#endif //__TASKPOOL_H__

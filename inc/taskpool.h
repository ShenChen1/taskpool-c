#ifndef __TASKPOOL_H__
#define __TASKPOOL_H__

typedef enum
{
    TASKPOOL_WORKER_TYPE_THREAD = 0,
    TASKPOOL_WORKER_TYPE_COROUTINE, //TODO

    TASKPOOL_WORKER_TYPE_NONE,
} taskpool_worker_type;

typedef struct
{
    taskpool_worker_type type;

} taskpool_worker_attr_t;

typedef struct
{
    taskpool_worker_type type;

    unsigned long cpu_mask;
    unsigned long sched_policy;

    int (*func)(void *);
    void *arg;

} taskpool_job_attr_t;

typedef struct taskpool
{
    /* Private date */
    void *priv;

    int (*deinit)(struct taskpool *self);

    int (*add_worker)(struct taskpool *self, taskpool_worker_attr_t *attr, int *key);
    int (*del_workers)(struct taskpool *self, int num, int *keys);

    int (*add_job)(struct taskpool *self, taskpool_job_attr_t *attr, int *key);
    int (*wait_jobs)(struct taskpool *self, int num, int *keys);

} taskpool_t;

taskpool_t *taskpool_init();

#endif //__TASKPOOL_H__

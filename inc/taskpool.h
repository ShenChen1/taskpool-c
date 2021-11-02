#ifndef __TASKPOOL_H__
#define __TASKPOOL_H__

#include <stddef.h>

typedef void *job_t;

typedef enum {
    TASKPOOL_WORKER_TYPE_THREAD = 0,
    TASKPOOL_WORKER_TYPE_COROUTINE, //TODO
    TASKPOOL_WORKER_TYPE_NONE,
} taskpool_worker_type_e;

typedef enum {
    TASKPOOL_JOB_STATUS_TODO = 0,
    TASKPOOL_JOB_STATUS_DOING,
    TASKPOOL_JOB_STATUS_DONE,
    TASKPOOL_JOB_STATUS_NONE,
} taskpool_job_status_e;

typedef struct {
    taskpool_worker_type_e type;
} taskpool_worker_attr_t;

typedef struct {
    taskpool_worker_type_e type;

    size_t sys_cpu_mask;        /* this job can run on which cpus */
    int sys_sched_policy;       /* the scheduling policy for this job */
    int sys_sched_priority;     /* the scheduling priority for this job */

    int (*func)(void *);        /* pointer to the function to do */
    void *arg;                  /* pointer to an argument */
} taskpool_job_attr_t;

typedef struct {
    taskpool_job_status_e status;
    int errno;
} taskpool_job_status_t;

typedef struct taskpool {
    /* Private date */
    void *priv;

    /**
     * @brief  Destory taskpool instance
     *
     * @param  self     taskpool instance
     * @return 0 on successs, -1 otherwise.
     */
    int (*deinit)(struct taskpool *self);

    /**
     * @brief Add one kind of worker
     *
     * @param  self     taskpool instance
     * @param  attr     the attribute of worker wanted to be created
     * @return 0 on successs, -1 otherwise.
     */
    int (*add_worker)(struct taskpool *self, const taskpool_worker_attr_t *attr);
    /**
     * @brief Delete one kind of worker
     *
     * @param  self     taskpool instance
     * @param  attr     the attribute of worker wanted to be deleted
     * @return 0 on successs, -1 otherwise.
     */
    int (*del_worker)(struct taskpool *self, const taskpool_worker_attr_t *attr);

    /**
     * @brief Add job
     *
     * @param  self     taskpool instance
     * @param  attr     the attribute of job wanted to be created
     * @param  job      return the job's handle if user requests
     * @return 0 on successs, -1 otherwise.
     */
    int (*add_job)(struct taskpool *self, const taskpool_job_attr_t *attr, job_t *job);
    /**
     * @brief Delete job
     *
     * @param  self     taskpool instance
     * @param  job      job's handle
     * @return 0 on successs, -1 otherwise.
     */
    int (*del_job)(struct taskpool *self, job_t job);

    /**
     * @brief Wait for the specified job done
     *
     * @param  self     taskpool instance
     * @param  job      job's handle
     * @return 0 on successs, -1 otherwise.
     */
    int (*wait_job_done)(struct taskpool *self, job_t job);
    /**
     * @brief Wait for all jobs done
     *
     * @param  self     taskpool instance
     * @return 0 on successs, -1 otherwise.
     */
    int (*wait_all_jobs_done)(struct taskpool *self);

    /**
     * @brief Get the specified job status
     *
     * @param  self     taskpool instance
     * @param  job      job's handle
     * @param  status   return the job's current status
     * @return 0 on successs, -1 otherwise.
     */
    int (*get_job_status)(struct taskpool *self, job_t job, taskpool_job_status_t *status);

} taskpool_t;

/**
 * @brief  Create taskpool instance
 *
 * @param
 * @return taskpool     created instance on success,
 *                      NULL on error
 */
taskpool_t *taskpool_init();

#endif //__TASKPOOL_H__

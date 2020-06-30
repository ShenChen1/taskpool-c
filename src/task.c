#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include "mem.h"
#include "log.h"
#include "task.h"

typedef struct {
    int (*create)(task_attr_t *attr, void **handle);
    int (*delete)(void *handle);
    int (*set_affinity)(void *handle, size_t cpumask);
    int (*set_schedpolicy)(void *handle, int policy);
    int (*set_schedpriority)(void *handle, int priority);

} task_func_t;

typedef struct {
    task_attr_t attr;
    task_func_t *func;
    void *handle;
} task_priv_t;

static int __thread_create(task_attr_t *attr, void **handle)
{
    int status;
    pthread_t *new = (pthread_t *)mem_alloc(sizeof(pthread_t));
    if (new == NULL) {
        errorf("mem_alloc err\n");
        return -1;
    }

    status = pthread_create(new, NULL, attr->routine, attr->arg);
    if (status) {
        errorf("pthread_create err\n");
        return -1;
    }

    status = pthread_detach(*new);
    assert(!status);

    *handle = new;
    return 0;
}

static int __thread_delete(void *handle)
{
    mem_free(handle);
    return 0;
}

static int __thread_set_affinity(void *handle, size_t cpumask)
{
    int i;
    cpu_set_t cpuset;
    pthread_t *thread = handle;

    CPU_ZERO(&cpuset);
    for (i = 0; i < get_nprocs_conf(); i++) {
        CPU_SET((cpumask >> i) & 0x1, &cpuset);
    }

    return pthread_setaffinity_np(*thread, sizeof(cpuset), &cpuset);
}

static int __thread_set_schedpolicy(void *handle, int policy)
{
    int tmp;
    pthread_t *thread = handle;
    struct sched_param schedprm;

    pthread_getschedparam(*thread, &tmp, &schedprm);
    return pthread_setschedparam(*thread, policy, &schedprm);
}

static int __thread_set_schedpriority(void *handle, int priority)
{
    pthread_t *thread = handle;

    return pthread_setschedprio(*thread, priority);
}

static task_func_t s_task_func[] = {
    [TASK_TYPE_THREAD] = {
        .create = __thread_create,
        .delete = __thread_delete,
        .set_affinity = __thread_set_affinity,
        .set_schedpolicy = __thread_set_schedpolicy,
        .set_schedpriority = __thread_set_schedpriority,
    },
};

int task_create(task_attr_t *attr, void **handle)
{
    int status;
    task_priv_t *priv = NULL;

    if (attr == NULL || handle == NULL) {
        errorf("paramter err\n");
        return -1;
    }

    priv = (task_priv_t *)mem_alloc(sizeof(task_priv_t));
    if (priv == NULL) {
        errorf("mem_alloc err\n");
        return -1;
    }

    memset(priv, sizeof(task_priv_t), 0);
    memcpy(&priv->attr, attr, sizeof(task_attr_t));
    priv->func = &s_task_func[attr->type];

    assert(priv->func->create);
    status = priv->func->create(&priv->attr, &priv->handle);
    if (status) {
        errorf("priv->func->create err\n");
        goto err;
    }

    *handle = priv;
    return 0;

err:
    if (priv) {
        mem_free(priv);
    }

    return -1;
}

int task_delete(void *handle)
{
    int status;
    task_priv_t *priv = handle;

    if (handle == NULL) {
        errorf("paramter err\n");
        return -1;
    }

    assert(priv->func->delete);
    status = priv->func->delete(priv->handle);
    if (status) {
        errorf("priv->func->delete err\n");
        return -1;
    }

    mem_free(priv);
    return 0;
}

int task_set_affinity(void *handle, size_t cpumask)
{
    task_priv_t *priv = handle;

    if (handle == NULL) {
        errorf("paramter err\n");
        return -1;
    }

    assert(priv->func->set_affinity);
    return priv->func->set_affinity(priv->handle, cpumask);
}

int task_set_schedpolicy(void *handle, int policy)
{
    task_priv_t *priv = handle;

    if (handle == NULL) {
        errorf("paramter err\n");
        return -1;
    }

    assert(priv->func->set_schedpolicy);
    return priv->func->set_schedpolicy(priv->handle, policy);
}

int task_set_schedpriority(void *handle, int priority)
{
    task_priv_t *priv = handle;

    if (handle == NULL) {
        errorf("paramter err\n");
        return -1;
    }

    assert(priv->func->set_schedpriority);
    return priv->func->set_schedpriority(priv->handle, priority);
}
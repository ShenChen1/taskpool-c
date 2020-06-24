#ifndef __TASK_H__
#define __TASK_H__

typedef enum
{
    TASK_TYPE_THREAD = 0,
    TASK_TYPE_COROUTINE, //TODO

    TASK_TYPE_NONE,
} task_type_e;

typedef struct
{
    task_type_e type;

    void *(*routine)(void *);
    void *arg;
} task_attr_t;

int task_create(task_attr_t *attr, void **handle);
int task_delete(void *handle);
int task_set_affinity(void *handle, size_t cpumask);
int task_set_schedpolicy(void *handle, int policy);
int task_set_schedpriority(void *handle, int priority);

#endif //__TASK_H__

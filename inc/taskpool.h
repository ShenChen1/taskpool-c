#ifndef __TASKPOOL_H__
#define __TASKPOOL_H__

typedef struct __tskpool {
    /* Private date */
    void *priv;

    int (* deinit)(struct __tskpool *self);

} taskpool;

taskpool* taskpoll_init();

#endif //__TASKPOOL_H__

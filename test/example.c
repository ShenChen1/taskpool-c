#include <stdio.h>
#include "taskpool.h"

int main()
{
    taskpool *pObj = taskpoll_init();
    printf("taskpool:%p\n", pObj);

    return 0;
}
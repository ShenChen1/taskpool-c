#include <stdio.h>
#include <stdlib.h>
#include "log.h"

void *mem_alloc(size_t size)
{
    void *ptr;

    ptr = malloc(size);
    tracef("[%s](%d) = %p\n", __func__, size, ptr);
    return ptr;
}

void mem_free(void *ptr)
{
    if (ptr == NULL)
        return;

    tracef("[%s](%p)\n", __func__, ptr);
    free(ptr);
}

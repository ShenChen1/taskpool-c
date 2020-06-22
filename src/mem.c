#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "log.h"
#include "mem.h"

#define ENTRY(ptr, type, member) \
    ((type *)((char *)(ptr) - (size_t)(&((type *)0)->member)))

// Rounding operations (efficient when n is a power of 2)
// Round down to the nearest multiple of n
#define ROUNDDOWN(a, n)               \
    ({                                \
        size_t __a = (size_t)(a);     \
        (typeof(a))(__a - __a % (n)); \
    })
// Round up to the nearest multiple of n
#define ROUNDUP(a, n)                                       \
    ({                                                      \
        size_t __n = (size_t)(n);                           \
        (typeof(a))(ROUNDDOWN((size_t)(a) + __n - 1, __n)); \
    })

typedef struct __obj
{
    union {
        struct __obj *next;
        size_t size;
    } header;
    char data[0];
} mem_obj_t;

#define MEM_ALIGN (8)
#define MEM_MAX_BYTES (128)

typedef struct
{
    pthread_mutex_t lock;

    size_t num;
    mem_obj_t *array[MEM_MAX_BYTES / MEM_ALIGN];

    size_t align;
    size_t max_bytes;
} mem_info_t;

static mem_info_t s_mem_info = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .num = MEM_MAX_BYTES / MEM_ALIGN,
    .align = MEM_ALIGN,
    .max_bytes = MEM_MAX_BYTES,
};

static size_t __freelist_index(size_t size, size_t align)
{
    return ((size + align - 1) / (size_t)align - 1);
}

void *mem_alloc(size_t size)
{
    size_t index;
    void *ret = NULL;
    mem_info_t *info = &s_mem_info;
    mem_obj_t *obj = NULL;

    if (size == 0)
    {
        return NULL;
    }

    if (info->max_bytes < size)
    {
        obj = (mem_obj_t *)malloc(sizeof(mem_obj_t) + ROUNDUP(size, info->align));
        if (obj == NULL)
        {
            errorf("malloc err\n");
            return NULL;
        }
        obj->header.size = size;
        return obj->data;
    }

    pthread_mutex_lock(&info->lock);
    size = ROUNDUP(size, info->align);
    index = __freelist_index(size, info->align);
    obj = info->array[index];
    if (obj == NULL)
    {
        obj = (mem_obj_t *)malloc(sizeof(mem_obj_t) + size);
        if (obj == NULL)
        {
            errorf("malloc err\n");
            ret = NULL;
            goto end;
        }
        obj->header.next = NULL;
        info->array[index] = obj;
    }

    info->array[index] = obj->header.next;
    obj->header.size = size;
    ret = obj->data;
end:
    pthread_mutex_unlock(&info->lock);

    return ret;
}

void mem_free(void *ptr)
{
    size_t index;
    mem_info_t *info = &s_mem_info;
    mem_obj_t *obj = NULL;

    if (ptr == NULL)
    {
        return;
    }

    obj = ENTRY(ptr, mem_obj_t, data);
    if (info->max_bytes < obj->header.size)
    {
        free(obj);
        return;
    }

    pthread_mutex_lock(&info->lock);
    index = __freelist_index(obj->header.size, info->align);
    obj->header.next = info->array[index];
    info->array[index] = obj;
    pthread_mutex_unlock(&info->lock);
}

static void __attribute__((destructor)) __mem_deinit()
{
    size_t i;
    mem_info_t *info = &s_mem_info;
    mem_obj_t *obj, *tmp = NULL;

    pthread_mutex_lock(&info->lock);
    for (i = 0; i < info->num; i++)
    {
        obj = info->array[i];
        while (obj)
        {
            tmp = obj;
            obj = obj->header.next;
            free(tmp);
        }
    }
    pthread_mutex_unlock(&info->lock);
}
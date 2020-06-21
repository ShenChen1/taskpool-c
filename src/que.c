#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "list.h"
#include "mem.h"
#include "que.h"
#include "log.h"

typedef struct
{
    list_t head;
    unsigned long count;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} que_priv_t;

typedef struct
{
    list_t list;
    void *element;
} que_node_t;

int que_create(void **handle)
{
    int status;
    que_priv_t *pPriv = NULL;

    if (handle == NULL)
    {
        errorf("paramter err\n");
        goto err;
    }

    pPriv = (que_priv_t *)mem_alloc(sizeof(que_priv_t));
    if (pPriv == NULL)
    {
        errorf("mem_alloc err\n");
        goto err;
    }

    memset(pPriv, 0, sizeof(que_priv_t));
    status = pthread_mutex_init(&pPriv->lock, NULL);
    if (status)
    {
        errorf("pthread_mutex_init err\n");
        goto err;
    }
    status = pthread_cond_init(&pPriv->cond, NULL);
    if (status)
    {
        errorf("pthread_cond_init err\n");
        goto err;
    }
    INIT_LIST_HEAD(&pPriv->head);

    *handle = pPriv;
    return 0;

err:
    if (pPriv)
    {
        pthread_mutex_destroy(&pPriv->lock);
        pthread_cond_destroy(&pPriv->cond);
        mem_free(pPriv);
    }

    return -1;
}

int que_delete(void *handle)
{
    que_priv_t *pPriv = (que_priv_t *)handle;
    que_node_t *pNode = NULL;
    list_t *p, *tmp;

    if (pPriv == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }

    pthread_mutex_lock(&pPriv->lock);
    list_for_each_safe(p, tmp, &pPriv->head)
    {
        pNode = list_entry(p, que_node_t, list);
        list_del(&pNode->list);
        pPriv->count--;
        tracef("pNode:%p\n", pNode);
        mem_free(pNode);
    }
    pthread_mutex_unlock(&pPriv->lock);

    pthread_mutex_destroy(&pPriv->lock);
    pthread_cond_destroy(&pPriv->cond);

    mem_free(pPriv);

    return 0;
}

int que_put(void *handle, void *element)
{
    que_priv_t *pPriv = (que_priv_t *)handle;
    que_node_t *pNode = NULL;

    if (pPriv == NULL || element == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }

    pNode = (que_node_t *)mem_alloc(sizeof(que_node_t));
    if (pNode == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }
    pNode->element = element;

    pthread_mutex_lock(&pPriv->lock);
    list_add_tail(&pNode->list, &pPriv->head);
    pPriv->count++;
    pthread_mutex_unlock(&pPriv->lock);

    pthread_cond_signal(&pPriv->cond);

    return 0;
}

int que_put_to_head(void *handle, void *element)
{
    que_priv_t *pPriv = (que_priv_t *)handle;
    que_node_t *pNode = NULL;

    if (pPriv == NULL || element == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }

    pNode = (que_node_t *)mem_alloc(sizeof(que_node_t));
    if (pNode == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }
    pNode->element = element;

    pthread_mutex_lock(&pPriv->lock);
    list_add(&pNode->list, &pPriv->head);
    pPriv->count++;
    pthread_mutex_unlock(&pPriv->lock);

    pthread_cond_signal(&pPriv->cond);

    return 0;
}

int que_get(void *handle, void **element, int isblock)
{
    int status = 0;
    que_priv_t *pPriv = (que_priv_t *)handle;
    que_node_t *pNode = NULL;

    if (pPriv == NULL || element == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }

    pthread_mutex_lock(&pPriv->lock);
    while (1)
    {
        if (!list_empty(&pPriv->head))
        {
            pNode = list_entry(pPriv->head.next, que_node_t, list);
            list_del(&pNode->list);
            pPriv->count--;
            *element = pNode->element;
            mem_free(pNode);
            break;
        }
        else
        {
            if (!isblock)
            {
                errorf("no element\n");
                status = -1;
                break;
            }

            status = pthread_cond_wait(&pPriv->cond, &pPriv->lock);
            if (status)
            {
                errorf("pthread_cond_wait err\n");
                status = -1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&pPriv->lock);

    return status;
}

int que_peek(void *handle, void **element)
{
    int status = -1;
    que_priv_t *pPriv = (que_priv_t *)handle;
    que_node_t *pNode = NULL;

    if (pPriv == NULL || element == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }

    pthread_mutex_lock(&pPriv->lock);
    if (!list_empty(&pPriv->head))
    {
        pNode = list_entry(pPriv->head.next, que_node_t, list);
        *element = pNode->element;
        status = 0;
    }
    pthread_mutex_unlock(&pPriv->lock);

    return status;
}

int que_remove(void *handle, void *element)
{
    int status = -1;
    que_priv_t *pPriv = (que_priv_t *)handle;
    que_node_t *pNode = NULL;
    list_t *p, *tmp;

    if (pPriv == NULL || element == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }

    pthread_mutex_lock(&pPriv->lock);
    list_for_each_safe(p, tmp, &pPriv->head)
    {
        pNode = list_entry(p, que_node_t, list);
        if (pNode->element == element)
        {
            list_del(&pNode->list);
            pPriv->count--;
            mem_free(pNode);
            status = 0;
            break;
        }
    }
    pthread_mutex_unlock(&pPriv->lock);

    return status;
}

int que_len(void *handle)
{
    int ret;
    que_priv_t *pPriv = (que_priv_t *)handle;
    if (pPriv == NULL)
    {
        errorf("paramter err\n");
        return -1;
    }

    pthread_mutex_lock(&pPriv->lock);
    ret = pPriv->count;
    pthread_mutex_unlock(&pPriv->lock);

    return ret;
}
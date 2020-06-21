#ifndef _QUE_H_
#define _QUE_H_

int que_create(void **handle);
int que_delete(void *handle);
int que_put(void *handle, void *element);
int que_put_to_head(void *handle, void *element);
int que_get(void *handle, void **element, int isblock);
int que_peek(void *handle, void **element);
int que_remove(void *handle, void *element);
int que_len(void *handle);

#endif //_QUE_H_
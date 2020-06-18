#ifndef _QUE_H_
#define _QUE_H_

int que_create(void **handle);
int que_delete(void *handle);
int que_put(void *handle, void *element);
int que_get(void *handle, void **element, int isblock);

#endif //_QUE_H_
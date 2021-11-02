#ifndef _MEM_H_
#define _MEM_H_

#include <stddef.h>

void *mem_alloc(size_t size);
void mem_free(void *ptr);

#endif //_MEM_H_
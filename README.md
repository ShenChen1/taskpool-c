# C taskpool

## Run an example

Build the example via CMAKE:

    cmake -H. -Bbuild;make -sC build


Then run the executable like this:

    ./build/example


## Basic usage

1. Include the header in your source file: `#include "taskpool.h"`
2. Create a taskpool instance: `taskpool_t *pObj = taskpool_init();`
3. Add/delete a worker to taskpool: `pObj->add_worker();`/`pObj->del_worker();`
4. Add/delete a job to taskpool: `pObj->add_job();`/`pObj->del_job();`
5. Wait a job done: `pObj->wait_job_done();`
6. Destory the taskpool instance: `pObj->deinit();`
#ifndef __THREAD_H__
#define __THREAD_H__
#include "types.h"
#include "scheduler.h"
#include "mem.h"
typedef struct thread_args {
    uint32_t c;
    uint32_t t;
    void* args;
} thread_args_t;

// thread management
int thread_init();
tcb_t* thread_get_new_tcb();
int thread_create (void *func, thread_args_t *args);

#endif

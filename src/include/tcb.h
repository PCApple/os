#ifndef __TCB_H__
#define __TCB_H__
#include <stdint.h>
typedef enum thread_state {
  UNUSED = 0, // set when the thread is not in use
  INITIALIZED = 1, // set when the thread is initialized, need to create fake stack
  RUNNING = 2, // set when the thread is running
  RUNNABLE = 3, // set when thread is paused but mid progress
  ZOMBIE = 4 // set when the thread is terminated but not cleaned yet
  } thread_state_t;

typedef struct task_control_block {
  //ids 
  uint32_t tid; // thread id
  uint32_t pid; // parent id (not used rn)
  // state info
  thread_state_t state; 
  //rms
  uint32_t c; // budget
  uint32_t t; // period
  uint32_t budget; // REMAINING budget
  uint32_t period_cnt; // ms into current period
  uint32_t is_idle; // if set, tick will not create new tasks
  // stack
  void *frame;
  void *esp;
  void* eip;
  void* args;
} tcb_t;
#endif
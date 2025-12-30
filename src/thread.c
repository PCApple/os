#include "include/thread.h"

// Creates a new thread that will execute the function `func` with the given arguments `args.args`.
// @param func the function to execute in the new thread
// @param args the arguments to pass to the function, including the period and budget, and the actual arguments to the function
// @return the thread ID of the newly created thread, or -1 on failure
int thread_create(void *func, thread_args_t *args) {
  if (tcb_count >= MAX_TASKS) return -1;
  uint32_t c = args->c;
  uint32_t t = args->t;
  void* frame = mem_kalloc(0);
  if (frame == NULL) return -1; /* allocation failed */
  void* new_stack = (void*)((char*)frame + PAGE_SIZE); /* allocate stack for new thread */
  tcb_t* new_tcb = thread_get_new_tcb();
  if (!new_tcb){
    mem_kfree(frame);
    return -1;
  }
  __asm__ volatile ("pushfl");
  __asm__ volatile ("cli");
  new_tcb->tid = scheduler.next_id;
  scheduler.next_id = scheduler.next_id + 1;
  if (scheduler.current) {
  new_tcb->pid =  scheduler.current->pid;
  }
  else {
    new_tcb->pid = 0; // main pid
  }
  __asm__ volatile ("popfl");
  new_tcb->state = INITIALIZED;
  new_tcb->eip = (void*) func;
  new_tcb->esp = (void*) new_stack;
  new_tcb->frame = frame;
  new_tcb->is_idle = 0;
  new_tcb->c = c;
  new_tcb->budget = c;
  new_tcb->period_cnt = -1;
  new_tcb->t = t;
  new_tcb->args = args->args;
  int revalue = scheduler_create_task(new_tcb, t);
  if (revalue < 0) {
    mem_kfree(frame);
    return -1;
  }
  return new_tcb->tid;
}
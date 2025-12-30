#include "include/scheduler.h"
scheduler_t scheduler;
tcb_t * tcb_array;
uint32_t tcb_count;

// switches to the new stack
// @param the address of the old stack pointer
// @param the new stack pointer
extern void context_switch(void** old_sp, void* new_sp);
extern void first_context_switch(void* new_sp);



int thread_init() {
  tcb_array = (tcb_t*) mem_kalloc(0); // get a page to store TCBs
  memset(tcb_array, 0, PAGE_SIZE);
  if (!tcb_array) return -1;
  tcb_count = 0;
  return 0;
}
tcb_t* thread_get_new_tcb() {
  if (tcb_count >= MAX_TASKS) return NULL;
  return &tcb_array[tcb_count++];
}
int scheduler_get_idx_by_tid(uint32_t tid) {
    for (int i = 0; i < scheduler.size; i++) {
        heap_node_t *node = &scheduler.run_queue[i];
        tcb_t *tcb = (tcb_t *)node->obj;
        if (tcb->tid == tid) {
            return i;
        }
    }
    return -1;
}
//finds the next runnable task
// @return the index of the next runnable task in the prio queue, -1 if fail
int scheduler_find_next() {
    if (scheduler.size == 0) {
        return -1;
    }
    tcb_t* min_tcb = scheduler.run_queue[0].obj;
    if (min_tcb == NULL) {
        return -1;
    }
    if ((min_tcb->state == RUNNABLE || min_tcb->state == INITIALIZED || min_tcb->state == RUNNING) && min_tcb->budget > 0) {
        return 0;
    }
    return -2;
}

// finds the next runnable task and context switches to it

// will test if a task is schedulable
// @param c the computation time of the task
// @param t the period of the task
// @return 0 if schedulable, -1 if not
int scheduler_test_utilization(uint32_t c, uint32_t t) {
    double utilization = (double)c / (double)t;

    for (int i = 0; i < scheduler.size; i++) {
        heap_node_t *node = &scheduler.run_queue[i];
        tcb_t *tcb = (tcb_t *)node->obj;
        if (!tcb->is_idle) utilization += (double)tcb->c / (double)tcb->t;
    }
    int idx = scheduler.size;
    if ( idx > MAX_TASKS-1) {
        idx = MAX_TASKS-1;
    }
    double rms_lookup = rms_utilization_table[idx];
    if (utilization > rms_lookup) {
        return -1; // not schedulable
    }
    return 0; // schedulable
}
// cleans up zombie threads
void scheduler_thread_clean(){
    for (int i = 0; i < scheduler.zombie_queue_size; i++) {
        tcb_t* tcb = scheduler.zombie_queue[i];
        memset(tcb->frame, 0, PAGE_SIZE);
        mem_kfree(tcb->frame);
        memset(tcb, 0, sizeof(tcb_t));
    }
    scheduler.zombie_queue_size = 0;
}
// cleans up and zombie threads AND THEN sets the current thread to zombie(it wont be cleared until the next context switch), then preempts
void scheduler_thread_exit(){
    tcb_t* curr = scheduler.current;
    __asm__ volatile ("cli");
    scheduler_thread_clean();
    curr->state = ZOMBIE;
    int idx = scheduler_get_idx_by_tid(curr->tid);
    heap_remove(&scheduler.run_queue[0], &scheduler.run_queue[idx]);
    scheduler.size--;
    scheduler.zombie_queue[scheduler.zombie_queue_size] = curr;
    scheduler.zombie_queue_size++;
    scheduler_preempt();
}
void thread_stub(void* eip, void* args){
    __asm__ volatile ("sti"); // sets the int flag if not already
    ((void (*)(void*))eip)(args);
    scheduler_thread_exit();
}
// initializes the stack for a task
int scheduler_init_stack(tcb_t* task) {
    uint32_t* esp = task->esp;
    *--esp = (uint32_t)scheduler_thread_exit;
    *--esp = (uint32_t)task->args;
    *--esp = (uint32_t)task->eip; // will call into this
    *--esp = 0;                    // fake INITIAL ebp
    uint32_t saved_ebp = (uint32_t) esp; 
    *--esp = (uint32_t)thread_stub; // this is where it will start
    *--esp = saved_ebp; // fake ebp for thread_stub
    *--esp = 0; // fake ebx
    *--esp = 0; // fake esi
    *--esp = 0; // fake edi
    task->esp = esp;
    return 0;
}
int scheduler_preempt(){
    tcb_t *current;
    int next_idx;
    current = scheduler.current;
    next_idx = scheduler_find_next();
    if (next_idx == -1) {
        return -1;
    }
    if (next_idx == -2) { // no other runnable task
        return 0;
    }
    if (scheduler.current == scheduler.run_queue[next_idx].obj) // if its the same task, no need to switch
    {
        return 0;
    }
    tcb_t *next = scheduler.run_queue[next_idx].obj;
    if (next == NULL) {
        return -1;
    }
    if (next->state == INITIALIZED){
        scheduler_init_stack(next);
    }
    scheduler_context_switch(current, next);
    return 0;
}
void scheduler_context_switch(tcb_t *prev, tcb_t *next) {
    char prev_buf[10];
    char next_buf[10];
    char period_buf[10];
    if (prev != NULL && prev->state == RUNNING) prev->state = RUNNABLE;
    next->state = RUNNING;
    scheduler.current = next;
    itoa(prev_buf, 10, prev->tid);
    itoa(next_buf, 10, next->tid);
    itoa(period_buf, 10, prev->period_cnt);
    //terminal_writestring("CS: ");
    // terminal_writestring(prev_buf);
    // terminal_writestring(" to ");
    // terminal_writestring(next_buf);
    // terminal_writestring(" S: ");
    // terminal_writestring(period_buf);
    // terminal_writestring(" |||");



    //get_esp(&prev->esp);
    if (scheduler.has_switched == 0) {
        scheduler.has_switched = 1;
        first_context_switch(next->esp);
    }
    else {
        context_switch(&prev->esp, next->esp);
    }
}

// adds task to heap
// @param task tcb of task, needs to be initialized
// @param prio priority of the task, set it to the period
// @return 0 if success, -1 if fail because either too many tasks or not schedulable
int scheduler_create_task(tcb_t* task, uint32_t prio) {
    __asm__ volatile ("pushfl");
    __asm__ volatile ("cli");
    if (scheduler.size >= MAX_TASKS) {
        return -1; // too many tasks
    }
    int is_schedulable = scheduler_test_utilization(task->c, task->t);
    if (is_schedulable < 0) {
        return -1; // not schedulable
    }
    heap_insert(&scheduler.run_queue[0], task, prio);
    scheduler.size++;
    __asm__ volatile ("popfl");
    return 0;
}
uint32_t scheduler_check_wait_queue(){
    uint32_t write_idx = 0;
    for (int read_idx = 0; read_idx < scheduler.wait_queue_size; read_idx++) {
        tcb_t* tcb = scheduler.wait_queue[read_idx];
        tcb->period_cnt++;
        if (tcb->period_cnt % tcb->t == 0) {
            tcb->period_cnt++;
            tcb->budget = tcb->c;
            heap_insert(&scheduler.run_queue[0], tcb, tcb->t);
            scheduler.size++;
        }
        else {
            scheduler.wait_queue[write_idx++] = tcb;
        }
    }
    for (int i = write_idx; i < MAX_TASKS; i++) {
        scheduler.wait_queue[i] = NULL;
    }
    scheduler.wait_queue_size = write_idx;
    return write_idx;
}

// called when irq0 is triggered. Will handle timer ticks
void scheduler_handle_tick(void) {
    // update periods and budgets
    uint8_t marked_for_removal[MAX_TASKS];
    uint8_t removal_cnt = 0;
    for (int i = 0; i < scheduler.size; i++) {
        tcb_t* tcb = (tcb_t*)scheduler.run_queue[i].obj;
        
        if (tcb != NULL) {
            tcb->period_cnt++;
            if (tcb->period_cnt % tcb->t == 0) {
                //  tcb->period_cnt = 0;
                tcb->budget = tcb->c;
                continue;
            }
            // remove one budget
            if (tcb->budget > 0 && tcb->state == RUNNING && !tcb->is_idle) {
                tcb->budget--;
                if (tcb->budget == 0) {
                  marked_for_removal[removal_cnt++] = i;
                }
            }
        }
    }
    scheduler_check_wait_queue();
    for (int j = 0; j < removal_cnt; j++) {
        tcb_t* tcb = (tcb_t*)scheduler.run_queue[marked_for_removal[j]].obj;
        heap_remove(&scheduler.run_queue[0], &scheduler.run_queue[marked_for_removal[j]]);
        scheduler.size--;
        scheduler.wait_queue[scheduler.wait_queue_size++] = tcb;
        
    }
    pic_send_eoi(0);
    //__asm__ volatile ("sti");
    scheduler_preempt();
}
// default behavior if no tasks are running
void scheduler_busy_run(void* args){
    while (1) {
        //__asm__ volatile ("cli");
        //scheduler_preempt();
        //__asm__ volatile ("sti");
    }
}
int scheduler_init(){
    scheduler.size = 0;
    scheduler.has_switched = 0;
    scheduler.current = NULL;
    scheduler.next_id = 1;
    int i = 0;
    for (i = 0; i < MAX_TASKS; i++) {
        scheduler.run_queue[i].obj = 0;
        scheduler.run_queue[i].prio = 0;
        scheduler.wait_queue[i] = 0;
        scheduler.zombie_queue[i] = 0;
    }
    scheduler.zombie_queue_size = 0;
    scheduler.wait_queue_size = 0;
    // create a tcb for the idle task
    tcb_t *idle = (tcb_t*) thread_get_new_tcb();
    if (!idle) return -1;
    idle->is_idle = 1;
    void* frame = mem_kalloc(0);
    void* new_stack = frame + PAGE_SIZE; // allocate stack for new thread
    idle->tid = 0;
    idle->pid = 0; // main pid
    idle->c = 1;
    idle->t = IDLE_PRIO;
    idle->budget = IDLE_PRIO;
    idle->period_cnt = -1;
    idle->eip = (void*) scheduler_busy_run;
    idle->esp = (void*) new_stack;
    idle->frame = frame;
    scheduler_create_task(idle, idle->t);
    scheduler_init_stack(idle);
    idle->state = RUNNING; // me when I lie
    scheduler.current = idle;
    return 0;
}
void scheduler_run() {
    __asm__ volatile ("cli");
    scheduler_preempt();
    __asm__ volatile ("sti");
}



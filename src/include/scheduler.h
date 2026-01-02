#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__
#include "types.h"
#include "tcb.h"
#include "heap.h"
#include "io.h"
#include "mem.h"
#include "pic.h"
#include "print.h"
#include "utils.h"


#define IDLE_PRIO 0xFFFFFFFE
#define ZOMBIE_PRIO 0xFFFFFFFF
#define MAX_TASKS 64 // why not
#define USER_CS 0x1B


typedef struct cxt {
  uint32_t edi; // source index
  uint32_t esi; // destination index
  uint32_t ebp; // base pointer
  uint32_t eip; // instruction pointer
  // scratch registers
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
} cxt_t;

typedef struct scheduler {
    uint8_t has_switched;
    uint32_t next_id;
    heap_node_t run_queue[MAX_TASKS]; // void* = tcb_t*
    tcb_t* wait_queue[MAX_TASKS];
    tcb_t* zombie_queue[MAX_TASKS];
    uint32_t wait_queue_size;
    uint32_t zombie_queue_size;
    tcb_t *current;
    uint32_t size;
} scheduler_t;

static const double rms_utilization_table[] = {
    1.000000,  // n = 1
    0.828427,  // n = 2
    0.779763,  // n = 3
    0.756828,  // n = 4
    0.743492,  // n = 5
    0.734772,  // n = 6
    0.728627,  // n = 7
    0.724062,  // n = 8
    0.720538,  // n = 9
    0.717735,  // n = 10
    0.715452,  // n = 11
    0.713557,  // n = 12
    0.711959,  // n = 13
    0.710593,  // n = 14
    0.709412,  // n = 15
    0.708381,  // n = 16
    0.707472,  // n = 17
    0.706666,  // n = 18
    0.705946,  // n = 19
    0.705298,  // n = 20
    0.704713,  // n = 21
    0.704182,  // n = 22
    0.703698,  // n = 23
    0.703254,  // n = 24
    0.702846,  // n = 25
    0.702469,  // n = 26
    0.702121,  // n = 27
    0.701798,  // n = 28
    0.701497,  // n = 29
    0.701217,  // n = 30
    0.700955,  // n = 31
    0.700709,  // n = 32
    0.700478,  // n = 33
    0.700261,  // n = 34
    0.700056,  // n = 35
    0.699863,  // n = 36
    0.699681,  // n = 37
    0.699508,  // n = 38
    0.699343,  // n = 39
    0.699188,  // n = 40
    0.699040,  // n = 41
    0.698898,  // n = 42
    0.698764,  // n = 43
    0.698636,  // n = 44
    0.698513,  // n = 45
    0.698396,  // n = 46
    0.698284,  // n = 47
    0.698176,  // n = 48
    0.698073,  // n = 49
    0.697974,  // n = 50
    0.697879,  // n = 51
    0.697788,  // n = 52
    0.697700,  // n = 53
    0.697615,  // n = 54
    0.697533,  // n = 55
    0.697455,  // n = 56
    0.697379,  // n = 57
    0.697306,  // n = 58
    0.697235,  // n = 59
    0.697166,  // n = 60
    0.697100,  // n = 61
    0.697036,  // n = 62
    0.696974,  // n = 63
    0.693147  // n -> ∞ (ln(2))
};

extern scheduler_t scheduler;
extern tcb_t * tcb_array;
extern uint32_t tcb_count;

// thread management
int thread_init();
tcb_t* thread_get_new_tcb();


// basic stuff
int scheduler_init();
int scheduler_create_task(tcb_t* task, uint32 prio);
int scheduler_preempt();
void scheduler_run();

// priority utils
int scheduler_set_prio(uint32 tid, uint32 prio);
int scheduler_get_prio(uint32 tid);


// RMS utils
int scheduler_is_schedulable(uint32 c, uint32 t);

// context switch
void scheduler_context_switch(tcb_t *prev, tcb_t *next);
int scheduler_find_next();
int scheduler_get_idx_by_tid(uint32_t tid);

void scheduler_busy_run(void* args);

void scheduler_handle_tick(void);
#endif

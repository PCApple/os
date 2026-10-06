#ifndef MUTEX_H
#define MUTEX_H

#include <stdatomic.h>

/* Nonrecursive spinning mutex. Only the owner may unlock it.
 * Use in thread context for short critical sections; do not acquire from
 * interrupt handlers or wait with scheduling/interrupts disabled.
 */
typedef struct mutex {
    atomic_uint locked;
} mutex_t;

/* Initialize before sharing; never reinitialize or copy a live mutex. */
void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
int mutex_try_lock(mutex_t *mutex); /* 1 if acquired, 0 if busy. */
void mutex_unlock(mutex_t *mutex);

#endif

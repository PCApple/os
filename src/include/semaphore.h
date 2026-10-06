#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>
#include "mutex.h"

/* Count is protected by mutex; access it only through these operations.
 * A permit has no owner: a different thread may post it.
 * Wait currently spins, releasing the mutex between attempts. Use only in
 * thread context with scheduling/interrupts enabled when waiting may occur.
 */
typedef struct semaphore {
    uint32_t count;
    mutex_t mutex;
} semaphore_t;

/* Initialize before sharing; never reinitialize or copy a live semaphore. */
void semaphore_init(semaphore_t *semaphore, uint32_t count);
void semaphore_wait(semaphore_t *semaphore);
/* Try to take one permit: 1 on success, 0 if none are available. */
int semaphore_try_wait(semaphore_t *semaphore);
/* Add one permit: 0 on success, -1 on UINT32_MAX overflow, leaving it unchanged. */
int semaphore_post(semaphore_t *semaphore);

#endif

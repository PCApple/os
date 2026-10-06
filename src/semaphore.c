#include "include/semaphore.h"

void semaphore_init(semaphore_t *semaphore, uint32_t count) {
    mutex_init(&semaphore->mutex);
    semaphore->count = count;
}

int semaphore_try_wait(semaphore_t *semaphore) {
    int acquired = 0;
    mutex_lock(&semaphore->mutex);
    if (semaphore->count > 0) {
        --semaphore->count;
        acquired = 1;
    }
    mutex_unlock(&semaphore->mutex);
    return acquired;
}

void semaphore_wait(semaphore_t *semaphore) {
    while (!semaphore_try_wait(semaphore)) {
        __asm__ volatile("pause");
    }
}

int semaphore_post(semaphore_t *semaphore) {
    int result = 0;
    mutex_lock(&semaphore->mutex);
    if (semaphore->count == UINT32_MAX) {
        result = -1;
    } else {
        ++semaphore->count;
    }
    mutex_unlock(&semaphore->mutex);
    return result;
}

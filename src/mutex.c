#include "include/mutex.h"

void mutex_init(mutex_t *mutex) {
    atomic_init(&mutex->locked, 0);
}

int mutex_try_lock(mutex_t *mutex) {
    unsigned expected = 0;
    return atomic_compare_exchange_strong_explicit(&mutex->locked, &expected, 1,
                                                   memory_order_acquire,
                                                   memory_order_relaxed);
}

void mutex_lock(mutex_t *mutex) {
    while (!mutex_try_lock(mutex)) {
        __asm__ volatile("pause");
    }
}

void mutex_unlock(mutex_t *mutex) {
    atomic_store_explicit(&mutex->locked, 0, memory_order_release);
}

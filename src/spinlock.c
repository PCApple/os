#include "include/spinlock.h"

void spinlock_init(spinlock_t* lock) {
    atomic_init(&lock->lock, 0);
}

void spinlock_acquire(spinlock_t* lock) {
    while (atomic_exchange_explicit(&lock->lock, 1, memory_order_acquire) == 1) {
        // busy-wait
    }
}
void spinlock_release(spinlock_t* lock) {
    atomic_store_explicit(&lock->lock, 0, memory_order_release);
}
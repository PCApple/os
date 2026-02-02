#ifndef SPINLOCK_H
#define SPINLOCK_H
#include <stdatomic.h>
#include <stdint.h>

typedef struct spinlock {
    atomic_uint lock;
} spinlock_t;

void spinlock_init(spinlock_t* spinlock);
void spinlock_acquire(spinlock_t* spinlock);
void spinlock_release(spinlock_t* spinlock);
#endif
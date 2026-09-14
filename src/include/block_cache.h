#ifndef BLOCK_CACHE_H
#define BLOCK_CACHE_H
#include "spinlock.h"
#include <stdalign.h>
#include <stdint.h>
#include "mem.h"
#include "print.h"
#include "ide.h"
#include "spinlock.h"

#define BLOCK_SIZE 512
#define CACHE_SIZE 16 // number of blocks to cache
// flag bits for cache_block_t
#define DIRTY_FLAG 0x1
#define PRESENT_FLAG 0x2
#define LOCKED_FLAG 0x4
typedef struct cache_block {
    uint32_t block_num;
    uint8_t data[BLOCK_SIZE];
    uint8_t flag_bits; // bit 0: dirty, bit 1: present, bit 2: locked(not used right now)
} cache_block_t;

static uint32_t next_evict_index = 0; // index of next block to evict, simple round robin eviction policy
static cache_block_t *cache; // cache of blocks, defined in init

void cache_init(void);
cache_block_t *cache_get(uint32_t block_num);
void cache_mark_dirty(cache_block_t *block);
void cache_release(cache_block_t *block);
void cache_flush_all(void);
#endif
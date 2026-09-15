#ifndef BLOCK_CACHE_H
#define BLOCK_CACHE_H
#include <stdint.h>

#define CACHE_DRIVE 1 /* Separate data disk; IDE protects drive 0. */
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

void cache_init(void);
cache_block_t *cache_get(uint32_t block_num);
void cache_mark_dirty(cache_block_t *block);
void cache_release(cache_block_t *block);
void cache_flush_all(void);
#endif
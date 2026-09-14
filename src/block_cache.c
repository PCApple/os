#include "include/block_cache.h"

cache_block_t *cache_get(uint32_t block_num){
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].block_num == block_num && (cache[i].flag_bits & PRESENT_FLAG)) {
            return &cache[i];
        }
    }
    // block not in cache, find empty slot or evict and add
    for (int i = 0; i < CACHE_SIZE; i++) {
        if ((cache[i].flag_bits & PRESENT_FLAG) == 0) { // empty slot
            cache[i].block_num = block_num;
            cache[i].flag_bits = PRESENT_FLAG; // mark as present, not dirty, not locked
            // read block from disk into cache
            ide_read_sectors(0, block_num, 1, DS, (uint32_t)cache[i].data);
            return &cache[i];
        }
    }
    // no empty slot, evict next block (round robin policy)
    if (cache[next_evict_index].flag_bits & DIRTY_FLAG) {
        ide_write_sectors(0, cache[next_evict_index].block_num, 1, DS, (uint32_t)cache[next_evict_index].data);
    }
    cache[next_evict_index].block_num = block_num;
    cache[next_evict_index].flag_bits = PRESENT_FLAG; // mark as present, not dirty, not locked
    // read block from disk into cache
    ide_read_sectors(0, block_num, 1, DS, (uint32_t)cache[next_evict_index].data);
    next_evict_index = (next_evict_index + 1) % CACHE_SIZE;
    return &cache[next_evict_index];   
}
void cache_mark_dirty(cache_block_t *block){
    if (block->flag_bits & PRESENT_FLAG) {
        block->flag_bits |= DIRTY_FLAG;
    }
}
void cache_release(cache_block_t *block){
    if (block->flag_bits & DIRTY_FLAG) {
        ide_write_sectors(0, block->block_num, 1, DS, (uint32_t)block->data);
    }
    block->flag_bits = 0;
    block->block_num = 0xFFFFFFFF; // mark as invalid
}
void cache_flush_all(void){
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].flag_bits & DIRTY_FLAG) {
            ide_write_sectors(0, cache[i].block_num, 1, DS, (uint32_t)*cache[i].data);
            cache[i].flag_bits &= ~DIRTY_FLAG; // clear dirty flag
        }
    }
    
}
void cache_init(void){
    cache = (cache_block_t*)mem_kalloc(sizeof(cache_block_t) * CACHE_SIZE);
    memset(cache, 0, sizeof(cache_block_t) * CACHE_SIZE);
    next_evict_index = 0;
    for (int i = 0; i < CACHE_SIZE; i++) {
        cache[i].block_num = 0xFFFFFFFF; // invalid block number
        cache[i].flag_bits = 0; // clear all flags
    }
}
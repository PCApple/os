#include "include/tests.h"
void test_simple_block_cache() {
    printk("Testing simple block cache...\n");
    // testing first block in cache and writing A
    cache_block_t *block1 = cache_get(0);
    if (block1 == NULL) {
        printk("Failed to get block 0 from cache\n");
        return;
    }
    block1->data[0] = 'A';
    cache_mark_dirty(block1);
    cache_release(block1);
    // testing that the block is still in cache and contains 'A'
    cache_block_t *block2 = cache_get(0);
    if (block2 == NULL) {
        printk("Failed to get block 0 from cache\n");
        return;
    }
    if (block2->data[0] != 'A') {
        printk("Block 0 data mismatch: expected 'A', got '%c'\n", block2->data[0]);
        return;
    }
    cache_release(block2);
    // testing eviction by filling the cache
    for (uint32_t i = 1; i < CACHE_SIZE + 1; i++) {
        cache_block_t *block = cache_get(i);
        if (block == NULL) {
            printk("Failed to get block %d from cache\n", i);
            return;
        }
        block->data[0] = 'B' + (i % 26);
        cache_mark_dirty(block);
        cache_release(block);
    }
    // read and verify that the first block is still in cache and contains abcs
    cache_block_t *block3 = cache_get(0);
    if (block3 == NULL) {
        printk("Failed to get block 0 from cache after eviction\n");
        return;
    }
    for (uint32_t i = 0; i < CACHE_SIZE; i++) {
        if (block3->data[i] != 'A' + (i % 26)) {
            printk("Block 0 data mismatch after eviction: expected %c, got '%c'\n", 'A' + (i % 26), block3->data[i]);
            return;
        }
    }
    printk("Simple block cache test completed successfully\n");

        
}
void test_multi_block_cache_flush() {
    printk("Testing multi block cache flush...\n");
    // testing nultiple blocks flush
    for (uint32_t i = 0; i < CACHE_SIZE; i++){
        cache_block_t *block = cache_get(i);
        if (block == NULL) {
            printk("Failed to get block %d from cache\n", i);
            return;
        }
        for (uint32_t j = 0; j < BLOCK_SIZE; j++) {
            block->data[j] = 'A' + (i % 26);
        }
        cache_mark_dirty(block);

    }
    cache_flush_all();
    // verify that all blocks are still in cache and contain the correct data
    for (uint32_t i = 0; i < CACHE_SIZE; i++){
        cache_block_t *block = cache_get(i);
        if (block == NULL) {
            printk("Failed to get block %d from cache after flush\n", i);
            return;
        }
        for (uint32_t j = 0; j < BLOCK_SIZE; j++) {
            if (block->data[j] != 'A' + (i % 26)) {
                printk("Block %d data mismatch after flush: expected %c, got '%c'\n", i, 'A' + (i % 26), block->data[j]);
                return;
            }
        }
    }
    printk("Multi block cache flush test completed successfully\n");
}
void test_block_cache_eviction(){
    printk("Testing block cache eviction...\n");
    // testing eviction by filling the cache
    for (uint32_t i = 0; i < CACHE_SIZE; i++) {
        cache_block_t *block = cache_get(i);
        if (block == NULL) {
            printk("Failed to get block %d from cache\n", i);
            return;
        }
        for (uint32_t j = 0; j < BLOCK_SIZE; j++) {
            block->data[j] = 'A' + (i % 26);
        }
        cache_mark_dirty(block);
    }
    // read and verify that the first block is still in cache and contains abcs
    cache_block_t *block3 = cache_get(CACHE_SIZE);
    if (block3 == NULL) {
        printk("Failed to get block 0 from cache after eviction\n");
        return;
    }
    
    printk("Block cache eviction test completed successfully\n");
}
void test_block_cache() {
    test_simple_block_cache();
    test_multi_block_cache_flush();
}
#ifndef LRU_CACHE_H
#define LRU_CACHE_H
#include "spinlock.h"
#include <stdalign.h>
#include <stdint.h>
#include "mem.h"
#include "print.h"
#include "ide.h"
#include "spinlock.h"


#define DS 0x10
#define CEILING(x,y) (((x) + (y) - 1) / (y))
#define MAX_REFS 1 // setting this to one 
#define BLOCK_SIZE 256
#define PARTITION_BITS_HASH 4

typedef struct cache_entry {
    uint8_t present;
    uint8_t dirty;
    uint32_t block_num;
    uint32_t refcnt;
    struct cache_entry *prev;
    struct cache_entry *next;
    struct cache_entry *hash_next;  // for hash collisions
    struct cache_entry *hash_prev;  // for hash collisions
    uint8_t data[BLOCK_SIZE];
} cache_entry_t;
typedef struct cache {
    spinlock_t lock;
    cache_entry_t* items; // hold entry contents, double linked list for LRU
    cache_entry_t** hashmap; // hash map that will point to entries in items
    cache_entry_t* head;
    cache_entry_t* tail;
    uint32_t size;
    uint32_t capacity;
    uint32_t drive_num;
} cache_t;



cache_t* lru_cache_init(int capacity, uint32_t drive_num);
void* lru_cache_get_and_lock(cache_t* cache, uint32_t block_num);
int lru_cache_update_and_release(cache_t* cache, uint32_t block_num, int is_modified);
int lru_cache_flush(cache_t* cache);
#endif
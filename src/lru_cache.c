#include "include/lru_cache.h"


// reads in one block of memory from disk using ATA IDE
// @param index the index
int fs_read_block_from_disk(uint32_t index, void* buffer, uint32_t drive_num) {
    char* sect_buffer = mem_kalloc(SECTOR_SIZE);
    int err = 0;
    if (sect_buffer == NULL) {
        return -1;
    }
    uint32_t block_addr = index * BLOCK_SIZE;
    uint8_t isLowBlock = 0;
    if (block_addr %512 == 0) {
        isLowBlock = 1;
    }
    err = ide_read_sectors(drive_num, block_addr, 1, DS, (uint32_t) sect_buffer);
    if (err != 0) {
        mem_kfree(sect_buffer);
        return -1;
    }
    if (isLowBlock) {
        err = memcpy(buffer, sect_buffer, BLOCK_SIZE);   
        if (err != 0) {
            mem_kfree(sect_buffer);
            return -1;
        }

    } else {
        err  = memcpy(buffer, sect_buffer+BLOCK_SIZE, BLOCK_SIZE);
        if (err != 0) {
            mem_kfree(sect_buffer);
            return -1;
        }
    }
    mem_kfree(sect_buffer);
    return 0;
}
// writes block(256 bytes) to disk using ATA IDE
// @param index the block index to write to
// @param buffer the buffer to write
// @return 0 on success, else fail
int fs_write_block_to_disk(uint32_t index, void*buffer, uint32_t drive_num) {
    char* sect_buffer = mem_kalloc(SECTOR_SIZE);
    int err = 0;
    uint32_t block_addr = index * BLOCK_SIZE;
    uint8_t isLowBlock = 0;
    if (block_addr %SECTOR_SIZE == 0) {
        isLowBlock = 1;
    }
    err = ide_read_sectors(drive_num, block_addr, 1, DS, (uint32_t) sect_buffer);
    if (err != 0) {
        mem_kfree(sect_buffer);
        return -1;
    }
    if (isLowBlock) {
        err = memcpy(sect_buffer, buffer, BLOCK_SIZE);   
        if (err != 0) {
            mem_kfree(sect_buffer);
            return -1;
        }

    } else {
        err  = memcpy(sect_buffer+BLOCK_SIZE, buffer, BLOCK_SIZE);
        if (err != 0) {
            mem_kfree(sect_buffer);
            return -1;
        }
    }
    err = ide_write_sectors(drive_num, block_addr, 1, DS, (uint32_t) sect_buffer);
    mem_kfree(sect_buffer);
    if (err != 0) {  
        return -1;
    }
    return 0;
}
// folding hash function, partition bits size defined by PARTITION_BITS_HASH in header
// @param k the key to hash
// @param m the size of the hash table
// @return the hashed index
uint32_t hash(uint32_t k, uint32_t m) {
    uint32_t sum = 0;
    for (int i = 0; i < PARTITION_BITS_HASH; i++) {
        sum += (k >> (i * PARTITION_BITS_HASH)) & ((1<<PARTITION_BITS_HASH)-1);
    }
    return sum % m;
}
// gets the cache entry from the hash map
// @param block_num the block number to get
// @param cache pointer to the cache
// @return pointer to the cache entry, NULL if not found
cache_entry_t* hashed_map_get(int block_num, cache_t* cache) {
    uint32_t hash_index = hash(block_num, cache->capacity);
    cache_entry_t* entry = cache->hashmap[hash_index];
    while (entry != NULL && entry->block_num != block_num) {
        entry = entry->hash_next;
    }
    return entry;
}
void hashed_map_put(cache_entry_t* entry, cache_t* cache) {
    uint32_t hash_index = hash(entry->block_num, cache->capacity);
    cache_entry_t* head = cache->hashmap[hash_index];
    if (head == NULL) {
        cache->hashmap[hash_index] = entry;
        entry->hash_next = NULL;
        entry->hash_prev = NULL;
    } else {
        while (head->hash_next != NULL) {
            head = head->hash_next;
        }
        head->hash_next = entry;
        entry->hash_prev = head;
        entry->hash_next = NULL;
    }
}
int hashed_map_remove(int block_num, cache_t* cache) {
    uint32_t hash_index = hash(block_num, cache->capacity);
    cache_entry_t* entry = cache->hashmap[hash_index];
    while (entry != NULL && entry->block_num != block_num) {
        entry = entry->hash_next;
    }
    if (entry != NULL) {
        while (entry != NULL) {
            if (entry->block_num == block_num) {
                if (entry->hash_prev != NULL) {
                    entry->hash_prev->hash_next = entry->hash_next;
                } else {
                    cache->hashmap[hash_index] = entry->hash_next;
                }
                if (entry->hash_next != NULL) {
                    entry->hash_next->hash_prev = entry->hash_prev;
                }
                return 0;
            }
            entry = entry->hash_next;
        }
    }
    return -1;
}
// adds an entry to the head of the doubly linked list
// @param entry pointer to the cache entry
// @param cache pointer to the cache
// @return 0 on success, -1 on failure
int ll_add_to_head(cache_entry_t* entry, cache_t* cache) {
    if (entry == NULL || cache == NULL) {
        return -1;
    }
    if (cache->head == NULL) { // empty list
        cache->head = entry;
        cache->tail = entry;
        entry->prev = NULL;
        entry->next = NULL;
    } else {
        entry->next = cache->head;
        cache->head->prev = entry;
        entry->prev = NULL;
        cache->head = entry;
    }
    return 0;
}
int ll_move_to_head(cache_entry_t* entry, cache_t* cache) {
    if (entry == NULL || cache == NULL || cache->head == entry) {
        return -1;
    }
    // unlink entry
    if (entry->prev != NULL) {
        entry->prev->next = entry->next;
    }
    if (entry->next != NULL) {
        entry->next->prev = entry->prev;
    }
    if (cache->tail == entry) {
        cache->tail = entry->prev;
    }
    // move to head
    entry->next = cache->head;
    if (cache->head != NULL) {
        cache->head->prev = entry;
    }
    entry->prev = NULL;
    cache->head = entry;
    return 0;
}
int ll_remove(cache_t* cache, cache_entry_t* entry) {
    if (cache == NULL || entry == NULL) {
        return -1;
    }
    if (entry->prev != NULL) {
        entry->prev->next = entry->next;
    } else {
        cache->head = entry->next;
    }
    if (entry->next != NULL) {
        entry->next->prev = entry->prev;
    } else {
        cache->tail = entry->prev;
    }
    return 0;
}
/*--------------------------------------External Functions------------------------------*/

// creates a new cache with given capacity
// @param capacity the maximum number of items the cache can hold
// @return pointer to the created cache
cache_t* lru_cache_init(int capacity, uint32_t drive_num) {
    cache_t* cache = (cache_t*)mem_kalloc(sizeof(cache_t));
    memset(cache, 0, sizeof(cache_t));
    cache->items = mem_kalloc(capacity * sizeof(cache_entry_t));
    for(int i = 0; i < capacity; i++) {
        ((cache_entry_t*)(cache->items))[i].present = 0;
    }
    cache->hashmap = mem_kalloc(capacity * sizeof(void*));
    for (int i = 0; i < capacity; i++) {
        cache->hashmap[i] = NULL;
    }
    cache->head = 0;
    cache->tail = 0;
    cache->size = 0;
    cache->capacity = capacity;
    cache->drive_num = drive_num;
    spinlock_init(&cache->lock);
    return cache;
}
// checks if the cache is full
// @param cache pointer to the cache
// @return 1 if full, 0 otherwise
int is_cache_full(cache_t* cache) {
    return cache->size == cache->capacity;
}
// checks if the cache is empty
// @param cache pointer to the cache
// @return 1 if empty, 0 otherwise
int is_cache_empty(cache_t* cache) {
    return cache->size == 0;
}
// updates and releases an item into the cache, ejects the least recently used item if full, buffer should have already beed given sono need to copy, really just update the dirty bit
// @param cache pointer to the cache
// @return 0 on success, -1 on failure
int lru_cache_update_and_release(cache_t* cache, uint32_t block_num, int is_modified) {
    spinlock_acquire(&cache->lock);
    cache_entry_t* entry = hashed_map_get(block_num, cache);
    if (entry == NULL) {
        return -1; // not found
    }
    if (is_modified) {
        entry->dirty = 1;
    }
    ll_move_to_head(entry, cache);
    entry->refcnt--;
    spinlock_release(&cache->lock);
    return 0;
}
// gets an item from the cache, if not found, evicts the least recently used item and reads from disk
// @param cache pointer to the cache
// @param block_num the block number to get
// @return pointer to the data, NULL on failure
void* lru_cache_get_and_lock(cache_t* cache, uint32_t block_num) {
    spinlock_acquire(&cache->lock);
    cache_entry_t* entry = hashed_map_get(block_num, cache);
    if (entry == NULL) { // not found, need to load into memory
        if (is_cache_full(cache)) { // cache full, need to evict
            cache_entry_t* lru_entry = cache->tail;
            while (lru_entry != NULL && lru_entry->refcnt > 0) { // find LRU that is not being used
                lru_entry = lru_entry->prev;
            }
            if (lru_entry == NULL) {
                spinlock_release(&cache->lock);
                return NULL; // all entries are being used, wack
            }
            // write back if dirty
            if (lru_entry->dirty) {
                fs_write_block_to_disk(lru_entry->block_num, lru_entry->data, cache->drive_num);
                lru_entry->dirty = 0;
            }
            // remove from hashmap and linked list
            hashed_map_remove(lru_entry->block_num, cache);
            ll_remove(cache, lru_entry);

            // now i load in the new block and place at the head
            fs_read_block_from_disk(block_num, lru_entry->data, cache->drive_num);
            lru_entry->block_num = block_num;
            lru_entry->present = 1;
            lru_entry->dirty = 0;
            lru_entry->refcnt = 1;
            ll_add_to_head(lru_entry, cache);
            hashed_map_put(lru_entry, cache);
            spinlock_release(&cache->lock);
            return lru_entry->data;
        } else { // cache not full, find an empty entry
            cache_entry_t* curr_entry = cache->items;

            for (int i = 0; i < cache->capacity; i++) {
                if (curr_entry->present == 0) {
                    // load in the block
                    fs_read_block_from_disk(block_num, curr_entry->data, cache->drive_num);
                    curr_entry->block_num = block_num;
                    curr_entry->present = 1;
                    curr_entry->dirty = 0;
                    curr_entry->refcnt = 1;
                    ll_add_to_head(curr_entry, cache);
                    hashed_map_put(curr_entry, cache);
                    cache->size++;
                    spinlock_release(&cache->lock);
                    return curr_entry->data;
                }
            }
            // should not reach here
            spinlock_release(&cache->lock);
            return NULL;

        }
    } else {
        // found in cache, move to head
        ll_move_to_head(entry, cache);
        entry->refcnt++;
        spinlock_release(&cache->lock);
        return entry->data;
    }
   
    spinlock_release(&cache->lock);
    return NULL;
}
// flushes all dirty entries to disk
// @param cache pointer to the cache
// @return 0 on success, -1 on failure
int lru_cache_flush(cache_t* cache) {
    for (int i = 0; i < cache->capacity; i++) {
        cache_entry_t* entry = &((cache_entry_t*)(cache->items))[i];
        if (entry->present && entry->dirty && entry->refcnt == 0) { // only flush if present, dirty and not being used
            int err = fs_write_block_to_disk(entry->block_num, entry->data, cache->drive_num);
            if (err != 0) {
                return -1;
            }
            entry->dirty = 0;
        }
    }
    return 0;
}
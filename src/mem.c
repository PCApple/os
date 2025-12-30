#include "include/mem.h"

page_t page_pool[MAX_PAGES];

// initialize memory pages from the memory map
// @param mmap the memory map structure given by GRUB
// @return number of initialized pages, -1 on failure
int mem_init(multiboot_info_t*mmap) {
    if (mmap == NULL) return -1;
    if (mmap->mmap_length == 0) return -1;
    int cnt = 0;

    /* Iterate the multiboot memory map using each entry's size field. */
    uint32_t cur = mmap->mmap_addr;
    uint32_t end = mmap->mmap_addr + mmap->mmap_length;
    while (cur < end) {
        multiboot_memory_map_t *entry = (multiboot_memory_map_t *)cur;
        /* entry->size is the size of the variable part of the entry; step by that + the size field. */
        uint32_t step = entry->size + sizeof(entry->size);
        /* Record start/length as 64-bit values from the multiboot entry. */
        uint64_t start_addr64 = entry->addr;
        uint64_t len64 = entry->len;
        uint32_t type = entry->type;

        /* advance pointer for next loop iteration */
        cur += step;

        if (type != MULTIBOOT_MEMORY_AVAILABLE) continue;
        if (start_addr64 + len64 <= START_MEM) continue; /* skip low memory */

        /* truncate to 32-bit addresses for this 32-bit kernel */
        uint32_t start_addr = (uint32_t)start_addr64;
        uint32_t size = (uint32_t)len64;

        /* iterate pages inside this region using a distinct offset variable (avoid shadowing) */
        for (uint32_t offset = 0; offset + PAGE_SIZE <= size; offset += PAGE_SIZE) {
            uint32_t addr = start_addr + offset;
            if (addr < START_MEM) continue;
            page_pool[cnt].flags = 0x39; /* present + available + read + write */
            page_pool[cnt].addr = (void *)((unsigned long)addr);
            page_pool[cnt].size = PAGE_SIZE;
            cnt++;
            if (cnt >= MAX_PAGES) break;
        }
        if (cnt >= MAX_PAGES) break;
    }

    if (cnt == 0) return -1; /* didn't find any usable pages */
    if (cnt < MAX_PAGES) {
        /* Initialize remaining pages as not present */
        for (int j = cnt; j < MAX_PAGES; j++) {
            page_pool[j].flags = 0;
            page_pool[j].addr = NULL;
            page_pool[j].size = 0;
        }
    }
    return cnt;
}

// Allocates a memory page
// @param: flags allocation flags bit 0 user(1) kernel(0), bit 1 size(0=4kb, 1=2MB)
// @return: address of page allocated

void* mem_kalloc(uint8_t flags) {
    uint8_t size_flag = (flags & 0x02) >> 1;
    for (int i = 0; i < MAX_PAGES; i++) {
        if (size_flag){
            uint32_t completed = 1;
            for (int j = 0; j < BIG_PAGE_COUNT; j++) { // see if we have 512 continuous pages
                if (!(page_pool[i + j].flags & (0x01)) || !(page_pool[i + j].flags & 0x08)) { // not present or not available
                    completed = 0;
                    break;
                }
            }
            if (completed)
            {
                for (int j = 0; j < BIG_PAGE_COUNT; j++) {
                    page_pool[i + j].flags &= ~0x08; // Mark the pages as not available
                    page_pool[i + j].flags |= 1<<6; // mark as big page
                }
                return page_pool[i].addr;
            }
            continue;
        }
        if (page_pool[i].flags & 0x01 && page_pool[i].flags & 0x08) { // Check if the page is present and available
            page_pool[i].flags &= ~0x08; // Mark the page as not available
            return page_pool[i].addr;
        }
    }
    return NULL;
}

// Frees a memory page
// @param addr address of the page to free
int mem_kfree(void* addr) {
    for (int i = 0; i < MAX_PAGES; i++) {
        if (page_pool[i].addr == addr) {
            if (page_pool[i].flags & (1<<6)) { // big page
                for (int j = 0; j < BIG_PAGE_COUNT; j++) {
                    page_pool[i + j].flags |= 0x08; // Mark the pages as available
                    page_pool[i + j].flags &= ~(1<<6); // unmark big page
                }
                return 0;
            }
            page_pool[i].flags |= 0x08; // Mark the page as available
            return 0;
        }
    }
    return -1;
}

// Compares two memory blocks
// @param s1 first memory block
// @param s2 second memory block
// @param n number of bytes to compare
// @return negative if s1 < s2, positive if s1 > s2, 0 if equal
int memcmp(void *s1, void *s2, uint32_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    for (uint32_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

// Sets a memory block to a specific value
// @param source pointer to the memory block
// @param value value to set
// @param n number of bytes to set
// @return 0 on success, -1 on failure
int memset(void* source, int value, uint32_t n) {
    if (source == NULL) return -1;
    if (n == 0) return 0;
    uint8_t* p = source;
    int i = 0;
    for (i = 0; i < n; i++) {
        p[i] = (uint8_t)value;
    }
    return 0;
}

// Copies a memory block
// @param dest destination memory block
// @param src source memory block
// @param n number of bytes to copy
// @return 0 on success, -1 on failure
int memcpy(void* dest, void* src, uint32_t n) {
    if (dest == NULL || src == NULL) return -1;
    if (n == 0) return 0;
    uint8_t* d = dest;
    const uint8_t* s = src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return 0;
}
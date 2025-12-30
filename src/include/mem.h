#ifndef __MEM_H__
#define __MEM_H__
#include "types.h"
#include "multiboot.h"

#define PAGE_SIZE 4096
#define MAX_PAGES 8192 // about 32MB
#define BIG_PAGE_SIZE 2097152
#define BIG_PAGE_COUNT (BIG_PAGE_SIZE / PAGE_SIZE) // should be 512
#define START_MEM 0x400000 //4MB

typedef struct page {
    uint8_t flags; //0 present, 1-2 ring, 3 available, 4 read, 5 write 6 big page(2MB)
    void* addr;
    uint32_t size;
} page_t;

int mem_init(multiboot_info_t*);
void* mem_kalloc(uint8_t flags);
int mem_kfree(void*);
int memset(void *, int, uint32_t);
int memcpy(void *, void *, uint32_t);
int memcmp(void *, void *, uint32_t);
#endif
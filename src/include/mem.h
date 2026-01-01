#ifndef __MEM_H__
#define __MEM_H__
#include "types.h"
#include "multiboot.h"
#include "utils.h"

#define PAGE_SIZE 4096
#define BIG_PAGE_SIZE 2097152
#define BIG_PAGE_COUNT (BIG_PAGE_SIZE / PAGE_SIZE) // should be 512
#define START_MEM 0x400000 //4MB
typedef struct alloc_header {
    uint32_t pages_allocated;
} alloc_header_t;
int mem_init(multiboot_info_t*);
void* mem_kalloc(size_t size);
int mem_kfree(void*);
int memset(void *, int, uint32_t);
int memcpy(void *, void *, uint32_t);
int memcmp(void *, void *, uint32_t);
#endif
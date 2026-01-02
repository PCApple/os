#include "include/mem.h"
extern char __kernel_start[];
extern char __kernel_end[];

void* kstart = (void*)__kernel_start;
void* kend   = (void*)__kernel_end; 
uint32_t total_pages;

char* pool_bitmap;


uint32_t align_up(uint32_t addr, uint32_t align) {
    if (align == 0) return addr;
    uint32_t remainder = addr % align;
    if (remainder == 0) return addr;
    return addr + (align - remainder);
}

uint32_t align_down(uint32_t addr, uint32_t align) {
    if (align == 0) return addr;
    uint32_t remainder = addr % align;
    if (remainder == 0) return addr;
    return addr - remainder;
}
void* bit_and_byte_to_addr(uint32_t bit_index, uint8_t byte_index) {
    uint32_t page_index = byte_index * 8 + bit_index;
    return (void*)(page_index * PAGE_SIZE);
}
uint64_t get_mem_size(multiboot_info_t*mmap){
    uint64_t total_size = 0;
    if (mmap == NULL) return 0;
    if (mmap->mmap_length == 0) return 0;
    uint32_t cur = mmap->mmap_addr;
    uint32_t end = mmap->mmap_addr + mmap->mmap_length;
    while (cur < end) {
        multiboot_memory_map_t *entry = (multiboot_memory_map_t *)cur;
        uint32_t step = entry->size + sizeof(entry->size);
        uint64_t len = entry->len;
        uint32_t type = entry->type;
        cur += step;
        total_size += len;
    }
    return total_size;

}

// initialize memory pages from the memory map
// @param mmap the memory map structure given by GRUB
// @return number of initialized pages, -1 on failure
int mem_init(multiboot_info_t*mmap) {
    if (mmap == NULL) return -1;
    if (mmap->mmap_length == 0) return -1;
    int cnt = 0;
    int first_free_page_found = 0; // need to skip first to protect the .eh_frame section

    // need to init some variables first
    uint32_t start = mmap->mmap_addr;
    uint32_t end = mmap->mmap_addr + mmap->mmap_length;
    uint64_t total_mem = get_mem_size(mmap);
    uint32_t max_pages = (uint32_t)(total_mem / PAGE_SIZE);
    uint32_t needed_bitmap_pages = align_up(max_pages/8, PAGE_SIZE) / PAGE_SIZE;
    // now we need to make space for the bitmap itself
    uint32_t bitmap_entry_start = -1;
    uint32_t bitmap_entry_end = -1;
    uint32_t temp_strt = -1;
    uint32_t cur = start;
    uint32_t remaining_pags_needed = needed_bitmap_pages;
    while (cur < end) {
        multiboot_memory_map_t* entry = (multiboot_memory_map_t *)cur;
        uint32_t step = entry->size + sizeof(entry->size);
        uint64_t len = entry->len;
        uint32_t type = entry->type;
        uint32_t addr = (uint32_t)entry->addr;
        
        if (entry->type != MULTIBOOT_MEMORY_AVAILABLE) {
            cur += step;
            continue;
        }
        if (addr + len <= (uint32_t)kstart) {
            cur += step;
            continue;
        }
        for (uint32_t offset = 0; offset + PAGE_SIZE <= len; offset += PAGE_SIZE){
            if (addr + offset < (uint32_t)kend) {
                continue;
            }
            if (!first_free_page_found) {
                first_free_page_found = 1;
                continue; // skip first free page to protect .eh_frame section
            }
            if (temp_strt == -1) {
                temp_strt = addr + offset;
                remaining_pags_needed = needed_bitmap_pages-1;
            }
            else {
                remaining_pags_needed--;
            }
            if (remaining_pags_needed == 0) {
                bitmap_entry_start = temp_strt;
                bitmap_entry_end = temp_strt + needed_bitmap_pages * PAGE_SIZE;
                cur = end; // to break outer loop
                break;
            }
        }
        cur += step;
    }
    if (bitmap_entry_start == -1) {
        return -1; // couldn't find space for bitmap
    }
    // initialize the bitmap
    pool_bitmap = (char*)bitmap_entry_start;
    memset(pool_bitmap, 0xFF, needed_bitmap_pages * PAGE_SIZE);
    // now initialize the page pool itself
    cur = start;
    while (cur < end) {
        multiboot_memory_map_t *entry = (multiboot_memory_map_t *) cur;
        uint32_t step = entry->size + sizeof(entry->size);
        if (entry->type != MULTIBOOT_MEMORY_AVAILABLE) {
            cur += step;
            continue;
        }
        uint64_t entry_start = entry->addr;
        uint64_t entry_len = entry->len;
        uint64_t entry_end = entry_start + entry_len;
        for (int offset = 0; offset + PAGE_SIZE <= entry_len; offset += PAGE_SIZE) {
            uint32_t pstart = entry_start + offset;
            uint32_t pend = pstart + PAGE_SIZE;
            if (entry->addr + PAGE_SIZE <= (uint32_t)kstart) {
                continue;
            }
            if ((uint32_t)kstart < pend && pstart < (uint32_t)kend){ //overlap, skip
                continue;
            }
            if (bitmap_entry_start-PAGE_SIZE < pend && pstart < bitmap_entry_end) { //overlap with bitmap, skip
                continue;
            }
            // find byte and bit in bitmap
            uint32_t page_index = pstart / PAGE_SIZE;
            uint32_t byte_index = page_index / 8;
            uint8_t bit_index = page_index % 8;
            // mark page as not used
            pool_bitmap[byte_index] &= ~(1 << bit_index);
            cnt ++;
        }
        cur += step;
    }
    total_pages = cnt;
    return cnt;
}

// Allocates a memory page
// @param: size number of bytes to allocate
// @return: address of page allocated

void* mem_kalloc(size_t size) {
    uint32_t pages_needed = ceiling(size + sizeof(alloc_header_t),PAGE_SIZE);
    uint32_t consecutive_free = 0;
    uint32_t start_page_index = 0; // on bitmap
    for (uint32_t i = 0; i < total_pages; i++) {
        uint32_t byte_index = i / 8;
        uint8_t bit_index = i % 8;
        if (!(pool_bitmap[byte_index] & (1 << bit_index))) { // page is free
            if (consecutive_free == 0) {
                start_page_index = i;
            }
            consecutive_free++;
            if (consecutive_free == pages_needed) {
                // mark pages as used
                for (uint32_t j = start_page_index; j < start_page_index + pages_needed; j++) {
                    uint32_t b_index = j / 8;
                    uint8_t bt_index = j % 8;
                    pool_bitmap[b_index] |= (1 << bt_index);
                }
                void* addr = bit_and_byte_to_addr(start_page_index % 8, start_page_index / 8);
                alloc_header_t* header = (alloc_header_t*)addr;
                header->pages_allocated = pages_needed;
                return (void*)((uint8_t*)addr + sizeof(alloc_header_t));
            }
        } else {
            consecutive_free = 0;
        }
    }
    return NULL;
}

// Frees a memory page
// @param addr address of the page to free
int mem_kfree(void* addr) {
    // get bit and byte index
    alloc_header_t* header = (alloc_header_t*)((uint8_t*)addr - sizeof(alloc_header_t));
    uint32_t pages_allocated = header->pages_allocated;
    uint32_t start_page_index = (uint32_t)(header) / PAGE_SIZE;
    for (uint32_t i = start_page_index; i < start_page_index + pages_allocated; i++) {
        uint32_t byte_index = i / 8;
        uint8_t bit_index = i % 8;
        if (pool_bitmap[byte_index] & (1 << bit_index)) { // page is used
            pool_bitmap[byte_index] &= ~(1 << bit_index); // mark it as free
        } else {
            return -1; // page was already free
        }
    }
    return 0;
}


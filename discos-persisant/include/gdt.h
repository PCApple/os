#ifndef __GDT_H__
#define __GDT_H__
#include "types.h"

#define GDT_SIZE 3 // null, kernel code, kernel data

#define KERN_CODE_ACCESS 0x9A
#define KERN_DATA_ACCESS 0x92
#define KERN_FLAGS 0xC
typedef struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access; // access byte: 0-accessed, 1-rw, 2-dc, 3-exec, 4-type(0-sysseg, 1-code/data), 5-dpl(0=kernel, 3=user), 6-present
    uint8_t limithigh_and_flags; // 0-3 limit high, 4-7 flags (0=reserved,1=longmode, 2=size(0-16bit,1-32bit), 3-granularity(0-byte,1-4kb))
    uint8_t base_high;

} gdt_entry_t;
typedef struct gdtr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdtr_t;

int gdt_init();



#endif
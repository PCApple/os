#include "include/gdt.h"

gdt_entry_t gdt[GDT_SIZE];
gdtr_t gdtr;

extern void lgdt(uint32 gdtr_ptr);

/*
* Sets a GDT entry at index IDX with the given BASE, LIMIT, ACCESS_BYTE, and FLAGS.
* @param idx The index of the GDT entry to set.
* @param base The base address for the segment.
* @param limit The limit of the segment. this is acually 20 bits, so max is 0xFFFFF, everything above that will be truncated
* @param access_byte The access byte for the segment. access byte: 0-accessed, 1-rw, 2-dc, 3-exec, 4-type(0-sysseg, 1-code/data), 5-dpl(0=kernel, 3=user), 6-present
* @param flags The flags for the segment. this is 4 bits: (0=reserved,1=longmode, 2=size(0-16bit,1-32bit), 3-granularity(0-byte,1-4kb))
*/
void gdt_set_entry(int idx, uint32_t base, uint32_t limit, uint8_t access_byte, uint8_t flags) {
    gdt_entry_t* entry = &gdt[idx];
    entry->base_low = base & 0xFFFF;
    entry->base_middle = (base >> 16)&0xFF;
    entry->base_high = (base>>24) & 0xFF;
    entry->limit_low = limit & 0xFFFF;
    entry->limithigh_and_flags = ((limit >> 16) & 0xF) | (flags & 0xF)<<4;
    entry->access = access_byte;
}
/*
* Initializes the GDT
*/
int gdt_init(){
    gdt_set_entry(0,0,0,0,0); // null segment
    gdt_set_entry(1,0,0xFFFFF,KERN_CODE_ACCESS,KERN_FLAGS); // kernel code segment
    gdt_set_entry(2, 0,0xFFFFFFFF, KERN_DATA_ACCESS, KERN_FLAGS); // kernel data segment
    gdt_set_entry(3, 0, 0xFFFFFFFF, USER_CODE_ACCESS, USER_FLAGS); // user code segment
    gdt_set_entry(4, 0, 0xFFFFFFFF, USER_DATA_ACCESS, USER_FLAGS); // user data segment
    gdtr.base = (uint32_t)&gdt;
    gdtr.limit = (uint32_t)GDT_SIZE * sizeof(gdt_entry_t);
    lgdt((uint32_t)&gdtr);
    return 0;
}
#ifndef __INT_H__
#define __INT_H__
#include <stdint.h>
#include "print.h"
#include "io.h"
#include "scheduler.h"
#include "pic.h"


#define PRESENT_BIT 0x80
#define INT_GATE_ATTR (0xE|0x80) // Interrupt gate, ring 0, present
#define KERNEL_CS 0x08 // Kernel code segment
#define GATE_TYPE_MASK 0x0F
#define DPL_MASK 0x60
#define MAX_DESCRIPTORS 256

typedef struct idt_entry {
    uint16_t    offset_low;
    uint16_t    kernel_cs;
    uint8_t     reserved; // do not touch
    uint8_t     attributes; // 0-3 gate type, 5-6 DPL, 7 present
    uint16_t    offset_high;
} __attribute__((packed)) idt_entry_t;

__attribute__((aligned(0x10))) 
static idt_entry_t idt[MAX_DESCRIPTORS]; // Create an array of IDT entries; aligned for performance


typedef struct {
	uint16_t	limit;
	uint32_t	base;
} __attribute__((packed)) idtr_t;

static idtr_t idtr;
void set_idt_entry(uint8_t int_num, void* handler_addr, uint8_t attributes);
void exception_handler(uint8_t int_num, uint32_t error_code);
void irq_handler(uint8_t int_num);
void idt_init();
#endif
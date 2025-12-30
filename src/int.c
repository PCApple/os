#include "include/int.h"

extern void* int_table[]; // defined in int_handler.S i hope
static int cnt = 0;

void exception_handler(uint8_t int_num, uint32_t error_code) {
    // Implementation of the interrupt handler
    // This function will handle the interrupt based on the interrupt number
    if (int_num < 32) { // If not the timer interrupt
        char buf[32];
        itoa(buf, 'd', int_num);
        terminal_writestring("Interrupt: ");
        terminal_writestring(buf);
        itoa(buf, 'd', error_code);
        terminal_writestring(" Error Code: ");
        terminal_writestring(buf);
        terminal_writestring("\n");
        terminal_writestring("Halting...");
        __asm__ volatile ("cli; hlt");
    }
}

void set_idt_entry(uint8_t int_num, void* handler_addr, uint8_t attributes) {
    idt[int_num].offset_low = (uint32_t)handler_addr & 0xFFFF;
    idt[int_num].kernel_cs = KERNEL_CS; // Kernel code segment
    idt[int_num].reserved = 0;
    idt[int_num].attributes = attributes;
    idt[int_num].offset_high = (uint32_t)handler_addr >> 16 & 0xFFFF;
}
void idt_init() {
    //pushf();
    //cli();
    idtr.limit = sizeof(idt_entry_t) * MAX_DESCRIPTORS - 1;
    idtr.base = (uint32_t)&idt;
    for (int i = 0; i < 256; i++) {
        if (i < 33) { // First 32 exceptions including irq0 hopefully
            set_idt_entry(i, int_table[i], INT_GATE_ATTR);
        }
        else {
            set_idt_entry(i, 0, 0); // Unused interrupts
        }
    }
    __asm__ volatile ("lidt %0" : : "m"(idtr)); // load the new IDT
    //popf();
}
void irq_handler(uint8_t int_num) {
    if (int_num == 32) {
        scheduler_handle_tick();
    }
    else {
        terminal_writestring("Unhandled IRQ: ");
        char buf[32];
        itoa(buf, 10, int_num);
        terminal_writestring(buf);
        terminal_writestring("\n");
    }
}

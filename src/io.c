#include "include/io.h"

void get_esp(uint32_t *esp) {
    __asm__ volatile("movl %%esp, %0" : "=r" (*esp));
}
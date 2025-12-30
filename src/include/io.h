#ifndef __IO_H__
#define __IO_H__
#include "types.h"

static inline unsigned char inb( unsigned short usPort ) {

    unsigned char uch;
   
    __asm__ volatile( "inb %1,%0" : "=a" (uch) : "Nd" (usPort) );
    return uch;
}

static inline void outb( unsigned char uch, unsigned short usPort ) {

    __asm__ volatile( "outb %0,%1" : : "a" (uch), "Nd" (usPort) );
}

static inline unsigned int inl( unsigned short usPort ) {

    unsigned int ui;
   
    __asm__ volatile( "inl %1,%0" : "=a" (ui) : "Nd" (usPort) );
    return ui;
}

static inline void outl( unsigned int ui, unsigned short usPort ) {

    __asm__ volatile( "outl %0,%1" : : "a" (ui), "Nd" (usPort) );
}
static inline unsigned short inw( unsigned short usPort ) {

    unsigned short us;
   
    __asm__ volatile( "inw %1,%0" : "=a" (us) : "Nd" (usPort) );
    return us;
}
static inline void outw( unsigned short us, unsigned short usPort ) {

    __asm__ volatile( "outw %0,%1" : : "a" (us), "Nd" (usPort) );
}

static inline void insl( unsigned short port, unsigned long addr, unsigned int count ) {
    __asm__ volatile ( "cld; rep insl" : "+D" (addr), "+c" (count) : "d" (port) : "memory" );
}
static inline void io_wait(void)
{
    outb(0, 0x80);
}

static inline void cli() {
    __asm__ volatile ("cli");
}

static inline void popf() {
    __asm__ volatile ("popf");
}

static inline void pushf() {
    __asm__ volatile ("pushf");
}
static inline void sti() {
    __asm__ volatile ("sti");
}


void get_esp(uint32_t *esp);
#endif
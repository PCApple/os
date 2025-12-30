#ifndef __PRINT_H
#define __PRINT_H
#include "types.h"
#include "stdarg.h"
enum vga_color
{
  COLOR_BLACK = 0,
  COLOR_BLUE = 1,
  COLOR_GREEN = 2,
  COLOR_CYAN = 3,
  COLOR_RED = 4,
  COLOR_MAGENTA = 5,
  COLOR_BROWN = 6,
  COLOR_LIGHT_GREY = 7,
  COLOR_DARK_GREY = 8,
  COLOR_LIGHT_BLUE = 9,
  COLOR_LIGHT_GREEN = 10,
  COLOR_LIGHT_CYAN = 11,
  COLOR_LIGHT_RED = 12,
  COLOR_LIGHT_MAGENTA = 13,
  COLOR_LIGHT_BROWN = 14,
  COLOR_WHITE = 15,
};
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 24;
 

/* Convert the integer D to a string and save the string in BUF. If
   BASE is equal to 'd', interpret that D is decimal, and if BASE is
   equal to 'x', interpret that D is hexadecimal. */
void itoa (char *buf, int base, int d);

uint8_t make_color(enum vga_color fg, enum vga_color bg);

uint16_t make_vgaentry(char c, uint8_t color);



void terminal_initialize();

void terminal_setcolor(uint8_t color);

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);

void terminal_putchar(char c);

void terminal_writestring(const char* data);

void terminal_clear();

// str stuff
int strcmp(const char* s1, const char* s2);
size_t strlen(const char* str);
void strncpy(char* dest, const char* src, size_t n);
void strcpy(char* dest, const char* src);
// printk
void printk(const char* format, ...);
#endif


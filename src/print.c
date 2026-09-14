#include "include/print.h"
#include <stdarg.h>
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;
/* Convert the integer D to a string and save the string in BUF. If
   BASE is equal to 'd', interpret that D is decimal, and if BASE is
   equal to 'x', interpret that D is hexadecimal. */
void itoa (char *buf, int base, int d)
{
  char *p = buf;
  char *p1, *p2;
  unsigned long ud = d;
  int divisor = 10;
     
  /* If %d is specified and D is minus, put `-' in the head. */
  if (base == 'd' && d < 0)
    {
      *p++ = '-';
      buf++;
      ud = -d;
    }
  else if (base == 'x')
    divisor = 16;
    
     
  /* Divide UD by DIVISOR until UD == 0. */
  do
    {
      int remainder = ud % divisor;
     
      *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    }
  while (ud /= divisor);
     
  /* Terminate BUF. */
  *p = 0;
     
  /* Reverse BUF. */
  p1 = buf;
  p2 = p - 1;
  while (p1 < p2)
    {
      char tmp = *p1;
      *p1 = *p2;
      *p2 = tmp;
      p1++;
      p2--;
    }
}

uint8_t make_color(enum vga_color fg, enum vga_color bg)
{
  return fg | bg << 4;
}
 
uint16_t make_vgaentry(char c, uint8_t color)
{
  uint16_t c16 = c;
  uint16_t color16 = color;
  return c16 | color16 << 8;
}
 
size_t strlen(const char* str)
{
  size_t ret = 0;
  while ( str[ret] != 0 )
    ret++;
  return ret;
}
 
void terminal_initialize()
{
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
  terminal_buffer = (uint16_t*) 0xB8000;
  for ( size_t y = 0; y < VGA_HEIGHT; y++ )
    {
      for ( size_t x = 0; x < VGA_WIDTH; x++ )
	{
	  const size_t index = y * VGA_WIDTH + x;
	  terminal_buffer[index] = make_vgaentry(' ', terminal_color);
	}
    }
}
 
void terminal_setcolor(uint8_t color)
{
  terminal_color = color;
}
 
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = make_vgaentry(c, color);
}
 
void terminal_putchar(char c)
{
  terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
  if ( ++terminal_column == VGA_WIDTH )
    {
      terminal_column = 0;
      if ( ++terminal_row == VGA_HEIGHT )
	{
	  terminal_row = 0;
	}
    }
}

void terminal_writestring(const char* data)
{
  int is_newline = 0;
  int len = strlen(data);
  char last_char = data[len - 1];
  if (last_char == '\n') {
    is_newline = 1;
    len--;
  }
  for ( size_t i = 0; i < len; i++ )
    terminal_putchar(data[i]);
  if (is_newline) {
    terminal_row++;
    terminal_column = 0;
  }
}
void terminal_clear()
{
  for (size_t y = 0; y < VGA_HEIGHT; y++)
  {
    for (size_t x = 0; x < VGA_WIDTH; x++)
    {
      terminal_putentryat(' ', terminal_color, x, y);
    }
  }
  terminal_row = 0;
  terminal_column = 0;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
void strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
}
void strcpy(char* dest, const char* src) {
    size_t sized = strlen(dest);
    size_t sizes = strlen(src);
    int n = 0;
    if (sized < sizes) {
        n = sized;
    } else {
        n = sizes;
    }
    strncpy(dest, src, n);
}

void printk(const char* format, ...){
  va_list ap;
  va_start(ap, format);
  char num_buf[64];
  for (size_t i = 0; format && format[i]; i++){
    if (format[i] != '%'){
      if (format[i] == '\n'){
        terminal_writestring("\n");
        continue;
      }
      terminal_putchar(format[i]);
      continue;
    }
    i++;
    char c = format[i];
    if (!c) break;
    if (c == '%'){
      terminal_putchar('%');
    } else if (c == 'c'){
      char ch = (char)va_arg(ap, int);
      terminal_putchar(ch);
    } else if (c == 's') {
      const char* s = va_arg(ap, const char*);
      terminal_writestring(s);
    } else if (c == 'd' || c == 'i') {
      int v = va_arg(ap, int);
      itoa(num_buf, 'd', v);
      terminal_writestring(num_buf);
    } else if (c == 'u') {
      uint32_t v = va_arg(ap, uint32_t);
      itoa(num_buf,'u',v);
      terminal_writestring(num_buf);
    } else if (c == 'x') {
      uint32_t v = va_arg(ap,uint32_t);
      itoa(num_buf,'x',v);
      terminal_writestring(num_buf);
    } else {
      terminal_putchar('%');
      terminal_putchar(c);
    }
  }
  va_end(ap);
}

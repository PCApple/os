#include "include/print.h"
#include "include/strings.h"
#include <stdarg.h>
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;
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

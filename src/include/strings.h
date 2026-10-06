#ifndef __STRINGS_H
#define __STRINGS_H

#include <stddef.h>

/* BASE is 'd' for decimal or 'x' for hexadecimal. */
void itoa(char *buf, int base, int d);

int strcmp(const char* s1, const char* s2);
size_t strlen(const char* str);
void strncpy(char* dest, const char* src, size_t n);
void strcpy(char* dest, const char* src);

#endif

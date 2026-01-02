#include "include/utils.h"

// ceiling division of two integers
// @param numerator the numerator
// @param denominator the denominator
// @return the ceiling of numerator / denominator
int ceiling(int numerator, int denominator) {
    if (numerator % denominator == 0) {
        return numerator / denominator;
    }
    else{
        return (numerator / denominator) + 1;
    }
}

// Compares two memory blocks
// @param s1 first memory block
// @param s2 second memory block
// @param n number of bytes to compare
// @return negative if s1 < s2, positive if s1 > s2, 0 if equal
int memcmp(void *s1, void *s2, uint32_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    for (uint32_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

// Sets a memory block to a specific value
// @param source pointer to the memory block
// @param value value to set
// @param n number of bytes to set
// @return 0 on success, -1 on failure
int memset(void* source, int value, uint32_t n) {
    if (source == NULL) return -1;
    if (n == 0) return 0;
    uint8_t* p = source;
    int i = 0;
    for (i = 0; i < n; i++) {
        p[i] = (uint8_t)value;
    }
    return 0;
}

// Copies a memory block
// @param dest destination memory block
// @param src source memory block
// @param n number of bytes to copy
// @return 0 on success, -1 on failure
int memcpy(void* dest, void* src, uint32_t n) {
    if (dest == NULL || src == NULL) return -1;
    if (n == 0) return 0;
    uint8_t* d = dest;
    const uint8_t* s = src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return 0;
}
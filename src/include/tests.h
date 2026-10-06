#ifndef _TESTS_H
#define _TESTS_H
#include "block_cache.h"
#include "print.h"

void test_block_cache(void);
/* Run once per boot, before mounting the filesystem. */
void test_fs(void);

#endif
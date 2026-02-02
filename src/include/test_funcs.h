#ifndef TEST_FUNCS_H
#define TEST_FUNCS_H

#include <stdint.h>
#include "print.h"
#include "scheduler.h"
#include "mem.h"
#include "pit.h"
#include "fs.h"
#include "mem.h"
#include "fs.h"
#include "print.h"

typedef struct test_args {
  uint32_t secs; // seconds to run
} test_args_t;
typedef struct char_bufs {
  char buf[10];
  char zero; //never unset
} char_bufs_t;

void test_write_final(void*args);
void test_simple_fs(void* args);
void test_big_fs(void* args);
void test_multi_fs(void* args);

#endif // TEST_FUNCS_H
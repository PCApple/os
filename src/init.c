#include "include/multiboot.h"
#include "include/mem.h"
#include "include/print.h"
#include "include/pic.h"
#include "include/scheduler.h"
#include "include/thread.h"
#include "include/test_funcs.h"
#include "include/int.h"
#include "include/pit.h"
#include "include/gdt.h"
#include "include/fs.h"
#include "include/ide.h"

char fs_buf[FS_SIZE];

void init(unsigned int magic_num, multiboot_info_t* binfo) {
  multiboot_info_t* mmap = binfo;
  if (magic_num != 0x2BADB002){
    terminal_writestring("ERROR: invalid magic_num");
    return;
  }
  gdt_init();
  terminal_writestring("GDT initialized\n");
  terminal_initialize();
  terminal_writestring("Terminal initialized\n");
  printk("Testing printk...\n");
  char* test_str = "testing123";
  printk("char: %c, string: %s, int: %i, %d, uint: %u, hex: %x\n", 'e',test_str,11,-3,-1,12);
  printk("Testing complete\n");
  int mem_ret = mem_init(mmap);
  if (mem_ret < 0) {
    terminal_writestring("Memory initialization failed\n");
    return;
  }
  PIC_init();
  terminal_writestring("PIC initialized\n");
  idt_init();
  terminal_writestring("IDT initialized\n");
  terminal_writestring("Memory initialization succeeded\n");
  void* fs_mem = &fs_buf;
  if (fs_mem == NULL) {
    terminal_writestring("Filesystem memory allocation failed\n");
    return;
  }
  terminal_writestring("Filesystem memory allocated\n");
  int fs_ret = fs_init(fs_mem, FS_SIZE);
  if (fs_ret != 0) {
    terminal_writestring("Filesystem initialization failed\n");
    return;
  }
  terminal_writestring("Filesystem initialized successfully\n");
  

  
  thread_init();
  terminal_writestring("Thread system initialized\n");
  scheduler_init();
  terminal_writestring("Scheduler initialized\n");
  // thread_args_t* args = (thread_args_t*)mem_alloc(0);
  // test_args_t *targs = (test_args_t *)mem_alloc(0);
  //ide_initialize(0, 0,0,0,0x0);
  //int exit_code = 0;
  //--------------------TESTS 1-4--------------------//
  // args[0].c = 4;
  // args[0].t = 20;
  // args[0].args = &targs[0];
  // exit_code = thread_create((void*)(test_big_fs), &args[0]);
  // if (exit_code < 0) {
  //   terminal_writestring("Thread (0) creation failed\n");
  //   return;
  // }
  //--------------------TEST 5--------------------//
  // args[0].c = 5;
  // args[0].t = 20;
  // args[0].args = &targs[0];
  // exit_code = thread_create((void*)(test_multi_fs), &args[0]);
  // if (exit_code < 0) {
  //   terminal_writestring("Thread (0) creation failed\n");
  //   return;  targs[1].secs = 0;
  // }
  // args[1].c = 6;
  // args[1].t = 20;
  // args[1].args = &targs[1];
  // exit_code = thread_create((void*)(test_multi_fs), &args[1]);
  // if (exit_code < 0) {
  //   terminal_writestring("Thread (1) creation failed\n");
  //   return;
  // }
  //--------------------END TESTS--------------------//
  //pit_init(20); // Initialize PIT with 20Hz 
  // terminal_writestring("PIT initialized\n");
  //IRQ_clear_mask(0);
  //terminal_initialize();
  //terminal_writestring("Starting scheduler...\n");
  //__asm__ volatile("sti");
  //scheduler_run();


  
  
  //double div0 = 4/0;

  while (1){}
}


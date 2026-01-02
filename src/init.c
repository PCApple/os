#include "include/multiboot.h"
#include "include/mem.h"
#include "include/print.h"
#include "include/scheduler.h"
#include "include/thread.h"
#include "include/gdt.h"
#include "include/fs.h"
#include "include/ide.h"

char fs_buf[FS_SIZE];

void init(unsigned int magic_num, multiboot_info_t* binfo) {
  multiboot_info_t* mmap = binfo;
  terminal_initialize();
  if (magic_num != 0x2BADB002){
    printk("ERROR: invalid magic_num");
    return;
  }
  gdt_init();
  printk("GDT initialized\n");
  printk("Terminal initialized\n");
  int mem_ret = mem_init(mmap);
  if (mem_ret < 0) {
    printk("Memory initialization failed\n");
    return;
  }
  printk("Memory initialization succeeded: total pages = %d\n", mem_ret);
  ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x0);
  printk("IDE initialized\n");
  //PIC_init();
  //printk("PIC initialized\n");
  //idt_init();
  //printk("IDT initialized\n");
  

  void* fs_mem = &fs_buf;
  if (fs_mem == NULL) {
    printk("Filesystem memory allocation failed\n");
    return;
  }
  printk("Filesystem memory allocated\n");
  int fs_ret = fs_init(fs_mem, FS_SIZE);
  if (fs_ret != 0) {
    printk("Filesystem initialization failed\n");
    return;
  }
  printk("Filesystem initialized successfully\n");  
  thread_init();
  printk("Thread system initialized\n");
  scheduler_init();
  printk("Scheduler initialized\n");
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
  //   printk("Thread (0) creation failed\n");
  //   return;
  // }
  //--------------------TEST 5--------------------//
  // args[0].c = 5;
  // args[0].t = 20;
  // args[0].args = &targs[0];
  // exit_code = thread_create((void*)(test_multi_fs), &args[0]);
  // if (exit_code < 0) {
  //   printk("Thread (0) creation failed\n");
  //   return;  targs[1].secs = 0;
  // }
  // args[1].c = 6;
  // args[1].t = 20;
  // args[1].args = &targs[1];
  // exit_code = thread_create((void*)(test_multi_fs), &args[1]);
  // if (exit_code < 0) {
  //   printk("Thread (1) creation failed\n");
  //   return;
  // }
  //--------------------END TESTS--------------------//
  //pit_init(20); // Initialize PIT with 20Hz 
  // printk("PIT initialized\n");
  //IRQ_clear_mask(0);
  //terminal_initialize();
  //printk("Starting scheduler...\n");
  //__asm__ volatile("sti");
  //scheduler_run();


  
  
  //double div0 = 4/0;

  while (1){}
}


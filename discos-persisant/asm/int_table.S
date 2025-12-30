.section .data
.align 4
.globl int_table
.extern exception_handler
.extern irq_handler
int_table:
  .long exception_0 //0
  .long exception_1 //1
  .long exception_2 //2
  .long exception_3 //3
  .long exception_4 //4
  .long exception_5 //5
  .long exception_6 //6
  .long exception_7 //7
  .long exception_8 //8
  .long exception_9 //9
  .long exception_10 //10
  .long exception_11 //11
  .long exception_12 //12
  .long exception_13 //13
  .long exception_14 //14
  .long 0 //15
  .long exception_16 //16
  .long exception_17 //17
  .long exception_18 //18
  .long exception_19 //19
  .long exception_20 // 20
  .long exception_21 // 21
  .long 0 // 22
  .long 0 // 23
  .long 0 // 24
  .long 0 // 25
  .long 0 // 26
  .long 0 // 27
  .long 0 // 28
  .long 0 // 29
  .long 0 // 30
  .long 0 // 31
  .long irq_0 // timer i think
.text
.align 4
exception_0:
pushl $0x0 // error code
  pushal
  pushl $0x0// error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_1:
pushl $0x0 // error code
  pushal
  pushl $0x1 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_2:
pushl $0x0 // error code
  pushal
  pushl $0x2 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_3:
pushl $0x0 // error code
  pushal
  pushl $0x3 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_4:
pushl $0x0 // error code
  pushal
  pushl $0x4 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_5:
pushl $0x0 // error code
  pushal
  pushl $0x5 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_6:
pushl $0x0 // error code
  pushal
  pushl $0x6 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_7:
pushl $0x0 // error code
  pushal
  pushl $0x7 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_8:
pushl $0x0 // error code
pushal
  pushl $0x8 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_9:
pushl $0x0 // error code
  pushal
  pushl $0x9 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_10:
pushal
  pushl $0xa // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_11:
pushal
  pushl $0xb // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_12:
pushal
  pushl $0xc // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_13:
  pushl $0xd // error no
  call exception_handler
  addl $8, %esp 
  iret
exception_14:
pushal
  pushl $0xe // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_16:
pushl $0x0 // error code
  pushal
  pushl $0x10 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_17:
pushal
  pushl $0x11// error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_18:
pushl $0x0 // error code
  pushal
  pushl $0x12 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_19:
pushl $0x0 // error code
  pushal
  pushl $0x13 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_20:
pushl $0x0 // error code
  pushal
  pushl $0x14 // error no
  call exception_handler
  addl $8, %esp
  popal  
  iret
exception_21:
pushl %eax 
  pushl %ecx
  pushl %edx
  pushl $0x15 // error no
  call exception_handler
  addl $8, %esp
  pushl %edx
  pushl %ecx
  pushl %eax  
  iret
irq_0:
  pushl %ebp
  movl %esp, %ebp
  pushl $0x0 // error code
  pushl %eax 
  pushl %ecx
  pushl %edx
  pushl $0x20 // error no
  call irq_handler
  addl $8, %esp
  pushl %edx
  pushl %ecx
  pushl %eax
  addl $8, %esp
  leave
  iret
  
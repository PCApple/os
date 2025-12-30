.globl context_switch
.text
.align 4
context_switch:
  movl 4(%esp), %ecx
  movl 8(%esp), %edx
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  movl %esp, (%ecx) // saving old stack pointer
  movl %edx, %esp  // loading in new stack pointer

  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
.globl first_context_switch
first_context_switch:
    movl 4(%esp), %edx

    // no need to do the first save

    movl %edx, %esp

    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret


.globl lgdt
.text
.align 4
lgdt:
    movl 4(%esp), %eax   // load pointer to GDT descriptor
    lgdt (%eax)         // load GDT
    ljmp $0x08, $1f // reloading cs
1:  movw $0x10, %ax
   	movw %ax, %ds
   	movw %ax, %es
   	movw %ax, %fs
   	movw %ax, %gs
   	movw %ax, %ss
    ret
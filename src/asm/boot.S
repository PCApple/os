/*
 * boot.S - Initial bootstrapping code
 *
 */
	.globl stack
	
	.bss
	.align 0x1000
	.comm stack, 0x1000 /* setup 4KB stack area in bss */
	.comm idt, 0x800 /* 2KB for IDT */


	.data
	.align 4

	.text
	
	.globl _start
	.globl setGdt
	.globl reload_CS

_start:
	jmp real_start

	/* Multiboot header -- Safe to place this header in 1st page for GRUB */
	.align 4
	.long 0x1BADB002 /* Multiboot magic number */
	.long 0x00000003 /* Align modules to 4KB, req. mem size */
			 /* See 'info multiboot' for further info */
	.long 0xE4524FFB /* Checksum */

real_start:
/* set up stack */
	movl $stack+0x1000, %esp /* setup 4Kbyte stack */
	/* save multiboot parameter, for eventual call to C code */

	push %ebx # push boot info pointer
	push %eax #push magic number
	call init /* start of C code */

	/* In case we return from the call, we want to suspend the processor */
	
	cli
	hlt
loop:
	jmp loop

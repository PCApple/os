CC = gcc
CFLAGS = -m32 -fno-builtin -fno-stack-protector -fno-strict-aliasing -fno-delete-null-pointer-checks -nostdinc -I. -g -Wall -std=c99
CPPFLAGS = -Wa,--32 -MMD
OBJS = asm/boot.o init.o mem.o print.o asm/int_table.o int.o io.o pic.o pit.o asm/cxt_switch.o thread.o scheduler.o test_funcs.o heap.o fs.o ide.o gdt.o asm/lgdt.o
PROGS = casos
MNT_POINT=/mnt/C

all: $(PROGS)

casos: $(OBJS)
	$(LD) -m elf_i386 -T casos.ld -o $@ $^

%: %.c

install: $(PROGS)
	cp $(PROGS) $(MNT_POINT)/boot
	sync

clean:
	-rm *.o *.d $(PROGS) *~

-include *.d

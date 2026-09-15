CC = gcc
CFLAGS = -m32 -ffreestanding -fno-builtin -fno-stack-protector -fno-strict-aliasing -fno-delete-null-pointer-checks -I. -g -Wall -std=c99
CPPFLAGS = -Wa,--32 -MMD
# Filesystem and filesystem-dependent tests are temporarily excluded.
OBJS = asm/boot.o init.o mem.o print.o asm/int_table.o int.o pic.o pit.o asm/cxt_switch.o thread.o scheduler.o heap.o ide.o gdt.o asm/lgdt.o utils.o tests.o block_cache.o disk.o
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

.PHONY: test
test:
	$(MAKE) -C ../tests test

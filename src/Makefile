CC = gcc
CFLAGS = -m32 -ffreestanding -fno-builtin -fno-stack-protector -fno-strict-aliasing -fno-delete-null-pointer-checks -I. -g -Wall -std=c99
CPPFLAGS = -Wa,--32 -MMD
include sources.mk
OBJS = $(ASM_SOURCES:.S=.o) $(C_SOURCES:.c=.o)
DEPS = $(OBJS:.o=.d)
OBJDUMP ?= objdump
PROGS = ../casos
MNT_POINT=/mnt/C

all: $(PROGS)

casos: $(PROGS)

$(PROGS): $(OBJS) Makefile sources.mk casos.ld
	$(LD) -m elf_i386 -T casos.ld -o $@ $(OBJS)

# Share the build's source list; preserve directories to avoid name collisions.
disassemble: $(PROGS)
	rm -rf -- dissasm
	@set -e; for obj in $(OBJS) $(PROGS); do \
		output="dissasm/$${obj#../}"; \
		output="$${output%.o}.txt"; \
		mkdir -p "$$(dirname "$$output")"; \
		$(OBJDUMP) -d -s "$$obj" > "$$output"; \
		echo "Disassembled $$obj -> $$output"; \
	done

%: %.c

install: $(PROGS)
	cp $(PROGS) $(MNT_POINT)/boot
	sync

clean:
	find . -type f \( -name '*.o' -o -name '*.d' -o -name '*~' \) -delete
	rm -f -- $(PROGS) casos
	rm -rf -- dissasm

-include $(DEPS)

.PHONY: all casos install clean disassemble test
test:
	$(MAKE) -C ../tests test

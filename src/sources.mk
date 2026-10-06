# Paths are relative to src/. Add or remove files here to select kernel sources.
# Keep legacy filesystem implementations excluded while fs.c is in use.
C_SOURCES := \
    init.c \
    mem.c \
    mutex.c \
    semaphore.c \
    print.c \
    strings.c \
    int.c \
    pic.c \
    pit.c \
    thread.c \
    scheduler.c \
    heap.c \
    ide.c \
    gdt.c \
    utils.c \
    tests.c \
    block_cache.c \
    disk.c \
    fs.c

ASM_SOURCES := \
    asm/boot.S \
    asm/int_table.S \
    asm/cxt_switch.S \
    asm/lgdt.S

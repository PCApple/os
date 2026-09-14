# OS code review — 2026-09-14

The current working tree is an experimental 32-bit kernel with bootstrapping, physical memory allocation, VGA output, interrupt handling, a rate-monotonic scheduler, IDE access, and a filesystem being migrated to disk-backed caches. It is not currently buildable, and independent memory-corruption and persistence defects would remain after fixing compilation.

Reviewed the working tree, including existing modifications to `src/fs.c`, `src/ide.c`, and `src/include/fs.h`. Those modifications were preserved. Findings describe the current snapshot, not necessarily regressions introduced by those edits.

## Validation and limits

- `make -n -B casos` in `src/` fails because `io.o` has no source or build rule.
- A syntax check using the Makefile's C flags fails at `<stdint.h>` because `-nostdinc` removes standard include paths without supplying replacement headers.
- A diagnostic-only check with `gcc -m32 -ffreestanding -fno-builtin -std=c11 -I. -fsyntax-only -fmax-errors=12 init.c fs.c` confirms incompatible filesystem calls. These alternate flags were not applied to the project.
- Temporary host-side harnesses compiled the actual `heap.c` and `lru_cache.c` implementations. The cache harness substitutes allocation and IDE functions; it checks cache logic and arguments, not hardware behavior. It reproduced the second-block loading failure, wrong sector address, ignored read failure, and leaked lock described below. The heap harness reproduced a broken heap invariant.
- No kernel boot or physical disk I/O was performed. Runtime effects outside those harnesses are established by source inspection, not a successful boot. Existing filesystem tests are disabled in the boot path.

## Findings, ordered by repair urgency

### 1. Build blockers and incompatible filesystem interfaces — blocking

Locations: [Makefile](src/Makefile#L2), [boot initialization](src/init.c#L42), [filesystem interface](src/include/fs.h#L95), [allocation caller](src/fs.c#L502), [file opening](src/fs.c#L904).

Besides the missing `io.o` and header search paths, the object list omits `lru_cache.o` and `spinlock.o`. `init()` calls `fs_init(fs_mem, FS_SIZE)`, while the declaration now accepts only a drive number. Numerous callers still use the old allocator and directory lookup signatures; `fs_open()` and `fs_unlink()` reference removed fields and `fs_ptr`.

Establish one consistent filesystem interface and a reproducible freestanding build before interpreting any existing binary as evidence for the current source.

### 2. Allocator truncates physical addresses at 8 MiB — critical

Locations: [address conversion](src/mem.c#L26), [allocation](src/mem.c#L175).

`bit_and_byte_to_addr()` accepts `byte_index` as `uint8_t`, but callers pass `start_page_index / 8`. At page index 2048, representing physical address `0x800000`, the byte index 256 truncates to zero. The allocator marks the intended page allocated but writes its header at address zero and returns address 4. Subsequent allocations can alias lower memory and overwrite kernel state.

Use a full-width index or compute the address directly from the page index. Separately, `total_pages` is the count of free pages, but the allocation loop treats it as the extent of the physical-address-indexed bitmap. This excludes valid high pages. Track bitmap extent and free-page count separately.

### 3. Filesystem block helper never returns its buffer to the caller — critical

Locations: [block helper](src/fs.c#L19), [inode helper](src/fs.c#L96).

`fs_get_and_lock_block(int, void *buffer)` assigns the cache address only to its local parameter. Callers pass uninitialized pointer variables such as `sb` and `inode_block`; successful returns leave those variables unchanged, followed by invalid dereferences. This affects essentially every migrated filesystem operation.

Return the pointer directly or use a pointer-to-pointer output argument. Define whether the inode helper returns a cached mutable inode or copies into caller-owned storage; currently it copies through pointers that callers have not initialized.

### 4. Filesystem blocks are sent to IDE as byte offsets instead of sector numbers — critical

Locations: [disk read adapter](src/lru_cache.c#L11), [disk write adapter](src/lru_cache.c#L46).

The adapter calculates `index * BLOCK_SIZE` and passes that directly as the IDE LBA. With 256-byte blocks and 512-byte sectors, block 1 belongs to sector 0 at offset 256, but the harness observed LBA 256. This reads and writes unrelated sectors and eventually exceeds the intended filesystem area.

Calculate sector number as byte offset divided by sector size, and the within-sector offset using the remainder. Serialize read-modify-write operations for two blocks sharing a sector when concurrent writes are enabled.

### 5. IDE commands do not program the requested sector count — high

Location: [ATA request setup](src/ide.c#L196).

`numsects` controls the software transfer loop, but the command setup never writes `ATA_REG_SECCOUNT0`. Only the high count register is written for LBA48. The device can therefore execute a different transfer length than the caller expects, leaving a partially completed command and compromising later transfers.

Program the low sector count before issuing each command. Polling loops also need bounded failure behavior; currently a permanently busy drive can hang initialization or I/O indefinitely.

### 6. Multi-entry caches stop after their first distinct block — high, reproduced

Location: [empty-entry search](src/lru_cache.c#L290).

The loop tests `curr_entry->present` repeatedly without advancing `curr_entry`. Once slot zero is occupied, every further miss in a cache with unused capacity returns NULL. Harness result: first load succeeded, second distinct load failed, cache size remained `1/3`.

Index `cache->items[i]` or advance the pointer each iteration.

### 7. Cache failures can lose data or permanently hold the lock — high, partly reproduced

Locations: [release](src/lru_cache.c#L236), [eviction and loading](src/lru_cache.c#L270).

On a read failure, cache loading still publishes a valid entry. On dirty eviction, a failed write is ignored and the only cached copy is overwritten. Separately, releasing a missing block returns `-1` without unlocking the cache. The harness confirmed a returned buffer despite an injected disk error and confirmed the lock remained held after a missing-key release.

Propagate disk errors, preserve dirty entries until writes succeed, and release locks on every exit path. Cache initialization should also check each allocation before dereferencing it.

### 8. Timer interrupt handler fails to restore interrupted registers — critical when enabled

Location: [IRQ0 assembly](src/asm/int_table.S#L205).

IRQ0 saves EAX, ECX, and EDX, calls C, then pushes those registers again instead of restoring their saved values. `leave` restores the stack frame, but the interrupted code resumes with the C handler's caller-clobbered register values. This can corrupt ordinary execution on any timer tick.

Use a balanced interrupt save/restore frame. Audit exception stubs at the same time: several place saved registers between the C arguments and the CPU error code, so the reported error code is actually a saved register; their cleanup is also inconsistent.

### 9. Interrupt masking is not restored on all exits — high

Locations: [root opening](src/fs.c#L902), [task admission](src/scheduler.c#L177).

The successful root branch of `fs_open("/")` returns without reversing its `cli`; several error branches do the same. `scheduler_create_task()` returns on admission failure before its `popfl`. When scheduling is active, these paths can permanently stop timer preemption. Unconditional `sti` elsewhere can also enable interrupts inside a caller's critical section.

Save and restore the prior interrupt state through one cleanup path, including failures.

### 10. Heap removal breaks priority ordering — high, reproduced

Location: [heap repair](src/heap.c#L23).

`heapify_helper()` swaps once but never continues down the affected subtree. Inserting priorities 1 through 7 and removing the minimum produced `[2, 7, 3, 4, 5, 6]`: priority 7 is above children 4 and 5. The scheduler can consequently select tasks out of priority order.

Implement complete sift-down. Also reconcile `MAX_HEAP_SIZE = 1024` with the scheduler's 64-element backing array; scanning for a sentinel in a full scheduler queue can read beyond its array.

### 11. Scheduler removes tasks using indices invalidated by queue updates — high

Location: [timer accounting](src/scheduler.c#L218).

The tick handler saves heap indices for tasks whose budgets expire, then calls `scheduler_check_wait_queue()`, which inserts awakened tasks and reorders the heap. Removal then uses the old indices. When a task wakes on the same tick another exhausts its budget, the wrong task can enter the wait queue.

Keep task identities rather than mutable heap indices, or finish removals before inserting awakened tasks. Task slots are also never reused after cleanup because `tcb_count` only increases, eventually preventing further thread creation.

### 12. Filesystem table supports fewer threads than the scheduler — high

Locations: [descriptor table](src/fs.c#L5), [descriptor access](src/fs.c#L920), [scheduler capacity](src/include/scheduler.h#L15).

The filesystem allocates tables for 16 threads but directly indexes them by a scheduler TID. The scheduler permits 64 tasks. A filesystem operation from TID 16 or greater accesses unrelated memory.

Associate descriptors with a validated task slot or store the table in the TCB. Do not assume a monotonically assigned TID is a bounded array index.

### 13. Directory and filename processing accesses beyond buffers — high

Locations: [directory traversal](src/fs.c#L247), [filename extraction](src/fs.c#L424), [string copy](src/print.c#L157).

Directory loops use `entries[index].filename[0] != '\0' || index < limit`. They necessarily evaluate an out-of-bounds element at the block boundary and can continue beyond it. Bounds must be checked before reading the element, with the correct continuation condition.

`read_filename()` can copy 14 characters into a 14-byte array and then write a terminator at index 14. Reject overlong names or reserve the terminator byte. Several public operations also inspect `path[strlen(path)-1]` without rejecting empty paths.

The custom `strcpy()` reads the destination's existing length and copies only the smaller of the two lengths, without guaranteeing a terminator. Copying into an empty buffer copies nothing; copying into uninitialized storage first reads uninitialized memory. Replace this with a capacity-aware copying contract and fix callers.

### 14. Block freeing uses a different bitmap mapping from allocation — high

Location: [block freeing](src/fs.c#L198).

Allocation packs 2048 block bits into each 256-byte bitmap block. Freeing instead divides by `DATA_BLOCKS / BITMAP_BLOCKS`, which is 1982. For data block 1982, allocation uses bitmap block zero while freeing uses bitmap block one. It can free a different live block or reject the free, causing leaks and eventual corruption.

Use `BLOCK_SIZE * 8` for bitmap block capacity and derive byte/bit offsets from the remainder. Validate block indices before subtracting the data-region base.

### 15. Filesystem migration lacks a complete mount/create/read/write path — blocking

Locations: [mount initialization](src/fs.c#L787), [creation lookup](src/fs.c#L599), [inode selection](src/fs.c#L618), [reading](src/fs.c#L1039).

`fs_init()` allocates caches but leaves filesystem validation, formatting/root initialization, and its return statement commented out. Creation returns an error when a name does not exist, while an existing name is rejected as a duplicate. Inode lookup returns a positive block index on success, but creation treats any nonzero result as a reason to skip that inode. Read/write paths still cast disk block numbers to RAM addresses.

These are substantive unfinished behaviors, not just signature mismatches. Define a complete block-index format and ownership contract, then finish mount, create, read, write, and flush together. Validate an existing image rather than interpreting arbitrary bytes as metadata.

## Project state and next steps

The subsystem split and filesystem tests provide a useful starting point. The tests cover direct, indirect, and double-indirect files, directory traversal, and multiple writers. They currently run only if manually enabled in `init.c`; there is no automated boot-to-result check. The checked-in boot path also disables PIC/IDT/PIT setup and scheduler execution, then spins forever.

Recommended sequence:

1. Restore a clean build with explicit toolchain/header requirements and all required objects.
2. Fix physical address conversion and interrupt register/state preservation before enabling preemption.
3. Fix block-to-sector translation, ATA transfer setup, and cache failure handling; verify them with a fake disk.
4. Complete one filesystem transaction path and verify write, flush, restart, and read-back on a disposable image.
5. Fix heap and scheduler lifecycle invariants; exercise simultaneous wakeup/budget expiry and more than 16 TIDs.
6. Turn existing kernel tests into a repeatable emulator run with a machine-readable pass/fail result. Add failure cases for full caches, failed disk writes, allocation exhaustion, malformed paths, and bitmap boundaries.

User mode, virtual memory, syscalls, a shell, SMP, and 64-bit support remain roadmap items rather than implemented capabilities. Stabilizing the existing kernel and persistence path should precede that expansion.

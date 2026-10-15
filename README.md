My operating system

Run `./build.sh` from the repository root to build `casos` in that root and generate
disassembly in `src/dissasm/`. If the boot image is mounted at `/mnt/casos`,
the script also copies the kernel into its boot directory. Run `./run.sh`
to launch QEMU using the disk images in `src/`.

Run `./clean.sh` to remove object files, dependency files, editor backups,
disassembly, and the built `casos` binary. Source files and disk images are kept.

Edit `src/sources.mk` to choose which files are built: list C files under
`C_SOURCES` and assembly `.S` files under `ASM_SOURCES`, with paths relative
to `src/`. Object files and disassembly are derived from those lists.
Changes to the lists trigger a relink, including when a source is removed.

For individual steps, use `make -C src casos` or `make -C src disassemble`.

Run `make -C src test` for the filesystem lookup tests (host GCC on Linux,
no disk images or QEMU required). They exercise the real `fs.c` and `strings.c`
with an in-memory cache, checking exact results, sparse direct/indirect/double
indirect blocks, filename boundaries, parent traversal across inode blocks,
cache failures, balanced releases, and preservation of cached data. Cases run
sequentially in one process, resetting the fixture between cases. Assertion
failures continue to the next case; crashes stop the process. A cache-call limit
catches repeated lookup loops; there is no timeout.

Directory lookup returns an inode index, with `-1` for failure. Parent lookup
returns a status and fills an inode handle; tests verify the inode and owning
block, require one outstanding cache acquisition on success, and release that
block as the caller. Additional cases check independent handles and simulate
directory data becoming unavailable immediately on release.

To debug, run `gdb tests/.build/fs_lookup`, then `break fs_lookup_parent` and
`run`. To stop at one specific case, use e.g. `break parent if which == 2`.
No fork or signal configuration is needed.

Run `make -C tests coverage` for per-function line coverage and an annotated
branch report in `tests/fs.c.gcov`, or `make -C tests sanitize` for AddressSanitizer
and UndefinedBehaviorSanitizer checks. Coverage reports still generate when
tests fail. Parent lookup expectations follow `fs_create`: absolute paths with
a nonempty leaf and no trailing slash; the leaf itself need not exist. These
are regression tests for intended behavior, so existing lookup bugs fail them.
The cache stub keeps blocks resident; real cache eviction is outside this suite.

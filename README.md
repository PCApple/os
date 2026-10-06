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

#!/bin/bash
set -e

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

make casos
echo "built casos"
./dumpall.sh clean
./dumpall.sh disassemble
if mountpoint -q /mnt/casos && [[ -d /mnt/casos/boot ]]; then
    sudo cp casos /mnt/casos/boot/casos
    sync
    echo "copied casos to /mnt/casos/boot/casos"
else
    echo "Build complete: $PWD/casos"
    echo "Skipped boot-disk copy: mount your boot image at /mnt/casos with a boot directory, then rerun this script."
fi

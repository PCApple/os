#!/bin/bash
set -e

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd -- "$repo_dir/src"

make casos disassemble
echo "built casos"
if mountpoint -q /mnt/casos && [[ -d /mnt/casos/boot ]]; then
    sudo cp "$repo_dir/casos" /mnt/casos/boot/casos
    sync
    echo "copied casos to /mnt/casos/boot/casos"
else
    echo "Build complete: $repo_dir/casos"
    echo "Skipped boot-disk copy: mount your boot image at /mnt/casos with a boot directory, then rerun this script."
fi

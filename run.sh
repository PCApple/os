#!/bin/bash
set -e

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/src"

sudo qemu-system-i386 -hda os.img -hdb data.img -m 256M -s -S &

#!/bin/bash
set -e

make casos
echo "built casos"
./dumpall.sh clean
./dumpall.sh disassemble
sudo cp casos /mnt/os2/boot
echo "copied casos"
sync
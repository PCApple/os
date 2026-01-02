#!/bin/bash
set -e

make casos
echo "built casos"
./dumpall.sh clean
./dumpall.sh disassemble
sudo cp casos /mnt/casos/boot
echo "copied casos"
sync
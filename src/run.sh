#!/bin/bash
sudo qemu-system-i386 -hda disk.img -hdb data.img -m 256M -s -S &
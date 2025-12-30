#!/bin/bash

pids=$(pgrep -x qemu-system-i386)

if [ -z "$pids" ]; then
    echo "No qemu-system-i386 process found."
else
    echo "Killing qemu-system-i386 process(es): $pids"
    kill $pids
    # kill -9 $pids
fi
#!/bin/bash

# List of object files
OBJS="asm/boot.o init.o mem.o fs.o print.o asm/int_table.o int.o io.o pic.o pit.o asm/cxt_switch.o thread.o scheduler.o test_funcs.o heap.o ide.o gdt.o asm/lgdt.o utils.o"

# Output directory (optional)
OUTDIR="./dissasm"
mkdir -p "$OUTDIR"

# Function to disassemble all object files
disassemble() {
    mkdir -p "$OUTDIR"
    for obj in $OBJS; do
        if [[ -f "$obj" ]]; then
            base=$(basename "$obj" .o)
            objdump -d -s "$obj" > "$OUTDIR/${base}.txt"
            echo "Disassembled $obj -> $OUTDIR/${base}.txt"
        else
            echo "Warning: $obj not found, skipping."
        fi
    done
}

# Function to clean generated output
clean() {
    if [[ -d "$OUTDIR" ]]; then
        rm -rf "$OUTDIR"
        echo "Cleaned output directory: $OUTDIR"
    else
        echo "Nothing to clean."
    fi
}

# Main logic
case "$1" in
    clean)
        clean
        ;;
    *)
        disassemble
        ;;
esac
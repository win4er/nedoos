#!/bin/bash

echo "=== Building NedoOS Kernel ==="

# Clean
rm -f myos.kernel
find . -name "*.o" -delete
find . -name "*.d" -delete

# Compile C files
echo "Compiling C files..."
for file in arch/i386/*.c drivers/*.c kernel/debug/*.c mm/*.c task/*.c kernel/*.c; do
    if [ -f "$file" ]; then
        echo "  $file"
        i686-elf-gcc -c "$file" -o "${file%.c}.o" -std=gnu11 -O2 -g \
            -ffreestanding -Wall -Wextra -Werror \
            -D__is_kernel \
            -Iinclude -Iinclude/kernel -Iarch/i386 -Idrivers -Imm -Itask -I. \
            -I../libc/include
    fi
done

# Compile Assembly files
echo "Compiling Assembly files..."
for file in arch/i386/*.S task/*.S; do
    if [ -f "$file" ] && [[ "$file" != *"crt"* ]]; then
        echo "  $file"
        i686-elf-gcc -c "$file" -o "${file%.S}.o" -O2 -g \
            -ffreestanding -Wall -Wextra -Werror \
            -D__is_kernel \
            -Iinclude -Iinclude/kernel -Iarch/i386 -Idrivers -Imm -Itask -I. \
            -I../libc/include
    fi
done

# Compile CRT files
echo "Compiling CRT files..."
i686-elf-gcc -c arch/i386/crti.S -o arch/i386/crti.o
i686-elf-gcc -c arch/i386/crtn.S -o arch/i386/crtn.o

# Make sure isr_asm.S is compiled
if [ -f arch/i386/isr_asm.S ]; then
    echo "Compiling isr_asm.S"
    i686-elf-gcc -c arch/i386/isr_asm.S -o arch/i386/isr_asm.o
else
    echo "ERROR: isr_asm.S not found!"
    exit 1
fi

# Link everything
echo "Linking kernel..."
i686-elf-gcc -T arch/i386/linker.ld -o myos.kernel \
    arch/i386/crti.o \
    /usr/lib/gcc/i686-elf/11.2.0/crtbegin.o \
    arch/i386/boot.o \
    arch/i386/gdt.o \
    arch/i386/gdt_flush.o \
    arch/i386/idt.o \
    arch/i386/isr.o \
    arch/i386/isr_asm.o \
    arch/i386/pic.o \
    arch/i386/tty.o \
    drivers/keyboard.o \
    drivers/timer.o \
    kernel/debug/debug.o \
    mm/pmm.o \
    mm/vmm.o \
    task/scheduler.o \
    task/switch.o \
    kernel/kernel.o \
    -L/home/tetsu/projects/nedoos/main/sysroot/usr/lib -lk \
    -nostdlib -lgcc \
    /usr/lib/gcc/i686-elf/11.2.0/crtend.o \
    arch/i386/crtn.o

if [ -f myos.kernel ]; then
    echo "✅ SUCCESS! Kernel compiled successfully!"
    file myos.kernel
    echo "Creating ISO and running in QEMU..."
    cd ..
    ./iso.sh
    ./qemu.sh
else
    echo "❌ ERROR: Kernel compilation failed"
    exit 1
fi

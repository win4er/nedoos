#!/bin/bash

# Компилируем isr_asm.S если его нет
if [ ! -f arch/i386/isr_asm.o ]; then
    echo "Compiling isr_asm.S..."
    i686-elf-gcc -c arch/i386/isr_asm.S -o arch/i386/isr_asm.o \
        -Iinclude -Iinclude/kernel
fi

# Линкуем со всеми объектными файлами
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
    cd .. && ./iso.sh && ./qemu.sh
else
    echo "❌ ERROR: Kernel compilation failed"
fi

#!/bin/bash

echo "=== NedoOS Kernel Full Rebuild ==="
cd /home/tetsu/projects/nedoos/main

# Полная очистка
echo "Cleaning..."
rm -rf sysroot isodir
find . -name "*.o" -delete
find . -name "*.d" -delete
find . -name "*.a" -delete
find . -name "*.kernel" -delete

# Сборка libc
echo "Building libc..."
cd libc
make clean
make CC=i686-elf-gcc AR=i686-elf-ar
mkdir -p ../sysroot/usr/lib
cp libk.a ../sysroot/usr/lib/
cd ..

# Установка заголовков
echo "Installing headers..."
./headers.sh

# Компиляция isr_asm.S отдельно
echo "Compiling isr_asm.S..."
cd kernel/arch/i386
i686-elf-gcc -c isr_asm.S -o isr_asm.o -I../../include -I../../include/kernel
cd ../..

# Сборка ядра
echo "Building kernel..."
cd kernel
make clean
make CC=i686-elf-gcc

# Проверка результата
if [ -f myos.kernel ]; then
    echo "✅ SUCCESS! Kernel compiled successfully!"
    file myos.kernel
    
    # Создание ISO и запуск
    echo "Creating ISO and running in QEMU..."
    cd ..
    ./iso.sh
    ./qemu.sh
else
    echo "❌ ERROR: Kernel compilation failed"
    echo "Attempting manual link..."
    
    # Ручная линковка с isr_asm.o
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
        echo "✅ Manual link successful!"
        file myos.kernel
        cd ..
        ./iso.sh
        ./qemu.sh
    else
        echo "❌ Manual link failed"
        
        # Проверка наличия объектных файлов
        echo "Checking object files:"
        ls -la arch/i386/*.o
        ls -la drivers/*.o 2>/dev/null
        ls -la kernel/debug/*.o 2>/dev/null
        ls -la mm/*.o 2>/dev/null
        ls -la task/*.o 2>/dev/null
        
        # Проверка символов в isr_asm.o
        echo "Symbols in isr_asm.o:"
        i686-elf-nm arch/i386/isr_asm.o | head -20
    fi
fi

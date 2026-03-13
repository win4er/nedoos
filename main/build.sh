#!/bin/bash

echo "=== Сборка NedoOS ==="

# Очистка
rm -rf isodir *.o *.kernel *.iso boot.o kernel.o myos.kernel

# Компиляция
echo "Компиляция boot.S..."
i686-elf-as kernel/arch/i386/boot.S -o boot.o

echo "Компиляция kernel.c..."
i686-elf-gcc -c kernel/kernel.c -o kernel.o -ffreestanding -nostdlib

# Линковка
echo "Линковка ядра..."
i686-elf-ld -T kernel/arch/i386/linker.ld -o myos.kernel boot.o kernel.o

# Создание диска
echo "Создание диска hd.img..."
dd if=/dev/zero of=hd.img bs=512 count=2880 2>/dev/null
mkfs.fat -F 12 hd.img >/dev/null 2>&1

# Тестовые файлы
echo "Hello World" > test.txt
mcopy -i hd.img test.txt ::test.txt 2>/dev/null

# Создание ISO
echo "Создание ISO..."
mkdir -p isodir/boot/grub
cp myos.kernel isodir/boot/
echo "menuentry \"NedoOS\" { multiboot /boot/myos.kernel; }" > isodir/boot/grub/grub.cfg
grub-mkrescue -o myos.iso isodir 2>/dev/null

echo "Готово! Запуск: qemu-system-i386 -kernel myos.kernel -drive file=hd.img,format=raw,if=ide"

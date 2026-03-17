#!/bin/bash

echo "=== Сборка NedoOS с сетью ==="

# Очистка
rm -rf isodir *.o *.kernel *.iso

# Компиляция boot.S
i686-elf-as kernel/arch/i386/boot.S -o boot.o

# Компиляция kernel.c
i686-elf-gcc -c kernel/kernel.c -o kernel.o -ffreestanding -nostdlib -Ikernel/include -Ikernel

# Компиляция драйвера E1000
i686-elf-gcc -c kernel/drivers/e1000.c -o e1000.o -ffreestanding -nostdlib -Ikernel/include -Ikernel

# Компиляция сети
i686-elf-gcc -c kernel/net/network.c -o network.o -ffreestanding -nostdlib -Ikernel/include -Ikernel

# Линковка
i686-elf-ld -T kernel/arch/i386/linker.ld -o myos.kernel \
    boot.o kernel.o e1000.o network.o

# Создание диска
dd if=/dev/zero of=hd.img bs=1M count=10 2>/dev/null
mkfs.fat -F 16 hd.img >/dev/null 2>&1

# Создание ISO
mkdir -p isodir/boot/grub
cp myos.kernel isodir/boot/
echo 'menuentry "NedoOS" { multiboot /boot/myos.kernel; }' > isodir/boot/grub/grub.cfg
grub-mkrescue -o myos.iso isodir 2>/dev/null

echo "✅ Готово!"
echo "Запуск: qemu-system-i386 -cdrom myos.iso -netdev user,id=net0 -device e1000,netdev=net0"

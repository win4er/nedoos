#!/bin/bash

echo "=== Сборка NedoOS с сетью ==="

# Очистка
rm -rf isodir *.o *.elf *.iso

# Компиляция
i686-elf-as kernel/arch/i386/boot.S -o boot.o
i686-elf-gcc -c kernel/kernel.c -o kernel.o -ffreestanding -nostdlib -Ikernel
i686-elf-gcc -c kernel/drivers/e1000.c -o e1000.o -ffreestanding -nostdlib -Ikernel
i686-elf-gcc -c kernel/net/network.c -o network.o -ffreestanding -nostdlib -Ikernel

# Линковка
i686-elf-ld -T kernel/arch/i386/linker.ld -o myos.elf \
    boot.o kernel.o e1000.o network.o

# Проверка мультизагрузки
if grub-file --is-x86-multiboot myos.elf; then
    echo "✅ MULTIBOOT OK"
else
    echo "❌ MULTIBOOT FAIL"
    exit 1
fi

# Создание ISO
mkdir -p isodir/boot/grub
cp myos.elf isodir/boot/myos.kernel
cat > isodir/boot/grub/grub.cfg << 'EOF'
set timeout=5
set default=0
menuentry "NedoOS" {
    multiboot /boot/myos.kernel
}
EOF

grub-mkrescue -o myos.iso isodir

echo "✅ Готово! Запуск: qemu-system-i386 -cdrom myos.iso -netdev user,id=net0 -device e1000,netdev=net0"

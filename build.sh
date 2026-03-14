cd /home/tetsu/projects/nedoos/main

# Удаляем реализации задач из kernel.c
sed -i '/void task_init(void) {/,/^}/d' kernel/kernel.c
sed -i '/int task_create(void/,/^}/d' kernel/kernel.c
sed -i '/void task_yield(void) {/,/^}/d' kernel/kernel.c

# Пересобираем
rm -f *.o myos.kernel
i686-elf-as kernel/arch/i386/boot.S -o boot.o
i686-elf-gcc -c kernel/kernel.c -o kernel.o -ffreestanding -nostdlib -Ikernel
i686-elf-gcc -c kernel/task.c -o task.o -ffreestanding -nostdlib -Ikernel
i686-elf-as kernel/arch/i386/switch.S -o switch.o
i686-elf-ld -T kernel/arch/i386/linker.ld -o myos.kernel boot.o kernel.o task.o switch.o

# Запуск
qemu-system-i386 -kernel myos.kernel -drive file=hd.img,format=raw,if=ide

# Получаем хост и архитектуру
DEFAULT_HOST := $(shell ../default-host.sh)
HOST ?= $(DEFAULT_HOST)
HOSTARCH := $(shell ../target-triplet-to-arch.sh $(HOST))
ARCHDIR := arch/$(HOSTARCH)

# Директории установки
DESTDIR ?=
PREFIX ?= /usr/local
EXEC_PREFIX ?= $(PREFIX)
BOOTDIR ?= $(EXEC_PREFIX)/boot
INCLUDEDIR ?= $(PREFIX)/include

# Компилятор и флаги
CFLAGS ?= -O2 -g
CPPFLAGS ?=
LDFLAGS ?=
LIBS ?=

CFLAGS := $(CFLAGS) -ffreestanding -Wall -Wextra -Werror
# Найдите строку с CPPFLAGS и измените:
CPPFLAGS := $(CPPFLAGS) -D__is_kernel -Iinclude -Iinclude/kernel -I$(ARCHDIR) -Idrivers -Imm -Itask -I. -I../libc/include
LDFLAGS := $(LDFLAGS) -ffreestanding -nostdlib
LIBS := $(LIBS) -nostdlib -lgcc

# Подключаем архитектурный конфиг
include $(ARCHDIR)/make.config

# Добавляем архитектурные флаги
CFLAGS := $(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS := $(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS := $(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS := $(LIBS) $(KERNEL_ARCH_LIBS)

# Объектные файлы - все указываем явно
ARCH_OBJS = \
$(ARCHDIR)/boot.o \
$(ARCHDIR)/gdt.o \
$(ARCHDIR)/gdt_flush.o \
$(ARCHDIR)/idt.o \
$(ARCHDIR)/isr.o \
$(ARCHDIR)/pic.o \
$(ARCHDIR)/tty.o

DRIVER_OBJS = \
drivers/keyboard.o \
drivers/timer.o

DEBUG_OBJS = \
kernel/debug/debug.o 

MM_OBJS = \
mm/pmm.o \
mm/vmm.o

TASK_OBJS = \
task/scheduler.o \
task/switch.o

KERNEL_OBJS = \
$(ARCH_OBJS) \
$(DRIVER_OBJS) \
$(DEBUG_OBJS) \
$(MM_OBJS) \
$(TASK_OBJS) \
kernel/kernel.o

# CRT объекты
CRTI_OBJ = $(ARCHDIR)/crti.o
CRTN_OBJ = $(ARCHDIR)/crtn.o

# Пытаемся найти crtbegin/crtend, но не падаем если их нет
CRTBEGIN_OBJ := $(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o 2>/dev/null || echo "")
CRTEND_OBJ := $(shell $(CC) $(CFLAGS) -print-file-name=crtend.o 2>/dev/null || echo "")

ifneq ($(CRTBEGIN_OBJ),)
    CRTBEGIN_OBJ_COPIED = $(ARCHDIR)/crtbegin.o
else
    CRTBEGIN_OBJ_COPIED =
endif

ifneq ($(CRTEND_OBJ),)
    CRTEND_OBJ_COPIED = $(ARCHDIR)/crtend.o
else
    CRTEND_OBJ_COPIED =
endif

# Все объекты
OBJS = \
$(CRTI_OBJ) \
$(CRTBEGIN_OBJ_COPIED) \
$(KERNEL_OBJS) \
$(CRTEND_OBJ_COPIED) \
$(CRTN_OBJ)

# Список для линковки
LINK_LIST = \
$(CRTI_OBJ) \
$(CRTBEGIN_OBJ_COPIED) \
$(KERNEL_OBJS) \
-L/home/tetsu/projects/nedoos/main/sysroot/usr/lib -lk \
$(LIBS) \
$(CRTEND_OBJ_COPIED) \
$(CRTN_OBJ)

# Цели
.PHONY: all clean install install-headers install-kernel

all: myos.kernel

myos.kernel: $(OBJS) $(ARCHDIR)/linker.ld
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST) $(LDFLAGS)
	@if command -v grub-file >/dev/null 2>&1; then \
		grub-file --is-x86-multiboot $@ && echo "Multiboot confirmed" || echo "Not a multiboot kernel!"; \
	fi

# Правила компиляции
%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

%.o: %.S
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

# Копирование crtbegin.o и crtend.o
$(ARCHDIR)/crtbegin.o:
	@if [ -n "$(CRTBEGIN_OBJ)" ]; then \
		echo "Copying $(CRTBEGIN_OBJ) to $@"; \
		cp "$(CRTBEGIN_OBJ)" $@; \
	else \
		echo "Warning: crtbegin.o not found, skipping"; \
		touch $@; \
	fi

$(ARCHDIR)/crtend.o:
	@if [ -n "$(CRTEND_OBJ)" ]; then \
		echo "Copying $(CRTEND_OBJ) to $@"; \
		cp "$(CRTEND_OBJ)" $@; \
	else \
		echo "Warning: crtend.o not found, skipping"; \
		touch $@; \
	fi

# Очистка
clean:
	rm -f myos.kernel
	rm -f $(OBJS) $(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o
	find . -name "*.o" -delete
	find . -name "*.d" -delete

# Установка
install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)/kernel
	cp -R include/* $(DESTDIR)$(INCLUDEDIR)/ 2>/dev/null || true
	# Копируем заголовки из поддиректорий
	for dir in debug drivers mm task; do \
		if [ -d "$$dir" ]; then \
			cp $$dir/*.h $(DESTDIR)$(INCLUDEDIR)/kernel/ 2>/dev/null || true; \
		fi \
	done

install-kernel: myos.kernel
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp myos.kernel $(DESTDIR)$(BOOTDIR)

# Подключаем зависимости
-include $(OBJS:.o=.d)

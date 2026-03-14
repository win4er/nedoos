ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* Сначала мультизагрузочный заголовок */
    .multiboot ALIGN(4) : {
        *(.multiboot)
    }

    /* Затем код */
    .text ALIGN(4) : {
        *(.text)
    }

    /* Данные */
    .rodata ALIGN(4) : {
        *(.rodata)
    }

    .data ALIGN(4) : {
        *(.data)
    }

    /* BSS */
    .bss ALIGN(4) : {
        *(COMMON)
        *(.bss)
    }
}

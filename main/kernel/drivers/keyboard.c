#include <stdint.h>
#include <stdio.h>
#include <kernel/io.h>      // для outb/inb
#include <kernel/regs.h>    // для struct regs
#include "keyboard.h"

// US keyboard layout scancode set 1
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static const char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

#define KEYBOARD_BUFFER_SIZE 256

static volatile char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile int buffer_head = 0;
static volatile int buffer_tail = 0;
static volatile int shift_pressed = 0;
static volatile int ctrl_pressed = 0;
static volatile int alt_pressed = 0;

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// IRQ handler for keyboard
static void keyboard_handler(struct regs *r) {
    (void)r;  // Параметр не используется
    
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    // Handle special keys
    switch(scancode) {
        case 0x2A: case 0x36:  // Shift pressed
            shift_pressed = 1;
            return;
        case 0xAA: case 0xB6:  // Shift released
            shift_pressed = 0;
            return;
        case 0x1D:  // Ctrl pressed
            ctrl_pressed = 1;
            return;
        case 0x9D:  // Ctrl released
            ctrl_pressed = 0;
            return;
        case 0x38:  // Alt pressed
            alt_pressed = 1;
            return;
        case 0xB8:  // Alt released
            alt_pressed = 0;
            return;
    }
    
    // Key release - ignore
    if (scancode & 0x80) {
        return;
    }
    
    // Convert scancode to ASCII
    char c = 0;
    if (scancode < sizeof(scancode_to_ascii)) {
        if (shift_pressed) {
            c = scancode_to_ascii_shift[scancode];
        } else {
            c = scancode_to_ascii[scancode];
        }
    }
    
    // Add to buffer if it's a printable character
    if (c) {
        int next_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
        if (next_head != buffer_tail) {  // Buffer not full
            keyboard_buffer[buffer_head] = c;
            buffer_head = next_head;
        }
    }
}

char keyboard_getchar(void) {
    while (buffer_head == buffer_tail) {
        // Wait for keypress (interrupts will happen)
        asm volatile("hlt");
    }
    
    char c = keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

int keyboard_getchar_nonblock(void) {
    if (buffer_head == buffer_tail) {
        return -1;  // No key available
    }
    
    char c = keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

void keyboard_clear_buffer(void) {
    buffer_head = buffer_tail = 0;
}

// Объявление внешней функции регистрации обработчиков
extern void irq_register_handler(int irq, void (*handler)(struct regs *));

void keyboard_install(void) {
    printf("Installing keyboard driver...\n");
    
    // Register IRQ handler (IRQ1 = keyboard)
    irq_register_handler(1, keyboard_handler);
    
    // Clear the buffer
    keyboard_clear_buffer();
    
    printf("Keyboard driver installed\n");
}

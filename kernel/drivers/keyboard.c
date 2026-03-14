#include "keyboard.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

static unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static char get_scancode(void) {
    return inb(KEYBOARD_DATA_PORT);
}

static char scancode_to_char(char scancode) {
    // US раскладка
    const char layout[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    };
    
    if (scancode < sizeof(layout)) {
        return layout[scancode];
    }
    return 0;
}

char keyboard_getchar(void) {
    char scancode;
    char c;
    
    do {
        while ((inb(KEYBOARD_STATUS_PORT) & 1) == 0);
        scancode = get_scancode();
        c = scancode_to_char(scancode);
    } while (c == 0);
    
    return c;
}

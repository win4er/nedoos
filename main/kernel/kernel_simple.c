// Простейшее ядро без зависимостей от libc
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static uint16_t* terminal_buffer = VGA_MEMORY;
static int terminal_row = 0;
static int terminal_column = 0;
static uint8_t terminal_color = 0x07; // серый на чёрном

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void terminal_initialize(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
        return;
    }
    
    terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(c, terminal_color);
    terminal_column++;
    if (terminal_column >= VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
    }
}

void terminal_writestring(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

void kernel_main(void) {
    terminal_initialize();
    terminal_writestring("NedoOS is alive!");
    
    while (1) {
        __asm__ volatile("hlt");
    }
}

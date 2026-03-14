// NedoOS - Многозадачность
#include "kernel.h"
#include "task.h"

// VGA
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static uint16_t* video = VGA_MEMORY;
static int row = 0;
static int col = 0;
static uint8_t current_color = 0x07;

void set_color(uint8_t fg) {
    current_color = fg | (0x00 << 4);
}

void newline(void) {
    col = 0;
    row++;
    if (row >= VGA_HEIGHT) {
        for (int i = 0; i < (VGA_HEIGHT-1) * VGA_WIDTH; i++) {
            video[i] = video[i + VGA_WIDTH];
        }
        for (int i = (VGA_HEIGHT-1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            video[i] = ' ' | (current_color << 8);
        }
        row = VGA_HEIGHT - 1;
    }
}

void putchar(char c) {
    if (c == '\n') { newline(); return; }
    if (c == '\b' && col > 0) { col--; video[row * VGA_WIDTH + col] = ' ' | (current_color << 8); return; }
    video[row * VGA_WIDTH + col] = (uint16_t)c | ((uint16_t)current_color << 8);
    col++;
    if (col >= VGA_WIDTH) newline();
}

void print(const char* s) {
    for (int i = 0; s[i]; i++) putchar(s[i]);
}

void print_dec(uint32_t num) {
    char buf[12];
    int i = 0;
    if (num == 0) { putchar('0'); return; }
    while (num) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    while (i) putchar(buf[--i]);
}

// I/O
uint8_t inb(uint16_t port) {
    uint8_t res;
    __asm__ volatile("inb %1, %0" : "=a"(res) : "Nd"(port));
    return res;
}

uint16_t inw(uint16_t port) {
    uint16_t res;
    __asm__ volatile("inw %1, %0" : "=a"(res) : "Nd"(port));
    return res;
}

void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Keyboard
#define KEY_DATA 0x60
#define KEY_STATUS 0x64

static int shift = 0;
static int caps = 0;

char getchar(void) {
    uint8_t sc;
    
    while ((inb(KEY_STATUS) & 1) == 0);
    sc = inb(KEY_DATA);
    
    if (sc == 0x2A || sc == 0x36) { shift = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { shift = 0; return 0; }
    if (sc == 0x3A) { caps = !caps; return 0; }
    if (sc & 0x80) return 0;
    
    const char small[] = {
        0,0,'1','2','3','4','5','6','7','8','9','0','-','=', '\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
        0,'a','s','d','f','g','h','j','k','l',';','\'','`',
        0,'\\','z','x','c','v','b','n','m',',','.','/',0,
        '*',0,' '
    };
    
    const char big[] = {
        0,0,'!','@','#','$','%','^','&','*','(',')','_','+', '\b',
        '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
        0,'A','S','D','F','G','H','J','K','L',':','"','~',
        0,'|','Z','X','C','V','B','N','M','<','>','?',0,
        '*',0,' '
    };
    
    if (sc < sizeof(small)) {
        if (shift ^ caps) return big[sc];
        return small[sc];
    }
    return 0;
}

// String
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

void strcpy(char* dest, const char* src) {
    while (*src) *dest++ = *src++;
    *dest = 0;
}

// Timer
static volatile uint32_t timer_ticks = 0;

void timer_handler(void) {
    timer_ticks++;
    task_yield();
}

void init_timer(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    print("Timer initialized\n");
}

void sleep(uint32_t ticks) {
    uint32_t target = timer_ticks + ticks;
    while (timer_ticks < target) {
        __asm__ volatile("hlt");
    }
}

// Тестовые задачи
void test_task1(void) {
    int counter = 0;
    while (1) {
        set_color(COLOR_RED);
        print("Task1: ");
        print_dec(counter++);
        print("\n");
        for (volatile int i = 0; i < 1000000; i++);
    }
}

void test_task2(void) {
    int counter = 100;
    while (1) {
        set_color(COLOR_GREEN);
        print("Task2: ");
        print_dec(counter++);
        print("\n");
        for (volatile int i = 0; i < 1000000; i++);
    }
}

// ATA
int ata_read(uint32_t lba, uint8_t* buf) {
    for (int i = 0; i < 1000; i++) {
        if ((inb(0x1F7) & 0xC0) == 0x40) break;
    }
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x20);
    for (int i = 0; i < 1000; i++) {
        if (inb(0x1F7) & 0x08) break;
    }
    for (int i = 0; i < 256; i++) {
        ((uint16_t*)buf)[i] = inw(0x1F0);
    }
    return 0;
}

int ata_write(uint32_t lba, uint8_t* buf) {
    for (int i = 0; i < 1000; i++) {
        if ((inb(0x1F7) & 0xC0) == 0x40) break;
    }
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30);
    for (int i = 0; i < 1000; i++) {
        if (inb(0x1F7) & 0x08) break;
    }
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ((uint16_t*)buf)[i]);
    }
    return 0;
}

// MAIN
void kernel_main(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        video[i] = ' ' | (current_color << 8);
    }
    row = 0; col = 0;
    
    set_color(COLOR_GREEN);
    print("NedoOS Multitasking\n");
    
    // Инициализация таймера
    init_timer(100);
    
    // Инициализация задач
    task_init();
    task_create(test_task1, "task1");
    task_create(test_task2, "task2");
    
    set_color(COLOR_CYAN);
    print("Commands: help, mount, ls, cat, cd, pwd, clear\n> ");
    
    char cmd[256];
    int pos = 0;
    
    while (1) {
        char c = getchar();
        
        if (c == '\n') {
            print("\n");
            cmd[pos] = 0;
            
            char* p = cmd;
            while (*p == ' ') p++;
            
            if (strcmp(p, "help") == 0) {
                set_color(COLOR_YELLOW);
                print("  mount   - mount disk\n");
                print("  ls      - list files\n");
                print("  cat     - view file\n");
                print("  cd      - change directory\n");
                print("  pwd     - print path\n");
                print("  clear   - clear screen\n");
            }
            else if (strcmp(p, "mount") == 0) {
                // TODO: mount
                print("Mount not implemented\n");
            }
            else if (strcmp(p, "ls") == 0) {
                // TODO: ls
                print("LS not implemented\n");
            }
            else if (strcmp(p, "clear") == 0) {
                for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
                    video[i] = ' ' | (current_color << 8);
                }
                row = 0; col = 0;
            }
            else if (p[0]) {
                set_color(COLOR_RED);
                print("Unknown: ");
                print(p);
                print("\n");
            }
            
            set_color(COLOR_GREY);
            print("> ");
            pos = 0;
        }
        else if (c == '\b' && pos > 0) {
            pos--;
            putchar('\b');
        }
        else if (c >= ' ' && c <= '~' && pos < 255) {
            cmd[pos++] = c;
            putchar(c);
        }
    }
}

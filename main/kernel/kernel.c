// NedoOS - Мега-версия со всеми фичами
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

// ==================== VGA ====================
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static uint16_t* video = VGA_MEMORY;
static int row = 0;
static int col = 0;
static uint8_t color = 0x07;

// Цвета
#define COLOR_BLACK   0x00
#define COLOR_BLUE    0x01
#define COLOR_GREEN   0x02
#define COLOR_CYAN    0x03
#define COLOR_RED     0x04
#define COLOR_MAGENTA 0x05
#define COLOR_BROWN   0x06
#define COLOR_GREY    0x07
#define COLOR_DARKGREY 0x08
#define COLOR_LIGHTBLUE 0x09
#define COLOR_LIGHTGREEN 0x0A
#define COLOR_LIGHTCYAN 0x0B
#define COLOR_LIGHTRED 0x0C
#define COLOR_LIGHTMAGENTA 0x0D
#define COLOR_YELLOW  0x0E
#define COLOR_WHITE   0x0F

static void set_color(uint8_t fg, uint8_t bg) {
    color = fg | (bg << 4);
}

static void newline(void) {
    col = 0;
    row++;
    if (row >= VGA_HEIGHT) {
        for (int i = 0; i < (VGA_HEIGHT-1) * VGA_WIDTH; i++) {
            video[i] = video[i + VGA_WIDTH];
        }
        for (int i = (VGA_HEIGHT-1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            video[i] = ' ' | (color << 8);
        }
        row = VGA_HEIGHT - 1;
    }
}

static void putchar_color(char c, uint8_t color) {
    if (c == '\n') { newline(); return; }
    if (c == '\b' && col > 0) { col--; video[row * VGA_WIDTH + col] = ' ' | (color << 8); return; }
    video[row * VGA_WIDTH + col] = (uint16_t)c | ((uint16_t)color << 8);
    col++;
    if (col >= VGA_WIDTH) newline();
}

static void putchar(char c) {
    putchar_color(c, color);
}

static void print_color(const char* s, uint8_t color) {
    for (int i = 0; s[i]; i++) putchar_color(s[i], color);
}

static void print(const char* s) {
    print_color(s, color);
}

static void print_dec(uint32_t num) {
    char buf[12];
    int i = 0;
    if (num == 0) { putchar('0'); return; }
    while (num) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    while (i) putchar(buf[--i]);
}

// ==================== I/O ====================
static uint8_t inb(uint16_t port) {
    uint8_t res;
    __asm__ volatile("inb %1, %0" : "=a"(res) : "Nd"(port));
    return res;
}

static uint16_t inw(uint16_t port) {
    uint16_t res;
    __asm__ volatile("inw %1, %0" : "=a"(res) : "Nd"(port));
    return res;
}

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// ==================== KEYBOARD ====================
#define KEY_DATA 0x60
#define KEY_STATUS 0x64

static int shift = 0;
static int caps = 0;

static char getchar(void) {
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

// ==================== STRING ====================
static int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

static void strcpy(char* dest, const char* src) {
    while (*src) *dest++ = *src++;
    *dest = 0;
}

// ==================== REBOOT ====================
static void reboot(void) {
    print_color("\nRebooting...\n", COLOR_YELLOW);
    // Ждём нажатия клавиши
    while (!(inb(0x64) & 1));
    inb(0x60);
    // Отправляем команду ресета
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    __asm__ volatile("hlt");
}

// ==================== ATA ====================
static int ata_read(uint32_t lba, uint8_t* buf) {
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

// ==================== FAT ====================
typedef struct {
    uint8_t name[8];
    uint8_t ext[3];
    uint8_t attr;
    uint8_t reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t cluster;
    uint32_t size;
} __attribute__((packed)) fat_entry_t;

static int mounted = 0;
static uint32_t root_sector = 0;
static uint32_t data_sector = 0;

static void mount(void) {
    uint8_t sec[512];
    print_color("\nMounting...\n", COLOR_YELLOW);
    if (ata_read(0, sec)) { print_color("ERR\n", COLOR_RED); return; }
    if (sec[54] == 'F' && sec[55] == 'A' && sec[56] == 'T') {
        uint16_t reserved = *(uint16_t*)(sec + 14);
        uint8_t fat_cnt = sec[16];
        uint16_t root_entries = *(uint16_t*)(sec + 17);
        uint16_t sectors_per_fat = *(uint16_t*)(sec + 22);
        uint32_t fat_start = reserved;
        uint32_t root_start = fat_start + fat_cnt * sectors_per_fat;
        uint32_t root_size = (root_entries * 32 + 511) / 512;
        root_sector = root_start;
        data_sector = root_start + root_size;
        mounted = 1;
        print_color("OK\n", COLOR_GREEN);
    } else print_color("Not FAT\n", COLOR_RED);
}

static void ls(void) {
    if (!mounted) { print_color("mount first\n", COLOR_RED); return; }
    uint8_t sec[512];
    fat_entry_t* e;
    if (ata_read(root_sector, sec)) return;
    for (int i = 0; i < 16; i++) {
        e = (fat_entry_t*)(sec + i*32);
        if (e->name[0] == 0) break;
        if (e->name[0] == 0xE5) continue;
        print_color("  ", COLOR_GREEN);
        for (int j = 0; j < 8 && e->name[j] != ' '; j++) putchar(e->name[j]);
        if (e->ext[0] != ' ') {
            putchar('.');
            for (int j = 0; j < 3 && e->ext[j] != ' '; j++) putchar(e->ext[j]);
        }
        print(" (");
        print_dec(e->size);
        print(")\n");
    }
}

static void cat(const char* fname) {
    if (!mounted) { print_color("mount first\n", COLOR_RED); return; }
    
    while (*fname == ' ') fname++;
    if (*fname == 0) { print_color("Usage: cat filename\n", COLOR_YELLOW); return; }
    
    uint8_t sec[512];
    fat_entry_t* e;
    uint16_t cluster = 0;
    uint32_t size = 0;
    
    char search[13];
    int si = 0;
    for (int i = 0; fname[i] && i < 12; i++) {
        char c = fname[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        search[si++] = c;
    }
    search[si] = 0;
    
    if (ata_read(root_sector, sec)) return;
    
    for (int i = 0; i < 16; i++) {
        e = (fat_entry_t*)(sec + i*32);
        if (e->name[0] == 0) break;
        if (e->name[0] == 0xE5) continue;
        
        char fatname[13];
        int fi = 0;
        for (int j = 0; j < 8 && e->name[j] != ' '; j++) fatname[fi++] = e->name[j];
        if (e->ext[0] != ' ') {
            fatname[fi++] = '.';
            for (int j = 0; j < 3 && e->ext[j] != ' '; j++) fatname[fi++] = e->ext[j];
        }
        fatname[fi] = 0;
        
        int match = 1;
        for (int j = 0; j < si; j++) {
            if (j >= fi || search[j] != fatname[j]) { match = 0; break; }
        }
        if (match && si == fi) {
            cluster = e->cluster;
            size = e->size;
            break;
        }
    }
    
    if (!cluster) { print_color("Not found\n", COLOR_RED); return; }
    
    uint32_t sector = data_sector + (cluster - 2);
    if (ata_read(sector, sec)) return;
    
    print("\n");
    for (uint32_t i = 0; i < size && i < 512; i++) {
        putchar(sec[i]);
    }
    print("\n");
}

// ==================== ECHO ====================
static void echo(const char* text) {
    while (*text == ' ') text++;
    if (*text) {
        print("\n");
        print(text);
        print("\n");
    } else {
        print("\n");
    }
}

// ==================== COLOR TEST ====================
static void color_test(void) {
    print("\n");
    set_color(COLOR_RED, COLOR_BLACK);     print(" Красный ");
    set_color(COLOR_GREEN, COLOR_BLACK);   print(" Зеленый ");
    set_color(COLOR_YELLOW, COLOR_BLACK);  print(" Желтый ");
    set_color(COLOR_BLUE, COLOR_BLACK);    print(" Синий ");
    set_color(COLOR_MAGENTA, COLOR_BLACK); print(" Пурпурный ");
    set_color(COLOR_CYAN, COLOR_BLACK);    print(" Голубой ");
    set_color(COLOR_WHITE, COLOR_BLACK);   print(" Белый ");
    set_color(COLOR_GREY, COLOR_BLACK);    print("\n");
}

// ==================== MAIN ====================
void kernel_main(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        video[i] = ' ' | (color << 8);
    }
    row = 0; col = 0;
    set_color(COLOR_GREEN, COLOR_BLACK);
    print("NedoOS Mega Edition\n");
    set_color(COLOR_GREY, COLOR_BLACK);
    print("Commands: help, mount, ls, cat, echo, color, reboot, clear\n");
    print("> ");
    
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
                set_color(COLOR_YELLOW, COLOR_BLACK);
                print("\n  mount  - mount FAT disk\n");
                print("  ls     - list files\n");
                print("  cat    - view file\n");
                print("  echo   - print text\n");
                print("  color  - show colors\n");
                print("  reboot - restart system\n");
                print("  clear  - clear screen\n");
                set_color(COLOR_GREY, COLOR_BLACK);
            }
            else if (strcmp(p, "mount") == 0) mount();
            else if (strcmp(p, "ls") == 0) ls();
            else if (p[0] == 'c' && p[1] == 'a' && p[2] == 't') {
                if (p[3] == ' ') cat(p + 4);
                else set_color(COLOR_RED, COLOR_BLACK); print("Use: cat filename\n"); set_color(COLOR_GREY, COLOR_BLACK);
            }
            else if (p[0] == 'e' && p[1] == 'c' && p[2] == 'h' && p[3] == 'o') {
                echo(p + 4);
            }
            else if (strcmp(p, "color") == 0) {
                color_test();
            }
            else if (strcmp(p, "reboot") == 0) {
                reboot();
            }
            else if (strcmp(p, "clear") == 0) {
                for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
                    video[i] = ' ' | (color << 8);
                }
                row = 0; col = 0;
            }
            else if (p[0]) {
                set_color(COLOR_RED, COLOR_BLACK);
                print("Unknown: ");
                print(p);
                print("\n");
                set_color(COLOR_GREY, COLOR_BLACK);
            }
            
            pos = 0;
            print("> ");
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

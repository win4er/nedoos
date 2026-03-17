#include "kernel.h"

// Прототипы функций
void path_join(char* dest, const char* a, const char* b);
uint16_t fat_alloc_cluster(void);
int ata_write(uint32_t lba, uint8_t* buf);
#include <stdint.h>

// Прототипы функций
void path_join(char* dest, const char* a, const char* b);
uint16_t fat_alloc_cluster(void);
int ata_write(uint32_t lba, uint8_t* buf);
#include <stddef.h>

// Прототипы функций
void path_join(char* dest, const char* a, const char* b);
uint16_t fat_alloc_cluster(void);
int ata_write(uint32_t lba, uint8_t* buf);

// ==================== АТРИБУТЫ FAT ====================
#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_VFAT       0x0F

// ==================== ENV ====================
#define MAX_ENV 32
#define ENV_NAME_LEN 32
#define ENV_VALUE_LEN 128
#define MAX_MATCHES 64

static char env_names[MAX_ENV][ENV_NAME_LEN];
static char env_values[MAX_ENV][ENV_VALUE_LEN];
static int env_count = 0;

// ==================== VGA ====================
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static uint16_t* video = VGA_MEMORY;
static int row = 0;
static int col = 0;
static uint8_t current_color = 0x07;

// Цвета
#define COLOR_RED     0x04
#define COLOR_GREEN   0x02
#define COLOR_YELLOW  0x0E
#define COLOR_CYAN    0x03
#define COLOR_GREY    0x07

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

// ==================== I/O ====================
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

// ==================== ВИРТУАЛЬНЫЕ ТЕРМИНАЛЫ ====================
#define MAX_TERMINALS 4
typedef struct {
    uint16_t buffer[VGA_WIDTH * VGA_HEIGHT];
    int row, col;
    uint8_t color;
    char cmd[256];
    int pos;
} terminal_t;

static terminal_t terminals[MAX_TERMINALS];
static int current_terminal = 0;

void vt_init(void) {
    for (int t = 0; t < MAX_TERMINALS; t++) {
        terminals[t].row = 0;
        terminals[t].col = 0;
        terminals[t].color = 0x07;
        terminals[t].pos = 0;
        terminals[t].cmd[0] = 0;
        for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
            terminals[t].buffer[i] = ' ' | (0x07 << 8);
        }
    }
}

void vt_save_current(void) {
    terminals[current_terminal].row = row;
    terminals[current_terminal].col = col;
    terminals[current_terminal].color = current_color;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        terminals[current_terminal].buffer[i] = video[i];
    }
}

void vt_restore(int term) {
    row = terminals[term].row;
    col = terminals[term].col;
    current_color = terminals[term].color;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        video[i] = terminals[term].buffer[i];
    }
}

void vt_switch(int term) {
    if (term < 0 || term >= MAX_TERMINALS || term == current_terminal) return;
    vt_save_current();
    current_terminal = term;
    vt_restore(term);
}

// ==================== KEYBOARD ====================
#define KEY_DATA 0x60
#define KEY_STATUS 0x64

static int shift = 0;
static int caps = 0;
static int alt = 0;
static int ctrl = 0;

// Для истории команд
#define MAX_HISTORY 16
#define MAX_CMD_LEN 256
static char history[MAX_HISTORY][MAX_CMD_LEN];
static int history_count = 0;
static int history_pos = -1;

// Для мыши
static int mouse_x = 40;
static int mouse_y = 12;
static int mouse_old_x = 40;
static int mouse_old_y = 12;
static int mouse_buttons = 0;
static int mouse_present = 0;
static uint16_t mouse_bg = 0;

char getchar(void) {
    uint8_t sc;
    
    while ((inb(KEY_STATUS) & 1) == 0);
    sc = inb(KEY_DATA);
    
    if (sc == 0x38) { alt = 1; return 0; }
    if (sc == 0xB8) { alt = 0; return 0; }
    if (sc == 0x1D) { ctrl = 1; return 0; }
    if (sc == 0x9D) { ctrl = 0; return 0; }
    if (sc == 0x2A || sc == 0x36) { shift = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { shift = 0; return 0; }
    if (sc == 0x3A) { caps = !caps; return 0; }
    
    if (sc == 0x3B && alt) { vt_switch(0); return 0; }
    if (sc == 0x3C && alt) { vt_switch(1); return 0; }
    if (sc == 0x3D && alt) { vt_switch(2); return 0; }
    if (sc == 0x3E && alt) { vt_switch(3); return 0; }
    
    if (sc == 0x48 && alt) return 0x80;
    if (sc == 0x50 && alt) return 0x81;
    
    if (sc == 0x0F) return '\t';
    
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

// ==================== MOUSE ====================
void mouse_init(void) {
    outb(0x64, 0xA8);
    outb(0x64, 0x20);
    uint8_t status = inb(0x60);
    status |= 2;
    outb(0x64, 0x60);
    outb(0x60, status);
    
    outb(0x64, 0xD4);
    outb(0x60, 0xF4);
    
    mouse_present = 1;
    mouse_old_x = mouse_x;
    mouse_old_y = mouse_y;
    mouse_bg = video[mouse_y * VGA_WIDTH + mouse_x];
    print("Mouse initialized\n");
}

void mouse_handler(void) {
    static int cycle = 0;
    static uint8_t mouse_data[3];
    
    mouse_data[cycle] = inb(0x60);
    cycle = (cycle + 1) % 3;
    
    if (cycle == 0) {
        video[mouse_old_y * VGA_WIDTH + mouse_old_x] = mouse_bg;
        
        mouse_buttons = mouse_data[0] & 7;
        int dx = (int)mouse_data[1];
        if (mouse_data[1] & 0x80) dx -= 256;
        int dy = -(int)mouse_data[2];
        if (mouse_data[2] & 0x80) dy -= 256;
        
        mouse_old_x = mouse_x;
        mouse_old_y = mouse_y;
        
        mouse_x += dx / 2;
        mouse_y += dy / 2;
        
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= VGA_WIDTH) mouse_x = VGA_WIDTH - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= VGA_HEIGHT) mouse_y = VGA_HEIGHT - 1;
        
        mouse_bg = video[mouse_y * VGA_WIDTH + mouse_x];
        video[mouse_y * VGA_WIDTH + mouse_x] = 0xDB0F;
    }
}

// ==================== STRING ====================
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

void strcpy(char* dest, const char* src) {
    while (*src) *dest++ = *src++;
    *dest = 0;
}

int strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

int strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (a[i] == 0) return 0;
    }
    return 0;
}

// ==================== ИСТОРИЯ КОМАНД ====================
void history_add(const char* cmd) {
    if (cmd[0] == 0) return;
    
    if (history_count > 0 && strcmp(history[history_count-1], cmd) == 0)
        return;
    
    strcpy(history[history_count % MAX_HISTORY], cmd);
    history_count++;
    history_pos = history_count;
}

const char* history_get_prev(void) {
    if (history_count == 0) return NULL;
    if (history_pos > 0) history_pos--;
    return history[history_pos % MAX_HISTORY];
}

const char* history_get_next(void) {
    if (history_count == 0) return NULL;
    history_pos++;
    if (history_pos >= history_count) history_pos = history_count - 1;
    return history[history_pos % MAX_HISTORY];
}

// ==================== TIMER ====================
static volatile uint32_t timer_ticks = 0;

void timer_handler(void) {
    timer_ticks++;
}

void init_timer(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    print("Timer initialized\n");
}

// ==================== СЕТЕВЫЕ ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ====================

// ==================== FAT STRUCTURES ====================
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
static uint32_t fat_sector = 0;
static uint32_t data_sector = 0;
static uint16_t bytes_per_sector = 512;
static uint8_t sectors_per_cluster = 1;
static uint16_t root_entries = 0;

static uint32_t current_dir_cluster = 0;
static char current_path[256] = "/";

// ==================== ATA ====================
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

// ==================== MOUNT ====================
void mount(void) {
    uint8_t sec[512];
    print("\nMounting...\n");
    if (ata_read(0, sec)) { print("ERR\n"); return; }
    
    if (sec[54] == 'F' && sec[55] == 'A' && sec[56] == 'T') {
        uint16_t reserved = *(uint16_t*)(sec + 14);
        uint8_t fat_cnt = sec[16];
        root_entries = *(uint16_t*)(sec + 17);
        uint16_t sectors_per_fat = *(uint16_t*)(sec + 22);
        bytes_per_sector = *(uint16_t*)(sec + 11);
        sectors_per_cluster = sec[13];
        
        fat_sector = reserved;
        uint32_t root_start = fat_sector + fat_cnt * sectors_per_fat;
        uint32_t root_size = (root_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;
        root_sector = root_start;
        data_sector = root_start + root_size;
        mounted = 1;
        current_dir_cluster = 0;
        strcpy(current_path, "/");
        print("OK\n");
    } else {
        print("Not FAT\n");
    }
}

// ==================== LS ====================
void ls(void) {
    if (!mounted) { print("mount first\n"); return; }
    
    uint8_t sec[512];
    fat_entry_t* e;
    uint32_t dir_sector;
    int entries_per_sector = bytes_per_sector / 32;
    
    if (current_dir_cluster == 0) {
        dir_sector = root_sector;
    } else {
        dir_sector = data_sector + (current_dir_cluster - 2) * sectors_per_cluster;
    }
    
    if (ata_read(dir_sector, sec)) return;
    
    for (int i = 0; i < entries_per_sector; i++) {
        e = (fat_entry_t*)(sec + i * 32);
        
        if (e->name[0] == 0) break;
        if (e->name[0] == 0xE5) continue;
        
        if (e->attr & ATTR_DIRECTORY) set_color(COLOR_CYAN);
        else set_color(COLOR_GREY);
        
        for (int j = 0; j < 8 && e->name[j] != ' '; j++) putchar(e->name[j]);
        if (e->ext[0] != ' ') {
            putchar('.');
            for (int j = 0; j < 3 && e->ext[j] != ' '; j++) putchar(e->ext[j]);
        }
        
        if (e->attr & ATTR_DIRECTORY) print("  <DIR>");
        
        print(" (");
        print_dec(e->size);
        print(")\n");
    }
}

// ==================== CAT ====================
void cat(const char* fname) {
    if (!mounted) { print("mount first\n"); return; }
    
    while (*fname == ' ') fname++;
    if (*fname == 0) { print("Use: cat filename\n"); return; }
    
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
    
    uint32_t dir_sector = (current_dir_cluster == 0) ? root_sector : 
                          data_sector + (current_dir_cluster - 2) * sectors_per_cluster;
    
    if (ata_read(dir_sector, sec)) return;
    
    int entries_per_sector = bytes_per_sector / 32;
    for (int i = 0; i < entries_per_sector; i++) {
        e = (fat_entry_t*)(sec + i * 32);
        if (e->name[0] == 0) break;
        if (e->name[0] == 0xE5) continue;
        if (e->attr & ATTR_DIRECTORY) continue;
        
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
    
    if (!cluster) { print("Not found\n"); return; }
    
    uint32_t sector = data_sector + (cluster - 2) * sectors_per_cluster;
    if (ata_read(sector, sec)) return;
    
    print("\n");
    for (uint32_t i = 0; i < size && i < bytes_per_sector; i++) {
        putchar(sec[i]);
    }
    print("\n");
}

// ==================== CD ====================
void cd(const char* dirname) {
    if (!mounted) { print("mount first\n"); return; }
    
    while (*dirname == ' ') dirname++;
    if (*dirname == 0) {
        current_dir_cluster = 0;
        strcpy(current_path, "/");
        return;
    }
    
    uint8_t sec[512];
    fat_entry_t* e;
    
    char search[9];
    int si = 0;
    for (int i = 0; dirname[i] && i < 8; i++) {
        char c = dirname[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        search[si++] = c;
    }
    search[si] = 0;
    
    uint32_t dir_sector = (current_dir_cluster == 0) ? root_sector : 
                          data_sector + (current_dir_cluster - 2) * sectors_per_cluster;
    
    if (ata_read(dir_sector, sec)) return;
    
    int entries_per_sector = bytes_per_sector / 32;
    for (int i = 0; i < entries_per_sector; i++) {
        e = (fat_entry_t*)(sec + i * 32);
        if (e->name[0] == 0) break;
        if (e->name[0] == 0xE5) continue;
        if (!(e->attr & ATTR_DIRECTORY)) continue;
        
        int match = 1;
        for (int j = 0; j < si; j++) {
            if (e->name[j] != search[j]) { match = 0; break; }
        }
        
        if (match) {
            current_dir_cluster = e->cluster;
            path_join(current_path, current_path, dirname);
            return;
        }
    }
    
    print("Directory not found\n");
}

// ==================== PWD ====================
void pwd(void) {
    print(current_path);
    print("\n");
}

// ==================== ECHO ====================
void echo(const char* text) {
    while (*text == ' ') text++;
    if (*text) {
        print("\n");
        print(text);
        print("\n");
    }
}

// ==================== COLOR TEST ====================
void color_test(void) {
    print("\n");
    set_color(COLOR_RED);     print(" Red ");
    set_color(COLOR_GREEN);   print(" Green ");
    set_color(COLOR_YELLOW);  print(" Yellow ");
    set_color(COLOR_CYAN);    print(" Cyan ");
    set_color(COLOR_GREY);    print("\n");
}

// ==================== REBOOT ====================
void reboot(void) {
    print("\nRebooting...\n");
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    __asm__ volatile("hlt");
}

// ==================== ПЕРЕМЕННЫЕ ОКРУЖЕНИЯ ====================
void env_set(const char* name, const char* value) {
    for (int i = 0; i < env_count; i++) {
        if (strcmp(env_names[i], name) == 0) {
            strcpy(env_values[i], value);
            return;
        }
    }
    
    if (env_count < MAX_ENV) {
        strcpy(env_names[env_count], name);
        strcpy(env_values[env_count], value);
        env_count++;
    }
}

void env_list(void) {
    for (int i = 0; i < env_count; i++) {
        print(env_names[i]);
        print("=");
        print(env_values[i]);
        print("\n");
    }
}

void export(const char* cmd) {
    char name[ENV_NAME_LEN];
    char value[ENV_VALUE_LEN];
    int i = 0;
    
    while (*cmd == ' ') cmd++;
    
    while (*cmd && *cmd != '=' && i < ENV_NAME_LEN-1) {
        name[i++] = *cmd++;
    }
    name[i] = 0;
    
    if (*cmd != '=') {
        print("Use: export NAME=value\n");
        return;
    }
    cmd++;
    
    i = 0;
    while (*cmd && i < ENV_VALUE_LEN-1) {
        value[i++] = *cmd++;
    }
    value[i] = 0;
    
    env_set(name, value);
}

// ==================== ПАКЕТНЫЙ МЕНЕДЖЕР ====================
void pkg_install(const char* pkg_name) {
    print("Installing package: ");
    print(pkg_name);
    print("\n");
}

void pkg_list(void) {
    print("Available packages:\n");
    print("  base     - Base system\n");
    print("  dev      - Development tools\n");
    print("  games    - Simple games\n");
    print("  utils    - Utilities\n");
}

void pkg_remove(const char* pkg_name) {
    print("Removing package: ");
    print(pkg_name);
    print("\n");
}

// ==================== ЗВУК (PC SPEAKER) ====================
void beep(int frequency, int duration) {
    outb(0x61, inb(0x61) | 3);
    
    uint32_t div = 1193180 / frequency;
    outb(0x43, 0xB6);
    outb(0x42, div & 0xFF);
    outb(0x42, (div >> 8) & 0xFF);
    
    for (volatile int i = 0; i < duration * 10000; i++);
    
    outb(0x61, inb(0x61) & 0xFC);
}

// ==================== РЕДАКТОР ТЕКСТА ====================
void edit(const char* filename) {
    print("\n=== EDITOR ===\n");
    print("Ctrl+S - save, Ctrl+Q - quit\n");
    
    char buffer[4096];
    int pos = 0;
    
    if (mounted) {
        uint8_t sec[512];
        fat_entry_t* e;
        uint16_t cluster = 0;
        uint32_t size = 0;
        uint32_t dir_sector = (current_dir_cluster == 0) ? root_sector : 
                              data_sector + (current_dir_cluster - 2) * sectors_per_cluster;
        
        if (ata_read(dir_sector, sec) == 0) {
            char search[13];
            int si = 0;
            for (int i = 0; filename[i] && i < 12; i++) {
                char c = filename[i];
                if (c >= 'a' && c <= 'z') c -= 32;
                search[si++] = c;
            }
            search[si] = 0;
            
            int entries_per_sector = bytes_per_sector / 32;
            for (int i = 0; i < entries_per_sector; i++) {
                e = (fat_entry_t*)(sec + i * 32);
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
        }
        
        if (cluster) {
            uint32_t sector = data_sector + (cluster - 2) * sectors_per_cluster;
            ata_read(sector, sec);
            for (uint32_t i = 0; i < size && i < 4096; i++) {
                buffer[i] = sec[i];
            }
            pos = size;
            print(buffer);
        }
    }
    
    while (1) {
        char c = getchar();
        
        if (c == 0x13 && ctrl) {
            if (!mounted) { print("\nNot mounted\n"); continue; }
            uint16_t cluster = fat_alloc_cluster();
            if (cluster) {
                uint8_t data[512];
                for (int i = 0; i < 512; i++) data[i] = 0;
                for (int i = 0; i < pos && i < 512; i++) data[i] = buffer[i];
                uint32_t sector = data_sector + (cluster - 2) * sectors_per_cluster;
                ata_write(sector, data);
                print("\nSaved\n");
            }
        }
        else if (c == 0x11 && ctrl) {
            print("\n");
            break;
        }
        else if (c == '\b' && pos > 0) {
            pos--;
            putchar('\b');
        }
        else if (c >= ' ' && c <= '~' && pos < 4095) {
            buffer[pos++] = c;
            putchar(c);
        }
    }
}

// ==================== USB ====================
void usb_init(void) {
    print("USB Controller init...\n");
    outb(0x64, 0xD4);
}

// ==================== PATH ====================
void path_join(char* dest, const char* a, const char* b) {
    int i = 0, j = 0;
    while (a[i]) { dest[i] = a[i]; i++; }
    if (i > 0 && dest[i-1] != '/') dest[i++] = '/';
    while (b[j]) { dest[i++] = b[j++]; }
    dest[i] = 0;
}

// ==================== FAT HELPERS ====================
static uint16_t fat_get_next_cluster(uint16_t cluster) {
    uint32_t fat_offset = cluster * 3 / 2;
    uint32_t fat_sec = fat_sector + fat_offset / bytes_per_sector;
    uint32_t fat_off = fat_offset % bytes_per_sector;
    
    uint8_t buf[512];
    if (ata_read(fat_sec, buf)) return 0xFF8;
    
    if (cluster & 1) {
        return (*(uint16_t*)(buf + fat_off) >> 4) & 0x0FFF;
    } else {
        return *(uint16_t*)(buf + fat_off) & 0x0FFF;
    }
}

static void fat_set_next_cluster(uint16_t cluster, uint16_t next) {
    uint32_t fat_offset = cluster * 3 / 2;
    uint32_t fat_sec = fat_sector + fat_offset / bytes_per_sector;
    uint32_t fat_off = fat_offset % bytes_per_sector;
    
    uint8_t buf[512];
    ata_read(fat_sec, buf);
    
    if (cluster & 1) {
        *(uint16_t*)(buf + fat_off) = (*(uint16_t*)(buf + fat_off) & 0x000F) | (next << 4);
    } else {
        *(uint16_t*)(buf + fat_off) = (*(uint16_t*)(buf + fat_off) & 0xF000) | next;
    }
    
    ata_write(fat_sec, buf);
}

uint16_t fat_alloc_cluster(void) {
    for (uint16_t i = 2; i < 512; i++) {
        if (fat_get_next_cluster(i) == 0) {
            fat_set_next_cluster(i, 0xFF8);
            return i;
        }
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

// ==================== MKDIR, WRITE, RM, RMDIR ====================
void mkdir(const char* dirname) {
    if (!mounted) { print("mount first\n"); return; }
    print("MKDIR not fully implemented\n");
}

void write_file(const char* filename, const char* content) {
    if (!mounted) { print("mount first\n"); return; }
    print("WRITE not fully implemented\n");
}

void rm(const char* filename) {
    if (!mounted) { print("mount first\n"); return; }
    print("RM not fully implemented\n");
}

void rmdir(const char* dirname) {
    if (!mounted) { print("mount first\n"); return; }
    print("RMDIR not fully implemented\n");
}

// ==================== СЕТЕВЫЕ ФУНКЦИИ (ПРОТОТИПЫ) ====================
void network_init(void);
void network_handler(void);
void ping(const char* ip_str);

// ==================== MAIN ====================
void kernel_main(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        video[i] = ' ' | (current_color << 8);
    }
    row = 0; col = 0;
    
    set_color(COLOR_GREEN);
    print("NedoOS Ultimate Edition with Network\n");
    
    vt_init();
    init_timer(100);
    mouse_init();
    usb_init();
    network_init();
    
    env_set("PATH", "/");
    env_set("HOME", "/");
    env_set("USER", "user");

    set_color(COLOR_CYAN);
    print("Commands: help, mount, ls, cat, cd, pwd, echo, color, reboot, clear, ping\n");
    print("Alt+F1..F4 - terminals | Alt+Up/Down - history\n> ");
    
    char cmd[256];
    int pos = 0;
    
    while (1) {
        char c = getchar();
        network_handler();
        
        if (c == '\n') {
            print("\n");
            cmd[pos] = 0;
            
            if (pos > 0) history_add(cmd);
            
            char* p = cmd;
            while (*p == ' ') p++;
            
            if (strcmp(p, "help") == 0) {
                set_color(COLOR_YELLOW);
                print("  mount   - mount FAT disk\n");
                print("  ls      - list files\n");
                print("  cat     - view file\n");
                print("  cd      - change directory\n");
                print("  pwd     - print path\n");
                print("  echo    - print text\n");
                print("  color   - show colors\n");
                print("  ping    - ping IP address\n");
                print("  reboot  - restart\n");
                print("  clear   - clear screen\n");
            }
            else if (strcmp(p, "mount") == 0) mount();
            else if (strcmp(p, "ls") == 0) ls();
            else if (p[0] == 'c' && p[1] == 'a' && p[2] == 't') {
                if (p[3] == ' ') cat(p + 4);
                else print("Use: cat filename\n");
            }
            else if (p[0] == 'c' && p[1] == 'd') {
                if (p[2] == ' ') cd(p + 3);
                else cd("");
            }
            else if (strcmp(p, "pwd") == 0) pwd();
            else if (p[0] == 'e' && p[1] == 'c' && p[2] == 'h' && p[3] == 'o') {
                echo(p + 4);
            }
            else if (strcmp(p, "color") == 0) color_test();
            else if (p[0] == 'p' && p[1] == 'i' && p[2] == 'n' && p[3] == 'g' && p[4] == ' ') {
                ping(p + 5);
            }
            else if (strcmp(p, "reboot") == 0) reboot();
            else if (strcmp(p, "clear") == 0) {
                for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
                    video[i] = ' ' | (current_color << 8);
                }
                row = 0; col = 0;
            }
			else if (strcmp(p, "ifconfig") == 0) {
				ifconfig();
			}
			else if (p[0] == 'n' && p[1] == 'c' && p[2] == ' ') {
				netcat(p + 3);
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
        else if (c == 0x80) {
            const char* h = history_get_prev();
            if (h) {
                while (pos > 0) {
                    pos--;
                    putchar('\b');
                }
                for (int i = 0; h[i]; i++) {
                    cmd[i] = h[i];
                    putchar(h[i]);
                }
                pos = strlen(h);
            }
        }
        else if (c == 0x81) {
            const char* h = history_get_next();
            if (h) {
                while (pos > 0) {
                    pos--;
                    putchar('\b');
                }
                for (int i = 0; h[i]; i++) {
                    cmd[i] = h[i];
                    putchar(h[i]);
                }
                pos = strlen(h);
            }
        }
        else if (c >= ' ' && c <= '~' && pos < 255) {
            cmd[pos++] = c;
            putchar(c);
        }

    }
}
extern uint8_t my_mac[6];
extern uint32_t my_ip;

void print_hex(uint8_t val) {
    const char* hex = "0123456789ABCDEF";
    putchar(hex[val >> 4]);
    putchar(hex[val & 0xF]);
}

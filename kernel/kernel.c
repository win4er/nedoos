// NedoOS - Мега-версия с виртуальными терминалами, мышью и сетью
#include <stdint.h>
#include <stddef.h>

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

static char* tab_matches[MAX_MATCHES];
static int tab_count = 0;

// ==================== СЕТЕВЫЕ ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ====================
uint8_t my_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
uint32_t my_ip = 0x0A00020F; // 10.0.2.15 (QEMU default)

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

typedef struct {
    uint8_t order;
    uint16_t name1[5];
    uint8_t attr;
    uint8_t type;
    uint8_t checksum;
    uint16_t name2[6];
    uint16_t first_cluster;
    uint16_t name3[2];
} __attribute__((packed)) vfat_entry_t;

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

static uint16_t fat_alloc_cluster(void) {
    for (uint16_t i = 2; i < 512; i++) {
        if (fat_get_next_cluster(i) == 0) {
            fat_set_next_cluster(i, 0xFF8);
            return i;
        }
    }
    return 0;
}

// ==================== VFAT ДЛИННЫЕ ИМЕНА ====================
int get_long_name(uint8_t* dir_sec, int start_entry, char* long_name) {
    vfat_entry_t* v;
    int entries_per_sector = bytes_per_sector / 32;
    int pos = 0;
    
    for (int i = start_entry; i < entries_per_sector; i++) {
        v = (vfat_entry_t*)(dir_sec + i * 32);
        if (v->attr != ATTR_VFAT) break;
        
        int order = v->order & 0x3F;
        if (order == 0) break;
        
        for (int j = 0; j < 5; j++) {
            if (v->name1[j] && v->name1[j] != 0xFFFF) {
                long_name[pos++] = v->name1[j] & 0xFF;
            }
        }
        for (int j = 0; j < 6; j++) {
            if (v->name2[j] && v->name2[j] != 0xFFFF) {
                long_name[pos++] = v->name2[j] & 0xFF;
            }
        }
        for (int j = 0; j < 2; j++) {
            if (v->name3[j] && v->name3[j] != 0xFFFF) {
                long_name[pos++] = v->name3[j] & 0xFF;
            }
        }
    }
    long_name[pos] = 0;
    return pos;
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

// ==================== LS С ДЛИННЫМИ ИМЕНАМИ ====================
void ls(void) {
    if (!mounted) { print("mount first\n"); return; }
    
    uint8_t sec[512];
    fat_entry_t* e;
    uint32_t dir_sector;
    int entries_per_sector = bytes_per_sector / 32;
    char long_name[256];
    
    if (current_dir_cluster == 0) {
        dir_sector = root_sector;
    } else {
        dir_sector = data_sector + (current_dir_cluster - 2) * sectors_per_cluster;
    }
    
    if (ata_read(dir_sector, sec)) return;
    
    for (int i = 0; i < entries_per_sector; i++) {
        e = (fat_entry_t*)(sec + i * 32);
        
        if (e->attr == ATTR_VFAT) continue;
        
        if (e->name[0] == 0) break;
        if (e->name[0] == 0xE5) continue;
        
        int has_long = 0;
        if (i > 0 && ((vfat_entry_t*)(sec + (i-1) * 32))->attr == ATTR_VFAT) {
            get_long_name(sec, i-1, long_name);
            has_long = 1;
        }
        
        if (e->attr & ATTR_DIRECTORY) set_color(COLOR_CYAN);
        else set_color(COLOR_GREY);
        
        if (has_long) {
            print(long_name);
        } else {
            for (int j = 0; j < 8 && e->name[j] != ' '; j++) putchar(e->name[j]);
            if (e->ext[0] != ' ') {
                putchar('.');
                for (int j = 0; j < 3 && e->ext[j] != ' '; j++) putchar(e->ext[j]);
            }
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

// ==================== MKDIR ====================
void mkdir(const char* dirname) {
    if (!mounted) { print("mount first\n"); return; }
    
    while (*dirname == ' ') dirname++;
    if (*dirname == 0) { print("Use: mkdir dirname\n"); return; }
    
    uint8_t sec[512];
    fat_entry_t* e;
    int free_entry = -1;
    int entries_per_sector = bytes_per_sector / 32;
    
    uint32_t dir_sector = (current_dir_cluster == 0) ? root_sector : 
                          data_sector + (current_dir_cluster - 2) * sectors_per_cluster;
    
    if (ata_read(dir_sector, sec)) return;
    
    for (int i = 0; i < entries_per_sector; i++) {
        e = (fat_entry_t*)(sec + i * 32);
        if (e->name[0] == 0 || e->name[0] == 0xE5) {
            free_entry = i;
            break;
        }
    }
    
    if (free_entry == -1) { print("Directory full\n"); return; }
    
    uint16_t new_cluster = fat_alloc_cluster();
    if (new_cluster == 0) { print("No free clusters\n"); return; }
    
    e = (fat_entry_t*)(sec + free_entry * 32);
    
    for (int j = 0; j < 8; j++) e->name[j] = ' ';
    for (int j = 0; j < 3; j++) e->ext[j] = ' ';
    
    int j = 0;
    while (dirname[j] && j < 8) {
        char c = dirname[j];
        if (c >= 'a' && c <= 'z') c -= 32;
        e->name[j] = c;
        j++;
    }
    
    e->attr = ATTR_DIRECTORY;
    e->cluster = new_cluster;
    e->size = 0;
    
    ata_write(dir_sector, sec);
    
    uint8_t new_sec[512];
    fat_entry_t* new_e;
    
    for (int i = 0; i < 512; i++) new_sec[i] = 0;
    
    new_e = (fat_entry_t*)new_sec;
    new_e->name[0] = '.';
    for (int j = 1; j < 8; j++) new_e->name[j] = ' ';
    new_e->ext[0] = ' ';
    new_e->attr = ATTR_DIRECTORY;
    new_e->cluster = new_cluster;
    
    new_e = (fat_entry_t*)(new_sec + 32);
    new_e->name[0] = '.';
    new_e->name[1] = '.';
    for (int j = 2; j < 8; j++) new_e->name[j] = ' ';
    new_e->ext[0] = ' ';
    new_e->attr = ATTR_DIRECTORY;
    new_e->cluster = (current_dir_cluster == 0) ? 0 : current_dir_cluster;
    
    uint32_t new_sector = data_sector + (new_cluster - 2) * sectors_per_cluster;
    ata_write(new_sector, new_sec);
    
    print("Directory created\n");
}

// ==================== WRITE ====================
void write_file(const char* filename, const char* content) {
    if (!mounted) { print("mount first\n"); return; }
    
    uint8_t sec[512];
    fat_entry_t* e;
    int free_entry = -1;
    int entries_per_sector = bytes_per_sector / 32;
    
    uint32_t dir_sector = (current_dir_cluster == 0) ? root_sector : 
                          data_sector + (current_dir_cluster - 2) * sectors_per_cluster;
    
    if (ata_read(dir_sector, sec)) return;
    
    for (int i = 0; i < entries_per_sector; i++) {
        e = (fat_entry_t*)(sec + i * 32);
        if (e->name[0] == 0 || e->name[0] == 0xE5) {
            free_entry = i;
            break;
        }
    }
    
    if (free_entry == -1) { print("Directory full\n"); return; }
    
    uint16_t cluster = fat_alloc_cluster();
    if (cluster == 0) { print("No free clusters\n"); return; }
    
    e = (fat_entry_t*)(sec + free_entry * 32);
    
    for (int j = 0; j < 8; j++) e->name[j] = ' ';
    for (int j = 0; j < 3; j++) e->ext[j] = ' ';
    
    int j = 0;
    while (filename[j] && filename[j] != '.' && j < 8) {
        char c = filename[j];
        if (c >= 'a' && c <= 'z') c -= 32;
        e->name[j] = c;
        j++;
    }
    
    if (filename[j] == '.') {
        j++;
        int k = 0;
        while (filename[j] && k < 3) {
            char c = filename[j];
            if (c >= 'a' && c <= 'z') c -= 32;
            e->ext[k++] = c;
            j++;
        }
    }
    
    e->attr = ATTR_ARCHIVE;
    e->cluster = cluster;
    
    int size = 0;
    while (content[size]) size++;
    e->size = size;
    
    ata_write(dir_sector, sec);
    
    uint8_t data[512];
    for (int i = 0; i < 512; i++) data[i] = 0;
    for (int i = 0; i < size && i < 512; i++) data[i] = content[i];
    
    uint32_t data_sec = data_sector + (cluster - 2) * sectors_per_cluster;
    ata_write(data_sec, data);
    
    print("File written\n");
}

// ==================== RM ====================
void rm(const char* filename) {
    if (!mounted) { print("mount first\n"); return; }
    
    uint8_t sec[512];
    fat_entry_t* e;
    int entries_per_sector = bytes_per_sector / 32;
    
    uint32_t dir_sector = (current_dir_cluster == 0) ? root_sector : 
                          data_sector + (current_dir_cluster - 2) * sectors_per_cluster;
    
    if (ata_read(dir_sector, sec)) return;
    
    char search[13];
    int si = 0;
    for (int i = 0; filename[i] && i < 12; i++) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        search[si++] = c;
    }
    search[si] = 0;
    
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
            e->name[0] = 0xE5;
            ata_write(dir_sector, sec);
            print("Deleted\n");
            return;
        }
    }
    print("Not found\n");
}

// ==================== RMDIR ====================
void rmdir(const char* dirname) {
    if (!mounted) { print("mount first\n"); return; }
    
    uint8_t sec[512];
    fat_entry_t* e;
    int entries_per_sector = bytes_per_sector / 32;
    
    uint32_t dir_sector = (current_dir_cluster == 0) ? root_sector : 
                          data_sector + (current_dir_cluster - 2) * sectors_per_cluster;
    
    if (ata_read(dir_sector, sec)) return;
    
    char search[9];
    int si = 0;
    for (int i = 0; dirname[i] && i < 8; i++) {
        char c = dirname[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        search[si++] = c;
    }
    search[si] = 0;
    
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
            e->name[0] = 0xE5;
            ata_write(dir_sector, sec);
            print("Directory removed\n");
            return;
        }
    }
    print("Directory not found\n");
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

const char* env_get(const char* name) {
    for (int i = 0; i < env_count; i++) {
        if (strcmp(env_names[i], name) == 0) {
            return env_values[i];
        }
    }
    return NULL;
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

// ==================== TAB-ДОПОЛНЕНИЕ ====================
void tab_complete(char* cmd, int* pos) {
    if (!mounted) return;
    
    uint8_t sec[512];
    fat_entry_t* e;
    uint32_t dir_sector;
    int entries_per_sector = bytes_per_sector / 32;
    char long_name[256];
    char partial[256];
    char matches[MAX_MATCHES][256];
    int match_count = 0;
    
    if (current_dir_cluster == 0) {
        dir_sector = root_sector;
    } else {
        dir_sector = data_sector + (current_dir_cluster - 2) * sectors_per_cluster;
    }
    
    if (ata_read(dir_sector, sec)) return;
    
    strcpy(partial, cmd);
    int len = strlen(partial);
    
    for (int i = 0; i < entries_per_sector && match_count < MAX_MATCHES; i++) {
        e = (fat_entry_t*)(sec + i * 32);
        if (e->attr == ATTR_VFAT) continue;
        if (e->name[0] == 0) break;
        if (e->name[0] == 0xE5) continue;
        
        char name[256];
        int has_long = 0;
        if (i > 0 && ((vfat_entry_t*)(sec + (i-1) * 32))->attr == ATTR_VFAT) {
            get_long_name(sec, i-1, name);
            has_long = 1;
        } else {
            int pos2 = 0;
            for (int j = 0; j < 8 && e->name[j] != ' '; j++) name[pos2++] = e->name[j];
            if (e->ext[0] != ' ') {
                name[pos2++] = '.';
                for (int j = 0; j < 3 && e->ext[j] != ' '; j++) name[pos2++] = e->ext[j];
            }
            name[pos2] = 0;
        }
        
        if (strncmp(name, partial, len) == 0) {
            strcpy(matches[match_count], name);
            match_count++;
        }
    }
    
    if (match_count == 1) {
        while (*pos > 0) {
            (*pos)--;
            putchar('\b');
        }
        for (int i = 0; matches[0][i]; i++) {
            cmd[i] = matches[0][i];
            putchar(matches[0][i]);
        }
        *pos = strlen(matches[0]);
    } else if (match_count > 1) {
        print("\n");
        for (int i = 0; i < match_count; i++) {
            print(matches[i]);
            print("  ");
        }
        print("\n> ");
        print(cmd);
    }
}

// ==================== ГРАФИКА VESA ====================
void vesa_init(void) {
    __asm__ volatile (
        "mov $0x4F02, %%ax\n"
        "mov $0x13, %%bx\n"
        "int $0x10"
        : : : "ax", "bx"
    );
    print("VESA graphics mode enabled\n");
}

void vesa_putpixel(int x, int y, uint8_t color) {
    uint8_t* vesa_mem = (uint8_t*)0xA0000;
    vesa_mem[y * 320 + x] = color;
}

void vesa_clear(uint8_t color) {
    uint8_t* vesa_mem = (uint8_t*)0xA0000;
    for (int i = 0; i < 320 * 200; i++) {
        vesa_mem[i] = color;
    }
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
    print("Commands: help, mount, ls, cat, cd, pwd, mkdir, rm, rmdir, write,\n");
    print("         echo, color, reboot, clear, ping\n");
    print("Alt+F1..F4 - terminals | Alt+Up/Down - history\n> ");
    
    char cmd[256];
    int pos = 0;
    
    while (1) {
        char c = getchar();
        network_handler(); // проверяем сетевые пакеты
        
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
                print("  mkdir   - create directory\n");
                print("  rm      - remove file\n");
                print("  rmdir   - remove directory\n");
                print("  write   - write file (write file.txt text)\n");
                print("  echo    - print text\n");
                print("  color   - show colors\n");
                print("  ping    - ping IP address\n");
                print("  reboot  - restart\n");
                print("  clear   - clear screen\n");
                print("  env     - list environment variables\n");
                print("  export  - set environment variable\n");
                print("  pkg     - package manager (list/install/remove)\n");
                print("  vesa    - switch to graphics mode\n");
                print("  beep    - make a beep\n");
                print("  edit    - text editor\n");
            }
            else if (strcmp(p, "env") == 0) {
                env_list();
            }
            else if (p[0] == 'e' && p[1] == 'x' && p[2] == 'p' && p[3] == 'o' && p[4] == 'r' && p[5] == 't') {
                export(p + 6);
            }
            else if (p[0] == 'p' && p[1] == 'k' && p[2] == 'g' && p[3] == ' ') {
                if (strcmp(p+4, "list") == 0) pkg_list();
                else if (strncmp(p+4, "install ", 8) == 0) pkg_install(p+12);
                else if (strncmp(p+4, "remove ", 7) == 0) pkg_remove(p+11);
                else print("Use: pkg list/install/remove\n");
            }
            else if (strcmp(p, "vesa") == 0) {
                vesa_init();
            }
            else if (p[0] == 'b' && p[1] == 'e' && p[2] == 'e' && p[3] == 'p') {
                beep(440, 10);
            }
            else if (p[0] == 'e' && p[1] == 'd' && p[2] == 'i' && p[3] == 't' && p[4] == ' ') {
                edit(p + 5);
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
            else if (p[0] == 'm' && p[1] == 'k' && p[2] == 'd' && p[3] == 'i' && p[4] == 'r') {
                if (p[5] == ' ') mkdir(p + 6);
                else print("Use: mkdir dirname\n");
            }
            else if (p[0] == 'r' && p[1] == 'm' && p[2] == ' ') {
                rm(p + 3);
            }
            else if (p[0] == 'r' && p[1] == 'm' && p[2] == 'd' && p[3] == 'i' && p[4] == 'r' && p[5] == ' ') {
                rmdir(p + 6);
            }
            else if (p[0] == 'w' && p[1] == 'r' && p[2] == 'i' && p[3] == 't' && p[4] == 'e' && p[5] == ' ') {
                char* space = p + 6;
                while (*space == ' ') space++;
                char* filename = space;
                while (*space && *space != ' ') space++;
                if (*space) {
                    *space = 0;
                    space++;
                    while (*space == ' ') space++;
                    write_file(filename, space);
                } else {
                    print("Use: write filename text\n");
                }
            }
            else if (p[0] == 'e' && p[1] == 'c' && p[2] == 'h' && p[3] == 'o') {
                echo(p + 4);
            }
            else if (p[0] == 'p' && p[1] == 'i' && p[2] == 'n' && p[3] == 'g' && p[4] == ' ') {
                ping(p + 5);
            }
            else if (strcmp(p, "color") == 0) color_test();
            else if (strcmp(p, "reboot") == 0) reboot();
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
        else if (c == '\t') {
            tab_complete(cmd, &pos);
        }
        else if (c >= ' ' && c <= '~' && pos < 255) {
            cmd[pos++] = c;
            putchar(c);
        }
    }
}

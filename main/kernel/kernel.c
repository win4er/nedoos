// NedoOS - Полная версия (без многозадачности)
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

// ==================== KEYBOARD ====================
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

// ==================== STRING ====================
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

void strcpy(char* dest, const char* src) {
    while (*src) *dest++ = *src++;
    *dest = 0;
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

void sleep(uint32_t ticks) {
    uint32_t target = timer_ticks + ticks;
    while (timer_ticks < target) {
        __asm__ volatile("hlt");
    }
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

#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20

// ==================== FAT STATE ====================
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

// ==================== MAIN ====================
void kernel_main(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        video[i] = ' ' | (current_color << 8);
    }
    row = 0; col = 0;
    
    set_color(COLOR_GREEN);
    print("NedoOS Ultimate Edition\n");
    
    // Инициализация таймера
    init_timer(100);
    
    set_color(COLOR_CYAN);
    print("Commands: help, mount, ls, cat, cd, pwd, mkdir, rm, rmdir, write, echo, color, reboot, clear\n> ");
    
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
        else if (c >= ' ' && c <= '~' && pos < 255) {
            cmd[pos++] = c;
            putchar(c);
        }
    }
}

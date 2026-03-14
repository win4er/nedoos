#ifndef FAT_H
#define FAT_H

#include <stdint.h>

// FAT12 Boot Sector
typedef struct {
    uint8_t jmp[3];
    uint8_t oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
} __attribute__((packed)) fat_boot_t;

// Directory entry
typedef struct {
    uint8_t name[8];
    uint8_t ext[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t create_time_ms;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_low;
    uint32_t size;
} __attribute__((packed)) fat_entry_t;

// File attributes
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20

// FAT filesystem structure
typedef struct {
    fat_boot_t boot;
    uint16_t* fat;
    uint32_t fat_start;
    uint32_t root_start;
    uint32_t data_start;
    uint8_t mounted;
} fat_fs_t;

// Functions
int fat_mount(fat_fs_t* fs);
void fat_ls(fat_fs_t* fs);
int fat_read(fat_fs_t* fs, const char* filename, uint8_t* buffer, uint32_t max_size);
void fat_format_filename(const uint8_t* fat_name, const uint8_t* fat_ext, char* out);

#endif

#include "fat.h"
#include "../drivers/ata.h"
#include <stdint.h>

static uint8_t sector_buffer[512];
static fat_fs_t global_fs;  // Временно, для простоты
static const char hex[] = "0123456789ABCDEF";

// Convert FAT filename to normal string
void fat_format_filename(const uint8_t* fat_name, const uint8_t* fat_ext, char* out) {
    int i, pos = 0;
    
    // Copy name
    for (i = 0; i < 8; i++) {
        if (fat_name[i] == ' ') break;
        out[pos++] = fat_name[i];
    }
    
    // Copy extension if exists
    if (fat_ext[0] != ' ') {
        out[pos++] = '.';
        for (i = 0; i < 3; i++) {
            if (fat_ext[i] == ' ') break;
            out[pos++] = fat_ext[i];
        }
    }
    
    out[pos] = '\0';
}

// Mount FAT filesystem
static const char hex[] = "0123456789ABCDEF";

static void mount_fat(void) {
    uint8_t sector[512];
    int i;
    
    print("\nMounting FAT...\n");
    
    print("Reading sector 0...");
    if (fdc_read_sector(0, sector) != 0) {
        print(" FAIL\n");
        return;
    }
    print(" OK\n");
    
    // Дампим первые 64 байта
    print("Sector 0 dump:\n");
    for (i = 0; i < 64; i++) {
        if (i % 16 == 0) {
            print("\n");
            print_dec(i);
            print(": ");
        }
        putchar(hex[(sector[i] >> 4) & 0xF]);
        putchar(hex[sector[i] & 0xF]);
        putchar(' ');
    }
    print("\n");
    
    // Проверяем FAT сигнатуру
    if (sector[54] == 'F' && sector[55] == 'A' && sector[56] == 'T') {
        print("FAT signature found!\n");
    } else {
        print("Not a FAT disk. Bytes 54-56: ");
        putchar(hex[(sector[54] >> 4) & 0xF]);
        putchar(hex[sector[54] & 0xF]);
        putchar(' ');
        putchar(hex[(sector[55] >> 4) & 0xF]);
        putchar(hex[sector[55] & 0xF]);
        putchar(' ');
        putchar(hex[(sector[56] >> 4) & 0xF]);
        putchar(hex[sector[56] & 0xF]);
        print("\n");
    }
}

// List files in root directory
void fat_ls(fat_fs_t* fs) {
    if (!fs->mounted) return;
    
    fat_entry_t* entry;
    char filename[13];
    uint32_t sector;
    uint32_t entries_per_sector = fs->boot.bytes_per_sector / 32;
    
    // Read root directory
    for (uint32_t i = 0; i < fs->boot.root_entries; i++) {
        // Read sector when needed
        if (i % entries_per_sector == 0) {
            sector = fs->root_start + i / entries_per_sector;
            if (ata_read_sector(sector, sector_buffer) != 0) {
                return;
            }
        }
        
        entry = (fat_entry_t*)(sector_buffer + (i % entries_per_sector) * 32);
        
        // Skip empty or deleted
        if (entry->name[0] == 0) continue;
        if (entry->name[0] == 0xE5) continue;
        
        // Skip volume label
        if (entry->attributes & FAT_ATTR_VOLUME_ID) continue;
        
        // Format and print filename
        fat_format_filename(entry->name, entry->ext, filename);
        
        // Print with attributes
        if (entry->attributes & FAT_ATTR_DIRECTORY) {
            // Directory - not implemented yet
        } else {
            // File - show size
            // Simple print without formatting for now
            print(filename);
            print(" (");
            
            // Convert size to string (simplified)
            uint32_t size = entry->size;
            char size_str[12];
            int j = 0;
            if (size == 0) {
                size_str[j++] = '0';
            } else {
                while (size > 0) {
                    size_str[j++] = '0' + (size % 10);
                    size /= 10;
                }
                // Reverse
                for (int k = 0; k < j/2; k++) {
                    char tmp = size_str[k];
                    size_str[k] = size_str[j-1-k];
                    size_str[j-1-k] = tmp;
                }
            }
            size_str[j] = '\0';
            
            print(size_str);
            print(" bytes)\n");
        }
    }
}

// Read file from root directory
int fat_read(fat_fs_t* fs, const char* filename, uint8_t* buffer, uint32_t max_size) {
    if (!fs->mounted) return -1;
    
    fat_entry_t* entry;
    uint32_t sector;
    uint32_t entries_per_sector = fs->boot.bytes_per_sector / 32;
    uint16_t cluster;
    uint32_t bytes_read = 0;
    uint32_t bytes_per_cluster = fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster;
    
    // Find file in root directory
    for (uint32_t i = 0; i < fs->boot.root_entries; i++) {
        if (i % entries_per_sector == 0) {
            sector = fs->root_start + i / entries_per_sector;
            if (ata_read_sector(sector, sector_buffer) != 0) {
                return -1;
            }
        }
        
        entry = (fat_entry_t*)(sector_buffer + (i % entries_per_sector) * 32);
        
        if (entry->name[0] == 0) continue;
        if (entry->name[0] == 0xE5) continue;
        if (entry->attributes & FAT_ATTR_VOLUME_ID) continue;
        if (entry->attributes & FAT_ATTR_DIRECTORY) continue;
        
        // Compare filename (simplified - only first char)
        // TODO: proper filename comparison
        if (entry->name[0] == filename[0]) {
            cluster = entry->cluster_low;
            break;
        }
    }
    
    if (cluster == 0) return -2;  // Not found
    
    // Read clusters
    uint32_t remaining = entry->size;
    if (remaining > max_size) remaining = max_size;
    
    while (remaining > 0 && cluster < 0x0FF8) {
        // Calculate sector for this cluster
        uint32_t cluster_sector = fs->data_start + (cluster - 2) * fs->boot.sectors_per_cluster;
        
        // Read all sectors in cluster
        for (int s = 0; s < fs->boot.sectors_per_cluster && remaining > 0; s++) {
            if (ata_read_sector(cluster_sector + s, buffer + bytes_read) != 0) {
                return bytes_read;
            }
            bytes_read += fs->boot.bytes_per_sector;
            remaining -= fs->boot.bytes_per_sector;
        }
        
        // Get next cluster from FAT
        uint32_t fat_index = cluster * 3 / 2;  // 12-bit entries
        uint32_t fat_sector = fs->fat_start + fat_index / fs->boot.bytes_per_sector;
        uint32_t fat_offset = fat_index % fs->boot.bytes_per_sector;
        
        if (ata_read_sector(fat_sector, sector_buffer) != 0) {
            return bytes_read;
        }
        
        uint16_t next;
        if (cluster & 1) {
            // Odd cluster - high 12 bits
            next = (*(uint16_t*)(sector_buffer + fat_offset)) >> 4;
        } else {
            // Even cluster - low 12 bits
            next = *(uint16_t*)(sector_buffer + fat_offset) & 0x0FFF;
        }
        
        cluster = next;
    }
    
    return bytes_read;
}

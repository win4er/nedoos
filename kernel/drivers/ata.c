// Floppy Disk Controller ports
#define FDC_DOR    0x3F2
#define FDC_MSR    0x3F4
#define FDC_FIFO   0x3F5
#define FDC_CTRL   0x3F7

// Floppy commands
#define FDC_CMD_SPECIFY      0x03
#define FDC_CMD_WRITE        0x05
#define FDC_CMD_READ         0x06
#define FDC_CMD_RECALIBRATE  0x07
#define FDC_CMD_SENSE_INT    0x08
#define FDC_CMD_SEEK         0x0F

static void fdc_wait_ready(void) {
    for (int i = 0; i < 10000; i++) {
        if (inb(FDC_MSR) & 0x80) return;
    }
}

static void fdc_send_command(uint8_t cmd) {
    fdc_wait_ready();
    outb(FDC_FIFO, cmd);
}

static uint8_t fdc_read_data(void) {
    fdc_wait_ready();
    return inb(FDC_FIFO);
}

static void fdc_reset(void) {
    outb(FDC_DOR, 0);
    outb(FDC_DOR, 0x0C);
    
    // Wait for reset
    for (int i = 0; i < 10000; i++) {
        if (inb(FDC_MSR) & 0xC0) break;
    }
    
    // Sense interrupt
    outb(FDC_FIFO, FDC_CMD_SENSE_INT);
    fdc_read_data();
    fdc_read_data();
}

static void fdc_seek(uint8_t track) {
    fdc_send_command(FDC_CMD_SEEK);
    fdc_send_command(0);      // drive 0
    fdc_send_command(track);
    
    // Wait for seek complete
    for (int i = 0; i < 10000; i++) {
        outb(FDC_FIFO, FDC_CMD_SENSE_INT);
        if (fdc_read_data() & 0x20) break;
    }
    fdc_read_data();  // status
    fdc_read_data();  // track
}

int fdc_read_sector(uint32_t lba, uint8_t* buffer) {
    uint8_t track = lba / 18;
    uint8_t sector = (lba % 18) + 1;
    uint8_t head = 0;  // только head 0 для простоты
    
    // Reset controller
    fdc_reset();
    
    // Seek to track
    fdc_seek(track);
    
    // Read command
    fdc_send_command(FDC_CMD_READ);
    fdc_send_command(head << 2);  // drive+head
    fdc_send_command(track);
    fdc_send_command(head);
    fdc_send_command(sector);
    fdc_send_command(2);  // 512 bytes/sector
    fdc_send_command(18); // sectors/track
    fdc_send_command(0x1B); // gap length
    fdc_send_command(0xFF); // data length
    
    // Read data
    for (int i = 0; i < 512; i++) {
        buffer[i] = fdc_read_data();
    }
    
    // Read result status
    for (int i = 0; i < 7; i++) {
        fdc_read_data();
    }
    
    return 0;
}

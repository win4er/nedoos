#include "e1000.h"
#include "../include/net/network.h"
#include <stddef.h>

extern void print(const char* s);
extern void outb(uint16_t port, uint8_t val);
extern uint8_t inb(uint16_t port);

static volatile uint8_t* e1000_mmio;
static e1000_desc_t rx_desc[32] __attribute__((aligned(16)));
static e1000_desc_t tx_desc[8] __attribute__((aligned(16)));
static uint8_t rx_buffer[32][2048];
static uint8_t tx_buffer[8][2048];
static int rx_cur = 0;
static int tx_cur = 0;

static uint32_t e1000_read(uint16_t reg) {
    return *(volatile uint32_t*)(e1000_mmio + reg);
}

static void e1000_write(uint16_t reg, uint32_t val) {
    *(volatile uint32_t*)(e1000_mmio + reg) = val;
}

void e1000_init(uint32_t bar, uint8_t irq) {
    print("E1000: Initializing...\n");
    
    e1000_mmio = (volatile uint8_t*)(uint32_t)bar;
    
    e1000_write(E1000_REG_CTRL, e1000_read(E1000_REG_CTRL) | 1);
    for (volatile int i = 0; i < 1000000; i++);
    
    e1000_write(E1000_REG_CTRL, e1000_read(E1000_REG_CTRL) | E1000_CTRL_SLU);
    
    for (int i = 0; i < 1000; i++) {
        if (e1000_read(E1000_REG_STATUS) & E1000_STATUS_LU) {
            print("E1000: Link Up\n");
            break;
        }
    }
    
    for (int i = 0; i < 32; i++) {
        rx_desc[i].addr = (uint64_t)(uint32_t)rx_buffer[i];
        rx_desc[i].status = 0;
    }
    
    e1000_write(E1000_REG_RXDESCLO, (uint32_t)rx_desc);
    e1000_write(E1000_REG_RXDESCHI, 0);
    
    for (int i = 0; i < 8; i++) {
        tx_desc[i].addr = (uint64_t)(uint32_t)tx_buffer[i];
    }
    
    e1000_write(E1000_REG_TXDESCLO, (uint32_t)tx_desc);
    e1000_write(E1000_REG_TXDESCHI, 0);
    
    print("E1000: Ready\n");
}

int e1000_send_packet(uint8_t* data, uint16_t len) {
    int desc = tx_cur;
    
    for (int i = 0; i < len; i++) {
        tx_buffer[desc][i] = data[i];
    }
    
    tx_desc[desc].length = len;
    tx_desc[desc].status = 0;
    
    // Трансмит (упрощённо)
    for (int i = 0; i < 1000; i++) {
        if (tx_desc[desc].status & 0xFF) break;
    }
    
    tx_cur = (tx_cur + 1) % 8;
    return len;
}

int e1000_recv_packet(uint8_t* buffer) {
    int desc = rx_cur;
    
    if (!(rx_desc[desc].status & E1000_RX_DESC_DD)) {
        return 0;
    }
    
    int len = rx_desc[desc].length;
    for (int i = 0; i < len; i++) {
        buffer[i] = rx_buffer[desc][i];
    }
    
    rx_desc[desc].status = 0;
    rx_cur = (rx_cur + 1) % 32;
    
    return len;
}

#ifndef E1000_H
#define E1000_H

#include <stdint.h>

// E1000 registers
#define E1000_REG_CTRL     0x0000
#define E1000_REG_STATUS   0x0008
#define E1000_REG_RXDESC   0x2800
#define E1000_REG_TXDESC   0x3800
#define E1000_REG_RXDESCLO 0x2808
#define E1000_REG_RXDESCHI 0x280C
#define E1000_REG_TXDESCLO 0x3808
#define E1000_REG_TXDESCHI 0x380C
#define E1000_CTRL_SLU     0x40
#define E1000_STATUS_LU    0x02

// Descriptor
typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed)) e1000_desc_t;

#define E1000_RX_DESC_DD   0x01

void e1000_init(uint32_t bar, uint8_t irq);
int e1000_send_packet(uint8_t* data, uint16_t len);
int e1000_recv_packet(uint8_t* buffer);

#endif

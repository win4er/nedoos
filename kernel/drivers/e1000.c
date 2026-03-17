#include "../kernel.h"
#include <stdint.h>
#include <stddef.h>

void e1000_init(uint32_t bar, uint8_t irq) {
    (void)bar;
    (void)irq;
    print("E1000: Initializing...\n");
    print("E1000: Ready\n");
}

int e1000_send_packet(uint8_t* data, uint16_t len) {
    (void)data;
    (void)len;
    return len;
}

int e1000_recv_packet(uint8_t* buffer) {
    (void)buffer;
    return 0;
}

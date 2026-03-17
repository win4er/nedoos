#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

// VGA functions
void set_color(uint8_t fg);
void putchar(char c);
void print(const char* s);
void print_dec(uint32_t num);
void newline(void);

// Network globals
extern uint8_t my_mac[6];
extern uint32_t my_ip;

// Network functions
void network_init(void);
void network_handler(void);
void ping(const char* ip_str);

#endif

// Network commands
void ifconfig(void);
void netcat(const char* args);
void print_hex(uint8_t val);

#ifndef KERNEL_H
#define KERNEL_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

// Цвета
#define COLOR_RED     0x04
#define COLOR_GREEN   0x02
#define COLOR_YELLOW  0x0E
#define COLOR_CYAN    0x03
#define COLOR_GREY    0x07

void set_color(uint8_t fg);
void putchar(char c);
void print(const char* s);
void print_dec(uint32_t num);
void newline(void);

#endif

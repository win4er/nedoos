#ifndef KERNEL_H
#define KERNEL_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

void print(const char* str);
void putchar(char c);
void clear_screen(void);
char getchar(void);

#endif

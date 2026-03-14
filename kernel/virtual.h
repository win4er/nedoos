#ifndef VIRTUAL_TERMINAL_H
#define VIRTUAL_TERMINAL_H

#define MAX_TERMINALS 4
#define TERM_WIDTH 80
#define TERM_HEIGHT 25
#define TERM_SIZE (TERM_WIDTH * TERM_HEIGHT * 2)

typedef struct {
    uint16_t buffer[TERM_WIDTH * TERM_HEIGHT];
    int row, col;
    uint8_t color;
    int active;
} terminal_t;

void vt_init(void);
void vt_switch(int num);
void vt_putchar(int term, char c);
void vt_print(int term, const char* s);

#endif

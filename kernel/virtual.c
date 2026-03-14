#include "virtual_terminal.h"

static terminal_t terminals[MAX_TERMINALS];
static int current_terminal = 0;

void vt_init(void) {
    for (int t = 0; t < MAX_TERMINALS; t++) {
        terminals[t].row = 0;
        terminals[t].col = 0;
        terminals[t].color = 0x07;
        terminals[t].active = (t == 0);
        
        // Очищаем буфер
        for (int i = 0; i < TERM_WIDTH * TERM_HEIGHT; i++) {
            terminals[t].buffer[i] = ' ' | (0x07 << 8);
        }
    }
}

void vt_switch(int num) {
    if (num < 0 || num >= MAX_TERMINALS) return;
    
    // Сохраняем текущий терминал
    terminals[current_terminal].row = row;
    terminals[current_terminal].col = col;
    terminals[current_terminal].color = current_color;
    
    // Сохраняем буфер текущего терминала в память
    for (int i = 0; i < TERM_WIDTH * TERM_HEIGHT; i++) {
        terminals[current_terminal].buffer[i] = video[i];
    }
    
    // Переключаемся
    current_terminal = num;
    
    // Восстанавливаем новый терминал
    row = terminals[num].row;
    col = terminals[num].col;
    current_color = terminals[num].color;
    
    // Копируем его буфер на экран
    for (int i = 0; i < TERM_WIDTH * TERM_HEIGHT; i++) {
        video[i] = terminals[num].buffer[i];
    }
}

// Обработка Alt+F1..F4 в getchar
if (sc == 0x3B && shift) { vt_switch(0); return 0; } // Alt+F1
if (sc == 0x3C && shift) { vt_switch(1); return 0; } // Alt+F2
if (sc == 0x3D && shift) { vt_switch(2); return 0; } // Alt+F3
if (sc == 0x3E && shift) { vt_switch(3); return 0; } // Alt+F4

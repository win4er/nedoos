#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

char keyboard_getchar(void);
int keyboard_getchar_nonblock(void);
void keyboard_install(void);
void keyboard_clear_buffer(void);

#endif

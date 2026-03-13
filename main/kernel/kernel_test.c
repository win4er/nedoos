void kernel_main(void) {
    unsigned char* vga = (unsigned char*) 0xB8000;
    const char* msg = "NedoOS is alive!";
    int i;
    
    // Очищаем экран
    for (i = 0; i < 80*25*2; i += 2) {
        vga[i] = ' ';
        vga[i+1] = 0x07;
    }
    
    // Выводим сообщение
    for (i = 0; msg[i] != '\0'; i++) {
        vga[i*2] = msg[i];
        vga[i*2+1] = 0x07;
    }
    
    while (1) {
        __asm__("hlt");
    }
}

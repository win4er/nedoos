void kernel_main(void) {
    unsigned char* video = (unsigned char*)0xB8000;
    const char* msg = "NedoOS v0.1";
    int i;
    
    // Очистка экрана
    for (i = 0; i < 80*25*2; i++) {
        video[i] = 0;
    }
    
    // Вывод сообщения
    for (i = 0; msg[i] != 0; i++) {
        video[i*2] = msg[i];
        video[i*2+1] = 0x07;
    }
    
    while(1) {
        __asm__("hlt");
    }
}

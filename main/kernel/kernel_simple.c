void kernel_main(void) {
    unsigned char* video_memory = (unsigned char*) 0xB8000;
    const char* message = "NedoOS Kernel v0.0.2";
    int i = 0;
    
    // Очищаем экран
    for (i = 0; i < 80*25*2; i += 2) {
        video_memory[i] = ' ';
        video_memory[i+1] = 0x07;
    }
    
    // Выводим сообщение
    i = 0;
    while (message[i] != '\0') {
        video_memory[i*2] = message[i];
        video_memory[i*2 + 1] = 0x07;
        i++;
    }
    
    // Бесконечный цикл
    while (1) {
        __asm__ volatile("hlt");
    }
}

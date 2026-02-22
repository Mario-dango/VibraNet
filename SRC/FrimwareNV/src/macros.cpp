#include "macros.h"

void resetESP(){
    (*(volatile uint32_t *)(0x60000700 + 0x30)) = 0x10; 
    while(1);
}

void uart_write_char(char c) {
    while (((UART0_STATUS >> UART_TX_FIFO_SHIFT) & UART_TX_FIFO_MASK) >= 126);
    
    UART0_FIFO = c;
}

void uart_print(const char* s) {
    while (*s) {
        uart_write_char(*s++);
    }
}

void uart_println(const char* s) {
    uart_print(s);
    uart_write_char('\r');
    uart_write_char('\n');
}
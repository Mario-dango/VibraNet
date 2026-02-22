// ================= HELPERS VISUALES =================

#include <stdio.h> // Para snprintf
#include "macros.h"
#include "feedback.h"

// Función de retardo de la ROM del ESP8266 (Microsegundos)
extern "C" void ets_delay_us(uint32_t us);

void setStatus(int pinOn) {
    setLOW(LED_YELLOW | LED_GREEN | LED_RED);
    switch(pinOn) {
        case LED_YELLOW: setHIGH(LED_YELLOW); break;
        case LED_GREEN: setHIGH(LED_GREEN); break;
        case LED_RED: setHIGH(LED_RED); break;
        default: break; // Todos apagados
    }
}

void blinkErrorAndRestart() {
    uart_println("!!! FALLO CRITICO - REINICIANDO SISTEMA !!!");
    for(int i=0; i<5; i++) { 
        setHIGH(LED_RED); 
        ets_delay_us(100000); // 100ms
        setLOW(LED_RED); 
        ets_delay_us(100000);
    }
    resetESP();
}

void printCurrentConfig(DeviceConfig config) {
    char buffer[32]; // Buffer para conversión de números

    uart_println("\n--- CONFIGURACION ACTUAL ---");
    uart_print("Modo: "); uart_println(config.mode == 1 ? "BURST (FFT)" : "SNAPSHOT (Acel)");
    
    uart_print("Sleep: "); snprintf(buffer, sizeof(buffer), "%d", config.sleep_time_s); uart_print(buffer); uart_println(" s");
    uart_print("Umbral: "); snprintf(buffer, sizeof(buffer), "%d", config.mpu_threshold); uart_println(buffer);
    uart_print("Freq: "); snprintf(buffer, sizeof(buffer), "%d", config.sampling_freq); uart_println(buffer);
    
    uart_println("----------------------------\n");
}
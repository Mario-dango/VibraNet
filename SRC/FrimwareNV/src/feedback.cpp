
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
    Serial.println("!!! FALLO CRITICO - REINICIANDO SISTEMA !!!");
    for(int i=0; i<5; i++) { 
        setHIGH(LED_RED); 
        ets_delay_us(100000); 
        setLOW(LED_RED);
        ets_delay_us(100000); }
    reinicio_ESP();
}

void printCurrentConfig(const DeviceConfig &config) {
    Serial.println("\n--- CONFIGURACION ACTUAL ---");
    Serial.print("Modo: "); Serial.println(config.mode == 1 ? "BURST (FFT)" : "SNAPSHOT (Acel)");
    Serial.print("Sleep: "); Serial.print(config.sleep_time_s); Serial.println(" s");
    Serial.print("Umbral: "); Serial.println(config.mpu_threshold);
    Serial.print("Freq: "); Serial.println(config.sampling_freq);
    Serial.println("----------------------------\n");
}
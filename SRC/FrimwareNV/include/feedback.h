// ================= HELPERS VISUALES =================

#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <stdint.h>

// Definimos la estructura aquí para compartirla entre archivos
struct DeviceConfig {
    int mode; 
    int sleep_time_s; 
    int mpu_threshold; 
    int sampling_freq; 
    uint32_t burst_size;
};

void setStatus(int pinOn);

void blinkErrorAndRestart();

void printCurrentConfig(DeviceConfig config);

#endif // FEEDBACK_H
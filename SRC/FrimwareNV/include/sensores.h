#ifndef SENSORES_H
#define SENSORES_H

#include <stdint.h>

// Forward declaration to avoid including Adafruit_MPU6050.h here
class Adafruit_MPU6050;

// ================= I2C RECOVERY =================

void aggressiveBusRecover();

void groundingPins();

void blindSensorReset(uint8_t addr);

// ================= CONFIG SENSORES =================
void setupMPU_Latch(Adafruit_MPU6050 &mpu, int MPU_ADDR);

// Función para limpiar la interrupción del MPU (Desbloquear el Latch)
void clearMPUInterrupt(uint8_t addr);

// ================= CAPTURA SNAPSHOT TRIAXIAL (X, Y, Z) =================
bool runSnapshotMode(Adafruit_MPU6050 &mpu, String sensorName);

// ================= CAPTURA BURST TRIAXIAL =================
bool performBurstCapture(Adafruit_MPU6050 &mpu, String sensorName);

// ================= LEER VOLTAJE DE BATERÍA =================
float leerBateria();

#endif // SENSORES_H
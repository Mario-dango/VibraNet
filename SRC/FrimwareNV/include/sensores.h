#ifndef SENSORES_H
#define SENSORES_H

#include <stdio.h>
#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <new>

#include "macros.h"
#include "string.h"

// ================= I2C RECOVERY =================
void aggressiveBusRecover();

void groundingPins();

void blindSensorReset(uint8_t addr);

// ================= CONFIG SENSORES =================
void setupMPU_Latch(
    Adafruit_MPU6050 &mpu, 
    int MPU_ADDR, 
    DeviceConfig &config
);

// Función para limpiar la interrupción del MPU (Desbloquear el Latch)
void clearMPUInterrupt(uint8_t addr);

// ================= CAPTURA SNAPSHOT TRIAXIAL (X, Y, Z) =================
bool runSnapshotMode(
    Adafruit_MPU6050 &mpu, 
    String sensorName, 
    PubSubClient &client, 
    const char* node_id
);

// ================= CAPTURA BURST TRIAXIAL =================
bool performBurstCapture(
    Adafruit_MPU6050 &mpu, 
    const String sensorName, 
    const char node_id[NODE_ID_SIZE], 
    PubSubClient &client, 
    DeviceConfig &config
);

// ================= LEER VOLTAJE DE BATERÍA =================
float leerBateria();

#endif // SENSORES_H
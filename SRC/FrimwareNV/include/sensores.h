#ifndef SENSORES_H
#define SENSORES_H

struct DeviceConfig; // Declaración adelantada para evitar dependencias circulares

// ================= I2C RECOVERY =================
void aggressiveBusRecover();

void groundingPins();

void blindSensorReset(uint8_t addr);

// ================= CONFIG SENSORES =================
void setupMPU_Latch(Adafruit_MPU6050 &mpu, int MPU_ADDR, DeviceConfig config);

// Función para limpiar la interrupción del MPU (Desbloquear el Latch)
void clearMPUInterrupt(uint8_t addr);

// ================= CAPTURA SNAPSHOT TRIAXIAL (X, Y, Z) =================
bool runSnapshotMode(Adafruit_MPU6050 &mpu, String sensorName, PubSubClient &client, String node_id);

// ================= CAPTURA BURST TRIAXIAL =================
bool performBurstCapture(Adafruit_MPU6050 &mpu, String sensorName, String node_id, PubSubClient &client, DeviceConfig config);

// ================= LEER VOLTAJE DE BATERÍA =================
float leerBateria();

#endif // SENSORES_H
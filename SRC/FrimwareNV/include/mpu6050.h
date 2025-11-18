#ifndef MPU6050_H
#define MPU6050_H
#include <stdint.h>
#include <stdbool.h>

// Definir uint8 si no está definido
typedef uint8_t uint8;

#include "i2c_master.h"

typedef struct {
    float x;
    float y;
    float z;
} accel_data_t;

typedef struct {
    float x;
    float y;
    float z;
} gyro_data_t;

typedef struct {
    accel_data_t accel;
    gyro_data_t gyro;
    uint32_t timestamp;
} sensor_data_t;

bool mpu6050_init(void);
bool mpu6050_read_data(sensor_data_t *data);

#endif // MPU6050_H
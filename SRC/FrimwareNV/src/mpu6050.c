#include "mpu6050.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>

// Registros MPU6050
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_SMPLRT_DIV   0x19
#define MPU6050_CONFIG       0x1A
#define MPU6050_GYRO_CONFIG  0x1B
#define MPU6050_ACCEL_CONFIG 0x1C

static bool write_reg(uint8_t reg, uint8_t data) {
    i2c_master_start();
    i2c_master_writeByte(MPU_ADDR << 1);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return false;
    }
    
    i2c_master_writeByte(reg);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return false;
    }
    
    i2c_master_writeByte(data);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return false;
    }
    
    i2c_master_stop();
    return true;
}

bool mpu6050_init(void) {
    // Inicializar I2C
    i2c_master_gpio_init();
    
    // Despertar el MPU6050
    if (!write_reg(MPU6050_PWR_MGMT_1, 0x00)) {
        return false;
    }
    
    // Configurar tasa de muestreo
    if (!write_reg(MPU6050_SMPLRT_DIV, 0x07)) {
        return false;
    }
    
    // Configurar filtro paso bajo
    if (!write_reg(MPU6050_CONFIG, 0x06)) {
        return false;
    }
    
    // Configurar rango del giroscopio a ±250°/s
    if (!write_reg(MPU6050_GYRO_CONFIG, 0x00)) {
        return false;
    }
    
    // Configurar rango del acelerómetro a ±2g
    if (!write_reg(MPU6050_ACCEL_CONFIG, 0x00)) {
        return false;
    }
    
    return true;
}

bool mpu6050_read_data(sensor_data_t *data) {
    uint8_t buffer[14];
    int16_t raw_data[7];
    
    // Leer 14 bytes de datos empezando desde ACCEL_XOUT_H
    i2c_master_start();
    i2c_master_writeByte((MPU_ADDR << 1));
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return false;
    }
    
    i2c_master_writeByte(MPU6050_ACCEL_XOUT_H);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return false;
    }
    
    i2c_master_start();
    i2c_master_writeByte((MPU_ADDR << 1) | 0x01);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return false;
    }
    
    for (int i = 0; i < 13; i++) {
        buffer[i] = i2c_master_readByte();
        i2c_master_send_ack();
    }
    buffer[13] = i2c_master_readByte();
    i2c_master_send_nack();
    i2c_master_stop();
    
    // Convertir bytes a valores raw
    for (int i = 0; i < 7; i++) {
        raw_data[i] = (buffer[i*2] << 8) | buffer[i*2+1];
    }
    
    // Convertir a valores físicos
    data->accel.x = raw_data[0] / 16384.0f;
    data->accel.y = raw_data[1] / 16384.0f;
    data->accel.z = raw_data[2] / 16384.0f;
    data->gyro.x = raw_data[4] / 131.0f;
    data->gyro.y = raw_data[5] / 131.0f;
    data->gyro.z = raw_data[6] / 131.0f;
    
    // Agregar timestamp
    data->timestamp = system_get_time() / 1000; // Convertir a milisegundos
    
    return true;
}
#ifndef CONFIG_H
#define CONFIG_H

// Identificación del nodo
#define NODE_ID "node001"

// Configuración del sensor
#define MPU_ADDR 0x68
#define SAMPLE_RATE_HZ 100

// Configuración WiFi
#define WIFI_SSID "TU_SSID"
#define WIFI_PASS "TU_PASSWORD"

// Tamaños de buffer y colas
#define SENSOR_QUEUE_SIZE 10

#endif // CONFIG_H
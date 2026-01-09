#ifndef CONFIG_H
#define CONFIG_H

// Identificación del nodo
#define NODE_ID "Dango_Node_001"

// Configuración del sensor
// MPU1 siempre en 0x68. MPU2 (opcional) debe tener el pin AD0 a 3.3V para ser 0x69
#define MPU1_ADDR 0x68 
#define MPU2_ADDR 0x69

// Configuración WiFi
#define WIFI_SSID "PAPETTI"
#define WIFI_PASS "Chulito$26"

// Configuración WiFi
// #define WIFI_SSID "MovistarFibra-5GHz-55E0E8"
// #define WIFI_PASS "L2QTE2P4UrJECosNhNtP"

// Configuración MQTT
#define MQTT_SERVER "192.168.100.68" // ¡PON TU IP DE DOCKER/MOSQUITTO!
#define MQTT_PORT 1883
#define MQTT_TOPIC "vibranet/data"

#endif // CONFIG_H
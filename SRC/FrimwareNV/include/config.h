#ifndef CONFIG_H
#define CONFIG_H

// Identificación del nodo
#define NODE_ID "Dango_Node_001"

// Configuración del sensor
// MPU1 siempre en 0x68. MPU2 (opcional) debe tener el pin AD0 a 3.3V para ser 0x69
#define MPU1_ADDR 0x68 
#define MPU2_ADDR 0x69

// Configuración WiFi
// #define WIFI_SSID "PAPETTI"
// #define WIFI_PASS "Chulito$26"

// Configuración WiFi
#define WIFI_SSID "bawy"
#define WIFI_PASS "Bawbaaw42"

// Configuración WiFi
// #define WIFI_SSID "MovistarFibra-5GHz-55E0E8"
// #define WIFI_PASS "L2QTE2P4UrJECosNhNtP"

// Configuración MQTT
// #define MQTT_SERVER "192.168.100.68" // ¡PON TU IP DE DOCKER/MOSQUITTO!

#define MQTT_SERVER "10.81.207.250" // ¡PON TU IP DE DOCKER/MOSQUITTO!

#define MQTT_PORT 1883
#define MQTT_TOPIC "vibranet/data"

#endif // CONFIG_H

/*
docker run cloudflare/cloudflared:latest tunnel --no-autoupdate run --token eyJhIjoiMTI0ODYyYzg3MDM1NzI0Y2ZhNmI3NjYwYjIyZTA2MTEiLCJ0IjoiNmJkMjllMTEtMTM4Ny00ZTliLWFhYmItMjdmODBkZmJhYWI4IiwicyI6Ik1UUXdZMll4TUdFdFlUVm1NUzAwTWpnd0xXSTRaVGd0WXpFMk9HTmlOR1F6TURCaiJ9
*/
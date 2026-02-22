#ifndef CONFIG_H
#define CONFIG_H

// Configuración del sensor
// MPU1 siempre en 0x68. MPU2 (opcional) debe tener el pin AD0 a 3.3V para ser 0x69
#define MPU1_ADDR 0x68 
#define MPU2_ADDR 0x69

// Configuración WiFi
#define WIFI_SSID "bawy"
#define WIFI_PASS "Bawbaaw42"

// Configuración WiFi
#define WIFI_SSID "momantai"
#define WIFI_PASS "42425640"

// Configuración MQTT
// IP de docker/mosquito
#define MQTT_SERVER "10.81.207.250" 

#define MQTT_PORT 1883
#define MQTT_TOPIC "vibranet/data"

#define LED_PIN 2
#define BATTERY_PIN A0

// #define NODE_ID "dango_node_001"   // Vibranet on protoboard
#define NODE_ID "dango_node_002"   // Vibranet on PCB
// #define NODE_ID "dango_node_003"   // Vibranet on protoboard

#endif // CONFIG_H

/*
docker run cloudflare/cloudflared:latest tunnel --no-autoupdate run --token eyJhIjoiMTI0ODYyYzg3MDM1NzI0Y2ZhNmI3NjYwYjIyZTA2MTEiLCJ0IjoiNmJkMjllMTEtMTM4Ny00ZTliLWFhYmItMjdmODBkZmJhYWI4IiwicyI6Ik1UUXdZMll4TUdFdFlUVm1NUzAwTWpnd0xXSTRaVGd0WXpFMk9HTmlOR1F6TURCaiJ9
*/
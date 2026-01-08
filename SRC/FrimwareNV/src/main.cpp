/**
 * main.cpp - Dango Prototype + LED Status
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFiManager.h>
#include <TaskScheduler.h>

#include "config.h" 

// ================= DEFINICIONES HARDWARE =================
#define LED_PIN 2 // D4 en Wemos/NodeMCU (LED Azul Integrado)

// ================= OBJETOS GLOBALES =================
WiFiClient espClient;
PubSubClient client(espClient);
Scheduler runner;

Adafruit_MPU6050 mpu1;
Adafruit_MPU6050 mpu2;
bool mpu1_active = false;
bool mpu2_active = false;

struct SensorReadings {
    float ax1, ay1, az1, gx1, gy1, gz1;
    float ax2, ay2, az2, gx2, gy2, gz2;
    bool ready;
} data;

// Variable para controlar parpadeo de error
bool ledState = HIGH; 

// ================= PROTOTIPOS =================
void taskReadSensorsCb();
void taskMqttPublishCb();
void taskConnectionWatchdogCb();
void taskErrorBlinkCb(); // <--- NUEVA TAREA
void setupSensors();
void flashLed();         // <--- NUEVA FUNCION

// ================= TAREAS (SCHEDULER) =================
Task tRead(100, TASK_FOREVER, &taskReadSensorsCb);
Task tPublish(500, TASK_FOREVER, &taskMqttPublishCb);
Task tWatchdog(5000, TASK_FOREVER, &taskConnectionWatchdogCb); // Bajamos a 5s para feedback más rápido
Task tErrorBlink(500, TASK_FOREVER, &taskErrorBlinkCb); // <--- Parpadeo de error (0.5s)

// ================= IMPLEMENTACIÓN =================

// Función auxiliar para un flash rápido (Bloqueante pero muy breve, 50ms)
void flashLed() {
    digitalWrite(LED_PIN, LOW);  // Encender
    delay(50);                   
    digitalWrite(LED_PIN, HIGH); // Apagar
}

void setupSensors() {
    Wire.begin(); 
    Serial.print("Iniciando MPU 1 (0x68)... ");
    if (mpu1.begin(MPU1_ADDR)) {
        mpu1.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu1.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu1.setFilterBandwidth(MPU6050_BAND_21_HZ);
        mpu1_active = true;
        Serial.println("OK!");
    } else {
        Serial.println("FALLÓ");
    }

    Serial.print("Iniciando MPU 2 (0x69)... ");
    if (mpu2.begin(MPU2_ADDR)) {
        mpu2.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu2.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu2.setFilterBandwidth(MPU6050_BAND_21_HZ);
        mpu2_active = true;
        Serial.println("OK!");
    } else {
        Serial.println("No detectado");
    }
}

// --- TAREA: LEER SENSORES ---
void taskReadSensorsCb() {
    sensors_event_t a, g, temp;
    if (mpu1_active) {
        mpu1.getEvent(&a, &g, &temp);
        data.ax1 = a.acceleration.x; data.ay1 = a.acceleration.y; data.az1 = a.acceleration.z;
    }
    if (mpu2_active) {
        mpu2.getEvent(&a, &g, &temp);
        data.ax2 = a.acceleration.x; data.ay2 = a.acceleration.y; data.az2 = a.acceleration.z;
    }
    data.ready = true;
}

// --- TAREA: ENVIAR MQTT (ESTADO OPERATIVO) ---
void taskMqttPublishCb() {
    // Solo enviamos si hay conexión MQTT
    if (!client.connected() || !data.ready) return;

    char payload[256];
    snprintf(payload, sizeof(payload), 
        "{\"id\":\"%s\", \"s1\":{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f}, \"s2\":{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f}}",
        NODE_ID,
        data.ax1, data.ay1, data.az1, 
        data.ax2, data.ay2, data.az2
    );

    if(client.publish(MQTT_TOPIC, payload)) {
        // <--- ÉXITO: Hacemos un flash rápido
        flashLed(); 
    }
    data.ready = false;
}

// --- TAREA: PARPADEO DE ERROR ---
void taskErrorBlinkCb() {
    // Esta tarea solo hace parpadear el LED.
    // La activamos o desactivamos desde el Watchdog.
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
}

// --- TAREA: WATCHDOG (GESTOR DE ESTADOS) ---
void taskConnectionWatchdogCb() {
    // 1. CHEQUEO WIFI
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi perdido. Reconectando...");
        WiFi.reconnect(); 
        return; 
    }

    // 2. CHEQUEO MQTT
    if (!client.connected()) {
        // <--- ESTADO ERROR: Activar parpadeo
        if (!tErrorBlink.isEnabled()) {
            tErrorBlink.enable(); 
            Serial.println("Estado: Error MQTT (Parpadeo activado)");
        }

        Serial.print("Conectando MQTT...");
        String clientId = String(NODE_ID) + "_" + String(random(0xffff), HEX);
        if (client.connect(clientId.c_str())) {
            Serial.println("Conectado!");
            client.publish("dango/status", "online");
            
            // <--- CONECTADO: Desactivar parpadeo y apagar LED
            tErrorBlink.disable();
            digitalWrite(LED_PIN, HIGH); // Apagar (Lógica inversa)
        }
    } else {
        // Si estamos conectados, asegurar que el parpadeo de error está apagado
        if (tErrorBlink.isEnabled()) {
            tErrorBlink.disable();
            digitalWrite(LED_PIN, HIGH);
        }
    }
}

// ================= SETUP =================
void setup() {
    Serial.begin(115200);
    
    // <--- CONFIGURAR LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // Empezar apagado

    setupSensors();

    WiFiManager wifiManager;
    
    // <--- CALLBACK MODO AP (PORTAL CAUTIVO)
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
        Serial.println("MODO CONFIGURACION ACTIVADO");
        digitalWrite(LED_PIN, LOW); // <--- LED ENCENDIDO FIJO (Atención)
    });

    wifiManager.setTimeout(180); 
    
    if(!wifiManager.autoConnect("Dango_Config")) {
        Serial.println("Timeout. Reiniciando...");
        ESP.reset(); 
        delay(1000);
    }

    // Al salir del portal, apagamos el LED momentáneamente
    digitalWrite(LED_PIN, HIGH); 
    Serial.println("WiFi OK");

    client.setServer(MQTT_SERVER, MQTT_PORT);

    runner.init();
    runner.addTask(tRead);
    runner.addTask(tPublish);
    runner.addTask(tWatchdog);
    runner.addTask(tErrorBlink); // <--- Agregar tarea led
    
    tRead.enable();
    tPublish.enable();
    tWatchdog.enable();
    // tErrorBlink empieza deshabilitada, se habilita si falla MQTT
}

void loop() {
    runner.execute(); 
    client.loop();    
}
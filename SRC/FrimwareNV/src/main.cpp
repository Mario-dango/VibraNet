/**
 * main.cpp - Vibranet Node v2.1 (Burst Mode Ready)
 * Soporta Wemos D1 Mini y ESP07
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

// ================= CONFIGURACIÓN BURST (ANÁLISIS FRECUENCIA) =================
// 256 muestras es suficiente para detectar fundamentales y armónicos básicos.
// Si subes a 512, vigila la RAM del ESP8266.
#define BURST_SAMPLES 256 
#define SAMPLING_FREQ 500 // Hz. Muestrea cada 2ms. (Nyquist max: 250Hz visible)
const unsigned long SAMPLING_PERIOD_US = 1000000 / SAMPLING_FREQ;

// ================= DEFINICIONES HARDWARE =================
#define LED_PIN 2      
#define BATTERY_PIN A0 
const float BATTERY_FACTOR = 0.0042; 

// ================= OBJETOS GLOBALES =================
WiFiClient espClient;
PubSubClient client(espClient);
Scheduler runner;

Adafruit_MPU6050 mpu1;
Adafruit_MPU6050 mpu2;
bool mpu1_active = false;
bool mpu2_active = false;

struct SensorReadings {
    float ax1, ay1, az1;
    float ax2, ay2, az2;
    bool ready;
} data;

// Variables de estado
bool ledState = HIGH; 
long publishInterval = 1000; 

// ================= PROTOTIPOS =================
void taskReadSensorsCb();
void taskMqttPublishCb();
void taskConnectionWatchdogCb();
void taskErrorBlinkCb();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void setupSensors();
void flashLed();
float getBatteryVoltage(); 
void performBurstCapture(); // <--- NUEVA FUNCIÓN DE CAPTURA

// ================= TAREAS (SCHEDULER) =================
Task tRead(100, TASK_FOREVER, &taskReadSensorsCb);
Task tPublish(publishInterval, TASK_FOREVER, &taskMqttPublishCb);
Task tWatchdog(5000, TASK_FOREVER, &taskConnectionWatchdogCb);
Task tErrorBlink(500, TASK_FOREVER, &taskErrorBlinkCb);

// ================= IMPLEMENTACIÓN =================

float getBatteryVoltage() {
    int raw = analogRead(BATTERY_PIN);
    if (raw < 10) return 0.0;
    return raw * BATTERY_FACTOR; 
}

void flashLed() {
    digitalWrite(LED_PIN, LOW); delay(50); digitalWrite(LED_PIN, HIGH);
}

// --- LÓGICA DE CAPTURA DE ALTA VELOCIDAD (BURST) ---
// --- FUNCIÓN DE CAPTURA DE ALTA VELOCIDAD (BURST) OPTIMIZADA ---
void performBurstCapture() {
    // 1. Pausar tareas
    tRead.disable();
    tPublish.disable();
    
    Serial.println(F(">>> INICIANDO CAPTURA BURST <<<"));
    flashLed();

    // 2. Asignación Dinámica
    float *vBuffer1 = nullptr;
    float *vBuffer2 = nullptr;

    // Intentamos asignar memoria
    if (mpu1_active) vBuffer1 = new float[BURST_SAMPLES];
    if (mpu2_active) vBuffer2 = new float[BURST_SAMPLES];

    // Verificación de seguridad: Si falta RAM, abortamos antes de que explote
    if ((mpu1_active && !vBuffer1) || (mpu2_active && !vBuffer2)) {
        Serial.println(F("ERROR CRÍTICO: Fallo de asignación de RAM"));
        if (vBuffer1) delete[] vBuffer1;
        if (vBuffer2) delete[] vBuffer2;
        tRead.enable(); tPublish.enable();
        return;
    }

    // 3. Bucle de Captura (Tiempo Real)
    unsigned long nextSampleTime = micros();
    
    for(int i=0; i<BURST_SAMPLES; i++) {
        sensors_event_t a, g, temp;
        
        while(micros() < nextSampleTime) {
            yield(); // Mantiene al Watchdog contento durante la espera
        }

        if (mpu1_active) {
            mpu1.getEvent(&a, &g, &temp);
            vBuffer1[i] = a.acceleration.x; 
        }
        if (mpu2_active) {
            mpu2.getEvent(&a, &g, &temp);
            vBuffer2[i] = a.acceleration.x;
        }

        nextSampleTime += SAMPLING_PERIOD_US;
    }

    Serial.println(F("Captura lista. Empaquetando..."));

    // 4. Empaquetar y Enviar (OPTIMIZADO)
    
    if (mpu1_active) {
        String payload;
        // TRUCO DE ORO: Reservar memoria de antemano.
        // 256 muestras * ~6 chars por numero + cabeceras JSON = ~2000 bytes
        payload.reserve(2500); 

        payload = "{\"id\":\"" + String(NODE_ID) + "\", \"type\":\"burst\", \"sensor\":\"s1\", \"fs\":" + String(SAMPLING_FREQ) + ", \"data\":[";
        
        for(int i=0; i<BURST_SAMPLES; i++) {
            payload += String(vBuffer1[i], 2);
            if(i < BURST_SAMPLES-1) payload += ",";
            
            // ALIMENTAR AL PERRO: Cada 50 números, dejamos respirar al chip
            if (i % 50 == 0) yield(); 
        }
        payload += "]}";
        
        client.publish("vibranet/burst", payload.c_str());
        Serial.print(F("Burst S1 enviado. Bytes: ")); Serial.println(payload.length());
        
        // Liberar memoria del String inmediatamente forzando vaciado (opcional pero sano)
        payload = ""; 
        delete[] vBuffer1; 
        delay(50); 
    }

    if (mpu2_active) {
        String payload;
        payload.reserve(2500); // Reservar memoria

        payload = "{\"id\":\"" + String(NODE_ID) + "\", \"type\":\"burst\", \"sensor\":\"s2\", \"fs\":" + String(SAMPLING_FREQ) + ", \"data\":[";
        
        for(int i=0; i<BURST_SAMPLES; i++) {
            payload += String(vBuffer2[i], 2);
            if(i < BURST_SAMPLES-1) payload += ",";
            
            if (i % 50 == 0) yield(); // <--- IMPORTANTE
        }
        payload += "]}";
        
        client.publish("vibranet/burst", payload.c_str());
        Serial.print(F("Burst S2 enviado. Bytes: ")); Serial.println(payload.length());
        delete[] vBuffer2;
    }

    // 5. Restaurar normalidad
    tRead.enable();
    tPublish.enable();
    flashLed();
}

// Callback modificado para escuchar el comando "BURST"
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) message += (char)payload[i];
    
    String configTopic = "vibranet/config/" + String(NODE_ID);
    
    if (String(topic) == configTopic) {
        Serial.print("Comando recibido: "); Serial.println(message);

        // Si recibimos "BURST", iniciamos secuencia
        if (message == "BURST") {
            performBurstCapture(); 
        } 
        else {
            // Lógica anterior de intervalo
            long newTime = message.toInt();
            if (newTime >= 100 && newTime <= 60000) {
                publishInterval = newTime;
                tPublish.setInterval(publishInterval);
            }
        }
    }
}

void setupSensors() {
    Wire.begin(); 
    // Configuramos los sensores
    if (mpu1.begin(MPU1_ADDR)) { 
        mpu1_active = true; 
        mpu1.setAccelerometerRange(MPU6050_RANGE_16_G); // Rango alto para impactos
        mpu1.setFilterBandwidth(MPU6050_BAND_260_HZ);    // Filtro abierto para FFT
        Serial.println("MPU1 Detectado");
    }
    if (mpu2.begin(MPU2_ADDR)) { 
        mpu2_active = true; 
        mpu2.setAccelerometerRange(MPU6050_RANGE_16_G);
        mpu2.setFilterBandwidth(MPU6050_BAND_260_HZ);
        Serial.println("MPU2 Detectado");
    }
}

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

void taskMqttPublishCb() {
    if (!client.connected() || !data.ready) return;
    char payload[350]; 
    String ssid = WiFi.SSID();
    float vbat = getBatteryVoltage();

    snprintf(payload, sizeof(payload), 
        "{\"id\":\"%s\", \"wifi\":\"%s\", \"vcc\":%.2f, \"s1\":{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f}, \"s2\":{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f}}",
        NODE_ID, ssid.c_str(), vbat,
        data.ax1, data.ay1, data.az1, data.ax2, data.ay2, data.az2
    );
    client.publish(MQTT_TOPIC, payload);
    data.ready = false;
    flashLed();
}

void taskConnectionWatchdogCb() {
    if (WiFi.status() != WL_CONNECTED) { WiFi.reconnect(); return; }
    if (!client.connected()) {
        if (!tErrorBlink.isEnabled()) tErrorBlink.enable(); 
        
        String clientId = String(NODE_ID) + "_" + String(random(0xffff), HEX);
        if (client.connect(clientId.c_str())) {
            
            String configTopic = "vibranet/config/" + String(NODE_ID);
            client.subscribe(configTopic.c_str());
            Serial.println("Reconectado a MQTT");

            // IMPORTANTE: Asegurar buffer de recepción/envío 
            client.setBufferSize(4096); 
            
            client.publish("dango/status", "online");
            tErrorBlink.disable();
            digitalWrite(LED_PIN, HIGH); 
        }
    } else {
        if (tErrorBlink.isEnabled()) {
            tErrorBlink.disable();
            digitalWrite(LED_PIN, HIGH);
        }
    }
}

void taskErrorBlinkCb() {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    setupSensors();

    WiFiManager wifiManager;
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
        digitalWrite(LED_PIN, LOW);
    });
    wifiManager.setTimeout(180); 
    if(!wifiManager.autoConnect("Dango_Config")) {
        ESP.reset(); delay(1000);
    }
    digitalWrite(LED_PIN, HIGH); 

    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(mqttCallback);
    // Configurar buffer antes de conectar
    client.setBufferSize(4096); 

    runner.init();
    runner.addTask(tRead); runner.addTask(tPublish); runner.addTask(tWatchdog); runner.addTask(tErrorBlink);
    
    tRead.enable(); tPublish.enable(); tWatchdog.enable();
}

void loop() {
    runner.execute(); 
    client.loop();    
}
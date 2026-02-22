/**
 * New branch
 * 
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFiManager.h> 
#include <FS.h>
#include <LittleFS.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>

#include "config.h" 
#include "macros.h"
#include "feedback.h"
#include "sensores.h"

// VERSIONES
#define HW_VER "1.5"
#define SW_VER "6.1" // Actualizado para reflejar el parche de energía

// VARIABLES

DeviceConfig config = {1, 60, 10, 500, 256};  

char node_id[32] = "dango_node_002"; // Valor por defecto si no hay config guardada
char mqtt_server[40] = ""; char mqtt_port[6] = "1883";

WiFiClient espClient; PubSubClient client(espClient); Adafruit_MPU6050 mpu1; Adafruit_MPU6050 mpu2;

bool mpu1_active = false; bool mpu2_active = false; bool shouldSaveConfig = false;

// ================= CONFIG JSON =================
void loadMQTTConfig() { if (LittleFS.exists("/mqtt_config.json")) { File f = LittleFS.open("/mqtt_config.json", "r"); if (f) { DynamicJsonDocument doc(512); deserializeJson(doc, f); strcpy(mqtt_server, doc["server"]); strcpy(mqtt_port, doc["port"]); f.close(); } } }
void saveMQTTConfig() { DynamicJsonDocument doc(512); doc["server"] = mqtt_server; doc["port"] = mqtt_port; File f = LittleFS.open("/mqtt_config.json", "w"); if (f) { serializeJson(doc, f); f.close(); } }
void loadSystemConfig() { if (LittleFS.exists("/sys_config.json")) { File f = LittleFS.open("/sys_config.json", "r"); if (f) { DynamicJsonDocument doc(512); deserializeJson(doc, f); config.mode = doc["mode"]|1; config.sleep_time_s = doc["sleep"]|900; config.mpu_threshold = doc["thr"]|20; config.sampling_freq = doc["freq"]|500; config.burst_size = doc["bsize"]|256; if(doc.containsKey("id")) strlcpy(node_id, doc["id"], sizeof(node_id)); f.close(); } } }
void saveSystemConfig() { File f = LittleFS.open("/sys_config.json", "w"); if (!f) return; DynamicJsonDocument doc(512); doc["mode"]=config.mode; doc["sleep"]=config.sleep_time_s; doc["thr"]=config.mpu_threshold; doc["freq"]=config.sampling_freq; doc["bsize"]=config.burst_size; doc["id"] = node_id; serializeJson(doc, f); f.close(); }
void saveConfigCallback() { shouldSaveConfig = true; }


// ================= HEARTBEAT & CALLBACKS =================
void publishStatus(String reason) {
    StaticJsonDocument<512> doc; // Aumentamos tamaño para SSID y versiones
    doc["id"] = node_id;
    doc["t"] = "status";
    doc["reason"] = reason;
    
    // Datos Dinámicos
    float vcc = leerBateria(); 
    doc["bat"] = serialized(String(vcc, 2));
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["ssid"] = WiFi.SSID(); // <--- NUEVO: Red actual
    
    // Datos Estáticos (Versiones)
    doc["hw"] = HW_VER;       // <--- NUEVO
    doc["sw"] = SW_VER;       // <--- NUEVO
    
    // Configuración
    JsonObject cfg = doc.createNestedObject("conf");
    cfg["mode"] = config.mode; 
    cfg["sleep"] = config.sleep_time_s; 
    cfg["thr"] = config.mpu_threshold; 
    cfg["fs"] = config.sampling_freq;

    char buffer[512];
    serializeJson(doc, buffer);
    
    String statusTopic = "vibranet/status/" + String(node_id);
    client.publish(statusTopic.c_str(), buffer, true);
    Serial.println(">> Status enviado (v" + String(SW_VER) + ") en red: " + WiFi.SSID());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) { Serial.println("Error JSON Config"); return; }

    bool changed = false; bool restartRequired = false; // Bandera para saber si reiniciamos
    Serial.println("\n--- Recibiendo Config Remota ---");
    if (doc.containsKey("id")) {
        const char* newId = doc["id"];
        if (strcmp(node_id, newId) != 0) { // Solo si es diferente
            strlcpy(node_id, newId, sizeof(node_id));
            Serial.println("Update NODE ID -> " + String(node_id));
            changed = true;
            restartRequired = true; // El ID requiere reinicio obligatorio
        }
    }
    if (doc.containsKey("sleep")) { config.sleep_time_s = doc["sleep"]; Serial.println("Update Sleep -> " + String(config.sleep_time_s)); changed = true; }
    if (doc.containsKey("mode"))  { config.mode = doc["mode"]; Serial.println("Update Mode -> " + String(config.mode)); changed = true; }
    if (doc.containsKey("thr"))   { config.mpu_threshold = doc["thr"]; Serial.println("Update Thr -> " + String(config.mpu_threshold)); changed = true; }
    if (doc.containsKey("fs"))    { config.sampling_freq = doc["freq"]; Serial.println("Update Freq -> " + String(config.sampling_freq)); changed = true; }

    if (changed) {
        saveSystemConfig();
        publishStatus("config_updated"); 
        printCurrentConfig(); // Muestra la config final tras el cambio
        if (restartRequired) {
            Serial.println(">> ID CAMBIADO. REINICIANDO EN 1s...");
            delay(1000);
            ESP.restart();
        }
    }
}
// ================= SETUP =================
void setup() {
    // 1. Iniciar Serial y esperar estabilización
    delay(500);
    Serial.begin(115200);
    Serial.println("\n\n>>> ARRANQUE VIBRANET V6.2 (LOW POWER MODE) <<<");

    // -----------------------------------------------------------
    // TRUCO SALVAVIDAS: LIMITAR CORRIENTE DEL WIFI
    // -----------------------------------------------------------
    // Forzamos modo estación primero
    WiFi.mode(WIFI_STA);
    
    // Ponemos la potencia de salida al MÍNIMO (0 dBm)
    // Esto reduce el pico de consumo drásticamente.
    // Rango: 0 (min) a 20.5 (max)
    WiFi.setOutputPower(0); 
    // -----------------------------------------------------------

    aggressiveBusRecover(); groundingPins();
    pinMode(LED_YELLOW, OUTPUT); pinMode(LED_GREEN, OUTPUT); pinMode(LED_RED, OUTPUT);
    setStatus(LED_YELLOW); 

    LittleFS.begin(); loadMQTTConfig(); loadSystemConfig(); 
    
    // DEBUG: Ver qué ID estamos usando
    Serial.print("Node ID: "); Serial.println(node_id);
    printCurrentConfig();

    Wire.begin(); Wire.setClock(10000); delay(50);
    blindSensorReset(MPU1_ADDR); blindSensorReset(MPU2_ADDR); delay(100); 
    Wire.setClock(100000); 
    
    if (!mpu1.begin(MPU1_ADDR)) { Serial.println("ERROR: MPU1 no responde."); blinkErrorAndRestart(); } 
    else { Serial.println("MPU1 OK."); mpu1_active = true; mpu1.setAccelerometerRange(MPU6050_RANGE_4_G); mpu1.setFilterBandwidth(MPU6050_BAND_21_HZ); }
    
    if (mpu2.begin(MPU2_ADDR)) { Serial.println("MPU2 OK."); mpu2_active = true; mpu2.setAccelerometerRange(MPU6050_RANGE_4_G); mpu2.setFilterBandwidth(MPU6050_BAND_21_HZ); }
    
    WiFiManager wm; 
    wm.setSaveConfigCallback(saveConfigCallback);
    
    // AUMENTA EL TIMEOUT para darle tiempo si la señal es débil por la baja potencia
    wm.setConnectTimeout(30); 
    wm.setConfigPortalTimeout(120);

    WiFiManagerParameter custom_mqtt_server("server", "MQTT Server IP", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
    wm.addParameter(&custom_mqtt_server); wm.addParameter(&custom_mqtt_port);
    
    if (!wm.autoConnect("VibraNet_AP")) {
        Serial.println("WiFi Fail");
        // Si falla, dormimos SIN radio para ahorrar energía
        ESP.deepSleep(5e6, WAKE_RF_DISABLED);
    }

    // --- OPCIONAL: SUBIDA DE POTENCIA ---
    // COMENTADO POR SEGURIDAD: Si usas el conversor USB-TTL, descomentar esto 
    // podría causar un reinicio justo ahora. Déjalo comentado para la prueba.
    // WiFi.setOutputPower(10); 

    if (shouldSaveConfig) { strcpy(mqtt_server, custom_mqtt_server.getValue()); strcpy(mqtt_port, custom_mqtt_port.getValue()); saveMQTTConfig(); }
    
    // CORRECCIÓN: Borrada la línea duplicada que tenías aquí
    client.setServer(mqtt_server, atoi(mqtt_port)); client.setCallback(mqttCallback); client.setBufferSize(8192);

    // Usamos la variable 'node_id' en lugar de la constante NODE_ID
    String clientId = String(node_id) + "_" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
        Serial.println("MQTT Conectado.");
        // Suscripción dinámica basada en el nombre actual
        client.subscribe(("vibranet/config/"+String(node_id)).c_str());

        // --- SALA DE ESPERA (500ms) ---
        Serial.print("Esperando config remota...");
        unsigned long startWait = millis();
        while (millis() - startWait < 500) { client.loop(); delay(10); }
        Serial.println(" Listo.");

        publishStatus("wake_up");
        
        // EJECUCIÓN SEGÚN MODO
        if (config.mode == 1) { // BURST
            Serial.println(">> Iniciando Captura BURST...");
            if (mpu1_active) performBurstCapture(mpu1, "s1");
            if (mpu2_active) performBurstCapture(mpu2, "s2");
        } else { // SNAPSHOT
            Serial.println(">> Iniciando Captura SNAPSHOT...");
            if (mpu1_active) runSnapshotMode(mpu1, "s1");
            if (mpu2_active) runSnapshotMode(mpu2, "s2");
        }
        
        if (mpu1_active) setupMPU_Latch(mpu1, MPU1_ADDR);
        if (mpu2_active) setupMPU_Latch(mpu2, MPU2_ADDR);

        // Limpiamos cualquier interrupción pendiente para que el pin INT suba a HIGH
        if (mpu1_active) clearMPUInterrupt(MPU1_ADDR);
        if (mpu2_active) clearMPUInterrupt(MPU2_ADDR);
        delay(150); 
        // -------------------------

        Serial.println("Durmiendo " + String(config.sleep_time_s) + "s (RF OFF)...");
        setStatus(LED_GREEN); delay(1000); setStatus(-1);
        
        // >>> DEEP SLEEP MEJORADO <<<
        // WAKE_RF_DISABLED: El próximo despertar será con la radio APAGADA,
        // evitando el pico de corriente inicial.
        ESP.deepSleep(config.sleep_time_s * 1000000ULL, WAKE_RF_DISABLED);
        
    } else {
        Serial.println("MQTT Fail -> Sleep 5s"); 
        // También usamos RF DISABLED aquí por seguridad
        ESP.deepSleep(5 * 1000000ULL, WAKE_RF_DISABLED); 
    }
}

void loop() {}
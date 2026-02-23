
#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <stdio.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <new>
#include <LittleFS.h>

#include "publicacion.h"
#include "macros.h"

// ================= CONFIG JSON =================
void loadMQTTConfig(WiFiManagerParameter mqtt_server, WiFiManagerParameter mqtt_port) { 
    if (LittleFS.exists("/mqtt_config.json")) { 
        File f = LittleFS.open("/mqtt_config.json", "r"); 
        if (f) { 
            DynamicJsonDocument doc(512); 
            deserializeJson(doc, f); 
            strcpy(mqtt_server, doc["server"]); 
            strcpy(mqtt_port, doc["port"]); f.close(); 
        } 
    } 
}

void saveMQTTConfig(WiFiManagerParameter mqtt_server, WiFiManagerParameter mqtt_port) { 
    DynamicJsonDocument doc(512); 
    doc["server"] = mqtt_server; 
    doc["port"] = mqtt_port; 
    File f = LittleFS.open("/mqtt_config.json", "w"); 
    if (f) { 
        serializeJson(doc, f); 
        f.close(); 
    } 
}

void loadSystemConfig(DeviceConfig &config, char node_id[32]) { 
    if (LittleFS.exists("/sys_config.json")) { 
        File f = LittleFS.open("/sys_config.json", "r"); 
        if (f) { 
            DynamicJsonDocument doc(512); 
            deserializeJson(doc, f); 
            config.mode = doc["mode"]|1; 
            config.sleep_time_s = doc["sleep"]|900; 
            config.mpu_threshold = doc["thr"]|20; 
            config.sampling_freq = doc["freq"]|500; 
            config.burst_size = doc["bsize"]|256; 
            if(doc.containsKey("id")) strlcpy(node_id, doc["id"], sizeof(node_id)); 
            f.close(); 
        } 
    } 
}

void saveSystemConfig(DeviceConfig config, char node_id[32]) { 
    File f = LittleFS.open("/sys_config.json", "w"); 
    if (!f) return; 
    DynamicJsonDocument doc(512); 
    doc["mode"]=config.mode; 
    doc["sleep"]=config.sleep_time_s; 
    doc["thr"]=config.mpu_threshold; 
    doc["freq"]=config.sampling_freq; 
    doc["bsize"]=config.burst_size; 
    doc["id"] = node_id; 
    serializeJson(doc, f); 
    f.close(); 
}


// ================= HEARTBEAT & CALLBACKS =================
void publishStatus(String reason, String node_id, DeviceConfig config, PubSubClient client) {
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
        printCurrentConfig(config); // Muestra la config final tras el cambio
        if (restartRequired) {
            Serial.println(">> ID CAMBIADO. REINICIANDO EN 1s...");
            delay(1000);
            ESP.restart();
        }
    }
}
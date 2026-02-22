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

// VERSIONES
#define HW_VER "1.5"
#define SW_VER "6.1" // Actualizado para reflejar el parche de energía

// VARIABLES

DeviceConfig config = {1, 60, 10, 500, 256};  

char node_id[32] = "dango_node_002"; // Valor por defecto si no hay config guardada
char mqtt_server[40] = ""; char mqtt_port[6] = "1883";

WiFiClient espClient; PubSubClient client(espClient); Adafruit_MPU6050 mpu1; Adafruit_MPU6050 mpu2;

bool mpu1_active = false; bool mpu2_active = false; bool shouldSaveConfig = false;

// ================= I2C RECOVERY =================
void aggressiveBusRecover() {
    Serial.println(">> [1] Lavado de Bus I2C...");
    pinMode(SDA_PIN, INPUT_PULLUP); pinMode(SCL_PIN, OUTPUT);
    for (int i = 0; i < 16; i++) {
        digitalWrite(SCL_PIN, LOW); delayMicroseconds(10);
        if (i == 8) pinMode(SDA_PIN, INPUT_PULLUP); 
        digitalWrite(SCL_PIN, HIGH); delayMicroseconds(10);
        if (digitalRead(SDA_PIN) == HIGH && i > 8) break;
    }
    pinMode(SDA_PIN, OUTPUT); digitalWrite(SDA_PIN, LOW); delayMicroseconds(10);
    digitalWrite(SCL_PIN, HIGH); delayMicroseconds(10); digitalWrite(SDA_PIN, HIGH); delayMicroseconds(10);
    pinMode(SDA_PIN, INPUT); pinMode(SCL_PIN, INPUT);
}

void groundingPins() {
    Serial.println(">> [2] Grounding SDA/SCL...");
    pinMode(SDA_PIN, OUTPUT); digitalWrite(SDA_PIN, LOW); pinMode(SCL_PIN, OUTPUT); digitalWrite(SCL_PIN, LOW);
    delay(100); pinMode(SDA_PIN, INPUT); pinMode(SCL_PIN, INPUT);
}

void blindSensorReset(uint8_t addr) {
    Wire.beginTransmission(addr); Wire.write(0x6B); Wire.write(0x80); Wire.endTransmission(); 
    Serial.print(">> [3] Reset Ciego -> 0x"); Serial.println(addr, HEX);
}

// ================= CONFIG JSON =================
void loadMQTTConfig() { if (LittleFS.exists("/mqtt_config.json")) { File f = LittleFS.open("/mqtt_config.json", "r"); if (f) { DynamicJsonDocument doc(512); deserializeJson(doc, f); strcpy(mqtt_server, doc["server"]); strcpy(mqtt_port, doc["port"]); f.close(); } } }
void saveMQTTConfig() { DynamicJsonDocument doc(512); doc["server"] = mqtt_server; doc["port"] = mqtt_port; File f = LittleFS.open("/mqtt_config.json", "w"); if (f) { serializeJson(doc, f); f.close(); } }
void loadSystemConfig() { if (LittleFS.exists("/sys_config.json")) { File f = LittleFS.open("/sys_config.json", "r"); if (f) { DynamicJsonDocument doc(512); deserializeJson(doc, f); config.mode = doc["mode"]|1; config.sleep_time_s = doc["sleep"]|900; config.mpu_threshold = doc["thr"]|20; config.sampling_freq = doc["freq"]|500; config.burst_size = doc["bsize"]|256; if(doc.containsKey("id")) strlcpy(node_id, doc["id"], sizeof(node_id)); f.close(); } } }
void saveSystemConfig() { File f = LittleFS.open("/sys_config.json", "w"); if (!f) return; DynamicJsonDocument doc(512); doc["mode"]=config.mode; doc["sleep"]=config.sleep_time_s; doc["thr"]=config.mpu_threshold; doc["freq"]=config.sampling_freq; doc["bsize"]=config.burst_size; doc["id"] = node_id; serializeJson(doc, f); f.close(); }
void saveConfigCallback() { shouldSaveConfig = true; }

// ================= CONFIG SENSORES =================
void setupMPU_Latch(Adafruit_MPU6050 &mpu, int MPU_ADDR) {
    // 1. Reset y Wake Up
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission();
    
    // 2. Filtro Pasa Altos (5Hz) - Para que no detecte gravedad estática
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x1C); Wire.write(0x01); Wire.endTransmission();
    
    // 3. CONFIGURACIÓN DEL PIN (CRÍTICO)
    // Usamos 0xF0 (11110000) -> Active Low, Open Drain, Latch.
    // Open Drain es OBLIGATORIO si tienes dos sensores unidos.
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x37); Wire.write(0xF0); Wire.endTransmission();
    
    // 4. Umbral de detección (Motion Threshold)
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x1F); Wire.write(config.mpu_threshold); Wire.endTransmission();
    
    // 5. Duración (Motion Duration) - 1ms
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x20); Wire.write(1); Wire.endTransmission();
    
    // 6. HABILITAR INTERRUPCIÓN (FALTABA ESTO)
    // Sin esto, el sensor mide el golpe pero no avisa por el pin INT.
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x38); Wire.write(0x40); Wire.endTransmission();
}

// Función para limpiar la interrupción del MPU (Desbloquear el Latch)
void clearMPUInterrupt(uint8_t addr) {
    Wire.beginTransmission(addr);
    Wire.write(0x3A); // Registro INT_STATUS
    Wire.endTransmission();
    Wire.requestFrom(addr, (uint8_t)1);
    Wire.read(); // Leemos el byte para limpiar el flag
    // Serial.println(">> INT Limpiado para addr 0x" + String(addr, HEX));
}

// ================= CAPTURA SNAPSHOT TRIAXIAL (X, Y, Z) =================
bool runSnapshotMode(Adafruit_MPU6050 &mpu, String sensorName) {
    sensors_event_t a, g, t; 
    mpu.getEvent(&a, &g, &t);
    
    // DEBUG SERIAL: Ver los datos capturados
    Serial.print(">> [" + sensorName + "] SNAPSHOT: ");
    Serial.print("X:"); Serial.print(a.acceleration.x, 2);
    Serial.print(" Y:"); Serial.print(a.acceleration.y, 2);
    Serial.print(" Z:"); Serial.println(a.acceleration.z, 2);

    StaticJsonDocument<384> doc; 
    doc["id"] = node_id; 
    doc["t"] = "snap"; 
    doc["s"] = sensorName;
    
    // AHORA ENVIAMOS LAS 3 DIRECCIONES
    doc["ax"] = serialized(String(a.acceleration.x, 2));
    doc["ay"] = serialized(String(a.acceleration.y, 2));
    doc["az"] = serialized(String(a.acceleration.z, 2));
    
    // VCC ya se manda en heartbeat, no hace falta aquí.
    
    char buffer[384]; 
    serializeJson(doc, buffer); 
    return client.publish("vibranet/data", buffer);
}

// ================= CAPTURA BURST TRIAXIAL =================
bool performBurstCapture(Adafruit_MPU6050 &mpu, String sensorName) {
    mpu.enableSleep(false); mpu.enableCycle(false); mpu.setFilterBandwidth(MPU6050_BAND_260_HZ);
    
    if (ESP.getFreeHeap() < 15000) { Serial.println("RAM Baja -> 128 muestras"); config.burst_size = 128; }

    float *waveformX = new (std::nothrow) float[config.burst_size];
    float *waveformY = new (std::nothrow) float[config.burst_size];
    float *waveformZ = new (std::nothrow) float[config.burst_size];

    if (!waveformX || !waveformY || !waveformZ) {
        Serial.println("Error RAM");
        if (waveformX) delete[] waveformX;
        if (waveformY) delete[] waveformY;
        if (waveformZ) delete[] waveformZ;
        return false; 
    }

    unsigned long period = 1000000 / config.sampling_freq;
    unsigned long nextSample = micros();
    sensors_event_t a, g, t;

    for (uint32_t i = 0; i < config.burst_size; i++) {
        while (micros() < nextSample) yield();
        mpu.getEvent(&a, &g, &t);
        waveformX[i] = a.acceleration.x; waveformY[i] = a.acceleration.y; waveformZ[i] = a.acceleration.z;
        nextSample += period;
    }

    String payload = ""; payload.reserve(500 + (config.burst_size * 22)); 
    payload = "{\"id\":\"" + String(node_id) + "\",\"s\":\"" + sensorName + "\",\"t\":\"burst\",\"fs\":" + String(config.sampling_freq) + ",";
    
    payload += "\"dx\":["; for (uint32_t i=0; i<config.burst_size; i++) { payload += String(waveformX[i], 2); if(i<config.burst_size-1) payload+=","; } payload += "],";
    payload += "\"dy\":["; for (uint32_t i=0; i<config.burst_size; i++) { payload += String(waveformY[i], 2); if(i<config.burst_size-1) payload+=","; } payload += "],";
    payload += "\"dz\":["; for (uint32_t i=0; i<config.burst_size; i++) { payload += String(waveformZ[i], 2); if(i<config.burst_size-1) payload+=","; } payload += "]}";

    bool res = client.publish("vibranet/burst", payload.c_str(), false);
    if(res) Serial.println(">> Burst " + sensorName + " enviado (" + String(payload.length()) + " bytes)");
    
    delete[] waveformX; delete[] waveformY; delete[] waveformZ;
    return res;
}

// ================= LEER VOLTAJE DE BATERÍA =================
float leerBateria() {
  // 1. Leemos el valor crudo (0 a 1023)
  int adcRaw = analogRead(A0);
  
  // 2. Convertimos ese número a Voltaje que llega al pin (0V a 1.0V)
  //    (1.0V / 1023 pasos = 0.0009775)
  float voltajePin = adcRaw * 0.0009775;
  
  // 3. Deshacemos el Divisor Resistivo (470k + 100k) / 100k = 5.7
  //    Multiplicamos por 5.7 para saber qué voltaje había ANTES de las resistencias
  float voltajeDivisor = voltajePin * 5.7;
  
  // 4. Sumamos lo que se comió el Diodo Schottky (Tu medición: 0.2V)
  float voltajeBateria = voltajeDivisor + 0.2;

  // --- Opcional: Ajuste fino ---
  // Si con esto te marca 4.05V y el multímetro dice 4.02V, cambia el 0.2 por 0.17
  
  return voltajeBateria;
}

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
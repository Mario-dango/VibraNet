/**
 * Vibranet Node v5.9 - Wake-on-Shake Fix
 * Mantiene la solución I2C V5.8 (Reset Ciego)
 * Agrega: Filtro Pasa Altos (HPF) para que la gravedad no active la interrupción.
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

// PINES I2C (Ajusta si es necesario)
#define SDA_PIN 4 
#define SCL_PIN 5

// LEDS
#define LED_YELLOW  12 
#define LED_GREEN   13 
#define LED_RED     14 

// VARIABLES
struct DeviceConfig {
    int mode; int sleep_time_s; int mpu_threshold; int sampling_freq; uint32_t burst_size;
};
DeviceConfig config = {1, 100, 4, 500, 256};  
char mqtt_server[40] = ""; char mqtt_port[6] = "1883";
WiFiClient espClient; PubSubClient client(espClient);
Adafruit_MPU6050 mpu1; Adafruit_MPU6050 mpu2;
bool mpu1_active = false; bool mpu2_active = false;
bool shouldSaveConfig = false;

// ================= HELPERS VISUALES =================
void setStatus(int pinOn) {
    digitalWrite(LED_YELLOW, (pinOn == LED_YELLOW) ? HIGH : LOW);
    digitalWrite(LED_GREEN, (pinOn == LED_GREEN) ? HIGH : LOW);
    digitalWrite(LED_RED, (pinOn == LED_RED) ? HIGH : LOW);
}

void blinkErrorAndRestart() {
    Serial.println("!!! FALLO CRITICO - REINICIANDO SISTEMA !!!");
    for(int i=0; i<5; i++) {
        digitalWrite(LED_RED, HIGH); delay(100); digitalWrite(LED_RED, LOW); delay(100);
    }
    ESP.restart();
}

// ================= I2C RECOVERY TOOLS =================
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
    pinMode(SDA_PIN, OUTPUT); digitalWrite(SDA_PIN, LOW);
    pinMode(SCL_PIN, OUTPUT); digitalWrite(SCL_PIN, LOW);
    delay(100); 
    pinMode(SDA_PIN, INPUT); pinMode(SCL_PIN, INPUT);
}

void blindSensorReset(uint8_t addr) {
    Wire.beginTransmission(addr); Wire.write(0x6B); Wire.write(0x80); Wire.endTransmission(); 
    Serial.print(">> [3] Reset Ciego -> 0x"); Serial.println(addr, HEX);
}

// ================= CONFIG HELPERS =================
// (Bloques compactados para brevedad - Funcionan igual que V5.8)
void loadMQTTConfig() { if (LittleFS.exists("/mqtt_config.json")) { File f = LittleFS.open("/mqtt_config.json", "r"); if (f) { DynamicJsonDocument doc(512); deserializeJson(doc, f); strcpy(mqtt_server, doc["server"]); strcpy(mqtt_port, doc["port"]); f.close(); } } }
void saveMQTTConfig() { DynamicJsonDocument doc(512); doc["server"] = mqtt_server; doc["port"] = mqtt_port; File f = LittleFS.open("/mqtt_config.json", "w"); if (f) { serializeJson(doc, f); f.close(); } }
void loadSystemConfig() { if (LittleFS.exists("/sys_config.json")) { File f = LittleFS.open("/sys_config.json", "r"); if (f) { DynamicJsonDocument doc(512); deserializeJson(doc, f); config.mode = doc["mode"]|1; config.sleep_time_s = doc["sleep"]|900; config.mpu_threshold = doc["thr"]|20; config.sampling_freq = doc["freq"]|500; config.burst_size = doc["bsize"]|256; f.close(); } } }
void saveSystemConfig() { File f = LittleFS.open("/sys_config.json", "w"); if (!f) return; DynamicJsonDocument doc(512); doc["mode"]=config.mode; doc["sleep"]=config.sleep_time_s; doc["thr"]=config.mpu_threshold; doc["freq"]=config.sampling_freq; doc["bsize"]=config.burst_size; serializeJson(doc, f); f.close(); }
void saveConfigCallback() { shouldSaveConfig = true; }

// ==================== AQUI ESTA LA MAGIA CORREGIDA ====================
void setupMPU_Latch(Adafruit_MPU6050 &mpu, int MPU_ADDR) {
    // 1. Resetear configuración de energía
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission();
    
    // 2. CONFIGURAR FILTRO PASA ALTOS (DHPF) - CRÍTICO
    // Registro 0x1C (ACCEL_CONFIG). Escribimos 0x01 (5Hz High Pass Filter).
    // Esto hace que el sensor IGNORE LA GRAVEDAD y solo vea cambios.
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x1C); Wire.write(0x01); Wire.endTransmission();

    // 3. Configurar Interrupción (Active Low, Open Drain, Latch, Clear on Read)
    // Bit 7=1 (Active Low), Bit 6=1 (Open Drain - MEJOR PARA WIRED-OR), Bit 5=1 (Latch), Bit 4=1 (Clear)
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x37); Wire.write(0b11110000); Wire.endTransmission();

    // 4. Configurar Umbral y Duración
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x1F); Wire.write(config.mpu_threshold); Wire.endTransmission();
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x20); Wire.write(1); Wire.endTransmission(); // 1ms duración

    // 5. Habilitar Interrupción por Movimiento (WOM)
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x38); Wire.write(0x40); Wire.endTransmission();
}
// ======================================================================

// ... Resto de Helpers de Captura ...
bool performBurstCapture(Adafruit_MPU6050 &mpu, String sensorName) {
    mpu.enableSleep(false); mpu.enableCycle(false); mpu.setFilterBandwidth(MPU6050_BAND_260_HZ);
    if (ESP.getFreeHeap() < 10000) config.burst_size = 128;
    float *waveform = new (std::nothrow) float[config.burst_size];
    if (!waveform) return false; 
    unsigned long period = 1000000 / config.sampling_freq;
    unsigned long nextSample = micros();
    sensors_event_t a, g, t;
    for (uint32_t i = 0; i < config.burst_size; i++) {
        while (micros() < nextSample) yield();
        mpu.getEvent(&a, &g, &t); waveform[i] = a.acceleration.x; nextSample += period;
    }
    String payload = ""; payload.reserve(200 + (config.burst_size * 8));
    payload = "{\"id\":\"" + String(NODE_ID) + "\",\"s\":\"" + sensorName + "\",\"t\":\"burst\",\"fs\":" + String(config.sampling_freq) + ",\"d\":[";
    for (uint32_t i = 0; i < config.burst_size; i++) { payload += String(waveform[i], 2); if (i < config.burst_size - 1) payload += ","; }
    payload += "]}";
    bool res = client.publish("vibranet/burst", payload.c_str(), false);
    delete[] waveform; return res;
}

bool runSnapshotMode(Adafruit_MPU6050 &mpu, String sensorName) {
    sensors_event_t a, g, t; mpu.getEvent(&a, &g, &t);
    StaticJsonDocument<256> doc; doc["id"] = NODE_ID; doc["t"] = "snap"; doc["s"] = sensorName;
    doc["ax"] = serialized(String(a.acceleration.x, 2)); doc["vcc"] = analogRead(A0) * 0.0042;
    char buffer[256]; serializeJson(doc, buffer); return client.publish("vibranet/data", buffer);
}
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    StaticJsonDocument<200> doc; deserializeJson(doc, payload, length);
    if (doc.containsKey("sleep")) { config.sleep_time_s = doc["sleep"]; saveSystemConfig(); }
}
void enterRescueMode() { Serial.println("Rescate..."); delay(1000); ESP.restart(); }

// ================= SETUP PRINCIPAL =================
void setup() {
    delay(200); 
    Serial.begin(115200);
    Serial.println("\n\n>>> ARRANQUE VIBRANET V5.9 (FINAL) <<<");

    aggressiveBusRecover();
    groundingPins();

    pinMode(LED_YELLOW, OUTPUT); pinMode(LED_GREEN, OUTPUT); pinMode(LED_RED, OUTPUT);
    setStatus(LED_YELLOW); 

    LittleFS.begin(); loadMQTTConfig(); loadSystemConfig(); 
    
    Wire.begin(); Wire.setClock(10000); delay(50);

    blindSensorReset(MPU1_ADDR);
    blindSensorReset(MPU2_ADDR);
    delay(100); 

    Wire.setClock(100000); 
    
    Serial.println(">>> Iniciando Libreria...");
    if (!mpu1.begin(MPU1_ADDR)) {
        Serial.println("ERROR: MPU1 no responde.");
        blinkErrorAndRestart(); 
    } else {
        Serial.println("MPU1 OK.");
        mpu1_active = true;
        mpu1.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu1.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }
    
    if (mpu2.begin(MPU2_ADDR)) { mpu2_active = true; mpu2.setAccelerometerRange(MPU6050_RANGE_4_G); mpu2.setFilterBandwidth(MPU6050_BAND_21_HZ); }
    
    WiFiManager wm; wm.setSaveConfigCallback(saveConfigCallback);
    WiFiManagerParameter custom_mqtt_server("server", "MQTT Server IP", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
    wm.addParameter(&custom_mqtt_server); wm.addParameter(&custom_mqtt_port);
    wm.setConfigPortalTimeout(60); 
    if (!wm.autoConnect("VibraNet_AP")) Serial.println("WiFi Fail");
    if (shouldSaveConfig) { strcpy(mqtt_server, custom_mqtt_server.getValue()); strcpy(mqtt_port, custom_mqtt_port.getValue()); saveMQTTConfig(); }
    client.setServer(mqtt_server, atoi(mqtt_port)); client.setCallback(mqttCallback); client.setBufferSize(8192);

    String clientId = String(NODE_ID) + "_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
        Serial.println("MQTT OK");
        client.subscribe(("vibranet/config/"+String(NODE_ID)).c_str());
        client.loop(); delay(100);
        
        if (mpu1_active) performBurstCapture(mpu1, "s1");
        if (mpu2_active) performBurstCapture(mpu2, "s2");
        
        // CONFIGURAR INTERRUPCIONES (Aquí usamos la función corregida)
        if (mpu1_active) setupMPU_Latch(mpu1, MPU1_ADDR);
        if (mpu2_active) setupMPU_Latch(mpu2, MPU2_ADDR);

        Serial.println("Success -> Sleep");
        setStatus(LED_GREEN); delay(500); setStatus(-1);
        ESP.deepSleep(config.sleep_time_s * 1000000ULL);
    } else {
        Serial.println("MQTT Fail -> Sleep"); ESP.deepSleep(10 * 1000000ULL); 
    }
}

void loop() {}
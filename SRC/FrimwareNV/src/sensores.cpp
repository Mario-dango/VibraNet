
#include "sensores.h"


extern "C" void ets_delay_us(uint32_t us);

// ================= I2C RECOVERY =================
void aggressiveBusRecover() {
    Serial.println(">> [1] Lavado de Bus I2C...");
    
    setPULLUP(CONF_SDA); setINPUT(SDA_PIN);
    setOUTPUT(SCL_PIN);

    
    for (int i = 0; i < 16; i++) {


        setLOW(SCL_PIN); ets_delay_us(10);
        if (i == 8) setPULLUP(CONF_SDA); setINPUT(SDA_PIN);
        setHIGH(SCL_PIN); ets_delay_us(10);
        if (READ_PIN(SDA_PIN) == 1 && i > 8) break;
    }
    setOUTPUT(SDA_PIN); setLOW(SDA_PIN); ets_delay_us(10);
    setHIGH(SCL_PIN); ets_delay_us(10); setHIGH(SDA_PIN); ets_delay_us(10);
    setINPUT(SDA_PIN); setINPUT(SCL_PIN);
}

void groundingPins() {
    Serial.println(">> [2] Grounding SDA/SCL...");
    setOUTPUT(SDA_PIN); setLOW(SDA_PIN); setOUTPUT(SCL_PIN); setLOW(SCL_PIN);
    ets_delay_us(100000); // 100ms
    setINPUT(SDA_PIN); setINPUT(SCL_PIN);
}

void blindSensorReset(uint8_t addr) {
    Wire.beginTransmission(addr); Wire.write(0x6B); Wire.write(0x80); Wire.endTransmission();
    Serial.print(">> [3] Reset Ciego -> 0x"); Serial.println(addr, HEX); 
}

// ================= CONFIG SENSORES =================
void setupMPU_Latch(
    Adafruit_MPU6050 &mpu, 
    int MPU_ADDR, 
    DeviceConfig &config
    ) {
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
bool runSnapshotMode(
    Adafruit_MPU6050 &mpu, 
    String sensorName, 
    PubSubClient &client, 
    const char* node_id
    ) {
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
bool performBurstCapture(
    Adafruit_MPU6050 &mpu, 
    const String sensorName, 
    const char node_id[NODE_ID_SIZE], 
    PubSubClient &client, 
    DeviceConfig &config)
{
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
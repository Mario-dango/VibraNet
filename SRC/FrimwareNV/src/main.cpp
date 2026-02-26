/**
 * Vibranet Node v6.0 - Triaxial Snapshot & Debug Edition
 * PATCH: Soft Start + WAKE_RF_DISABLED (Solución Brownout)
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
#include "sensores.h"
#include "feedback.h"
#include "publicacion.h"

DeviceConfig config = {1, 60, 10, 500, 256};

char node_id[NODE_ID_SIZE] = "dango_node_002"; // Valor por defecto si no hay config guardada

char mqtt_server[MQTT_SERVER_SIZE] = "";
char mqtt_port[MQTT_PORT_SIZE] = "1883";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_MPU6050 mpu1;
Adafruit_MPU6050 mpu2;

bool mpu1_active = false;
bool mpu2_active = false;
bool shouldSaveConfig = false;

// ================= SETUP =================
void setup()
{
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

    aggressiveBusRecover();
    groundingPins();
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    setStatus(LED_YELLOW);

    LittleFS.begin();
    loadMQTTConfig(mqtt_server, mqtt_port);
    loadSystemConfig(config, node_id);
    initMqttContext(node_id, config, client);

    // DEBUG: Ver qué ID estamos usando
    Serial.print("Node ID: ");
    Serial.println(node_id);
    printCurrentConfig(config);

    Wire.begin();
    Wire.setClock(10000);
    delay(50);
    blindSensorReset(MPU1_ADDR);
    blindSensorReset(MPU2_ADDR);
    delay(100);
    Wire.setClock(100000);

    if (!mpu1.begin(MPU1_ADDR))
    {
        Serial.println("ERROR: MPU1 no responde.");
        blinkErrorAndRestart();
    }
    else
    {
        Serial.println("MPU1 OK.");
        mpu1_active = true;
        mpu1.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu1.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }

    if (mpu2.begin(MPU2_ADDR))
    {
        Serial.println("MPU2 OK.");
        mpu2_active = true;
        mpu2.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu2.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }

    WiFiManager wm;
    wm.setSaveConfigCallback(saveConfigCallback);

    // AUMENTA EL TIMEOUT para darle tiempo si la señal es débil por la baja potencia
    wm.setConnectTimeout(30);
    wm.setConfigPortalTimeout(120);

    WiFiManagerParameter custom_mqtt_server("server", "MQTT Server IP", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);

    if (!wm.autoConnect("VibraNet_AP"))
    {
        Serial.println("WiFi Fail");
        // Si falla, dormimos SIN radio para ahorrar energía
        ESP.deepSleep(5e6, WAKE_RF_DISABLED);
    }

    // --- OPCIONAL: SUBIDA DE POTENCIA ---
    // COMENTADO POR SEGURIDAD: Si usas el conversor USB-TTL, descomentar esto
    // podría causar un reinicio justo ahora. Déjalo comentado para la prueba.
    // WiFi.setOutputPower(10);

    if (shouldSaveConfig)
    {
        strcpy(mqtt_server, custom_mqtt_server.getValue());
        strcpy(mqtt_port, custom_mqtt_port.getValue());
        saveMQTTConfig(mqtt_server, mqtt_port);
    }

    // CORRECCIÓN: Borrada la línea duplicada que tenías aquí
    client.setServer(mqtt_server, atoi(mqtt_port));
    client.setCallback(mqttCallback);
    client.setBufferSize(8192);

    // Usamos la variable 'node_id' en lugar de la constante NODE_ID
    String clientId = String(node_id) + "_" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str()))
    {
        Serial.println("MQTT Conectado.");
        // Suscripción dinámica basada en el nombre actual
        client.subscribe(("vibranet/config/" + String(node_id)).c_str());

        // --- SALA DE ESPERA (500ms) ---
        Serial.print("Esperando config remota...");
        unsigned long startWait = millis();
        while (millis() - startWait < 500)
        {
            client.loop();
            delay(10);
        }
        Serial.println(" Listo.");

        publishStatus("wake_up", node_id, config, client);

        // EJECUCIÓN SEGÚN MODO
        if (config.mode == 1)
        { // BURST
            Serial.println(">> Iniciando Captura BURST...");
            if (mpu1_active)
                performBurstCapture(mpu1, "s1", node_id, client, config);
            if (mpu2_active)
                performBurstCapture(mpu2, "s2", node_id, client, config);
        }
        else
        { // SNAPSHOT
            Serial.println(">> Iniciando Captura SNAPSHOT...");
            if (mpu1_active)
                runSnapshotMode(mpu1, "s1", client, node_id);
            if (mpu2_active)
                runSnapshotMode(mpu2, "s2", client, node_id);
        }

        if (mpu1_active)
            setupMPU_Latch(mpu1, MPU1_ADDR, config);
        if (mpu2_active)
            setupMPU_Latch(mpu2, MPU2_ADDR, config);

        // Limpiamos cualquier interrupción pendiente para que el pin INT suba a HIGH
        if (mpu1_active)
            clearMPUInterrupt(MPU1_ADDR);
        if (mpu2_active)
            clearMPUInterrupt(MPU2_ADDR);
        delay(150);
        // -------------------------

        Serial.println("Durmiendo " + String(config.sleep_time_s) + "s (RF OFF)...");
        setStatus(LED_GREEN);
        delay(1000);
        setStatus(-1);

        // >>> DEEP SLEEP MEJORADO <<<
        // WAKE_RF_DISABLED: El próximo despertar será con la radio APAGADA,
        // evitando el pico de corriente inicial.
        ESP.deepSleep(config.sleep_time_s * 1000000ULL, WAKE_RF_DISABLED);
    }
    else
    {
        Serial.println("MQTT Fail -> Sleep 5s");
        // También usamos RF DISABLED aquí por seguridad
        ESP.deepSleep(5 * 1000000ULL, WAKE_RF_DISABLED);
    }
}

void loop() {}
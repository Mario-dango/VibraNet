
#include "publicacion.h"

// Variables "locales" para evitar pasar parámetros de más en mqttCallback
static char *ptrNodeId = nullptr;         // Valor por defecto, se puede actualizar por MQTT
static DeviceConfig *ptrConfig = nullptr; // Configuración global del dispositivo, se puede actualizar por MQTT
static PubSubClient *ptrClient = nullptr; // Puntero al cliente MQTT, se asigna en setup() para usarlo en mqttCallback

// Setup MQTT Context
void initMqttContext(
    char *mainNodeId,
    DeviceConfig &mainConfig,
    PubSubClient &mainClient)
{

    ptrNodeId = mainNodeId;  // Copia segura del string
    ptrConfig = &mainConfig; // Copia la estructura
    ptrClient = &mainClient; // Asigna el puntero
}

void saveConfigCallback()
{
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

// ================= CONFIG JSON =================
void loadMQTTConfig(
    char mqtt_server[MQTT_SERVER_SIZE],
    char mqtt_port[MQTT_PORT_SIZE])
{
    if (LittleFS.exists("/mqtt_config.json"))
    {
        File f = LittleFS.open("/mqtt_config.json", "r");
        if (f)
        {
            DynamicJsonDocument doc(512);
            deserializeJson(doc, f);
            strlcpy(mqtt_server, doc["server"], MQTT_SERVER_SIZE);
            strlcpy(mqtt_port, doc["port"], MQTT_PORT_SIZE);
            f.close();
        }
    }
}

void saveMQTTConfig(
    const char mqtt_server[MQTT_SERVER_SIZE], 
    const char mqtt_port[MQTT_PORT_SIZE])
{
    DynamicJsonDocument doc(512);
    doc["server"] = mqtt_server;
    doc["port"] = mqtt_port;
    File f = LittleFS.open("/mqtt_config.json", "w");
    if (f)
    {
        serializeJson(doc, f);
        f.close();
    }
}

void loadSystemConfig(
    DeviceConfig &config,
    char node_id[NODE_ID_SIZE])
{
    if (LittleFS.exists("/sys_config.json"))
    {
        File f = LittleFS.open("/sys_config.json", "r");
        if (f)
        {
            DynamicJsonDocument doc(512);
            deserializeJson(doc, f);
            config.mode = doc["mode"] | 1;
            config.sleep_time_s = doc["sleep"] | 900;
            config.mpu_threshold = doc["thr"] | 20;
            config.sampling_freq = doc["freq"] | 500;
            config.burst_size = doc["bsize"] | 256;
            if (doc.containsKey("id"))
                strlcpy(ptrNodeId, doc["id"], NODE_ID_SIZE);
            f.close();
        }
    }
}

void saveSystemConfig(
    DeviceConfig &config,
    char node_id[NODE_ID_SIZE])
{
    File f = LittleFS.open("/sys_config.json", "w");
    if (!f)
        return;
    DynamicJsonDocument doc(512);
    doc["mode"] = config.mode;
    doc["sleep"] = config.sleep_time_s;
    doc["thr"] = config.mpu_threshold;
    doc["freq"] = config.sampling_freq;
    doc["bsize"] = config.burst_size;
    doc["id"] = node_id;
    serializeJson(doc, f);
    f.close();
}

// ================= HEARTBEAT & CALLBACKS =================
void publishStatus(
    const String &reason,
    const String &node_id,
    DeviceConfig &config,
    PubSubClient &client)
{
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
    doc["hw"] = HW_VER; // <--- NUEVO
    doc["sw"] = SW_VER; // <--- NUEVO

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

void mqttCallback(
    char *topic,
    byte *payload,
    unsigned int length)
{
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error)
    {
        Serial.println("Error JSON Config");
        return;
    }

    bool changed = false;
    bool restartRequired = false; // Bandera para saber si reiniciamos
    Serial.println("\n--- Recibiendo Config Remota ---");
    if (doc.containsKey("id"))
    {
        const char *newId = doc["id"];
        if (strcmp(ptrNodeId, newId) != 0)
        {
            strlcpy(ptrNodeId, newId, sizeof(ptrNodeId));
            Serial.println("Update NODE ID -> " + String(ptrNodeId));
            changed = true;
            restartRequired = true; // El ID requiere reinicio obligatorio
        }
    }
    if (doc.containsKey("sleep"))
    {
        ptrConfig->sleep_time_s = doc["sleep"];
        Serial.println("Update Sleep -> " + String(ptrConfig->sleep_time_s));
        changed = true;
    }
    if (doc.containsKey("mode"))
    {
        ptrConfig->mode = doc["mode"];
        Serial.println("Update Mode -> " + String(ptrConfig->mode));
        changed = true;
    }
    if (doc.containsKey("thr"))
    {
        ptrConfig->mpu_threshold = doc["thr"];
        Serial.println("Update Thr -> " + String(ptrConfig->mpu_threshold));
        changed = true;
    }
    if (doc.containsKey("fs"))
    {
        ptrConfig->sampling_freq = doc["freq"];
        Serial.println("Update Freq -> " + String(ptrConfig->sampling_freq));
        changed = true;
    }

    if (changed)
    {
        saveSystemConfig(*ptrConfig, ptrNodeId);
        publishStatus("config_updated", ptrNodeId, *ptrConfig, *ptrClient);
        printCurrentConfig(*ptrConfig); // Muestra la config final tras el cambio
        if (restartRequired)
        {
            Serial.println(">> ID CAMBIADO. REINICIANDO EN 1s...");
            delay(1000);
            reinicio_ESP();
        }
    }
}

// PUBLICACION.CPP
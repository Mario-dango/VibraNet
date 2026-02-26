#ifndef PUBLICACION_H
#define PUBLICACION_H

#include <stdio.h>
#include <Arduino.h>

#include <PubSubClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <WiFiClient.h>

#include "macros.h"
#include "sensores.h"
#include "feedback.h"

// Necesito trabajar `node_id` como un `static char[]` para no poner parámetros de más en `mqttCallback`
// y para evitar el siguiente error:
// warning: 'sizeof' on array function parameter 'node_id' will return size of 'char*' [-Wsizeof-array-argument]
//
//      strlcpy(node_id, newId, sizeof(node_id));
//                                     ~^~~~~~~~
// note: declared here
//      char node_id[32],
//      ~~~~~^~~~~~~~~~~

void initMqttContext(
    char *mainNodeId,
    DeviceConfig &mainConfig,
    PubSubClient &mainClient
);

extern bool shouldSaveConfig;
void saveConfigCallback();

// ================= CONFIG JSON =================

void loadMQTTConfig(
    char mqtt_server[MQTT_SERVER_SIZE],
    char mqtt_port[MQTT_PORT_SIZE]);

void saveMQTTConfig(
    const char mqtt_server[MQTT_SERVER_SIZE],
    const char mqtt_port[MQTT_PORT_SIZE]);

void loadSystemConfig(
    DeviceConfig &config,
    char node_id[NODE_ID_SIZE]);

void saveSystemConfig(
    DeviceConfig &config,
    const char node_id[NODE_ID_SIZE]);

void saveConfigCallback();

// ================= HEARTBEAT & CALLBACKS =================
void publishStatus(
    const String &reason,
    const String &node_id,
    DeviceConfig &config,
    PubSubClient &client);

void mqttCallback(
    char *topic,
    byte *payload,
    unsigned int length);

#endif // PUBLICATION_H
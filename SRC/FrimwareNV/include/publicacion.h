#ifndef PUBLICACION_H
#define PUBLICACION_H

// ================= CONFIG JSON =================
void loadMQTTConfig();

void saveMQTTConfig();

void loadSystemConfig();

void saveSystemConfig();

void saveConfigCallback();


// ================= HEARTBEAT & CALLBACKS =================
void publishStatus(String reason);

void mqttCallback(char* topic, byte* payload, unsigned int length);
#endif // PUBLICATION_H
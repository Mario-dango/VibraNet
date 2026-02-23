#ifndef FEEDBACK_H
#define FEEDBACK_H

#include "macros.h" // Necesario para DeviceConfig y definiciones de pines

void setStatus(int pinOn);
void blinkErrorAndRestart();
void printCurrentConfig(DeviceConfig config);

#endif // FEEDBACK_H
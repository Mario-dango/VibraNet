#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <Arduino.h>
#include <stdio.h>
#include "macros.h"
#include "feedback.h"

void setStatus(int pinOn);

void blinkErrorAndRestart();

void printCurrentConfig(const DeviceConfig &config);

#endif // FEEDBACK_H
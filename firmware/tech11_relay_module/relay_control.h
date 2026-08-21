#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include <Arduino.h>

void setupRelayPins();
void activateRelay(int relayNum, int durationMs);
void updateRelayTimers();

#endif

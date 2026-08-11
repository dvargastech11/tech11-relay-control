#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include <Arduino.h>

extern const int relayPins[];

void setupRelayPins();
void activateRelay(int relayNum, int durationMs);

#endif

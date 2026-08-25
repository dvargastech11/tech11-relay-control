#ifndef EXTERNAL_INPUT_H
#define EXTERNAL_INPUT_H

#include <Arduino.h>

void setupExternalInput();
void updateExternalInput(); // call every loop() - detects state changes, logs them

// Current state: true = contact closed (LOW, shorted to GND), false = open (HIGH)
bool isExternalInputClosed();

// How long (ms) the input has been continuously in its CURRENT state
unsigned long externalInputStateDurationMs();

// True if closed AND has been closed longer than EXTERNAL_INPUT_STUCK_THRESHOLD_MS
bool isExternalInputStuckClosed();

#endif

#include "external_input.h"
#include "config.h"
#include "activity_log.h"

static bool lastState = false; // false = open, true = closed
static unsigned long stateChangedAt = 0;
static bool stateInitialized = false;

void setupExternalInput() {
  pinMode(EXTERNAL_INPUT_PIN, INPUT_PULLUP);

  // Read the actual current state at boot rather than assuming "open" -
  // if it's already closed when the device starts, we still want the
  // stuck-closed timer to start counting from boot, not silently miss it.
  lastState = (digitalRead(EXTERNAL_INPUT_PIN) == LOW);
  stateChangedAt = millis();
  stateInitialized = true;

  addLog("IN", String("External input monitor started - initial state: ") +
                (lastState ? "closed" : "open"));
}

void updateExternalInput() {
  if (!stateInitialized) return;

  bool currentState = (digitalRead(EXTERNAL_INPUT_PIN) == LOW);

  if (currentState != lastState) {
    unsigned long heldForMs = millis() - stateChangedAt;
    addLog("IN", String("External input changed: ") + (lastState ? "closed" : "open") +
                  " -> " + (currentState ? "closed" : "open") +
                  " (was " + (lastState ? "closed" : "open") + " for " +
                  String(heldForMs / 1000) + "s)");

    lastState = currentState;
    stateChangedAt = millis();
  }
}

bool isExternalInputClosed() {
  return lastState;
}

unsigned long externalInputStateDurationMs() {
  return millis() - stateChangedAt;
}

bool isExternalInputStuckClosed() {
  return lastState && (externalInputStateDurationMs() >= EXTERNAL_INPUT_STUCK_THRESHOLD_MS);
}

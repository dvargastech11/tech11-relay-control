#include "relay_control.h"
#include "config.h"
#include "activity_log.h"

// Test config: 2 relays on GPIO14/27. Replace this array (and add
// MCP23017 addressing) when scaling to real multi-relay boards.
const int relayPins[NUM_RELAYS] = { 14, 27 };

struct RelayPulseArgs {
  int pin;
  int relayNum;
  int durationMs;
};

static void relayPulseTask(void *pv) {
  RelayPulseArgs *args = (RelayPulseArgs *)pv;
  digitalWrite(args->pin, RELAY_ACTIVE_LEVEL);
  vTaskDelay(pdMS_TO_TICKS(args->durationMs));
  digitalWrite(args->pin, RELAY_INACTIVE_LEVEL);
  addLog("OUT", "Relay #" + String(args->relayNum) + " released");
  delete args;
  vTaskDelete(NULL);
}

void setupRelayPins() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], RELAY_INACTIVE_LEVEL);
  }
}

void activateRelay(int relayNum, int durationMs) {
  RelayPulseArgs *args = new RelayPulseArgs();
  args->pin = relayPins[relayNum - 1];
  args->relayNum = relayNum;
  args->durationMs = durationMs;
  addLog("OUT", "Relay #" + String(relayNum) + " activated for " + String(durationMs) + "ms");
  xTaskCreate(relayPulseTask, "RelayPulse", 2048, (void*)args, 1, NULL);
}

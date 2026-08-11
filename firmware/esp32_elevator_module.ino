/*
  ESP32 Elevator Relay Module Firmware
  -------------------------------------
  One module = one elevator. Drives up to 48 relays (uses 47) via
  3x MCP23017 I2C I/O expanders. Joins the elevator room's WiFi AP
  and runs a small HTTP server so the room's Raspberry Pi can command it.

  Command (from Pi):
    GET http://<module-ip>/activate?relay=<1-47>&duration=<ms>

  Response: "OK" or "ERR:<reason>"

  Wiring:
    ESP32 GPIO21 (SDA) -> all 3x MCP23017 SDA (shared bus)
    ESP32 GPIO22 (SCL) -> all 3x MCP23017 SCL (shared bus)
    MCP23017 #1 address 0x20 (A0/A1/A2 = GND/GND/GND) -> relays 1-16
    MCP23017 #2 address 0x21 (A0=VCC, A1/A2=GND)       -> relays 17-32
    MCP23017 #3 address 0x22 (A1=VCC, A0/A2=GND)       -> relays 33-48 (47 used)

  Library required (install via Arduino Library Manager):
    "Adafruit MCP23017 Arduino Library"

  IMPORTANT: Set MODULE_STATIC_IP below to a unique, reserved address
  for THIS elevator's module (e.g. .11 for elevator 1, .12 for elevator 2, etc.)
  so the Pi can reliably address it without relying on mDNS.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MCP23017.h>

// ---- WIFI CONFIG ----
const char* WIFI_SSID = "ELEVATOR-ROOM-AP";     // <-- change to your AP's SSID
const char* WIFI_PASSWORD = "CHANGE_ME";        // <-- change to your AP's password

// ---- STATIC IP CONFIG (edit per module) ----
// Give each of the 6 modules in a room a unique last octet, e.g. .11 - .16
IPAddress MODULE_STATIC_IP(192, 168, 50, 11);   // <-- unique per module
IPAddress GATEWAY(192, 168, 50, 1);              // <-- your AP/router IP
IPAddress SUBNET(255, 255, 255, 0);

// ---- RELAY CONFIG ----
#define NUM_RELAYS 47   // this elevator's floor count (matches 2-49 skipping 13)

Adafruit_MCP23017 mcp1; // relays 1-16
Adafruit_MCP23017 mcp2; // relays 17-32
Adafruit_MCP23017 mcp3; // relays 33-48 (only 33-47 used)

// Active level: most relay boards driven through MCP23017 + ULN2803 are active LOW.
// Confirm with a multimeter before trusting this on real hardware.
const int RELAY_ACTIVE_LEVEL = LOW;
const int RELAY_INACTIVE_LEVEL = HIGH;

bool relayActive[NUM_RELAYS];
unsigned long relayStartTime[NUM_RELAYS];
unsigned long relayDuration[NUM_RELAYS];

WebServer server(80);

// Map a 1-indexed relay number to (expander, pin)
void getExpanderAndPin(int relayNum, Adafruit_MCP23017** expander, int* pin) {
  int idx = relayNum - 1; // 0-indexed
  if (idx < 16) {
    *expander = &mcp1;
    *pin = idx;
  } else if (idx < 32) {
    *expander = &mcp2;
    *pin = idx - 16;
  } else {
    *expander = &mcp3;
    *pin = idx - 32;
  }
}

void setupRelays() {
  mcp1.begin(0x20);
  mcp2.begin(0x21);
  mcp3.begin(0x22);

  for (int i = 1; i <= NUM_RELAYS; i++) {
    Adafruit_MCP23017* expander;
    int pin;
    getExpanderAndPin(i, &expander, &pin);
    expander->pinMode(pin, OUTPUT);
    expander->digitalWrite(pin, RELAY_INACTIVE_LEVEL);

    relayActive[i - 1] = false;
    relayStartTime[i - 1] = 0;
    relayDuration[i - 1] = 0;
  }
}

void activateRelay(int relayNum, unsigned long duration) {
  Adafruit_MCP23017* expander;
  int pin;
  getExpanderAndPin(relayNum, &expander, &pin);

  expander->digitalWrite(pin, RELAY_ACTIVE_LEVEL);

  int idx = relayNum - 1;
  relayActive[idx] = true;
  relayStartTime[idx] = millis();
  relayDuration[idx] = duration;
}

void updateRelayTimers() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (relayActive[i] && (now - relayStartTime[i] >= relayDuration[i])) {
      Adafruit_MCP23017* expander;
      int pin;
      getExpanderAndPin(i + 1, &expander, &pin);
      expander->digitalWrite(pin, RELAY_INACTIVE_LEVEL);
      relayActive[i] = false;
    }
  }
}

// ---- HTTP HANDLERS ----

void handleActivate() {
  if (!server.hasArg("relay") || !server.hasArg("duration")) {
    server.send(400, "text/plain", "ERR:missing relay or duration param");
    return;
  }

  int relayNum = server.arg("relay").toInt();
  long duration = server.arg("duration").toInt();

  if (relayNum < 1 || relayNum > NUM_RELAYS) {
    server.send(400, "text/plain", "ERR:relay out of range");
    return;
  }
  if (duration <= 0) {
    server.send(400, "text/plain", "ERR:invalid duration");
    return;
  }

  activateRelay(relayNum, duration);
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  server.send(200, "text/plain", "Module OK, " + String(NUM_RELAYS) + " relays");
}

void setup() {
  Serial.begin(115200);

  setupRelays();

  WiFi.config(MODULE_STATIC_IP, GATEWAY, SUBNET);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());

  server.on("/activate", handleActivate);
  server.on("/status", handleStatus);
  server.begin();

  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
  updateRelayTimers();
}

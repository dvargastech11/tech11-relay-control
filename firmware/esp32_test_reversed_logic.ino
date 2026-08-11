#include <WiFi.h>
#include <WebServer.h>

// --- Network Configuration ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

IPAddress local_IP(192, 168, 55, 230); // Test IP for this module
IPAddress gateway(192, 168, 55, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// Map array indices to physical GPIOs
// Index 0 -> GPIO16 (Relay 1), Index 1 -> GPIO17 (Relay 2)
const int relayPins[2] = { 16, 17 };

// REVERSED LOGIC: active-LOW relay board
// Energized  = LOW
// De-energized = HIGH
const int RELAY_ACTIVE_LEVEL = LOW;
const int RELAY_INACTIVE_LEVEL = HIGH;

struct RelayPulseArgs {
  int pin;
  int relayNum;
  int durationMs;
};

// Async pulse handler
void relayPulseTask(void *pvParameters) {
  RelayPulseArgs *args = (RelayPulseArgs *)pvParameters;

  Serial.printf("[TASK] Energizing Relay #%d on GPIO%d for %dms...\n", args->relayNum, args->pin, args->durationMs);
  digitalWrite(args->pin, RELAY_ACTIVE_LEVEL); // LOW = energized (reversed logic)

  vTaskDelay(pdMS_TO_TICKS(args->durationMs));

  digitalWrite(args->pin, RELAY_INACTIVE_LEVEL);  // HIGH = de-energized
  Serial.printf("[TASK] Relay #%d pulse complete. De-energized.\n", args->relayNum);

  delete args;
  vTaskDelete(NULL);
}

void handleTrigger() {
  Serial.println("\n[HTTP] Incoming request to /trigger");

  if (server.method() != HTTP_POST) {
    Serial.println("[HTTP ERROR] Method Not Allowed (Expected POST)");
    server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
    return;
  }

  String body = server.arg("plain");
  Serial.printf("[HTTP] Payload received: %s\n", body.c_str());

  int relayNum = 0;

  // Direct string matching for test payload
  if (body.indexOf("\"relay\":1") != -1 || body.indexOf("\"relay\": 1") != -1) {
    relayNum = 1;
  } else if (body.indexOf("\"relay\":2") != -1 || body.indexOf("\"relay\": 2") != -1) {
    relayNum = 2;
  } else {
    Serial.println("[HTTP ERROR] Invalid JSON or relay parameter out of bounds");
    server.send(400, "application/json", "{\"error\":\"Invalid payload or relay out of bounds\"}");
    return;
  }

  int targetPin = relayPins[relayNum - 1];
  Serial.printf("[HTTP SUCCESS] Valid request for Relay #%d (GPIO%d)\n", relayNum, targetPin);

  RelayPulseArgs *args = new RelayPulseArgs();
  args->pin = targetPin;
  args->relayNum = relayNum;
  args->durationMs = 5000; // 5-second hold

  xTaskCreate(relayPulseTask, "RelayPulse", 2048, (void*)args, 1, NULL);

  server.send(200, "application/json", "{\"status\":\"success\",\"triggered_relay\":" + String(relayNum) + "}");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==========================================");
  Serial.println("  Tech 11 Relay Control Module Booting ");
  Serial.println("  (REVERSED / ACTIVE-LOW relay logic)     ");
  Serial.println("==========================================");

  // Initialize GPIO outputs to INACTIVE (de-energized) state
  Serial.println("[SYS] Initializing GPIO outputs...");
  for (int i = 0; i < 2; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], RELAY_INACTIVE_LEVEL); // HIGH = off, for active-LOW boards
    Serial.printf("      GPIO%d set to OUTPUT, inactive (HIGH)\n", relayPins[i]);
  }

  // Network initialization
  Serial.println("[WIFI] Configuring static IP address...");
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("[WIFI ERROR] Static IP configuration failed!");
  }

  Serial.printf("[WIFI] Connecting to SSID: %s\n", ssid);
  WiFi.begin(ssid, password);

  int attemptCounter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attemptCounter++;
    if (attemptCounter % 20 == 0) {
      Serial.println();
    }
  }

  // Wi-Fi Status Output
  Serial.println("\n------------------------------------------");
  Serial.println("[WIFI] STATUS: Connected successfully!");
  Serial.printf( "[WIFI] IP Address:  %s\n", WiFi.localIP().toString().c_str());
  Serial.printf( "[WIFI] Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf( "[WIFI] Gateway:     %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf( "[WIFI] MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.printf( "[WIFI] RSSI Signal: %d dBm\n", WiFi.RSSI());
  Serial.println("------------------------------------------");

  server.on("/trigger", handleTrigger);
  server.begin();
  Serial.println("[HTTP] Web Server started on port 80");
  Serial.println("[SYS] System ready and listening for trigger commands.\n");
}

void loop() {
  server.handleClient();
  delay(2);
}

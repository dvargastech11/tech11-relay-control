#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

// --- Network Configuration ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

IPAddress local_IP(192, 168, 55, 230); // Static IP for this module
IPAddress gateway(192, 168, 55, 1);
IPAddress subnet(255, 255, 255, 0);

// --- OTA Configuration (no password) ---
String ota_hostname; // set in setup() as "Tech11_XXXX" using last 4 hex digits of the MAC

String generateDeviceName() {
  String mac = WiFi.macAddress(); // e.g. "AA:BB:CC:DD:EE:FF"
  mac.replace(":", "");
  String last4 = mac.substring(mac.length() - 4);
  return "Tech11_" + last4;
}

WebServer server(80);

// --- Glitch-Free GPIO Pins ---
// Moved from GPIO16/17 to prevent hardware boot-toggling.
// Index 0 -> GPIO25 (Relay 1), Index 1 -> GPIO26 (Relay 2)
const int relayPins[2] = { 25, 26 };

struct RelayPulseArgs {
  int pin;
  int relayNum;
  int durationMs;
};

// Async pulse handler (FreeRTOS Task)
void relayPulseTask(void *pvParameters) {
  RelayPulseArgs *args = (RelayPulseArgs *)pvParameters;
  
  Serial.printf("[TASK] Energizing Relay #%d on GPIO%d for %dms...\n", args->relayNum, args->pin, args->durationMs);
  digitalWrite(args->pin, HIGH); // Closes the circuit (Active-HIGH)
  
  vTaskDelay(pdMS_TO_TICKS(args->durationMs));
  
  digitalWrite(args->pin, LOW);  // De-energize (Fails Open / Returns to NO)
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

  // Direct string parsing logic for test bench
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
  args->durationMs = 5000; // 5-second hold spec

  xTaskCreate(relayPulseTask, "RelayPulse", 2048, (void*)args, 1, NULL);

  server.send(200, "application/json", "{\"status\":\"success\",\"triggered_relay\":" + String(relayNum) + "}");
}

// --- Local diagnostic page ---
// For on-site troubleshooting only. Fires the same /trigger endpoint the
// Pi uses, so this exercises the exact same relay path - useful to confirm
// a module is wired correctly even if the Pi/network path is down.
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>";
  html += ota_hostname;
  html += " - Diagnostic</title>";
  html += "<style>body{font-family:Arial;background:#1a1a1a;color:#eee;text-align:center;padding:40px;}"
          "button{padding:16px 24px;font-size:16px;margin:10px;border:none;border-radius:8px;"
          "background:#2d6cdf;color:white;cursor:pointer;}"
          "button:active{background:#2fbf4f;}"
          "#status{margin-top:20px;color:#aaa;}</style></head><body>";
  html += "<h2>";
  html += ota_hostname;
  html += "</h2>";
  html += "<p style='color:#aaa;'>Local diagnostic page - fires the same relay path the Pi uses.</p>";
  html += "<button onclick=\"trig(1)\">Test Relay 1</button>";
  html += "<button onclick=\"trig(2)\">Test Relay 2</button>";
  html += "<div id='status'></div>";
  html += "<script>";
  html += "async function trig(n){";
  html += "document.getElementById('status').innerText='Sending relay '+n+'...';";
  html += "try{const r=await fetch('/trigger',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay:n})});";
  html += "const t=await r.text();document.getElementById('status').innerText=t;";
  html += "}catch(e){document.getElementById('status').innerText='Error: '+e;}}";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void setupOTA() {
  ArduinoOTA.setHostname(ota_hostname.c_str());
  // No password set - OTA updates accepted from anyone on the network.
  ArduinoOTA.setPort(3232);

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("\n[OTA] Update starting, downloading " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update successfully finished. Rebooting system...");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA ERROR] Code [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Authentication Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] Network listeners initialized (no auth).");
}

void setup() {
  // CRITICAL: Immediately force pins LOW before any initialization delays,
  // serial allocation, or Wi-Fi handshakes to ensure strict fail-open execution.
  for (int i = 0; i < 2; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  Serial.begin(115200);
  delay(500); // Safe delay now that pins are locked LOW

  Serial.println("\n==========================================");
  Serial.println("  Tech 11 Relay Control Module Booting ");
  Serial.println("==========================================");
  Serial.println("[SYS] GPIO outputs explicitly locked LOW.");

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

  // Wi-Fi Diagnostics
  Serial.println("\n------------------------------------------");
  Serial.println("[WIFI] STATUS: Connected successfully!");
  Serial.printf( "[WIFI] IP Address:  %s\n", WiFi.localIP().toString().c_str());
  Serial.printf( "[WIFI] Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf( "[WIFI] Gateway:     %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf( "[WIFI] RSSI Signal: %d dBm\n", WiFi.RSSI());
  Serial.println("------------------------------------------");

  // Auto-generate this device's name from its MAC (factory default naming)
  ota_hostname = generateDeviceName();
  Serial.printf("[SYS] Device name: %s\n", ota_hostname.c_str());

  // Start OTA Subsystem
  setupOTA();

  // Register endpoints & boot HTTP API
  server.on("/", handleRoot);
  server.on("/trigger", handleTrigger);
  server.begin();
  Serial.println("[HTTP] Web Server started on port 80");
  Serial.println("[SYS] System fully operational and listening.\n");
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  delay(2); 
}

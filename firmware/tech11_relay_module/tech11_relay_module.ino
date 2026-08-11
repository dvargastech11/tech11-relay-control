/*
  Tech 11 Relay Control Module - Main Sketch
  ---------------------------------------------
  This file only contains setup()/loop(). All actual logic lives in the
  other files in this same sketch folder:

    config.h            - all the CHANGE-THIS constants
    network_config.*     - WiFi connect, AP fallback, saved settings
    auth.*                - admin login, forced password change, API key
    relay_control.*      - relay pins and non-blocking activation
    activity_log.*       - NTP timestamp + ring buffer log
    discovery.*           - UDP discovery responder
    ota_update.*          - GitHub OTA + rollback
    web_handlers.*        - every HTTP route handler

  Library dependencies (Arduino Library Manager):
    ArduinoJson (Benoit Blanchon)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <esp_ota_ops.h>

#include "config.h"
#include "network_config.h"
#include "auth.h"
#include "relay_control.h"
#include "activity_log.h"
#include "discovery.h"
#include "ota_update.h"
#include "web_handlers.h"

// Shared globals - declared extern in the headers that need them
WebServer server(80);
Preferences prefs;

void setup() {
  Serial.begin(115200);
  delay(500);

  setupRelayPins();
  loadNetworkConfig();
  loadAuthConfig();
  setupWiFiWithFallback();

  ensureDeviceNameSet();  // MAC is only valid now that WiFi has initialized
  deviceApiKey = computeDeviceApiKey();

  if (!isInAPMode) {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, ntpServer.c_str());
  }

  setupDiscovery();

  ArduinoOTA.setHostname(deviceName.c_str());
  ArduinoOTA.setPort(3232);
  // No OTA password set - add ArduinoOTA.setPassword("...") before production.
  ArduinoOTA.begin();

  registerWebHandlers();
  server.begin();

  esp_ota_mark_app_valid_cancel_rollback();

  Serial.println("[SYS] " + deviceName + " ready. IP: " +
                  (isInAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()));
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  handleDiscoveryRequests();

  static unsigned long lastAutoUpdateCheck = 0;
  unsigned long now = millis();
  const unsigned long AUTO_UPDATE_CHECK_INTERVAL_MS = 24UL * 60 * 60 * 1000;

  if (!isInAPMode && now - lastAutoUpdateCheck > AUTO_UPDATE_CHECK_INTERVAL_MS) {
    lastAutoUpdateCheck = now;
    checkForUpdates(false);
  }
}

/*
  Tech 11 Relay Control Module - Main Sketch
  ---------------------------------------------
  This file only contains setup()/loop(). All actual logic lives in the
  other files in this same sketch folder:

    config.h            - all the CHANGE-THIS constants (incl. ETH pins)
    network_config.*     - WiFi primary connection, AP setup fallback
    auth.*                - admin login, forced password change, API key
    relay_control.*      - MCP23017-driven relays, board scan/online detect
    activity_log.*       - NTP timestamp + ring buffer log
    discovery.*           - UDP discovery responder
    ota_update.*          - GitHub OTA + rollback
    web_handlers.*        - every HTTP route handler

  NETWORK ARCHITECTURE: WiFi STA is the primary connection (TEMPORARY -
  reverted from Ethernet while an Ethernet PHY power issue - undervoltage
  causing "power up timeout" - is set aside for later work). All the
  Ethernet code (network_config.cpp's connectToEthernet()/etc, config.h's
  ETH_PHY_* pins) is fully intact and unused - re-enabling it later is
  just a matter of swapping which function setupNetworkWithFallback()
  calls.

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
  setupNetworkWithFallback(); // tries WiFi STA first, falls back to setup AP

  ensureDeviceNameSet();  // MAC is only valid now that the network stack has initialized
  deviceApiKey = computeDeviceApiKey();

  if (!isInAPMode) {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, ntpServer.c_str());

    // Discovery and OTA both require the Pi/network to actually reach this
    // device - pointless while isolated in the AP fallback setup network,
    // and skipping them here also trims the attack surface during setup.
    setupDiscovery();

    ArduinoOTA.setHostname(deviceName.c_str());
    ArduinoOTA.setPort(3232);
    // No OTA password set - add ArduinoOTA.setPassword("...") before production.
    ArduinoOTA.begin();
  }

  registerWebHandlers();
  server.begin();

  esp_ota_mark_app_valid_cancel_rollback();

  Serial.println("[SYS] " + deviceName + " ready. IP: " +
                  (isInAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()));
}

void loop() {
  server.handleClient();
  updateRelayTimers();
  updateTestMode();

  if (!isInAPMode) {
    ArduinoOTA.handle();
    handleDiscoveryRequests();

    static unsigned long lastAutoUpdateCheck = 0;
    unsigned long now = millis();
    const unsigned long AUTO_UPDATE_CHECK_INTERVAL_MS = 24UL * 60 * 60 * 1000;

    if (now - lastAutoUpdateCheck > AUTO_UPDATE_CHECK_INTERVAL_MS) {
      lastAutoUpdateCheck = now;
      checkForUpdates(false);
    }
  } else {
    // In AP fallback mode: periodically retry the originally configured
    // WiFi in the background (AP stays up the whole time for setup access).
    // If it connects, restart to cleanly re-enter normal operation -
    // simpler and safer than trying to hot-swap discovery/OTA/NTP state
    // at runtime.
    static unsigned long lastReconnectAttempt = 0;
    const unsigned long RECONNECT_RETRY_INTERVAL_MS = 60UL * 1000; // every 60s
    unsigned long now = millis();

    if (now - lastReconnectAttempt > RECONNECT_RETRY_INTERVAL_MS) {
      lastReconnectAttempt = now;
      Serial.println("[WIFI] AP mode: attempting background reconnect to configured network...");
      startBackgroundReconnectAttempt();
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WIFI] Reconnected to configured network - restarting to fully exit AP mode...");
      delay(500);
      ESP.restart();
    }
  }
}

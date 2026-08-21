#include "auth.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <mbedtls/md.h>

extern WebServer server;   // defined in the main .ino
extern Preferences prefs;  // defined in the main .ino

const char* ADMIN_USERNAME = "admin";
String adminPassword = "T1123456";
bool mustChangePassword = true;
String deviceApiKey;

String computeDeviceApiKey() {
  String mac = WiFi.macAddress();
  const mbedtls_md_info_t *mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

  // One-shot HMAC call - avoids the separate mbedtls_md_hmac_starts/update/
  // finish streaming functions, which some newer mbedtls versions (bundled
  // with newer/alpha ESP32 cores) have removed or renamed. This single
  // function has been stable across mbedtls versions and needs no manual
  // context setup/teardown.
  unsigned char result[32];
  mbedtls_md_hmac(
    mdInfo,
    (const unsigned char*)MASTER_SECRET, strlen(MASTER_SECRET),
    (const unsigned char*)mac.c_str(), mac.length(),
    result
  );

  String hex = "";
  char buf[3];
  for (int i = 0; i < 32; i++) {
    sprintf(buf, "%02x", result[i]);
    hex += buf;
  }
  return hex;
}

void loadAuthConfig() {
  prefs.begin("auth", true);
  adminPassword = prefs.getString("password", "T1123456");
  mustChangePassword = prefs.getBool("mustchange", true);
  prefs.end();
}

void saveAuthConfig(String newPassword, bool changed) {
  prefs.begin("auth", false);
  prefs.putString("password", newPassword);
  prefs.putBool("mustchange", changed);
  prefs.end();
}

bool checkAuth() {
  if (!server.authenticate(ADMIN_USERNAME, adminPassword.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

bool checkForcedPasswordChange() {
  if (mustChangePassword) {
    server.sendHeader("Location", "/change-password");
    server.send(302);
    return true;
  }
  return false;
}

bool checkApiKey() {
  if (!server.hasHeader("X-API-Key") || server.header("X-API-Key") != deviceApiKey) {
    server.send(401, "application/json", "{\"error\":\"Invalid or missing API key\"}");
    return false;
  }
  return true;
}

bool checkApiKeySilent() {
  // Same check as checkApiKey(), but doesn't send a response itself - needed
  // when checking auth mid-multipart-upload, where sending a response early
  // would conflict with the still-streaming request body.
  return server.hasHeader("X-API-Key") && server.header("X-API-Key") == deviceApiKey;
}

#include "auth.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <mbedtls/md.h>
#include <string.h>

extern WebServer server;   // defined in the main .ino
extern Preferences prefs;  // defined in the main .ino

const char* ADMIN_USERNAME = "admin";
String adminPassword = "12345678";
bool mustChangePassword = false; // no longer forced - see checkForcedPasswordChange() below
String deviceApiKey;

String computeDeviceApiKey() {
  String mac = WiFi.macAddress();

  // Manual HMAC-SHA256 (RFC 2104) using only the plain digest functions
  // (mbedtls_md_starts/update/finish, hmac flag = 0). This alpha ESP32
  // core's bundled mbedtls has neither the streaming HMAC API
  // (mbedtls_md_hmac_starts/update/finish) nor the one-shot convenience
  // function (mbedtls_md_hmac) - both were tried and both failed to
  // compile. Implementing the construction by hand avoids depending on
  // any HMAC-specific symbol at all, only the base digest primitives
  // that are guaranteed to exist.
  const char* keyStr = MASTER_SECRET;
  size_t keyLen = strlen(keyStr);

  const size_t blockSize = 64; // SHA-256 block size
  const size_t hashSize = 32;

  const mbedtls_md_info_t *mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

  unsigned char keyBlock[blockSize];
  memset(keyBlock, 0, blockSize);

  if (keyLen > blockSize) {
    // Key longer than one block - hash it down to 32 bytes first
    mbedtls_md_context_t hashCtx;
    mbedtls_md_init(&hashCtx);
    mbedtls_md_setup(&hashCtx, mdInfo, 0);
    mbedtls_md_starts(&hashCtx);
    mbedtls_md_update(&hashCtx, (const unsigned char*)keyStr, keyLen);
    mbedtls_md_finish(&hashCtx, keyBlock);
    mbedtls_md_free(&hashCtx);
  } else {
    memcpy(keyBlock, keyStr, keyLen);
  }

  unsigned char ipad[blockSize];
  unsigned char opad[blockSize];
  for (size_t i = 0; i < blockSize; i++) {
    ipad[i] = keyBlock[i] ^ 0x36;
    opad[i] = keyBlock[i] ^ 0x5c;
  }

  // Inner hash: SHA256(ipad || message)
  unsigned char innerHash[hashSize];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mdInfo, 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, ipad, blockSize);
  mbedtls_md_update(&ctx, (const unsigned char*)mac.c_str(), mac.length());
  mbedtls_md_finish(&ctx, innerHash);
  mbedtls_md_free(&ctx);

  // Outer hash: SHA256(opad || innerHash) - this is the final HMAC result
  unsigned char result[hashSize];
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mdInfo, 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, opad, blockSize);
  mbedtls_md_update(&ctx, innerHash, hashSize);
  mbedtls_md_finish(&ctx, result);
  mbedtls_md_free(&ctx);

  String hex = "";
  char buf[3];
  for (size_t i = 0; i < hashSize; i++) {
    sprintf(buf, "%02x", result[i]);
    hex += buf;
  }
  return hex;
}

void loadAuthConfig() {
  prefs.begin("auth", true);
  adminPassword = prefs.getString("password", "12345678");
  mustChangePassword = prefs.getBool("mustchange", false);
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
  // Password changes are no longer forced - this always returns false.
  // The /change-password page still exists and works if someone wants to
  // change the password voluntarily, it's just never required.
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

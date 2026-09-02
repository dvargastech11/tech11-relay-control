#include "ota_update.h"
#include "config.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>

String lastUpdateCheckResult = "Not checked yet.";

String fetchRemoteVersion() {
  WiFiClientSecure client;
  client.setInsecure(); // see README note on cert pinning for production

  // GitHub's raw content servers (raw.githubusercontent.com) sit behind a
  // CDN that caches responses for a few minutes. Without cache-busting,
  // checking right after publishing a new version can return a stale
  // cached copy of version.txt, making a genuinely available update look
  // like "up to date". Appending a changing query param + explicit
  // no-cache headers reliably bypasses that cache.
  String url = String(GITHUB_VERSION_URL) + "?cachebust=" + String(millis());

  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Pragma", "no-cache");

  String result = "";
  if (http.GET() == 200) {
    result = http.getString();
    result.trim();
  }
  http.end();
  return result;
}

void performGitHubUpdate() {
  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.rebootOnUpdate(true);

  String url = String(GITHUB_FIRMWARE_URL) + "?cachebust=" + String(millis());
  t_httpUpdate_return result = httpUpdate.update(client, url);

  if (result == HTTP_UPDATE_FAILED) {
    lastUpdateCheckResult = "Update FAILED: " + httpUpdate.getLastErrorString();
  } else if (result == HTTP_UPDATE_NO_UPDATES) {
    lastUpdateCheckResult = "No update applied.";
  } else if (result == HTTP_UPDATE_OK) {
    lastUpdateCheckResult = "Update successful, rebooting.";
  }
}

// Compares two "main.overhaul.small" version strings part by part as
// integers. Returns true if `a` is newer than `b`.
// NOTE: a naive toFloat() comparison doesn't work here - "1.0.1".toFloat()
// and "1.0.0".toFloat() BOTH parse to 1.0 (the float parser stops at the
// second decimal point), so that comparison could never detect an update
// beyond the first two version parts. This was a real bug - fixed here by
// actually parsing and comparing each of the three parts as integers.
bool isNewerVersion(String a, String b) {
  int aParts[3] = {0, 0, 0};
  int bParts[3] = {0, 0, 0};

  int idx = 0;
  int start = 0;
  for (int i = 0; i <= a.length() && idx < 3; i++) {
    if (i == a.length() || a.charAt(i) == '.') {
      aParts[idx++] = a.substring(start, i).toInt();
      start = i + 1;
    }
  }

  idx = 0;
  start = 0;
  for (int i = 0; i <= b.length() && idx < 3; i++) {
    if (i == b.length() || b.charAt(i) == '.') {
      bParts[idx++] = b.substring(start, i).toInt();
      start = i + 1;
    }
  }

  for (int i = 0; i < 3; i++) {
    if (aParts[i] != bParts[i]) return aParts[i] > bParts[i];
  }
  return false; // identical
}

void checkForUpdates(bool forceApply) {
  String remoteVersion = fetchRemoteVersion();

  if (remoteVersion.length() == 0) {
    lastUpdateCheckResult = "Could not reach GitHub to check version.";
    return;
  }

  if (isNewerVersion(remoteVersion, String(CURRENT_FIRMWARE_VERSION))) {
    lastUpdateCheckResult = "New version available: " + remoteVersion +
                             " (current: " + String(CURRENT_FIRMWARE_VERSION) + ")";
    if (forceApply) performGitHubUpdate();
  } else {
    lastUpdateCheckResult = "Up to date (version " + String(CURRENT_FIRMWARE_VERSION) + ")";
  }
}

String getRunningPartitionLabel() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  return running ? String(running->label) : "unknown";
}

bool rollbackAvailable() {
  const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
  const esp_partition_t *running = esp_ota_get_running_partition();
  return (next != NULL && next != running);
}

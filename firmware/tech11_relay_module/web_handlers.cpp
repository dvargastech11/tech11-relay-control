#include "web_handlers.h"
#include "config.h"
#include "auth.h"
#include "network_config.h"
#include "relay_control.h"
#include "activity_log.h"
#include "ota_update.h"

#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>

extern WebServer server; // defined in the main .ino

// ============================================================
// HUMAN HANDLERS (admin login required via checkAuth())
// ============================================================

static void handleRoot() {
  if (!checkAuth()) return;
  if (checkForcedPasswordChange()) return;

  String html = "<!DOCTYPE html><html><head><title>" + deviceName + "</title>";
  html += "<style>body{font-family:Arial;background:#1a1a1a;color:#eee;padding:30px;max-width:500px;margin:0 auto;}"
          "h3{color:#aaa;border-top:1px solid #333;padding-top:16px;}"
          "button{padding:12px 18px;margin:4px;border:none;border-radius:8px;background:#2d6cdf;color:white;cursor:pointer;}"
          "a{color:#2d6cdf;} .banner{padding:12px;border-radius:8px;margin-bottom:16px;}</style></head><body>";

  html += "<div class='banner' style='background:" + String(isInAPMode ? "#7a1f1f" : "#1f4f2f") + ";'>";
  html += isInAPMode ? "SETUP MODE - not connected to WiFi. Fix network settings below."
                      : "Connected (" + wifiSSID + ") - IP: " + WiFi.localIP().toString();
  html += "</div>";

  html += "<h2>" + deviceName + "</h2>";

  html += "<h3>Relay Test</h3>";
  for (int i = 1; i <= NUM_RELAYS; i++) {
    html += "<button onclick=\"trig(" + String(i) + ")\">Test Relay " + String(i) + "</button>";
  }
  html += "<div id='status' style='color:#aaa;margin-top:10px;'></div>";

  html += "<h3>Links</h3>";
  html += "<p><a href='/network/save-page'>Network Settings</a></p>";
  html += "<p><a href='/logs'>Activity Log</a></p>";
  html += "<p><a href='/update'>Manual Firmware Upload</a></p>";
  html += "<p><a href='/config/backup'>Download Config Backup</a> | <a href='/config/restore'>Restore Config</a></p>";
  html += "<p><a href='/change-password'>Change Password</a></p>";

  html += "<h3>GitHub Firmware</h3>";
  html += "<p style='color:#aaa;'>Version: " + String(CURRENT_FIRMWARE_VERSION) + " (" + getRunningPartitionLabel() + ")</p>";
  html += "<button onclick='checkUpdate()'>Check for Updates</button>";
  html += "<button onclick='applyUpdate()'>Apply Update</button>";
  if (rollbackAvailable()) html += "<button onclick='rollback()' style='background:#a33;'>Rollback</button>";
  html += "<div id='updateStatus' style='color:#aaa;margin-top:8px;'></div>";

  html += "<script>";
  html += "async function trig(n){document.getElementById('status').innerText='Sending relay '+n+'...';"
          "try{const r=await fetch('/trigger',{method:'POST',headers:{'Content-Type':'application/json','X-API-Key':'" + deviceApiKey + "'},body:JSON.stringify({relay:n})});"
          "document.getElementById('status').innerText=await r.text();}catch(e){document.getElementById('status').innerText='Error: '+e;}}";
  html += "async function checkUpdate(){document.getElementById('updateStatus').innerText='Checking...';"
          "const r=await fetch('/update/check');document.getElementById('updateStatus').innerText=await r.text();}";
  html += "async function applyUpdate(){if(!confirm('Reboots if update applied. Continue?'))return;"
          "const r=await fetch('/update/apply',{method:'POST'});document.getElementById('updateStatus').innerText=await r.text();}";
  html += "async function rollback(){if(!confirm('Revert to previous firmware and reboot?'))return;"
          "await fetch('/update/rollback',{method:'POST'});alert('Rolling back...');}";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

static void handleChangePasswordPage() {
  if (!checkAuth()) return;

  String html = "<!DOCTYPE html><html><head><title>Change Password</title>";
  html += "<style>body{font-family:Arial;background:#1a1a1a;color:#eee;padding:40px;max-width:400px;margin:0 auto;}"
          "input{width:100%;padding:10px;margin:8px 0;border-radius:6px;border:1px solid #444;background:#262626;color:#eee;box-sizing:border-box;}"
          "button{padding:12px 20px;border:none;border-radius:8px;background:#2d6cdf;color:white;cursor:pointer;width:100%;margin-top:10px;}"
          "#msg{color:#e74c3c;margin-top:10px;}</style></head><body>";
  html += "<h2>Set a New Password</h2>";
  if (mustChangePassword) html += "<p style='color:#e74c3c;'>You must change the default password before continuing.</p>";
  html += "<form action='/change-password' method='POST'>";
  html += "<input type='password' name='newpass' placeholder='New Password' required minlength='8'>";
  html += "<input type='password' name='confirmpass' placeholder='Confirm New Password' required minlength='8'>";
  html += "<button type='submit'>Update Password</button></form></body></html>";

  server.send(200, "text/html", html);
}

static void handleChangePasswordSubmit() {
  if (!checkAuth()) return;

  String newPass = server.arg("newpass");
  String confirmPass = server.arg("confirmpass");

  if (newPass.length() < 8 || newPass != confirmPass) {
    server.send(400, "text/html",
      "<body style='background:#1a1a1a;color:#e74c3c;font-family:Arial;padding:40px;'>"
      "Passwords must match and be 8+ characters. <a href='/change-password' style='color:#2d6cdf;'>Try again</a></body>");
    return;
  }

  adminPassword = newPass;
  saveAuthConfig(newPass, false);
  mustChangePassword = false;

  server.send(200, "text/html",
    "<body style='background:#1a1a1a;color:#2fbf4f;font-family:Arial;padding:40px;'>"
    "Password updated. <a href='/' style='color:#2d6cdf;'>Continue</a></body>");
}

static void handleNetworkSavePage() {
  if (!checkAuth()) return;
  if (checkForcedPasswordChange()) return;

  String html = "<!DOCTYPE html><html><head><title>Network Settings</title>";
  html += "<style>body{font-family:Arial;background:#1a1a1a;color:#eee;padding:30px;max-width:420px;margin:0 auto;}"
          "input{width:100%;padding:8px;margin:6px 0;border-radius:6px;border:1px solid #444;background:#262626;color:#eee;box-sizing:border-box;}"
          "button{padding:12px;border:none;border-radius:8px;background:#2d6cdf;color:white;cursor:pointer;width:100%;margin-top:10px;}"
          "label{font-size:14px;color:#ccc;} .field{margin-bottom:14px;} .radio-row{margin-bottom:6px;}"
          "h3{color:#aaa;border-top:1px solid #333;padding-top:14px;margin-top:20px;}</style></head><body>";
  html += "<h2>Network Settings</h2>";
  html += "<form action='/network/save' method='POST'>";

  html += "<div class='field'><label>Device Name</label><input type='text' name='devname' value='" + deviceName + "'></div>";

  html += "<div class='radio-row'><label><input type='radio' name='mode' value='dhcp' " +
          String(useDHCP ? "checked" : "") + "> DHCP</label></div>";
  html += "<div class='field'><label><input type='radio' name='mode' value='static' " +
          String(!useDHCP ? "checked" : "") + "> Static IP</label></div>";

  html += "<h3>Static IP Settings</h3>";
  html += "<div class='field'><label>IP Address</label><input type='text' name='ip' value='" + staticIP.toString() + "'></div>";
  html += "<div class='field'><label>Gateway</label><input type='text' name='gw' value='" + gatewayIP.toString() + "'></div>";
  html += "<div class='field'><label>Subnet Mask</label><input type='text' name='sn' value='" + subnetMask.toString() + "'></div>";
  html += "<div class='field'><label>DNS 1</label><input type='text' name='dns1' value='" + dns1.toString() + "'></div>";
  html += "<div class='field'><label>DNS 2</label><input type='text' name='dns2' value='" + dns2.toString() + "'></div>";

  html += "<h3>Time Sync (NTP)</h3>";
  html += "<div class='field'><label>NTP Server</label><input type='text' name='ntp' value='" + ntpServer + "' placeholder='pool.ntp.org'></div>";

  html += "<h3>WiFi</h3>";
  html += "<div class='field'><label>WiFi SSID</label><input type='text' name='ssid' value='" + wifiSSID + "'></div>";
  html += "<div class='field'><label>WiFi Password (leave blank to keep current)</label><input type='password' name='wifipass' value=''></div>";

  html += "<button type='submit'>Save &amp; Reboot</button></form>";
  html += "<p><a href='/' style='color:#2d6cdf;'>&larr; Back</a></p></body></html>";

  server.send(200, "text/html", html);
}

static void handleNetworkSave() {
  if (!checkAuth()) return;

  String name = server.arg("devname");
  bool dhcp = (server.arg("mode") == "dhcp");
  String dns1Arg = server.arg("dns1");
  String dns2Arg = server.arg("dns2");
  String ntpArg = server.arg("ntp");
  if (ntpArg.length() == 0) ntpArg = "pool.ntp.org"; // guard against an empty field

  saveNetworkConfig(name, dhcp, server.arg("ip"), server.arg("gw"), server.arg("sn"), dns1Arg, dns2Arg, ntpArg);

  String newSsid = server.arg("ssid");
  String newWifiPass = server.arg("wifipass");
  if (newSsid.length() > 0) saveWifiCredentials(newSsid, newWifiPass);

  server.send(200, "text/html", "<body style='background:#1a1a1a;color:#eee;font-family:Arial;padding:40px;'>"
    "<h2>Saved. Rebooting...</h2></body>");
  delay(1000);
  ESP.restart();
}

static void handleLogs() {
  if (!checkAuth()) return;
  if (checkForcedPasswordChange()) return;

  String html = "<!DOCTYPE html><html><head><title>Activity Log</title><meta http-equiv='refresh' content='3'>";
  html += "<style>body{font-family:monospace;background:#1a1a1a;color:#eee;padding:20px;}"
          "table{width:100%;border-collapse:collapse;}th,td{padding:6px 10px;text-align:left;border-bottom:1px solid #333;font-size:13px;}"
          "th{color:#aaa;} .in{color:#2fbf4f;} .out{color:#2d6cdf;} a{color:#2d6cdf;}</style></head><body>";
  html += "<h2>Activity Log</h2><p><a href='/'>&larr; Back</a> | Auto-refreshes every 3s</p>";
  html += "<table><tr><th>Time</th><th>Direction</th><th>Message</th></tr>";

  for (int i = 0; i < logCount; i++) {
    int idx = (logWriteIndex - 1 - i + LOG_SIZE) % LOG_SIZE;
    String cls = (activityLog[idx].direction == "IN") ? "in" : "out";
    html += "<tr><td>" + activityLog[idx].timestamp + "</td><td class='" + cls + "'>" +
            activityLog[idx].direction + "</td><td>" + activityLog[idx].message + "</td></tr>";
  }
  html += "</table></body></html>";

  server.send(200, "text/html", html);
}

static void handleUpdatePage() {
  if (!checkAuth()) return;
  if (checkForcedPasswordChange()) return;

  String html = "<!DOCTYPE html><html><head><title>Firmware Update</title>";
  html += "<style>body{font-family:Arial;background:#1a1a1a;color:#eee;padding:30px;max-width:500px;margin:0 auto;}"
          "input[type=file]{margin:15px 0;color:#eee;}button{padding:12px 20px;border:none;border-radius:8px;background:#2d6cdf;color:white;cursor:pointer;}"
          "#status{margin-top:15px;color:#aaa;}</style></head><body>";
  html += "<h2>Firmware Update (Manual Upload)</h2>";
  html += "<input type='file' id='fw' accept='.bin'><br><button onclick='doUpload()'>Upload &amp; Flash</button>";
  html += "<div id='status'></div><p><a href='/' style='color:#2d6cdf;'>&larr; Back</a></p>";
  html += "<script>function doUpload(){const f=document.getElementById('fw').files[0];"
          "if(!f){document.getElementById('status').innerText='Select a file first.';return;}"
          "const xhr=new XMLHttpRequest();const fd=new FormData();fd.append('firmware',f);"
          "xhr.onload=function(){document.getElementById('status').innerText=xhr.responseText;};"
          "xhr.open('POST','/update');xhr.send(fd);}</script></body></html>";

  server.send(200, "text/html", html);
}

static void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    Update.end(true);
  }
}

static void handleUpdateResult() {
  if (Update.hasError()) {
    server.send(200, "text/plain", "Update FAILED.");
  } else {
    server.send(200, "text/plain", "Update successful. Rebooting...");
    delay(1000);
    ESP.restart();
  }
}

static void handleCheckUpdate() {
  if (!checkAuth()) return;
  checkForUpdates(false);
  server.send(200, "text/plain", lastUpdateCheckResult);
}

static void handleApplyUpdate() {
  if (!checkAuth()) return;
  server.send(200, "text/plain", "Checking and applying if newer version found...");
  checkForUpdates(true);
}

static void handleRollback() {
  if (!checkAuth()) return;
  esp_ota_mark_app_invalid_rollback_and_reboot();
  server.send(200, "text/plain", "Rollback failed: no previous firmware available.");
}

static void handleFirmwareStatus() {
  if (!checkAuth()) return;
  server.send(200, "text/plain", "Running: " + getRunningPartitionLabel() +
              " | Rollback available: " + String(rollbackAvailable() ? "Yes" : "No"));
}

static void handleConfigBackup() {
  if (!checkAuth()) return;

  StaticJsonDocument<512> doc;
  doc["deviceName"] = deviceName;
  doc["useDHCP"] = useDHCP;
  doc["staticIP"] = staticIP.toString();
  doc["gateway"] = gatewayIP.toString();
  doc["subnet"] = subnetMask.toString();
  doc["dns1"] = dns1.toString();
  doc["dns2"] = dns2.toString();
  doc["ntpServer"] = ntpServer;
  doc["wifiSSID"] = wifiSSID;
  doc["firmwareVersion"] = CURRENT_FIRMWARE_VERSION;

  String output;
  serializeJsonPretty(doc, output);

  server.sendHeader("Content-Disposition", "attachment; filename=" + deviceName + "_config_backup.json");
  server.send(200, "application/json", output);
}

static void handleConfigRestorePage() {
  if (!checkAuth()) return;

  String html = "<!DOCTYPE html><html><head><title>Restore Configuration</title>";
  html += "<style>body{font-family:Arial;background:#1a1a1a;color:#eee;padding:30px;max-width:450px;margin:0 auto;}"
          "button{padding:12px 20px;border:none;border-radius:8px;background:#2d6cdf;color:white;cursor:pointer;}"
          "#status{margin-top:15px;color:#aaa;}</style></head><body>";
  html += "<h2>Restore Configuration</h2><input type='file' id='cfg' accept='.json'><br>";
  html += "<button onclick='doRestore()'>Restore &amp; Reboot</button><div id='status'></div>";
  html += "<script>async function doRestore(){const f=document.getElementById('cfg').files[0];"
          "if(!f){document.getElementById('status').innerText='Select a file first.';return;}"
          "const text=await f.text();const r=await fetch('/config/restore',{method:'POST',headers:{'Content-Type':'application/json'},body:text});"
          "document.getElementById('status').innerText=await r.text();}</script></body></html>";

  server.send(200, "text/html", html);
}

static void handleConfigRestoreSubmit() {
  if (!checkAuth()) return;

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "Invalid config file.");
    return;
  }

  saveNetworkConfig(
    doc["deviceName"] | deviceName, doc["useDHCP"] | useDHCP,
    doc["staticIP"] | staticIP.toString(), doc["gateway"] | gatewayIP.toString(),
    doc["subnet"] | subnetMask.toString(), doc["dns1"] | dns1.toString(),
    doc["dns2"] | dns2.toString(), doc["ntpServer"] | ntpServer
  );
  if (doc["wifiSSID"]) saveWifiCredentials(doc["wifiSSID"].as<String>(), "");

  server.send(200, "text/plain", "Restored. WiFi/admin passwords were not included - re-enter if needed. Rebooting...");
  delay(1000);
  ESP.restart();
}

// ============================================================
// MACHINE HANDLERS (API key required via checkApiKey())
// ============================================================

static void handleTrigger() {
  if (!checkApiKey()) return;
  addLog("IN", "HTTP /trigger received");

  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
    return;
  }

  String body = server.arg("plain");
  int relayNum = 0;
  for (int i = 1; i <= NUM_RELAYS; i++) {
    if (body.indexOf("\"relay\":" + String(i)) != -1 || body.indexOf("\"relay\": " + String(i)) != -1) {
      relayNum = i;
      break;
    }
  }
  if (relayNum == 0) {
    server.send(400, "application/json", "{\"error\":\"Invalid relay\"}");
    return;
  }

  activateRelay(relayNum, 5000);
  server.send(200, "application/json", "{\"status\":\"success\",\"triggered_relay\":" + String(relayNum) + "}");
}

static void handleStatus() {
  if (!checkApiKey()) return;

  StaticJsonDocument<512> doc;
  doc["name"] = deviceName;
  doc["ip"] = WiFi.localIP().toString();
  doc["mac"] = WiFi.macAddress();
  doc["firmwareVersion"] = CURRENT_FIRMWARE_VERSION;
  doc["uptimeSec"] = millis() / 1000;
  doc["useDHCP"] = useDHCP;
  doc["gateway"] = gatewayIP.toString();
  doc["subnet"] = subnetMask.toString();
  doc["rssi"] = WiFi.RSSI();

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

static void handleRebootCommand() {
  if (!checkApiKey()) return;
  server.send(200, "application/json", "{\"status\":\"rebooting\"}");
  delay(500);
  ESP.restart();
}

static void handleApiNetworkSave() {
  if (!checkApiKey()) return;

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  saveNetworkConfig(
    doc["deviceName"] | deviceName, doc["useDHCP"] | useDHCP,
    doc["staticIP"] | staticIP.toString(), doc["gateway"] | gatewayIP.toString(),
    doc["subnet"] | subnetMask.toString(), doc["dns1"] | dns1.toString(),
    doc["dns2"] | dns2.toString(), doc["ntpServer"] | ntpServer
  );

  server.send(200, "application/json", "{\"status\":\"saved, rebooting\"}");
  delay(500);
  ESP.restart();
}

// ============================================================
// ROUTE REGISTRATION
// ============================================================

void registerWebHandlers() {
  // ESP32 WebServer only exposes custom headers (like X-API-Key) through
  // server.hasHeader()/server.header() if they're explicitly collected first.
  // Without this, checkApiKey() always sees the header as "missing" even
  // when the client sends it correctly.
  static const char* collectedHeaders[] = { "X-API-Key" };
  server.collectHeaders(collectedHeaders, 1);

  server.on("/", handleRoot);
  server.on("/change-password", HTTP_GET, handleChangePasswordPage);
  server.on("/change-password", HTTP_POST, handleChangePasswordSubmit);
  server.on("/network/save-page", HTTP_GET, handleNetworkSavePage);
  server.on("/network/save", HTTP_POST, handleNetworkSave);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateResult, handleUpdateUpload);
  server.on("/update/check", HTTP_GET, handleCheckUpdate);
  server.on("/update/apply", HTTP_POST, handleApplyUpdate);
  server.on("/update/rollback", HTTP_POST, handleRollback);
  server.on("/update/status", HTTP_GET, handleFirmwareStatus);
  server.on("/config/backup", HTTP_GET, handleConfigBackup);
  server.on("/config/restore", HTTP_GET, handleConfigRestorePage);
  server.on("/config/restore", HTTP_POST, handleConfigRestoreSubmit);

  server.on("/trigger", HTTP_POST, handleTrigger);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/reboot", HTTP_POST, handleRebootCommand);
  server.on("/network/api-save", HTTP_POST, handleApiNetworkSave);
}

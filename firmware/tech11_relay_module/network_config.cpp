#include "network_config.h"
#include "config.h"
#include <Preferences.h>

extern Preferences prefs; // defined in the main .ino, shared with auth.cpp

String deviceName;
bool useDHCP = false;
IPAddress staticIP(192, 168, 55, 230);
IPAddress gatewayIP(192, 168, 55, 1);
IPAddress subnetMask(255, 255, 255, 0);
String wifiSSID;
String wifiPassword;
bool isInAPMode = false;

static IPAddress apIP(192, 168, 4, 1);
static IPAddress apGateway(192, 168, 4, 1);
static IPAddress apSubnet(255, 255, 255, 0);

String generateDeviceName() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return "Tech11_" + mac.substring(mac.length() - 4);
}

void loadNetworkConfig() {
  prefs.begin("netcfg", true);
  deviceName = prefs.getString("devname", "");
  useDHCP = prefs.getBool("dhcp", false);
  staticIP.fromString(prefs.getString("ip", staticIP.toString()));
  gatewayIP.fromString(prefs.getString("gw", gatewayIP.toString()));
  subnetMask.fromString(prefs.getString("sn", subnetMask.toString()));
  wifiSSID = prefs.getString("wifissid", DEFAULT_WIFI_SSID);
  wifiPassword = prefs.getString("wifipass", DEFAULT_WIFI_PASSWORD);
  prefs.end();
  // NOTE: device name auto-generation (if empty) now happens in
  // ensureDeviceNameSet(), called AFTER WiFi is initialized - the MAC
  // address isn't valid/populated until then, otherwise you get "Tech11_0000".
}

void ensureDeviceNameSet() {
  if (deviceName.length() == 0) {
    deviceName = generateDeviceName();
    saveNetworkConfig(deviceName, useDHCP, staticIP.toString(), gatewayIP.toString(), subnetMask.toString());
  }
}

void saveNetworkConfig(String name, bool dhcp, String ip, String gw, String sn) {
  prefs.begin("netcfg", false);
  prefs.putString("devname", name);
  prefs.putBool("dhcp", dhcp);
  prefs.putString("ip", ip);
  prefs.putString("gw", gw);
  prefs.putString("sn", sn);
  prefs.end();
}

void saveWifiCredentials(String ssid, String password) {
  prefs.begin("netcfg", false);
  prefs.putString("wifissid", ssid);
  if (password.length() > 0) prefs.putString("wifipass", password);
  prefs.end();
}

bool connectToWiFi() {
  if (!useDHCP) WiFi.config(staticIP, gatewayIP, subnetMask);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(deviceName.c_str());
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) return false;
    delay(300);
  }
  return true;
}

void startFallbackAP() {
  isInAPMode = true;
  String apSsid = "Tech11-Setup-" + deviceName.substring(deviceName.length() - 4);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  WiFi.softAP(apSsid.c_str(), AP_PASSWORD);
  Serial.println("[WIFI] AP fallback active: " + apSsid + " @ " + WiFi.softAPIP().toString());
}

void setupWiFiWithFallback() {
  if (!connectToWiFi()) startFallbackAP();
}

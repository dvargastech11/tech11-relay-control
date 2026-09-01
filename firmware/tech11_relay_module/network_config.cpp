#include "network_config.h"
#include "config.h"
#include <Preferences.h>
#include <ETH.h>

extern Preferences prefs; // defined in the main .ino, shared with auth.cpp

String deviceName;
bool useDHCP = true;
IPAddress staticIP(192, 168, 55, 230);
IPAddress gatewayIP(192, 168, 55, 1);
IPAddress subnetMask(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);
String ntpServer = "pool.ntp.org";
String wifiSSID;
String wifiPassword;
bool isInAPMode = false;

static IPAddress apIP(192, 168, 4, 1);
static IPAddress apGateway(192, 168, 4, 1);
static IPAddress apSubnet(255, 255, 255, 0);

static volatile bool ethGotIP = false;

static void onNetworkEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname(deviceName.length() > 0 ? deviceName.c_str() : "tech11-relay-module");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ethGotIP = true;
      Serial.print("[ETH] Got IP: ");
      Serial.println(ETH.localIP());
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
      ethGotIP = false;
      break;
    default:
      break;
  }
}

String generateDeviceName() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return "Tech11_" + mac.substring(mac.length() - 4);
}

void loadNetworkConfig() {
  prefs.begin("netcfg", true);
  deviceName = prefs.getString("devname", "");
  useDHCP = prefs.getBool("dhcp", true);
  staticIP.fromString(prefs.getString("ip", staticIP.toString()));
  gatewayIP.fromString(prefs.getString("gw", gatewayIP.toString()));
  subnetMask.fromString(prefs.getString("sn", subnetMask.toString()));
  dns1.fromString(prefs.getString("dns1", dns1.toString()));
  dns2.fromString(prefs.getString("dns2", dns2.toString()));
  ntpServer = prefs.getString("ntp", NTP_SERVER);
  // Project default (Tech11 network) - a fresh device with no saved
  // credentials yet connects straight to this instead of falling into AP
  // setup mode. Once NVS has been written (either from this default
  // succeeding once, or via the Network Settings page), NVS always wins
  // on every subsequent boot - this default is only ever used the very
  // first time.
  wifiSSID = prefs.getString("wifissid", DEFAULT_WIFI_SSID);
  wifiPassword = prefs.getString("wifipass", DEFAULT_WIFI_PASSWORD);
  prefs.end();
  // NOTE: device name auto-generation (if empty) now happens in
  // ensureDeviceNameSet(), called AFTER the network is initialized - the
  // MAC address isn't valid/populated until then, otherwise you get
  // "Tech11_0000".
}

void ensureDeviceNameSet() {
  if (deviceName.length() == 0) {
    deviceName = generateDeviceName();
    saveNetworkConfig(deviceName, useDHCP, staticIP.toString(), gatewayIP.toString(),
                       subnetMask.toString(), dns1.toString(), dns2.toString(), ntpServer);
  }
}

void saveNetworkConfig(String name, bool dhcp, String ip, String gw, String sn,
                        String dns1Str, String dns2Str, String ntpStr) {
  prefs.begin("netcfg", false);
  prefs.putString("devname", name);
  prefs.putBool("dhcp", dhcp);
  prefs.putString("ip", ip);
  prefs.putString("gw", gw);
  prefs.putString("sn", sn);
  prefs.putString("dns1", dns1Str);
  prefs.putString("dns2", dns2Str);
  prefs.putString("ntp", ntpStr);
  prefs.end();
}

void saveWifiCredentials(String ssid, String password) {
  prefs.begin("netcfg", false);
  prefs.putString("wifissid", ssid);
  if (password.length() > 0) prefs.putString("wifipass", password);
  prefs.end();
}

// ---- WiFi STA (PRIMARY connection, temporarily restored) ----

bool connectToWiFi() {
  if (!useDHCP) WiFi.config(staticIP, gatewayIP, subnetMask, dns1, dns2);
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
  WiFi.mode(WIFI_AP_STA); // AP_STA so background reconnect attempts can run while the setup AP stays up
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  WiFi.softAP(apSsid.c_str()); // no password - open network for easier setup access
  Serial.println("[WIFI] AP fallback active: " + apSsid + " @ " + WiFi.softAPIP().toString());
}

void startBackgroundReconnectAttempt() {
  // Non-blocking - WiFi.begin() returns immediately and connects
  // asynchronously. The caller (main loop) checks WiFi.status() on a
  // later pass to see if it succeeded. Safe to call repeatedly.
  if (!useDHCP) WiFi.config(staticIP, gatewayIP, subnetMask, dns1, dns2);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
}

void setupNetworkWithFallback() {
  // WiFi STA is primary again (temporarily, while an Ethernet PHY power
  // issue - undervoltage causing "power up timeout" - is set aside for
  // later). See connectToEthernet() below, fully intact and unused.
  if (!connectToWiFi()) {
    startFallbackAP();
  }
}

// ---- Ethernet (currently UNUSED - not called from setup()/loop()) ----
// Kept fully intact so re-enabling this later is just a matter of
// swapping setupNetworkWithFallback() to call connectToEthernet() again,
// same as it did before this temporary revert.

bool connectToEthernet() {
  ethGotIP = false;
  WiFi.onEvent(onNetworkEvent); // ETH events route through the same dispatcher as WiFi events

  if (!useDHCP) {
    ETH.config(staticIP, gatewayIP, subnetMask, dns1, dns2);
  }

  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);

  unsigned long start = millis();
  while (!ethGotIP) {
    if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) return false;
    delay(100);
  }
  return true;
}

bool isEthernetConnected() {
  return ethGotIP;
}

void startBackgroundEthernetRetry() {
  if (ethGotIP) return;
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
}

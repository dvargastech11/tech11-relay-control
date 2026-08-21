#include "network_config.h"
#include "config.h"
#include <Preferences.h>
#include <ETH.h>

extern Preferences prefs; // defined in the main .ino, shared with auth.cpp

String deviceName;
bool useDHCP = false;
IPAddress staticIP(192, 168, 55, 230);
IPAddress gatewayIP(192, 168, 55, 1);
IPAddress subnetMask(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);
String ntpServer = "pool.ntp.org";
bool isInAPMode = false;

static IPAddress apIP(192, 168, 4, 1);
static IPAddress apGateway(192, 168, 4, 1);
static IPAddress apSubnet(255, 255, 255, 0);

static volatile bool ethGotIP = false;

static void onNetworkEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      // Hostname must be set right after ETH_START, before DHCP/static config
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
  // WiFi.macAddress() reads the factory-burned MAC from eFuse - valid and
  // stable even when Ethernet is the active interface and WiFi radio is
  // otherwise idle, so this still works fine as a naming source.
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
  dns1.fromString(prefs.getString("dns1", dns1.toString()));
  dns2.fromString(prefs.getString("dns2", dns2.toString()));
  ntpServer = prefs.getString("ntp", NTP_SERVER);
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

void startFallbackAP() {
  isInAPMode = true;
  String apSsid = "Tech11-Setup-" + deviceName.substring(deviceName.length() - 4);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  WiFi.softAP(apSsid.c_str(), AP_PASSWORD);
  Serial.println("[NET] Ethernet not connected - setup AP active: " + apSsid +
                  " @ " + WiFi.softAPIP().toString());
}

void startBackgroundEthernetRetry() {
  // ETH.begin() was already called once in setupNetworkWithFallback(). The
  // PHY itself keeps trying to establish link on its own; this just gives
  // it another nudge in case the cable was plugged in after the initial
  // attempt timed out. isEthernetConnected() (checked by the caller) is
  // what actually detects success.
  if (ethGotIP) return;
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
}

void setupNetworkWithFallback() {
  if (!connectToEthernet()) {
    startFallbackAP();
  }
}

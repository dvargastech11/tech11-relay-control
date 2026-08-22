#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>

extern String deviceName;
extern bool useDHCP;
extern IPAddress staticIP;
extern IPAddress gatewayIP;
extern IPAddress subnetMask;
extern IPAddress dns1;
extern IPAddress dns2;
extern String ntpServer;
extern String wifiSSID;
extern String wifiPassword;
extern bool isInAPMode; // true = not connected (WiFi or Ethernet), hosting setup AP

String generateDeviceName();
void ensureDeviceNameSet();
void loadNetworkConfig();
void saveNetworkConfig(String name, bool dhcp, String ip, String gw, String sn,
                        String dns1Str, String dns2Str, String ntpStr);
void saveWifiCredentials(String ssid, String password);

// WiFi STA is the PRIMARY connection again (temporarily reverted from
// Ethernet while an Ethernet PHY power issue is being worked out - see
// connectToEthernet()/etc below, kept intact and ready to re-enable later).
bool connectToWiFi();
void startFallbackAP();
void setupNetworkWithFallback();
void startBackgroundReconnectAttempt();

// Ethernet functions - currently UNUSED (not called from setup()/loop()),
// kept fully intact so re-enabling Ethernet later is just a matter of
// swapping which of these two blocks setupNetworkWithFallback() calls.
bool connectToEthernet();
bool isEthernetConnected();
void startBackgroundEthernetRetry();

#endif

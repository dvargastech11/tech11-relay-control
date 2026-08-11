#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>

extern String deviceName;
extern bool useDHCP;
extern IPAddress staticIP;
extern IPAddress gatewayIP;
extern IPAddress subnetMask;
extern String wifiSSID;
extern String wifiPassword;
extern bool isInAPMode;

String generateDeviceName();
void ensureDeviceNameSet();
void loadNetworkConfig();
void saveNetworkConfig(String name, bool dhcp, String ip, String gw, String sn);
void saveWifiCredentials(String ssid, String password);
bool connectToWiFi();
void startFallbackAP();
void setupWiFiWithFallback();

#endif

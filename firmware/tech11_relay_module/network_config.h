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
extern bool isInAPMode; // true = Ethernet not connected, hosting setup AP over WiFi

String generateDeviceName();
void ensureDeviceNameSet();
void loadNetworkConfig();
void saveNetworkConfig(String name, bool dhcp, String ip, String gw, String sn,
                        String dns1Str, String dns2Str, String ntpStr);

// Ethernet is the primary connection - WiFi is only used for the setup AP,
// there is no WiFi STA client mode in production anymore.
bool connectToEthernet();
bool isEthernetConnected();
void startFallbackAP();
void setupNetworkWithFallback();
void startBackgroundEthernetRetry();

#endif

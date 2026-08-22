#include "discovery.h"
#include "config.h"
#include "network_config.h"
#include <WiFi.h>
#include <WiFiUdp.h>

static WiFiUDP discoveryUdp;

void setupDiscovery() {
  discoveryUdp.begin(DISCOVERY_PORT);
}

void handleDiscoveryRequests() {
  int packetSize = discoveryUdp.parsePacket();
  if (packetSize <= 0) return;

  char incoming[64];
  int len = discoveryUdp.read(incoming, sizeof(incoming) - 1);
  incoming[len] = 0;

  if (strcmp(incoming, DISCOVERY_MESSAGE_EXPECTED) == 0) {
    String reply = "{\"name\":\"" + deviceName + "\",\"ip\":\"" + WiFi.localIP().toString() +
                   "\",\"mac\":\"" + WiFi.macAddress() + "\"}";
    discoveryUdp.beginPacket(discoveryUdp.remoteIP(), discoveryUdp.remotePort());
    discoveryUdp.write((const uint8_t*)reply.c_str(), reply.length());
    discoveryUdp.endPacket();
  }
}

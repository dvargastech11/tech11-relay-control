#include "discovery.h"
#include "config.h"
#include "network_config.h"
#include <WiFi.h>
#include <ETH.h>
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
    // Discovery only ever runs when Ethernet is connected (main .ino gates
    // setupDiscovery()/handleDiscoveryRequests() behind !isInAPMode), so
    // report the Ethernet IP here - not WiFi.localIP(), which is
    // meaningless now that there's no WiFi STA client mode in production.
    // MAC stays as WiFi.macAddress() since that's what the API key and
    // device name are derived from - must stay consistent across the codebase.
    String reply = "{\"name\":\"" + deviceName + "\",\"ip\":\"" + ETH.localIP().toString() +
                   "\",\"mac\":\"" + WiFi.macAddress() + "\"}";
    discoveryUdp.beginPacket(discoveryUdp.remoteIP(), discoveryUdp.remotePort());
    discoveryUdp.write((const uint8_t*)reply.c_str(), reply.length());
    discoveryUdp.endPacket();
  }
}

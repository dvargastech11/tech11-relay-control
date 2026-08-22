#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// CHANGE THESE BEFORE FLASHING / BEFORE PRODUCTION
// ============================================================

// NOTE: Ethernet (LAN8720) is the PRIMARY network connection - see the
// ETH_PHY_* pins below. WiFi is used ONLY as a fallback setup network
// (Tech11-Setup-XXXX) when Ethernet isn't connected - there is no WiFi STA
// client mode in production anymore. A fresh device with no Ethernet link
// yet will boot straight into the setup AP.

// Must match MASTER_SECRET in the Flask server's devices.py EXACTLY.
#define MASTER_SECRET "Tech11-Master-Secret-ChangeThisBeforeProduction-2026"

// GitHub OTA source (public repo raw content URLs)
#define CURRENT_FIRMWARE_VERSION "1.0"
#define GITHUB_VERSION_URL  "https://raw.githubusercontent.com/dvargastech11/tech11-relay-control/main/firmware/version.txt"
#define GITHUB_FIRMWARE_URL "https://raw.githubusercontent.com/dvargastech11/tech11-relay-control/main/firmware/firmware.bin"

// AP fallback mode - currently OPEN (no password), see network_config.cpp's
// startFallbackAP(). AP_PASSWORD below is unused but kept in case password
// protection on the setup network is wanted again later.
#define AP_PASSWORD "SetupPass123"
#define WIFI_CONNECT_TIMEOUT_MS 15000UL

// Ethernet (LAN8720 PHY) pinout - matches the standard RMII wiring:
// MDC->GPIO23, MDIO->GPIO18, clock fed into GPIO0 from the board's own
// crystal (this board has no ESP32-controlled power-enable pin, hence -1).
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  0
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_POWER -1
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN

// ============================================================
// Usually don't need to change these
// ============================================================

// Discovery protocol
#define DISCOVERY_MESSAGE_EXPECTED "TECH11_DISCOVER"
#define DISCOVERY_PORT 4210

// NTP
// Default NTP server, used only the first time the device boots (before
// any config is saved). Editable afterward on the Network Settings page.
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (-5 * 3600)  // EST - adjust for your timezone
#define DAYLIGHT_OFFSET_SEC 3600

// Relays - PRODUCTION config: 47 channels across 3x MCP23017 I2C expanders
// (16 channels each, 48 available, 47 used - matches one elevator's floor
// count). Replaces the earlier 2-relay direct-GPIO test config.
#define NUM_RELAYS 47
#define RELAY_ACTIVE_LEVEL HIGH
#define RELAY_INACTIVE_LEVEL LOW

// I2C addresses for the 3 MCP23017 boards, confirmed via i2c_scanner.ino
// testing. Full 8-combination table (each switch subtracts a fixed value
// from the 0x27 baseline): OFF/OFF/OFF=0x27, ON/OFF/OFF=0x26, OFF/ON/OFF=
// 0x25, ON/ON/OFF=0x24, OFF/OFF/ON=0x23, ON/OFF/ON=0x22, OFF/ON/ON=0x21,
// ON/ON/ON=0x20. Using the simplest single-switch combos for the 3 boards:
//   Board 1: switches all OFF        -> 0x27
//   Board 2: switch 1 ON, others OFF -> 0x26
//   Board 3: switch 2 ON, others OFF -> 0x25
#define MCP23017_ADDR_1 0x27  // relays 1-16
#define MCP23017_ADDR_2 0x26  // relays 17-32
#define MCP23017_ADDR_3 0x25  // relays 33-47, only 15 of 16 channels used

#endif

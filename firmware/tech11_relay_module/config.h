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

// I2C addresses for the 3 MCP23017 boards. Board 1 is set to this specific
// SG-IO-E017 board's FACTORY DEFAULT (0x27, per its manual - "DIP switch
// connected to high level by default, displayed address is 0x27"), so no
// DIP switch changes are needed for the first board. Boards 2 and 3 will
// need their DIP switches changed to two other distinct addresses once
// they arrive - use firmware/i2c_scanner/i2c_scanner.ino to confirm the
// actual resulting address after any switch change, since the manual
// doesn't give an exact switch-to-bit truth table.
#define MCP23017_ADDR_1 0x27  // relays 1-16 (factory default, no DIP changes needed)
#define MCP23017_ADDR_2 0x21  // relays 17-32 (placeholder - confirm via scanner once board 2 arrives)
#define MCP23017_ADDR_3 0x22  // relays 33-47, only 15 of 16 channels used (placeholder - confirm via scanner once board 3 arrives)

#endif

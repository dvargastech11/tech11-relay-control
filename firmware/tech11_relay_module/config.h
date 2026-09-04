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
#define CURRENT_FIRMWARE_VERSION "1.0.2"
#define GITHUB_VERSION_URL  "https://raw.githubusercontent.com/dvargastech11/tech11-relay-control/main/firmware/version.txt"
#define GITHUB_FIRMWARE_URL "https://raw.githubusercontent.com/dvargastech11/tech11-relay-control/main/firmware/firmware.bin"

// AP fallback mode - currently OPEN (no password), see network_config.cpp's
// startFallbackAP(). AP_PASSWORD below is unused but kept in case password
// protection on the setup network is wanted again later.
#define AP_PASSWORD "SetupPass123"
#define WIFI_CONNECT_TIMEOUT_MS 45000UL // hidden networks can take longer than 15s for the full probe/auth/DHCP sequence

// Project WiFi default - all 12 devices join this same dedicated network.
// NOTE: this is a deliberate departure from the earlier "blank base
// firmware, AP-mode-only" design - baked in here specifically so devices
// don't each need manual AP-portal setup during mass deployment. A device
// only falls back to a NEW value here if its NVS has never been written
// (see loadNetworkConfig()'s defaults) - once configured (via this default
// OR the Network Settings page), NVS always wins on subsequent boots.
// SSID is hidden - WiFi.begin(ssid, password) actively probes for the
// exact SSID rather than relying on a passive scan, so this connects fine
// without any extra "hidden network" handling.
#define DEFAULT_WIFI_SSID "Tech11"
#define DEFAULT_WIFI_PASSWORD "Way2WiFi123$"

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
#define NUM_RELAYS 48  // Lobby (always present) + up to 47 numbered floors = 48 total - no spare channel left in a fully-configured elevator
#define RELAY_ACTIVE_LEVEL LOW    // ACEIRMC 16-channel relay board is low-level trigger
#define RELAY_INACTIVE_LEVEL HIGH

// MCP23017 board addressing is AUTO-DISCOVERED at boot (see relay_control.cpp's
// scanMCPBoards()) - it scans the full valid address range (0x20-0x27) and
// assigns whichever boards respond, in ascending address order, as Board
// 1/2/3. No manual address configuration needed here anymore - just set
// each board's DIP switches to any 3 distinct addresses within 0x20-0x27.

// External digital input monitoring - a spare GPIO watching an EXTERNAL
// relay's dry contact, unrelated to the 48 floor-call relays above. With
// the internal pull-up enabled: contact open -> HIGH, contact closed ->
// LOW (shorted to GND). Not one of our own driven outputs - purely a
// sensing input for something else in the system.
#define EXTERNAL_INPUT_PIN 4
#define EXTERNAL_INPUT_STUCK_THRESHOLD_MS (60UL * 60UL * 1000UL) // 1 hour

#endif

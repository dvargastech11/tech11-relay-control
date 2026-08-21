#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// CHANGE THESE BEFORE FLASHING / BEFORE PRODUCTION
// ============================================================

// NOTE: There is deliberately NO hardcoded default WiFi SSID/password here.
// This firmware is meant to be identical "base firmware" across every
// device - a fresh, unconfigured device has no saved WiFi credentials in
// its NVS storage, so it always comes up in AP fallback mode on first
// boot. WiFi is then configured once through the AP setup portal (or the
// normal Network Settings page once connected) and saved to NVS via the
// Preferences library - a completely separate flash partition from the
// firmware code itself. Future firmware updates (OTA or Pi-initiated
// push) only replace the code partition, so saved WiFi config always
// survives an update untouched.

// Must match MASTER_SECRET in the Flask server's devices.py EXACTLY.
#define MASTER_SECRET "Tech11-Master-Secret-ChangeThisBeforeProduction-2026"

// GitHub OTA source (public repo raw content URLs)
#define CURRENT_FIRMWARE_VERSION "1.0"
#define GITHUB_VERSION_URL  "https://raw.githubusercontent.com/dvargastech11/tech11-relay-control/main/firmware/version.txt"
#define GITHUB_FIRMWARE_URL "https://raw.githubusercontent.com/dvargastech11/tech11-relay-control/main/firmware/firmware.bin"

// AP fallback mode (used whenever no WiFi is configured yet, or the
// configured WiFi can't be reached)
#define AP_PASSWORD "SetupPass123"
#define WIFI_CONNECT_TIMEOUT_MS 15000UL

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

// I2C addresses for the 3 MCP23017 boards - set via each board's onboard
// address DIP switch/jumpers to match these exactly.
#define MCP23017_ADDR_1 0x20  // relays 1-16
#define MCP23017_ADDR_2 0x21  // relays 17-32
#define MCP23017_ADDR_3 0x22  // relays 33-47 (only 15 of 16 channels used)

#endif

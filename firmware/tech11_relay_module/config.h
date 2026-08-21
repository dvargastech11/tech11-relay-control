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

// Relays (test config - 2 relays; scale up for real MCP23017 boards)
#define NUM_RELAYS 2
#define RELAY_ACTIVE_LEVEL HIGH
#define RELAY_INACTIVE_LEVEL LOW

#endif

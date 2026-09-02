#include "oled_display.h"
#include "config.h"
#include "network_config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

/*
  OLED status display - GME12864-11 module (128x64, I2C).

  DRIVER CHIP UNCERTAINTY: this module number is sold with either an
  SSD1306 or SH1106 driver depending on manufacturer/batch - we don't have
  a confirmed datasheet for this specific unit, so this defaults to
  SSD1306 (the more common variant at this size/interface). VERIFY with
  the I2C scanner (firmware/i2c_scanner) - should show up at 0x3C either
  way, that doesn't tell driver chip apart. The real tell: if the display
  shows garbled pixels, a shifted image, or content cut off on one edge,
  that's the classic symptom of an SH1106 panel being driven with SSD1306
  init/addressing (SH1106 has a 132-pixel-wide internal buffer vs
  SSD1306's exact 128, offsetting everything a few pixels). If that
  happens, swap the Adafruit_SSD1306 library/includes below for
  Adafruit_SH110X and re-test.

  5 lines total:
    Line 1: header (device name)
    Lines 2-5: the 4 most recent PRODUCTION floor-call requests, most
               recent at top, as "Floor N   HH:MM"

  Shares the existing I2C bus (SDA=GPIO21, SCL=GPIO22) with the MCP23017
  relay boards - no separate wiring needed beyond power (3.3V/GND) and
  the two I2C lines. Address 0x3C doesn't conflict with the MCP23017
  range (0x20-0x27), so all boards + this display coexist on one bus.
*/

#define RECENT_CALLS_SHOWN 4

static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool displayAvailable = false;

struct RecentCall {
  int channelNum;
  char timeStr[6]; // "HH:MM"
};

static RecentCall recentCalls[RECENT_CALLS_SHOWN];
static int recentCallCount = 0; // how many slots are actually filled (0..RECENT_CALLS_SHOWN)

static unsigned long lastRedrawMs = 0;
const unsigned long REDRAW_INTERVAL_MS = 1000;

static String currentTimeShort() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) {
    return "--:--"; // NTP not synced yet (e.g. still in AP setup mode)
  }
  char buf[6];
  strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
  return String(buf);
}

void setupOledDisplay() {
  // begin() returns false if the display isn't found at this address -
  // treat that as "no display installed" rather than a hard failure, since
  // this is an optional accessory, not required for the elevator control
  // system to function.
  displayAvailable = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);

  if (!displayAvailable) {
    Serial.println("[OLED] Not detected at 0x3C - continuing without display");
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Starting up...");
  display.display();
}

void logFloorRequest(int channelNum) {
  // Shift everything down one slot, insert the new call at the front -
  // simple approach for a buffer this small (4 entries), no need for a
  // proper ring buffer with wraparound indexing.
  for (int i = RECENT_CALLS_SHOWN - 1; i > 0; i--) {
    recentCalls[i] = recentCalls[i - 1];
  }
  recentCalls[0].channelNum = channelNum;
  currentTimeShort().toCharArray(recentCalls[0].timeStr, sizeof(recentCalls[0].timeStr));

  if (recentCallCount < RECENT_CALLS_SHOWN) recentCallCount++;
}

void updateOledDisplay() {
  if (!displayAvailable) return;

  unsigned long now = millis();
  if (now - lastRedrawMs < REDRAW_INTERVAL_MS) return;
  lastRedrawMs = now;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(deviceName);

  for (int i = 0; i < RECENT_CALLS_SHOWN; i++) {
    display.setCursor(0, (i + 1) * 12); // 12px line height at text size 1 leaves comfortable spacing
    if (i < recentCallCount) {
      display.print("Floor ");
      display.print(recentCalls[i].channelNum);
      display.print("   ");
      display.println(recentCalls[i].timeStr);
    } else {
      display.println("-");
    }
  }

  display.display();
}

#include "relay_control.h"
#include "config.h"
#include "activity_log.h"
#include <Wire.h>

/*
  Production relay driving: 47 channels across 3x MCP23017 I2C expanders.
  ---------------------------------------------------------------------------
  Each MCP23017 exposes 16 GPIO pins split across two 8-bit ports (GPIOA,
  GPIOB). Channel-to-board mapping:
    relay 1-16  -> board 1 (MCP23017_ADDR_1)
    relay 17-32 -> board 2 (MCP23017_ADDR_2)
    relay 33-47 -> board 3 (MCP23017_ADDR_3), pins 0-14 of 16 used

  Timing is millis()-based and checked in updateRelayTimers() from the main
  loop() - NOT FreeRTOS tasks like the earlier 2-relay direct-GPIO version.
  This avoids two tasks potentially hitting the shared I2C bus at the same
  moment, which the Arduino Wire library doesn't guard against on its own.
*/

// MCP23017 register addresses (BANK=0, the power-on default mode)
#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_GPIOA  0x12
#define MCP_GPIOB  0x13

static const uint8_t mcpAddresses[3] = { MCP23017_ADDR_1, MCP23017_ADDR_2, MCP23017_ADDR_3 };

// Per-channel non-blocking timer state
static bool relayActive[NUM_RELAYS];
static unsigned long relayStartTime[NUM_RELAYS];
static unsigned long relayDuration[NUM_RELAYS];

// Maps a 1-indexed relay number to which MCP23017 board (0-2) and which
// register/bit on that board.
static void resolveChannel(int relayNum, uint8_t &address, uint8_t &reg, uint8_t &bit) {
  int idx = relayNum - 1; // 0-indexed
  int boardIndex = idx / 16;
  int pinOnBoard = idx % 16;

  address = mcpAddresses[boardIndex];
  reg = (pinOnBoard < 8) ? MCP_GPIOA : MCP_GPIOB;
  bit = pinOnBoard % 8;
}

static void mcpWriteRegister(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

static uint8_t mcpReadRegister(uint8_t address, uint8_t reg) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(address, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

static void setChannelBit(uint8_t address, uint8_t reg, uint8_t bit, bool high) {
  uint8_t current = mcpReadRegister(address, reg);
  if (high) {
    current |= (1 << bit);
  } else {
    current &= ~(1 << bit);
  }
  mcpWriteRegister(address, reg, current);
}

void setupRelayPins() {
  Wire.begin(); // default ESP32 I2C pins: SDA=GPIO21, SCL=GPIO22

  for (int b = 0; b < 3; b++) {
    // All 16 pins as outputs (IODIR bit 0 = output)
    mcpWriteRegister(mcpAddresses[b], MCP_IODIRA, 0x00);
    mcpWriteRegister(mcpAddresses[b], MCP_IODIRB, 0x00);
    // Start all pins at the inactive level
    uint8_t inactiveByte = (RELAY_INACTIVE_LEVEL == HIGH) ? 0xFF : 0x00;
    mcpWriteRegister(mcpAddresses[b], MCP_GPIOA, inactiveByte);
    mcpWriteRegister(mcpAddresses[b], MCP_GPIOB, inactiveByte);
  }

  for (int i = 0; i < NUM_RELAYS; i++) {
    relayActive[i] = false;
    relayStartTime[i] = 0;
    relayDuration[i] = 0;
  }
}

void activateRelay(int relayNum, int durationMs) {
  if (relayNum < 1 || relayNum > NUM_RELAYS) return;

  uint8_t address, reg, bit;
  resolveChannel(relayNum, address, reg, bit);
  setChannelBit(address, reg, bit, RELAY_ACTIVE_LEVEL == HIGH);

  int idx = relayNum - 1;
  relayActive[idx] = true;
  relayStartTime[idx] = millis();
  relayDuration[idx] = durationMs;

  addLog("OUT", "Relay #" + String(relayNum) + " activated for " + String(durationMs) + "ms");
}

void updateRelayTimers() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (relayActive[i] && (now - relayStartTime[i] >= relayDuration[i])) {
      uint8_t address, reg, bit;
      resolveChannel(i + 1, address, reg, bit);
      setChannelBit(address, reg, bit, RELAY_INACTIVE_LEVEL == HIGH);

      relayActive[i] = false;
      addLog("OUT", "Relay #" + String(i + 1) + " released");
    }
  }
}

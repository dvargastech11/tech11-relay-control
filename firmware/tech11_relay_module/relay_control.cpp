#include "relay_control.h"
#include "config.h"
#include "activity_log.h"
#include <Wire.h>

/*
  Production relay driving: up to 48 channels across 3x MCP23017 I2C
  expanders (16 each). 47 are actually wired to floor buttons (NUM_RELAYS);
  channel 48 is spare hardware capacity, still testable via the diagnostic
  page but never reachable through the production /trigger endpoint.
  ---------------------------------------------------------------------------
  BOARD ADDRESSING IS AUTO-DISCOVERED, not hardcoded. On boot,
  scanMCPBoards() probes every valid MCP23017 address (0x20-0x27, the full
  range covered by the A0/A1/A2 address pins) and takes whichever ones
  respond, in ascending address order, as Board 1/2/3. This means boards
  can be set to ANY valid address via their DIP switches - no need to
  determine a specific address and hardcode it in config.h, and no need
  to keep boards in a specific physical order. Only the first
  NUM_MCP_BOARDS (3) found are used; if more than 3 respond, the extras
  are logged and ignored.

  Boards that aren't physically present/wired yet are simply not found
  during the scan and are skipped during init/writes rather than causing
  I2C errors or hangs - this lets you bring boards online incrementally as
  hardware arrives.

  Timing is millis()-based and checked in updateRelayTimers() from the main
  loop() - NOT FreeRTOS tasks, since two tasks hitting the shared I2C bus
  at the same moment isn't guarded against by the Arduino Wire library.
*/

// MCP23017 register addresses (BANK=0, the power-on default mode)
#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_GPIOA  0x12
#define MCP_GPIOB  0x13

// Full valid MCP23017 address range (A0/A1/A2 pins give 8 combinations)
#define MCP23017_SCAN_START 0x20
#define MCP23017_SCAN_END   0x27

static uint8_t discoveredAddresses[NUM_MCP_BOARDS] = { 0, 0, 0 };
static bool boardOnline[NUM_MCP_BOARDS] = { false, false, false };

// Per-channel non-blocking timer state - sized to full hardware capacity
static bool relayActive[HARDWARE_CHANNELS];
static unsigned long relayStartTime[HARDWARE_CHANNELS];
static unsigned long relayDuration[HARDWARE_CHANNELS];

// Maps a 1-indexed channel number (1-48) to which MCP23017 board (0-2) and
// which register/bit on that board.
static void resolveChannel(int channelNum, uint8_t &address, uint8_t &reg, uint8_t &bit, int &boardIndexOut) {
  int idx = channelNum - 1; // 0-indexed
  int boardIndex = idx / 16;
  int pinOnBoard = idx % 16;

  address = discoveredAddresses[boardIndex];
  reg = (pinOnBoard < 8) ? MCP_GPIOA : MCP_GPIOB;
  bit = pinOnBoard % 8;
  boardIndexOut = boardIndex;
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

void scanMCPBoards() {
  // Reset state before rescanning
  for (int i = 0; i < NUM_MCP_BOARDS; i++) {
    discoveredAddresses[i] = 0;
    boardOnline[i] = false;
  }

  int foundCount = 0;
  Serial.println("[MCP] Scanning I2C addresses 0x20-0x27 for MCP23017 boards...");

  for (uint8_t addr = MCP23017_SCAN_START; addr <= MCP23017_SCAN_END; addr++) {
    Wire.beginTransmission(addr);
    uint8_t result = Wire.endTransmission();

    if (result == 0) {
      if (foundCount < NUM_MCP_BOARDS) {
        discoveredAddresses[foundCount] = addr;
        boardOnline[foundCount] = true;
        Serial.printf("[MCP] Found board at 0x%02X -> assigned as Board %d (relays %d-%d)\n",
                      addr, foundCount + 1, foundCount * 16 + 1, foundCount * 16 + 16);
      } else {
        Serial.printf("[MCP] Found board at 0x%02X but all %d board slots are already filled - ignoring\n",
                      addr, NUM_MCP_BOARDS);
      }
      foundCount++;
    }
  }

  for (int i = foundCount; i < NUM_MCP_BOARDS; i++) {
    Serial.printf("[MCP] Board %d: not detected (fewer than %d boards found on the bus)\n", i + 1, NUM_MCP_BOARDS);
  }
}

bool isBoardOnline(int boardIndex) {
  if (boardIndex < 0 || boardIndex >= NUM_MCP_BOARDS) return false;
  return boardOnline[boardIndex];
}

uint8_t getBoardAddress(int boardIndex) {
  if (boardIndex < 0 || boardIndex >= NUM_MCP_BOARDS) return 0;
  return discoveredAddresses[boardIndex];
}

void setupRelayPins() {
  Wire.begin(); // default ESP32 I2C pins: SDA=GPIO21, SCL=GPIO22

  scanMCPBoards();

  uint8_t inactiveByte = (RELAY_INACTIVE_LEVEL == HIGH) ? 0xFF : 0x00;

  for (int b = 0; b < NUM_MCP_BOARDS; b++) {
    if (!boardOnline[b]) continue; // skip boards not physically present yet

    // All 16 pins as outputs (IODIR bit 0 = output)
    mcpWriteRegister(discoveredAddresses[b], MCP_IODIRA, 0x00);
    mcpWriteRegister(discoveredAddresses[b], MCP_IODIRB, 0x00);
    // Start all pins at the inactive level
    mcpWriteRegister(discoveredAddresses[b], MCP_GPIOA, inactiveByte);
    mcpWriteRegister(discoveredAddresses[b], MCP_GPIOB, inactiveByte);
  }

  for (int i = 0; i < HARDWARE_CHANNELS; i++) {
    relayActive[i] = false;
    relayStartTime[i] = 0;
    relayDuration[i] = 0;
  }
}

static void activateChannelInternal(int channelNum, int durationMs) {
  if (channelNum < 1 || channelNum > HARDWARE_CHANNELS) return;

  uint8_t address, reg, bit;
  int boardIndex;
  resolveChannel(channelNum, address, reg, bit, boardIndex);

  if (!isBoardOnline(boardIndex)) {
    addLog("OUT", "Channel #" + String(channelNum) + " skipped - board " +
                  String(boardIndex + 1) + " not online");
    return;
  }

  setChannelBit(address, reg, bit, RELAY_ACTIVE_LEVEL == HIGH);

  int idx = channelNum - 1;
  relayActive[idx] = true;
  relayStartTime[idx] = millis();
  relayDuration[idx] = durationMs;

  addLog("OUT", "Channel #" + String(channelNum) + " activated for " + String(durationMs) + "ms");
}

void activateRelay(int relayNum, int durationMs) {
  // Production path - capped at NUM_RELAYS (47), the actual floors in use.
  if (relayNum < 1 || relayNum > NUM_RELAYS) return;
  activateChannelInternal(relayNum, durationMs);
}

void activateChannelDiag(int channel1to48, int durationMs) {
  // Diagnostic path - allows testing the full 48-channel hardware capacity,
  // including the one spare channel not mapped to any floor.
  activateChannelInternal(channel1to48, durationMs);
}

void updateRelayTimers() {
  unsigned long now = millis();
  for (int i = 0; i < HARDWARE_CHANNELS; i++) {
    if (relayActive[i] && (now - relayStartTime[i] >= relayDuration[i])) {
      uint8_t address, reg, bit;
      int boardIndex;
      resolveChannel(i + 1, address, reg, bit, boardIndex);

      if (isBoardOnline(boardIndex)) {
        setChannelBit(address, reg, bit, RELAY_INACTIVE_LEVEL == HIGH);
      }

      relayActive[i] = false;
      addLog("OUT", "Channel #" + String(i + 1) + " released");
    }
  }
}

// ============================================================
// Test modes (continuous - not single-shot)
// ============================================================

enum TestMode { TEST_NONE, TEST_ALL_ON, TEST_ALL_OFF, TEST_CYCLE_ALL, TEST_CYCLE_CHANNEL, TEST_CYCLE_BOARD };

static TestMode currentTestMode = TEST_NONE;
static int testHoldMs = 1000;
static int testTargetChannel = 1;   // 1-indexed, used by TEST_CYCLE_CHANNEL
static int testTargetBoard = 0;     // 0-indexed, used by TEST_CYCLE_BOARD
static bool testCycleOn = false;    // current phase of the cycle (true = energized)
static unsigned long testLastToggle = 0;

// Directly sets one channel's physical state, bypassing the normal
// single-shot timer array entirely - used by all test mode functions.
static void setChannelDirect(int channelNum, bool active) {
  uint8_t address, reg, bit;
  int boardIndex;
  resolveChannel(channelNum, address, reg, bit, boardIndex);
  if (!isBoardOnline(boardIndex)) return;
  setChannelBit(address, reg, bit, active ? (RELAY_ACTIVE_LEVEL == HIGH) : (RELAY_INACTIVE_LEVEL == HIGH));
}

static void setAllChannelsDirect(bool active) {
  for (int ch = 1; ch <= HARDWARE_CHANNELS; ch++) {
    setChannelDirect(ch, active);
  }
}

static void setBoardChannelsDirect(int boardIndex, bool active) {
  for (int p = 0; p < 16; p++) {
    int channelNum = boardIndex * 16 + p + 1;
    setChannelDirect(channelNum, active);
  }
}

void startTestAllOn() {
  stopTestMode();
  setAllChannelsDirect(true);
  currentTestMode = TEST_ALL_ON;
  addLog("OUT", "Test mode: ALL ON (holding)");
}

void startTestAllOff() {
  stopTestMode();
  setAllChannelsDirect(false);
  currentTestMode = TEST_ALL_OFF;
  addLog("OUT", "Test mode: ALL OFF (holding)");
}

void startTestCycleAll(int holdMs) {
  stopTestMode();
  currentTestMode = TEST_CYCLE_ALL;
  testHoldMs = holdMs;
  testCycleOn = true;
  testLastToggle = millis();
  setAllChannelsDirect(true);
  addLog("OUT", "Test mode: cycling ALL every " + String(holdMs) + "ms");
}

void startTestCycleChannel(int channel1to48, int holdMs) {
  stopTestMode();
  currentTestMode = TEST_CYCLE_CHANNEL;
  testTargetChannel = channel1to48;
  testHoldMs = holdMs;
  testCycleOn = true;
  testLastToggle = millis();
  setChannelDirect(channel1to48, true);
  addLog("OUT", "Test mode: cycling channel #" + String(channel1to48) + " every " + String(holdMs) + "ms");
}

void startTestCycleBoard(int boardIndex, int holdMs) {
  stopTestMode();
  currentTestMode = TEST_CYCLE_BOARD;
  testTargetBoard = boardIndex;
  testHoldMs = holdMs;
  testCycleOn = true;
  testLastToggle = millis();
  setBoardChannelsDirect(boardIndex, true);
  addLog("OUT", "Test mode: cycling board " + String(boardIndex + 1) + " every " + String(holdMs) + "ms");
}

void stopTestMode() {
  if (currentTestMode == TEST_NONE) return;

  switch (currentTestMode) {
    case TEST_ALL_ON:
    case TEST_ALL_OFF:
    case TEST_CYCLE_ALL:
      setAllChannelsDirect(false);
      break;
    case TEST_CYCLE_CHANNEL:
      setChannelDirect(testTargetChannel, false);
      break;
    case TEST_CYCLE_BOARD:
      setBoardChannelsDirect(testTargetBoard, false);
      break;
    default:
      break;
  }

  currentTestMode = TEST_NONE;
  addLog("OUT", "Test mode stopped, relays OFF");
}

void updateTestMode() {
  if (currentTestMode == TEST_NONE || currentTestMode == TEST_ALL_ON || currentTestMode == TEST_ALL_OFF) {
    return; // hold modes don't need periodic toggling
  }

  unsigned long now = millis();
  if (now - testLastToggle < (unsigned long)testHoldMs) return;

  testLastToggle = now;
  testCycleOn = !testCycleOn;

  switch (currentTestMode) {
    case TEST_CYCLE_ALL:
      setAllChannelsDirect(testCycleOn);
      break;
    case TEST_CYCLE_CHANNEL:
      setChannelDirect(testTargetChannel, testCycleOn);
      break;
    case TEST_CYCLE_BOARD:
      setBoardChannelsDirect(testTargetBoard, testCycleOn);
      break;
    default:
      break;
  }
}

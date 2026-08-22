#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include <Arduino.h>

#define HARDWARE_CHANNELS 48   // 3 boards x 16 - total physical capacity
#define NUM_MCP_BOARDS 3

void setupRelayPins();
void activateRelay(int relayNum, int durationMs);   // production path, capped at NUM_RELAYS
void activateChannelDiag(int channel1to48, int durationMs); // diagnostic path, full 48 channels
void updateRelayTimers();

void scanMCPBoards();               // re-checks which boards actually respond
bool isBoardOnline(int boardIndex); // 0, 1, or 2
uint8_t getBoardAddress(int boardIndex);

// ---- Test modes (continuous, not single-shot) ----
// All of these bypass the normal single-shot activateRelay() timer array -
// they hold or cycle indefinitely until stopTestMode() is called. Call
// updateTestMode() every loop() iteration for the cycle modes to actually
// toggle over time.
void startTestAllOn();
void startTestAllOff();
void startTestCycleAll(int holdMs);
void startTestCycleChannel(int channel1to48, int holdMs);
void startTestCycleBoard(int boardIndex, int holdMs); // 0, 1, or 2
void stopTestMode();
void updateTestMode();

#endif

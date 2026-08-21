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

#endif

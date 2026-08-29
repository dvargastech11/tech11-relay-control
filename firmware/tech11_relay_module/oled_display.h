#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Arduino.h>

void setupOledDisplay();
void updateOledDisplay(); // call every loop() - internally throttled, redraws periodically

// Call this from the PRODUCTION relay activation path only (not diagnostic
// test triggers) whenever a real floor-call request comes through.
void logFloorRequest(int channelNum);

#endif

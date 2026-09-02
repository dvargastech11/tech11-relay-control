#include "activity_log.h"
#include <time.h>

LogEntry activityLog[LOG_SIZE];
int logWriteIndex = 0;
int logCount = 0;

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Unsynced (uptime " + String(millis() / 1000) + "s)";
  }
  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

void addLog(String direction, String message) {
  String timestamp = getTimestamp();
  activityLog[logWriteIndex] = { timestamp, direction, message };
  logWriteIndex = (logWriteIndex + 1) % LOG_SIZE;
  if (logCount < LOG_SIZE) logCount++;

  // Also print live to Serial - so relay activity (and everything else
  // that goes through this same function) is visible in real time over
  // USB, not just after the fact via the /logs web page.
  Serial.println("[" + timestamp + "] " + direction + ": " + message);
}

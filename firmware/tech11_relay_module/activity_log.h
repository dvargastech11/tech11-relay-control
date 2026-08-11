#ifndef ACTIVITY_LOG_H
#define ACTIVITY_LOG_H

#include <Arduino.h>

#define LOG_SIZE 50

struct LogEntry {
  String timestamp;
  String direction;
  String message;
};

extern LogEntry activityLog[LOG_SIZE];
extern int logWriteIndex;
extern int logCount;

String getTimestamp();
void addLog(String direction, String message);

#endif

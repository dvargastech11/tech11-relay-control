#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>

extern String lastUpdateCheckResult;

String fetchRemoteVersion();
void performGitHubUpdate();
void checkForUpdates(bool forceApply);
String getRunningPartitionLabel();
bool rollbackAvailable();

#endif

#ifndef AUTH_H
#define AUTH_H

#include <Arduino.h>

extern const char* ADMIN_USERNAME;
extern String adminPassword;
extern bool mustChangePassword;
extern String deviceApiKey;

String computeDeviceApiKey();
void loadAuthConfig();
void saveAuthConfig(String newPassword, bool changed);

bool checkAuth();
bool checkForcedPasswordChange();
bool checkApiKey();
bool checkApiKeySilent();

#endif

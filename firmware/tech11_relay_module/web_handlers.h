#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

// Registers every HTTP route with the global server object.
// Call once from setup(), after WiFi/auth/relay/discovery are initialized.
void registerWebHandlers();

#endif

#ifndef APP_H
#define APP_H

#include "config.h"

// Prototypes for application-layer functions
void initializeCommunication();
bool isStatusUpdateDue();
void handleIncomingCommands();
void processIncomingStream(Stream &port);
void sendStatusUpdate();
void sendToPort(Stream &port, const String& message);
void sendErrorToPort(Stream &port, const char* errorMessage);
void sendToggleConfirmation(Stream &port, const char* relayName, bool newState);
void printDebugInfo();

#endif // APP_H
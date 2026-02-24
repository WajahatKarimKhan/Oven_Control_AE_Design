#ifndef HAL_H
#define HAL_H

#include "config.h"

// Prototypes for HAL functions
void initializePins();
void initializeSensors();
void readTemperatureSensors();
void applyRelayStates();
void toggleRelay(int pin, bool &stateVariable);

#endif // HAL_H
#ifndef OVEN_LOGIC_H
#define OVEN_LOGIC_H

#include "config.h"

// =================================================================
// MAIN LOGIC INTERFACE
// =================================================================

// Called once at setup
void initializeLogic();

// Called every loop to handle state transitions (IDLE -> PREHEAT -> READY -> RUNNING)
void updateStateMachine();

// Called every loop to calculate PID and set Relays
void updateRelayLogic();

#endif // OVEN_LOGIC_H
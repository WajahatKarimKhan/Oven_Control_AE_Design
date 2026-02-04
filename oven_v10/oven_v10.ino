/*
  =================================================================
  BECO Oven - PID Controller Version
  =================================================================
*/

#include "config.h"
#include "app.h"
#include "oven_logic.h"
#include "hal.h"
#include "drivers.h"
#include "logger.h" // <--- NEW INCLUDE

void setup() {
  initializeCommunication();
  initializePins();
  initializeSensors();
  setHardcodedTime();
  
  // Initialize SD Logger (Pin 10)
  initializeLogger(); // <--- NEW INITIALIZATION
  
  initializeLogic();
  Serial.println("Initialization complete. PID Controller Running.");
}

void loop() {
  // 1. Handle Commands (Settings updates, etc.)
  handleIncomingCommands();

  // 2. State Machine Logic
  updateStateMachine();
  
  // 3. Sensor Reading & Logging (Slow, e.g., every 3 seconds)
  if (isStatusUpdateDue()) {
    readTemperatureSensors(); // Updates currentTemps[]
    sendStatusUpdate();       // Sends JSON to Serial/App
    
    logSystemData();          // <--- NEW: Log metrics to SD Card
    
    printDebugInfo();   
  }

  // 4. Relay Logic & PID (FAST - Must run every loop)
  // This manages the Time Proportioned Control windows.
  updateRelayLogic();
}
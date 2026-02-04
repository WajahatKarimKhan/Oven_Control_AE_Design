/*
  =================================================================
  BECO Oven - Thermostat Slave Controller
  Arduino Due Version - V2.8.3 (RS485 Pin Fix)
  
  Refactored into 4-Layer Architecture.
  - app.h:       Communication and high-level tasks
  - oven_logic.h: Core state machine and thermostat rules
  - hal.h:       Hardware Abstraction (pins, sensors, relays)
  - drivers.h:   Driver instances and flash storage
  =================================================================
*/

#include "config.h"
#include "app.h"
#include "oven_logic.h"
#include "hal.h"
#include "drivers.h"

// =================================================================
// SECTION 1: SETUP
// =================================================================
void setup() {
  // Start serial ports (from app.h)
  initializeCommunication();

  // Initialize hardware (from hal.h)
  initializePins();
  initializeSensors();

  // Load settings and set time (from drivers.h)
  setHardcodedTime();
  
  // Set the initial state based on loaded settings (from oven_logic.h)
  initializeLogic();

  Serial.println("Initialization complete. Controller is running.");
}

// =================================================================
// SECTION 2: MAIN LOOP
// =================================================================
void loop() {
  // Layer 1: Check for incoming commands (e.g., from Serial or RS485)
  handleIncomingCommands();

  // Layer 2: Run the main oven state machine (IDLE, PREHEATING, etc.)
  updateStateMachine();
  
  // --- Periodic Update (non-blocking) ---
  if (isStatusUpdateDue()) {
    
    // Layer 3: Read all sensor data
    readTemperatureSensors();
    
    // Layer 2: Apply thermostat rules based on new temps
    updateRelayLogic();
    
    // Layer 1: Send the JSON status update
    sendStatusUpdate();
    
    // Layer 1: Print debug info to local Serial
    printDebugInfo();   
  }
}
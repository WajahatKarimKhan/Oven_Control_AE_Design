#include "oven_logic.h"
#include "hal.h"       // Needs applyRelayStates()
#include "drivers.h"   // Needs saveSettings()

void initializeLogic() {
  loadSettings();

  if (settings.scheduledUnixTime != 0) {
    currentState = AWAITING_SCHEDULE;
    Serial.println("Restored state to AWAITING_SCHEDULE.");
  } else {
    currentState = IDLE;
    Serial.println("Loaded settings. Controller is running in IDLE state.");
  }
}

void updateStateMachine() {
  // This function is called every loop and manages the oven's state
  switch (currentState) {
    case IDLE:
      // In IDLE, we do nothing and wait for commands (handled in app.cpp)
      break;
      
    case AWAITING_SCHEDULE:
      if (settings.scheduledUnixTime != 0 && rtc.now().unixtime() >= settings.scheduledUnixTime) {
        Serial.println("SCHEDULED PREHEAT STARTING!");
        Serial.println("Moving to PREHEATING state.");
        settings.scheduledUnixTime = 0; // Clear the schedule
        saveSettings();
        
        currentState = PREHEATING;
        preheatComplete = false;
        preheatStartTime = millis(); 
        manualControlActive = false;
        manualAlarmActive = false; 
      }
      break;
      
    case PREHEATING: {
      // Logic: Transition to RUNNING if preheat is complete (flag set in updateRelayLogic)
      if (preheatComplete) {
        Serial.println("Preheat target reached. Starting RUNNING sequence.");
        currentState = RUNNING;
        recipeStartTime = millis(); 
        
        // Reset Steam Valve Logic for the new run
        steamValveOpenTime = millis();
        steamValveOpened = false; // Used to track if we've handled the valve
      }
      break;
    }
      
    case RUNNING: {
      // Check if the recipe time has passed
      unsigned long recipeDurationMillis = settings.recipeTimeMinutes * 60000UL;
      
      if (millis() - recipeStartTime >= recipeDurationMillis) {
        Serial.println("Recipe Timer Finished.");
        Serial.println("Moving to ALARM_COMPLETION state.");
        
        currentState = ALARM_COMPLETION;
        alarmStartTime = millis(); // Start the 30-second timer
        
        // Force immediate update so heaters turn off and alarm turns on NOW
        updateRelayLogic();
      }
      break;
    }

    case ALARM_COMPLETION: {
      // Check if 30 seconds have passed since alarm started
      if (millis() - alarmStartTime >= 30000UL) {
        Serial.println("Alarm sequence finished (30s timeout).");
        Serial.println("System entering IDLE.");
        currentState = IDLE;
        
        // --- ROBUST SHUTDOWN SEQUENCE ---
        // 1. Explicitly zero out all relay states in memory
        relayStates.rod1 = false;
        relayStates.rod2 = false;
        relayStates.rodSteam = false;
        relayStates.valve = false;
        relayStates.light = false;
        relayStates.alarm = false; 
        
        // 2. Force apply to hardware immediately
        // We bypass updateRelayLogic() here to ensure no logic flags (like manualControlActive) 
        // prevent the shutdown.
        applyRelayStates(); 
      }
      break;
    }
  }
}

void updateRelayLogic() {
  // If manual control is active (e.g., via App debugging), skip automatic logic
  if (manualControlActive) return;

  // Read latest temperatures for convenience
  float tempRod1 = currentTemps[0];
  float tempRodSteam = currentTemps[1];
  float tempRod2 = currentTemps[2];

  switch (currentState) {
    case IDLE:
    case AWAITING_SCHEDULE:
      // Safe State: Everything OFF
      relayStates.rod1 = false;
      relayStates.rod2 = false;
      relayStates.rodSteam = false;
      relayStates.valve = false;
      relayStates.light = false;
      relayStates.alarm = false;
      break;
      
    case PREHEATING: {
        // Check if all sensors reached the preheat temperature
        bool allAtTemp = (tempRod1 >= settings.preheatTemperature) &&
                         (tempRod2 >= settings.preheatTemperature) &&
                         (tempRodSteam >= settings.preheatTemperature);
                         
        if (allAtTemp) {
          preheatComplete = true; // Signal State Machine to move to RUNNING
          
          // Prevent overshoot by turning off immediately
          relayStates.rod1 = false;
          relayStates.rod2 = false;
          relayStates.rodSteam = false;
        } else {
          // Bang-Bang Control: Turn ON if below target
          relayStates.rod1 = (tempRod1 < settings.preheatTemperature); 
          relayStates.rod2 = (tempRod2 < settings.preheatTemperature);
          relayStates.rodSteam = (tempRodSteam < settings.preheatTemperature);
        }
        
        relayStates.light = true;
        relayStates.valve = false;
        relayStates.alarm = false;
        break;
      }
      
    case RUNNING:
      // 1. Thermostat Control (maintain specific thresholds)
      relayStates.rod1 = (tempRod1 < settings.thresholds.rod1); 
      relayStates.rod2 = (tempRod2 < settings.thresholds.rod2); 
      relayStates.rodSteam = (tempRodSteam < settings.thresholds.rodSteam);
      
      // 2. Steam Valve Logic: Open for first 10 seconds of RUNNING
      if (millis() - steamValveOpenTime < 10000UL) {
        relayStates.valve = true;
      } else {
        relayStates.valve = false;
      }
      
      relayStates.light = true;
      relayStates.alarm = false;
      break;
        
    case ALARM_COMPLETION:
      // Completion Logic: Heaters OFF, Light ON, Alarm ON
      relayStates.rod1 = false;
      relayStates.rod2 = false;
      relayStates.rodSteam = false;
      relayStates.valve = false;
      relayStates.light = true;
      
      // RING THE ALARM
      relayStates.alarm = true;
      break;
  }
  // ---------------------------------------------------------
  // OVERRIDE: App-Triggered Steam (20-Second Timer)
  // ---------------------------------------------------------
  if (appSteamActive) {
    unsigned long elapsed = millis() - appSteamStartTime;
    
    if (elapsed < 20000UL) { 
      // Timer is running (less than 20 seconds)
      relayStates.valve = true; // Force Valve OPEN (overrides thermostat)
    } 
    else {
      // Timer finished (20 seconds passed)
      Serial.println("App Steam Timer Expired: Turning Valve OFF.");
      relayStates.valve = false; // Force Valve OFF
      appSteamActive = false;    // Reset flag so normal logic resumes
    }
  }
  // ---------------------------------------------------------
  // Apply the calculated states to the physical pins
  applyRelayStates();
}
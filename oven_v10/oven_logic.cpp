#include "oven_logic.h"
#include "hal.h"       // Needs applyRelayStates()
#include "drivers.h"   // Needs saveSettings()

// --- Global Timer Variables for TPC ---
unsigned long windowStartTimeRod1 = 0;
unsigned long windowStartTimeRod2 = 0;
unsigned long windowStartTimeSteam = 0;

// =================================================================
// Direct OR PID
// =================================================================
#define PID_CTRL 1
#define DIRECT_CTRL 0

bool ctrl_Mode=1;

#define RELAY_ON 1
#define RELAY_OFF 0
#define MIN_ACTUATION_TIME 2000

//Decisional Limit Parametrization
unsigned long MIN_ACTUATION_TIME_ROD_1 = 2000;
unsigned long MIN_ACTUATION_TIME_ROD_2 = 1000;
unsigned long MIN_ACTUATION_TIME_STEAM = 1000;
//-----PREHEATING Temperature Tolerences-----
unsigned int PREHEAT_TOLERANCE_ROD_1 = 3;
unsigned int PREHEAT_TOLERANCE_ROD_2 = 3;
unsigned int PREHEAT_TOLERANCE_STEAM = 3;

// =================================================================
// GENERIC HELPER FUNCTIONS
// =================================================================

void configurePid(QuickPID &pid, PidParams &params) {
  pid.SetTunings(params.kp, params.ki, params.kd);
  pid.SetOutputLimits(0, PID_WINDOW_SIZE);
  pid.SetMode(QuickPID::AUTOMATIC);
}

bool calculateTpcState(double pidOutput, unsigned long &windowStartTime, unsigned long min_actuation_time) {
  unsigned long now = millis();
  unsigned long relay_status = 0;
  if (now - windowStartTime >= PID_WINDOW_SIZE) {
    windowStartTime += PID_WINDOW_SIZE;
  }
  if (now - windowStartTime >= PID_WINDOW_SIZE) {
      windowStartTime = now;
  }
  if (pidOutput > (now - windowStartTime)){
    if(pidOutput >= min_actuation_time){
      relay_status=RELAY_ON;
    }
    else{ relay_status=RELAY_OFF;}
  }
  else{
     relay_status=RELAY_OFF;
  } 
  return relay_status; 
}

// =================================================================
// LOGIC SUB-ROUTINES
// =================================================================

void updatePidInputs() {
  pidInputRod1 = (double)currentTemps[0];
  pidInputRod2 = (double)currentTemps[2]; 
  pidInputSteam = (double)currentTemps[1];
}

void updatePidSetpoints() {
  // Set Points from LCD

  double targetRod1 = 0;
  double targetRod2 = 0;
  double targetSteam = 0;

  // LOGIC: aim for Threshold 
  if (currentState == PREHEATING || currentState == READY || currentState == RUNNING) {
     targetRod1 = (double)settings.thresholds.rod1;
     targetRod2 = (double)settings.thresholds.rod2;
     targetSteam = (double)settings.thresholds.rodSteam;
    //added  other rods conditions too
     if (currentState == PREHEATING) {
         if (pidInputRod1 >= (targetRod1 - PREHEAT_TOLERANCE_ROD_1) 
          && pidInputRod2 >= (targetRod2 - PREHEAT_TOLERANCE_ROD_2) 
          && pidInputSteam >= (targetSteam - PREHEAT_TOLERANCE_STEAM)) { 
             preheatComplete = true; 
         }
     }
  } 
  
  pidSetpointRod1 = targetRod1;
  pidSetpointRod2 = targetRod2;
  pidSetpointSteam = targetSteam;
}

void computePids() {
  pidRod1.Compute();
  pidRod2.Compute();
  pidSteam.Compute();
}

void applyHeaterLogic() {
  if (ctrl_Mode == PID_CTRL) {
  relayStates.rod1     = calculateTpcState(pidOutputRod1, windowStartTimeRod1, MIN_ACTUATION_TIME_ROD_1);
  relayStates.rod2     = calculateTpcState(pidOutputRod2, windowStartTimeRod2, MIN_ACTUATION_TIME_ROD_2);
  relayStates.rodSteam = calculateTpcState(pidOutputSteam, windowStartTimeSteam, MIN_ACTUATION_TIME_STEAM);
  }
  else if (ctrl_Mode == DIRECT_CTRL){
  // --- BANG-BANG MODE ---
    // Rod 1
    if (pidInputRod1 <= settings.thresholds.rod1 - 20) relayStates.rod1 = RELAY_ON;
    else if (pidInputRod1 >= settings.thresholds.rod1)          relayStates.rod1 = RELAY_OFF;

    // Rod 2
    if (pidInputRod2 <= settings.thresholds.rod2 - 5) relayStates.rod2 = RELAY_ON;
    else if (pidInputRod2 >= settings.thresholds.rod2)          relayStates.rod2 = RELAY_OFF;

    // Rod Steam
    if (pidInputSteam <= settings.thresholds.rodSteam - 5) relayStates.rodSteam = RELAY_ON;
    else if (pidInputSteam >= settings.thresholds.rodSteam)          relayStates.rodSteam = RELAY_OFF;
  }
}

void applyValveAndAuxLogic() {
  unsigned long now = millis();

  // 1. Valve Logic (Manual Toggle with 20s Timeout)
  // ----------------------------------------------------
  
  // Check Timer: If manual override has been ON for > 20s, turn it OFF.
  if (manualValveOverride && (now - steamValveOpenTime >= 20000UL)) {
      manualValveOverride = false;
      Serial.println("Valve Safety Timeout: 20s limit reached.");
  }
  
  // Safety Check: Steam Rod must be > 160C (Updated)
  bool isSteamTempSafe = (currentTemps[1] >= STEAM_SAFETY_THRESHOLD);
  
  // Final Decision
  if (manualValveOverride && isSteamTempSafe) {
    relayStates.valve = true;
  } else {
    relayStates.valve = false;
  }
  // ----------------------------------------------------

  // 2. Aux Logic (Light/Alarm)
  if (currentState == RUNNING) {
    relayStates.light = true;
    relayStates.alarm = false;
  } 
  else if (currentState == READY) {
    relayStates.light = true;
    // Pulse Alarm: Beep for 500ms every 10 seconds
    if ((now % 10000UL) < 500UL) {
      relayStates.alarm = true;
    } else {
      relayStates.alarm = false;
    }
  }
  else if (currentState == ALARM_COMPLETION) {
    relayStates.light = true;
    relayStates.alarm = true;
    
    // Force actuators OFF
    relayStates.rod1 = false;
    relayStates.rod2 = false;
    relayStates.rodSteam = false;
    relayStates.valve = false;
  }
}

void applySafetyOverrides() {
  if (currentState == IDLE || currentState == AWAITING_SCHEDULE) {
     relayStates.rod1 = false;
     relayStates.rod2 = false;
     relayStates.rodSteam = false;
     relayStates.alarm = false;
  }
}

// =================================================================
// MAIN INTERFACE FUNCTIONS
// =================================================================

void initializeLogic() {
  loadSettings();
  configurePid(pidRod1, settings.rod1Pid);
  configurePid(pidRod2, settings.rod2Pid);
  configurePid(pidSteam, settings.rodSteamPid);

  // --- STAGGERED START TIMES ---
  // Offset each window by 1000ms to prevent simultaneous inrush current
  unsigned long now = millis();
  windowStartTimeRod1 = now;
  windowStartTimeRod2 = now + 1000; 
  windowStartTimeSteam = now + 2000;

  if (settings.scheduledUnixTime != 0) {
    currentState = AWAITING_SCHEDULE;
  } else {
    currentState = IDLE;
  }
}

void updateStateMachine() {
  switch (currentState) {
    case IDLE: break;
      
    case AWAITING_SCHEDULE:
      if (settings.scheduledUnixTime != 0 && rtc.now().unixtime() >= settings.scheduledUnixTime) {
        settings.scheduledUnixTime = 0; 
        saveSettings();
        currentState = PREHEATING;
        preheatStartTime = millis(); 
        preheatComplete = false;
      }
      break;
      
    case PREHEATING:
      if (preheatComplete) {
        currentState = READY;
        holdingStartTime = millis(); 
      }
      break;

    case READY:
      if (millis() - holdingStartTime > (settings.holdingTimeMinutes * 60000UL)) {
         currentState = IDLE;
      }
      break;
      
    case RUNNING:
      if (millis() - recipeStartTime >= (settings.recipeTimeMinutes * 60000UL)) {
        currentState = ALARM_COMPLETION;
        alarmStartTime = millis();
      }
      break;
        
    case ALARM_COMPLETION:
      if (millis() - alarmStartTime >= 30000UL) { 
         currentState = IDLE;
      }
      break;
  }
}

void updateRelayLogic() {
  updatePidInputs();
  updatePidSetpoints();
  computePids();
  applyHeaterLogic();
  applyValveAndAuxLogic();
  applySafetyOverrides(); 
  applyRelayStates(); 
}
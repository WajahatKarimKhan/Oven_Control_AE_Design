/*
  logger.cpp - Handles SD Card Logging
*/
#include "logger.h"
#include "config.h"  
#include <SPI.h>
#include <SD.h>

// Definitions
const char* LOG_FILENAME = "ovenlog.csv";

// --- EXTERNAL VARIABLES ---
// Access the global 'settings' object defined in config.h
extern PersistentSettings settings; 

// Access the global 'currentTemps' array (Float, as per config.h)
extern float currentTemps[3];

// Access the global 'relayStates'
extern RelayStates relayStates;

// Access the global 'currentState'
extern OvenState currentState;

extern double pidOutputRod1;
extern double pidOutputRod2;
extern double pidOutputSteam;

void initializeLogger() {
  Serial.print("Initializing SD Card on CS Pin ");
  Serial.print(SD_CS_PIN);
  Serial.println("...");

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card Initialization FAILED!");
    return;
  }
  Serial.println("SD Card Initialized.");

  // Check if file exists. If NO, create it and write the NEW header.
  if (!SD.exists(LOG_FILENAME)) {
    File logFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (logFile) {
      // Header with 9 PID columns (3 per rod)
      logFile.println("date,time,state,set_rod1,set_rod2,set_rod3,live_rod1,live_rod2,live_rod3,Kp_rod1,Ki_rod1,Kd_rod1,pid_rod1,pid_rod2,pid_rod3,Kp_rod2,Ki_rod2,Kd_rod2,Kp_rod3,Ki_rod3,Kd_rod3,rel_rod1,rel_rod2,rel_rod3,rel_valve");
      logFile.close();
      Serial.println("Created new log file with updated headers.");
    }
  }
}

void logSystemData() {
  File logFile = SD.open(LOG_FILENAME, FILE_WRITE);
  
  if (logFile) {
    DateTime now = rtc.now();
    char buf[20];

    // 1. Date (YYYY-MM-DD)
    sprintf(buf, "%04d-%02d-%02d", now.year(), now.month(), now.day());
    logFile.print(buf);
    logFile.print(",");

    // 2. Time (HH:MM:SS)
    sprintf(buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    logFile.print(buf);
    logFile.print(",");

    // 3. State
    String stateStr = "IDLE";
    if (currentState == PREHEATING) stateStr = "PREHEAT";
    else if (currentState == READY) stateStr = "READY";
    else if (currentState == RUNNING) stateStr = "RUNNING";
    else if (currentState == ALARM_COMPLETION) stateStr = "DONE";
    else if (currentState == AWAITING_SCHEDULE) stateStr = "SCHED";
    logFile.print(stateStr);
    logFile.print(",");


    // --- SET TEMPERATURES ---
    logFile.print(settings.thresholds.rod1);
    logFile.print(",");
    logFile.print(settings.thresholds.rod2);
    logFile.print(",");
    logFile.print(settings.thresholds.rodSteam);
    logFile.print(",");

    // --- LIVE TEMPERATURES ---
    logFile.print(currentTemps[0]); // Rod 1
    logFile.print(",");
    logFile.print(currentTemps[2]); // Rod 2
    logFile.print(",");
    logFile.print(currentTemps[1]); // Rod 3 (Steam)
    logFile.print(",");

    // --- PID PARAMETERS (ROD 1) ---
    // Uses constants from config.h instead of QuickPID methods
    logFile.print(ROD1_DEFAULT_KP); logFile.print(",");
    logFile.print(ROD1_DEFAULT_KI); logFile.print(",");
    logFile.print(ROD1_DEFAULT_KD); logFile.print(",");
    logFile.print(pidOutputRod1); logFile.print(",");

    // --- PID PARAMETERS (ROD 2) ---
    logFile.print(ROD2_DEFAULT_KP); logFile.print(",");
    logFile.print(ROD2_DEFAULT_KI); logFile.print(",");
    logFile.print(ROD2_DEFAULT_KD); logFile.print(",");
    logFile.print(pidOutputRod2); logFile.print(",");

    // --- PID PARAMETERS (ROD 3 / STEAM) ---
    logFile.print(STEAM_DEFAULT_KP); logFile.print(",");
    logFile.print(STEAM_DEFAULT_KI); logFile.print(",");
    logFile.print(STEAM_DEFAULT_KD); logFile.print(",");
    logFile.print(pidOutputSteam); logFile.print(",");

    // --- RELAY STATES ---
    logFile.print(relayStates.rod1 ? "1" : "0");
    logFile.print(",");
    logFile.print(relayStates.rod2 ? "1" : "0");
    logFile.print(",");
    logFile.print(relayStates.rodSteam ? "1" : "0");
    logFile.print(",");
    
    // Valve (Last item, NO comma)
    logFile.print(relayStates.valve ? "1" : "0");

    logFile.println(); // End Line
    logFile.close();
  } else {
    Serial.println("Error opening log file for writing.");
  }
}
/*
  logger.cpp - Handles SD Card Logging
*/
#include "logger.h"
#include "config.h"
#include <SPI.h>
#include <SD.h>

extern unsigned long windowStartTimeRod1;
extern unsigned long windowStartTimeRod2;
extern unsigned long windowStartTimeSteam;
extern unsigned long decisional_state_time;

extern unsigned long log_Paramters[RODCOUT][LogVariabe];
extern unsigned long Log_funcTime[funtionQTY];

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
      logFile.println("date,time,state,set_rod1,set_rod2,set_rod3,live_rod1,live_rod2,live_rod3,Kp_rod1,Ki_rod1,Kd_rod1,pid_rod1,Kp_rod2,Ki_rod2,Kd_rod2,pid_rod2,Kp_rod3,Ki_rod3,Kd_rod3,pid_rod3,rel_rod1,rel_rod2,rel_rod3,rel_valve,now,wst_rod_1,wst_rod_2,wst_steam,now - wst_rod_1,now - wst_rod_2,now - wst_steam,x_rod1,x_rod2,x_steam");
      logFile.close();
      Serial.println("Created new log file with updated headers.");
    }
  }
}

void logSystemData() {
  unsigned long temp = millis();
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
    logFile.print(currentTemps[0]);  // Rod 1
    logFile.print(",");
    logFile.print(currentTemps[2]);  // Rod 2
    logFile.print(",");
    logFile.print(currentTemps[1]);  // Rod 3 (Steam)
    logFile.print(",");

    // --- PID PARAMETERS (ROD 1) ---
    // Uses constants from config.h instead of QuickPID methods
    logFile.print(settings.rod1Pid.kp);
    logFile.print(",");
    logFile.print(settings.rod1Pid.ki);
    logFile.print(",");
    logFile.print(settings.rod1Pid.kd);
    logFile.print(",");
    logFile.print(pidOutputRod1);
    logFile.print(",");

    // --- PID PARAMETERS (ROD 2) ---
    logFile.print(settings.rod2Pid.kp);
    logFile.print(",");
    logFile.print(settings.rod2Pid.ki);
    logFile.print(",");
    logFile.print(settings.rod2Pid.kd);
    logFile.print(",");
    logFile.print(pidOutputRod2);
    logFile.print(",");

    // --- PID PARAMETERS (ROD 3 / STEAM) ---
    logFile.print(settings.rodSteamPid.kp);
    logFile.print(",");
    logFile.print(settings.rodSteamPid.ki);
    logFile.print(",");
    logFile.print(settings.rodSteamPid.kd);
    logFile.print(",");
    logFile.print(pidOutputSteam);
    logFile.print(",");

    // --- RELAY STATES ---
    logFile.print(relayStates.rod1 ? "1" : "0");
    logFile.print(",");
    logFile.print(relayStates.rod2 ? "1" : "0");
    logFile.print(",");
    logFile.print(relayStates.rodSteam ? "1" : "0");
    logFile.print(",");

    // Valve (Last item, NO comma)
    logFile.print(relayStates.valve ? "1" : "0");
    logFile.print(",");
    
    //NOW
    logFile.print(decisional_state_time);
    logFile.print(",");

    //window start times
    logFile.print(windowStartTimeRod1);
    logFile.print(",");
    logFile.print(windowStartTimeRod2);
    logFile.print(",");
    logFile.print(windowStartTimeSteam);
    logFile.print(",");
    //window start times
    logFile.print(log_Paramters[1][2]);
    logFile.print(",");
    logFile.print(log_Paramters[0][2]);
    logFile.print(",");
    logFile.print(log_Paramters[2][2]);
    logFile.print(",");
    //state log of tpc function
    logFile.print(log_Paramters[1][1]);
    logFile.print(",");
    logFile.print(log_Paramters[0][1]);
    logFile.print(",");
    logFile.print(log_Paramters[2][1]);
    logFile.print(",");

    logFile.print(log_Paramters[1][0]);
    logFile.print(",");
    logFile.print(log_Paramters[0][0]);
    logFile.print(",");
    logFile.print(log_Paramters[2][0]);
    logFile.print(",");
    
    logFile.print(log_Paramters[1][3]);
    logFile.print(",");
    logFile.print(log_Paramters[0][3]);
    logFile.print(",");
    logFile.print(log_Paramters[2][3]);
    logFile.print(",");
    
    logFile.print(log_Paramters[1][4] );
    logFile.print(",");
    logFile.print(log_Paramters[0][4] );
    logFile.print(",");
    logFile.print(log_Paramters[2][4] );
    logFile.print(",");
    
    //set tuning
    logFile.print(log_Paramters[1][5] );
    logFile.print(",");
    logFile.print(log_Paramters[0][5] );
    logFile.print(",");
    logFile.print(log_Paramters[2][5] );
    logFile.print(",");
     
    for (int i = 0; i < funtionQTY; i++) {
      logFile.print(Log_funcTime[i]);
      logFile.print(",");
    } 
    logFile.println();  // End Line
    logFile.close();
  } else {
    Serial.println("Error opening log file for writing.");
  }
  Log_funcTime[6] = millis() - temp;
}

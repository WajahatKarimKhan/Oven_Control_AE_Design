#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <RTClib.h>
#include <max6675.h>
#include <SPI.h>
#include <DueFlashStorage.h>
#include "io_map.h"  
#include "pid_lib.h" 


const int CTRL_OFFSET = 10;
// PID Configuration
const int PID_COMPUTE_FREQ = 100; // Compute every 100ms
const int PID_WINDOW_SIZE = 6000; // Time Proportional Control Window

// --- SAFETY ---
const float STEAM_SAFETY_THRESHOLD = 160.0;

// --- INDIVIDUAL PID DEFAULTS ---

// ROD 1: Upper (Convection)
const double ROD1_DEFAULT_KP = 250.0;   
const double ROD1_DEFAULT_KI = 0.0;   
const double ROD1_DEFAULT_KD = 0.0;    

// ROD 2: Lower (Induction)
const double ROD2_DEFAULT_KP = 450.0;   
const double ROD2_DEFAULT_KI = 0.0;    
const double ROD2_DEFAULT_KD = 0.0;    

// ROD STEAM: Steam Generator
const double STEAM_DEFAULT_KP = 450.0; 
const double STEAM_DEFAULT_KI = 0.0;  
const double STEAM_DEFAULT_KD = 0.0;   

// =================================================================
// GLOBAL STRUCTS & ENUMS
// =================================================================

struct PidParams {
  double kp;
  double ki;
  double kd;
};

struct Thresholds {
  int rod1     = 0;
  int rod2     = 0;
  int rodSteam = 0;
  int fan      = 0;
  int siren    = 0;
};

struct PersistentSettings {
  Thresholds thresholds;
  int _legacyPreheatTemp; 
  int recipeTimeMinutes;
  uint32_t scheduledUnixTime; 
  int holdingTimeMinutes; 
  
  // Individual PID configurations
  PidParams rod1Pid;
  PidParams rod2Pid;
  PidParams rodSteamPid;
};

struct RelayStates {
  bool rod1       = false;
  bool rod2       = false;
  bool rodSteam   = false;
  bool valve      = false;
  bool light      = false;
  bool alarm      = false;
  bool fan        = false; 
};

enum OvenState {
  IDLE,
  PREHEATING,        
  READY,             
  RUNNING,           
  AWAITING_SCHEDULE,
  ALARM_COMPLETION 
};

// =================================================================
// EXTERN DECLARATIONS
// =================================================================

extern MAX6675 tempSensorRod1;
extern MAX6675 tempSensorRodSteam;
extern MAX6675 tempSensorRod2;
extern RTC_DS3231 rtc;
extern DueFlashStorage dueFlashStorage;
extern QuickPID pidRod1;
extern QuickPID pidRod2;
extern QuickPID pidSteam;

extern Stream* activePort;
extern unsigned long lastStatusUpdateTime;
extern const long statusUpdateInterval;

extern PersistentSettings settings;
extern RelayStates relayStates;
extern float currentTemps[3]; 
extern bool manualControlActive;
extern bool manualAlarmActive; 
extern bool manualValveOverride; 
extern unsigned long steamValveOpenTime;

extern double pidSetpointRod1, pidInputRod1, pidOutputRod1;
extern double pidSetpointRod2, pidInputRod2, pidOutputRod2;
extern double pidSetpointSteam, pidInputSteam, pidOutputSteam;

extern OvenState currentState;
extern unsigned long recipeStartTime;
extern unsigned long preheatStartTime; 
extern bool preheatComplete;
extern unsigned long holdingStartTime; 
extern unsigned long alarmStartTime;

#endif // CONFIG_H
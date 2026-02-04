#ifndef CONFIG_H
#define CONFIG_H

// =================================================================
// LIBRARIES
// =================================================================
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <RTClib.h>
#include <max6675.h>
#include <SPI.h>
#include <DueFlashStorage.h>

// =================================================================
// PIN DEFINITIONS
// =================================================================
// --- Thermocouple (MAX6675) Chip Select (CS) Pins ---
const int TEMP_CS_PIN_ROD1      = 11;
const int TEMP_CS_PIN_ROD_STEAM = 13;
const int TEMP_CS_PIN_ROD2      = 12;
// --- Thermocouple (MAX6675) SHARED Pins ---
const int TEMP_SCLK_PIN         = 8; 
const int TEMP_MISO_PIN         = 9;
// --- 6-Channel Relay Card Pins (A0-A5) ---
const int RELAY_PIN_ROD1         = A0; 
const int RELAY_PIN_ROD2         = A1; 
const int RELAY_PIN_STEAM_HEATER = A2; 
const int RELAY_PIN_VALVE        = A3; 
const int RELAY_PIN_ALARM        = A4; 
const int RELAY_PIN_LIGHT        = A5; 
// --- RS485 Communication Pin ---
const int RS485_DE_RE_PIN = A8;
// --- Relay Configuration ---
const int RELAY_ON  = LOW;
const int RELAY_OFF = HIGH; 

// --- New App Steam Logic Variables ---
extern bool appSteamActive;           // Flag to track if app triggered steam
extern unsigned long appSteamStartTime; // Timestamp for the 20s timer
// =================================================================
// GLOBAL STRUCTS & ENUMS
// =================================================================

// --- Persistent Settings Struct ---
struct Thresholds {
  int rod1     = 230;
  int rod2     = 210;
  int rodSteam = 210;
  int fan      = 180;
  int siren    = 250;
};

struct PersistentSettings {
  Thresholds thresholds;
  int preheatTemperature;
  int recipeTimeMinutes;
  uint32_t scheduledUnixTime; // 0 = no schedule
};

// --- State Tracking Struct ---
struct RelayStates {
  bool rod1       = false;
  bool rod2       = false;
  bool rodSteam   = false;
  bool valve      = false;
  bool light      = false;
  bool alarm      = false;
};

// --- State Machine Enum ---
enum OvenState {
  IDLE,
  PREHEATING,
  RUNNING,
  AWAITING_SCHEDULE,
  ALARM_COMPLETION // NEW: For 30-second end siren
};

// =================================================================
// EXTERN DECLARATIONS (Global variables defined in drivers.cpp)
// =================================================================

// --- Component Objects ---
extern MAX6675 tempSensorRod1;
extern MAX6675 tempSensorRodSteam;
extern MAX6675 tempSensorRod2;
extern RTC_DS3231 rtc;
extern DueFlashStorage dueFlashStorage;

// --- Communication ---
extern Stream* activePort;
extern unsigned long lastStatusUpdateTime;
extern const long statusUpdateInterval;

// --- Settings & State ---
extern PersistentSettings settings;
extern RelayStates relayStates;
extern float currentTemps[3]; 
extern bool manualControlActive;
extern bool manualAlarmActive; // Note: No longer used, but kept to avoid errors

// --- State Machine ---
extern OvenState currentState;
extern unsigned long recipeStartTime;
extern unsigned long preheatStartTime; 
extern bool preheatComplete;

// --- New Logic State Variables ---
extern bool steamValveOpened;     // Tracks one-shot steam
extern unsigned long steamValveOpenTime; // Timer for 10s steam
extern unsigned long alarmStartTime;     // Timer for 30s siren

#endif // CONFIG_H
#include "drivers.h"

// =================================================================
// GLOBAL VARIABLE DEFINITIONS (matches externs in config.h)
// =================================================================

// --- Component Objects ---
MAX6675 tempSensorRod1(TEMP_SCLK_PIN, TEMP_CS_PIN_ROD1, TEMP_MISO_PIN);
MAX6675 tempSensorRodSteam(TEMP_SCLK_PIN, TEMP_CS_PIN_ROD_STEAM, TEMP_MISO_PIN);
MAX6675 tempSensorRod2(TEMP_SCLK_PIN, TEMP_CS_PIN_ROD2, TEMP_MISO_PIN);
RTC_DS3231 rtc;
DueFlashStorage dueFlashStorage;

// --- Communication ---
Stream* activePort = &SerialUSB; // Default to USB
unsigned long lastStatusUpdateTime = 0;
const long statusUpdateInterval = 3000; // 3 seconds

// --- Settings & State ---
PersistentSettings settings;
RelayStates relayStates;
float currentTemps[3] = {0.0, 0.0, 0.0}; 
bool manualControlActive = false;
bool manualAlarmActive = false;

// --- State Machine ---
OvenState currentState = IDLE;
unsigned long recipeStartTime = 0;
unsigned long preheatStartTime = 0; 
bool preheatComplete = false;

// --- New Logic State Variables ---
bool steamValveOpened = false;
unsigned long steamValveOpenTime = 0;
unsigned long alarmStartTime = 0;

// ADD THESE LINES:
bool appSteamActive = false;
unsigned long appSteamStartTime = 0;
// =================================================================
// DRIVER-SPECIFIC FUNCTIONS (Flash, RTC)
// =================================================================

void loadSettings() {
  Serial.println("Loading settings from Flash...");
  byte* buffer = dueFlashStorage.readAddress(0);
  memcpy(&settings, buffer, sizeof(settings));

  if (settings.preheatTemperature == 0xFFFFFFFF || settings.preheatTemperature < 0) { 
    Serial.println("No valid settings found, loading defaults.");
    settings.thresholds.rod1 = 230;
    settings.thresholds.rod2 = 210;
    settings.thresholds.rodSteam = 210;
    settings.thresholds.fan = 180;
    settings.thresholds.siren = 250;
    settings.preheatTemperature = 180;
    settings.recipeTimeMinutes = 30;
    settings.scheduledUnixTime = 0;
    saveSettings();
  } else {
    Serial.println("Settings loaded successfully.");
  }
}

void saveSettings() {
  Serial.println("Saving settings to Flash...");
  dueFlashStorage.write(0, (byte*)&settings, sizeof(settings));
  Serial.println("Settings saved.");
}

void setHardcodedTime() {
  Serial.println("RTC lost power, setting time to hardcoded value!");
  // ... (Full function copied from original) ...
  // rtc.adjust(DateTime(2026, 1, 11, 13, 54, 0)); // <-- EXAMPLE: 1:52 PM GMT
  Serial.println("RTC time has been set.");

  // Get GMT time from RTC
  DateTime nowGMT = rtc.now();
  // Create a 5-hour offset for PKT
  TimeSpan offset = TimeSpan(0, 5, 0, 0); 
  // Apply offset for local time
  DateTime nowPKT = nowGMT + offset;

  Serial.print("Current RTC Time (PKT): "); // Changed label
  Serial.print(nowPKT.year()); Serial.print('/');
  Serial.print(nowPKT.month()); Serial.print('/');
  Serial.print(nowPKT.day()); Serial.print(' ');
  Serial.print(nowPKT.hour()); Serial.print(':');
  Serial.print(nowPKT.minute()); Serial.print(':');
  Serial.println(nowPKT.second());
}
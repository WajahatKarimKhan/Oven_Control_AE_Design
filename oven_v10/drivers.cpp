#include "drivers.h"

// =================================================================
// GLOBAL VARIABLE DEFINITIONS
// =================================================================

MAX6675 tempSensorRod1(TEMP_SCLK_PIN, TEMP_CS_PIN_ROD1, TEMP_MISO_PIN);
MAX6675 tempSensorRodSteam(TEMP_SCLK_PIN, TEMP_CS_PIN_ROD_STEAM, TEMP_MISO_PIN);
MAX6675 tempSensorRod2(TEMP_SCLK_PIN, TEMP_CS_PIN_ROD2, TEMP_MISO_PIN);
RTC_DS3231 rtc;
DueFlashStorage dueFlashStorage;

Stream* activePort = &SerialUSB; 
unsigned long lastStatusUpdateTime = 0;
const long statusUpdateInterval = 3000; 

PersistentSettings settings;
RelayStates relayStates;
float currentTemps[3] = {0.0, 0.0, 0.0}; 
bool manualControlActive = false;
bool manualAlarmActive = false;

bool manualValveOverride = false;

double pidSetpointRod1, pidInputRod1, pidOutputRod1;
double pidSetpointRod2, pidInputRod2, pidOutputRod2;
double pidSetpointSteam, pidInputSteam, pidOutputSteam;

QuickPID pidRod1(&pidInputRod1, &pidOutputRod1, &pidSetpointRod1, 0, 0, 0, QuickPID::DIRECT);
QuickPID pidRod2(&pidInputRod2, &pidOutputRod2, &pidSetpointRod2, 0, 0, 0, QuickPID::DIRECT);
QuickPID pidSteam(&pidInputSteam, &pidOutputSteam, &pidSetpointSteam, 0, 0, 0, QuickPID::DIRECT);

OvenState currentState = IDLE;
unsigned long recipeStartTime = 0;
unsigned long preheatStartTime = 0; 
bool preheatComplete = false;

bool steamValveOpened = false;
unsigned long steamValveOpenTime = 0;
unsigned long alarmStartTime = 0;
unsigned long holdingStartTime = 0;

// =================================================================
// FUNCTIONS
// =================================================================

void loadSettings() {
  Serial.println("Loading settings from Flash...");
  byte* b = dueFlashStorage.readAddress(0);
  PersistentSettings savedSettings; 
  memcpy(&savedSettings, b, sizeof(PersistentSettings));

  // Validation Check
  if (savedSettings.holdingTimeMinutes < 0 || savedSettings.holdingTimeMinutes > 180) { 
    Serial.println("No valid settings found, loading DEFAULTS.");
    
    settings.thresholds.rod1 = 0;
    settings.thresholds.rod2 = 0;
    settings.thresholds.rodSteam = 0;
    settings.thresholds.fan = 0;
    settings.thresholds.siren = 0;
    
    settings.recipeTimeMinutes = 0;
    settings.holdingTimeMinutes = 30; 
    settings.scheduledUnixTime = 0;

    // --- APPLY INDIVIDUAL PID DEFAULTS ---
    settings.rod1Pid = {ROD1_DEFAULT_KP, ROD1_DEFAULT_KI, ROD1_DEFAULT_KD};
    settings.rod2Pid = {ROD2_DEFAULT_KP, ROD2_DEFAULT_KI, ROD2_DEFAULT_KD};
    settings.rodSteamPid = {STEAM_DEFAULT_KP, STEAM_DEFAULT_KI, STEAM_DEFAULT_KD};

    saveSettings();
  } else {
    Serial.println("Settings loaded successfully.");
    settings = savedSettings;
  }
}

void saveSettings() {
  Serial.println("Saving settings to Flash...");
  dueFlashStorage.write(0, (byte*)&settings, sizeof(settings));
  Serial.println("Settings saved.");
}

void setHardcodedTime() {
    if (rtc.lostPower()) {
        Serial.println("RTC lost power, setting default time...");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
}
#ifndef IO_MAP_H
#define IO_MAP_H

// =================================================================
// PIN DEFINITIONS
// =================================================================

// --- Thermocouple (MAX6675) Chip Select (CS) Pins ---
const int TEMP_CS_PIN_ROD1      = 11;
const int TEMP_CS_PIN_ROD_STEAM = 13;
const int TEMP_CS_PIN_ROD2      = 12; 

// Define CS Pin for SD Card
const int SD_CS_PIN = 10;

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

// =================================================================
// RELAY LOGIC (Normally Open - NO)
// =================================================================
// ACTIVE LOW Logic for standard Relay Modules
// LOW  = Energize (Switch Closed -> ON)
// HIGH = De-energize (Switch Open -> OFF)

const int RELAY_ON  = LOW;  
const int RELAY_OFF = HIGH; 

#endif // IO_MAP_H
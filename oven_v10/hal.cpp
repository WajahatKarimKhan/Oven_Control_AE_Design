#include "hal.h"

#define RELAY_SWITCHING_ROD_1 1500
#define RELAY_SWITCHING_ROD_2 1500
#define RELAY_SWITCHING_STEAM 1500

void initializePins() {
  Serial.println("Initializing pins...");
  pinMode(RELAY_PIN_ROD1,         OUTPUT); digitalWrite(RELAY_PIN_ROD1,         RELAY_OFF);
  pinMode(RELAY_PIN_ROD2,         OUTPUT); digitalWrite(RELAY_PIN_ROD2,         RELAY_OFF);
  pinMode(RELAY_PIN_STEAM_HEATER, OUTPUT); digitalWrite(RELAY_PIN_STEAM_HEATER, RELAY_OFF);
  pinMode(RELAY_PIN_VALVE,        OUTPUT); digitalWrite(RELAY_PIN_VALVE,        RELAY_OFF);
  pinMode(RELAY_PIN_ALARM,        OUTPUT); digitalWrite(RELAY_PIN_ALARM,        RELAY_OFF);
  pinMode(RELAY_PIN_LIGHT,        OUTPUT); digitalWrite(RELAY_PIN_LIGHT,        RELAY_OFF);

  // RS485 pin
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW); 
}

void initializeSensors() {
  Serial.println("Initializing sensors...");
  Wire.begin(); 
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC! Check wiring.");
  } else {
    Serial.println("RTC found.");
  }
  delay(500); 
  Serial.println("MAX6675 sensors ready.");
}

void readTemperatureSensors() {
  currentTemps[0] = tempSensorRod1.readCelsius();
  currentTemps[1] = tempSensorRodSteam.readCelsius();
  currentTemps[2] = tempSensorRod2.readCelsius();
}

void applyRelayStates() {
  digitalWrite(RELAY_PIN_ROD1,         relayStates.rod1     ? RELAY_ON : RELAY_OFF);
  //add delay
  delay(RELAY_SWITCHING_ROD_1);
  digitalWrite(RELAY_PIN_ROD2,         relayStates.rod2     ? RELAY_ON : RELAY_OFF);
  //add delay
  delay(RELAY_SWITCHING_ROD_2);
  digitalWrite(RELAY_PIN_STEAM_HEATER, relayStates.rodSteam ? RELAY_ON : RELAY_OFF);
  //add delay
  delay(RELAY_SWITCHING_STEAM);
  digitalWrite(RELAY_PIN_VALVE,        relayStates.valve    ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY_PIN_ALARM,        relayStates.alarm    ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY_PIN_LIGHT,        relayStates.light    ? RELAY_ON : RELAY_OFF);
}

void toggleRelay(int pin, bool &stateVariable) {
  stateVariable = !stateVariable;
  digitalWrite(pin, stateVariable ? RELAY_ON : RELAY_OFF);
}
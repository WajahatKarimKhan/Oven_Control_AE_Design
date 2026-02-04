#include "app.h"
#include "hal.h"      
#include "drivers.h"  

const long GMT_OFFSET_SEC = 18000; 

void initializeCommunication() {
  Serial.begin(9600);
  Serial.println("Arduino Due Oven Controller V6.4 (Individual PID) Initializing...");

  SerialUSB.begin(9600);
  Serial1.begin(9600); // RS485 port
}

bool isStatusUpdateDue() {
  if (millis() - lastStatusUpdateTime >= statusUpdateInterval) {
    lastStatusUpdateTime = millis();
    return true;
  }
  return false;
}

void handleIncomingCommands() {
  if (SerialUSB.available() > 0) processIncomingStream(SerialUSB);
  if (Serial1.available() > 0) processIncomingStream(Serial1);
}

void processIncomingStream(Stream &port) {
  String incomingString = port.readStringUntil('\n');
  if (incomingString.length() == 0) return;

  Serial.print("Rcvd<- "); Serial.println(incomingString); 

  StaticJsonDocument<1024> doc; 
  DeserializationError error = deserializeJson(doc, incomingString);

  if (error) {
    sendErrorToPort(port, "Invalid JSON");
    return;
  }

  const char* command = doc["cmd"];

  if (strcmp(command, "SET_THRESHOLDS") == 0) {
    settings.thresholds.rod1     = doc["rod1"];
    settings.thresholds.rod2     = doc["rod2"];
    settings.thresholds.rodSteam = doc["steam"];
    settings.recipeTimeMinutes   = doc["time"];
    
    if (doc.containsKey("holding")) settings.holdingTimeMinutes = doc["holding"];
    else settings.holdingTimeMinutes = 30;

    if (doc.containsKey("schedule")) {
      unsigned long schedTime = doc["schedule"];
      if (schedTime > 0) {
        settings.scheduledUnixTime = schedTime;
        currentState = AWAITING_SCHEDULE; 
        Serial.print("Schedule set for: "); Serial.println(schedTime);
      } else {
        settings.scheduledUnixTime = 0;
        if (currentState == AWAITING_SCHEDULE) currentState = IDLE;
      }
    } else {
      if (currentState == AWAITING_SCHEDULE) {
          settings.scheduledUnixTime = 0;
          currentState = IDLE;
      }
    }

    saveSettings();
    sendToPort(port, "{\"status\":\"ok\", \"msg\":\"Settings Saved\"}");
  }

  // --- NEW: SET INDIVIDUAL PID COMMAND ---
  else if (strcmp(command, "SET_PID") == 0) {
    const char* target = doc["target"]; // "rod1", "rod2", "steam"
    double kp = doc["kp"];
    double ki = doc["ki"];
    double kd = doc["kd"];
    
    PidParams params = {kp, ki, kd};
    bool updated = false;

    if (strcmp(target, "rod1") == 0) {
        settings.rod1Pid = params;
        pidRod1.SetTunings(kp, ki, kd);
        updated = true;
    } else if (strcmp(target, "rod2") == 0) {
        settings.rod2Pid = params;
        pidRod2.SetTunings(kp, ki, kd);
        updated = true;
    } else if (strcmp(target, "steam") == 0) {
        settings.rodSteamPid = params;
        pidSteam.SetTunings(kp, ki, kd);
        updated = true;
    }
    
    if (updated) {
      saveSettings();
      sendToPort(port, "{\"status\":\"ok\", \"msg\":\"PID Tuned\"}");
    } else {
      sendErrorToPort(port, "Invalid Target (rod1/rod2/steam)");
    }
  }

  else if (strcmp(command, "SET_TIME") == 0) {
    if (doc.containsKey("timestamp")) {
      unsigned long ts = doc["timestamp"];
      rtc.adjust(DateTime(ts + GMT_OFFSET_SEC)); 
      sendToPort(port, "{\"status\":\"ok\", \"msg\":\"Time Updated\"}");
    }
  }
  
  else if (strcmp(command, "START_PREHEAT") == 0) {
    currentState = PREHEATING;
    settings.scheduledUnixTime = 0; 
    preheatStartTime = millis(); 
    preheatComplete = false;
    saveSettings(); 
    sendToPort(port, "{\"status\":\"ok\", \"msg\":\"Preheating Started\"}");
  }

  else if (strcmp(command, "RUN_RECIPE") == 0) {
    currentState = RUNNING;
    recipeStartTime = millis();
    sendToPort(port, "{\"status\":\"ok\", \"msg\":\"Recipe Running\"}");
  }

  else if (strcmp(command, "STOP") == 0) {
    currentState = IDLE;
    settings.scheduledUnixTime = 0;
    manualControlActive = false;
    manualValveOverride = false;
    saveSettings();
    sendToPort(port, "{\"status\":\"ok\", \"msg\":\"Stopped\"}");
  }

  else if (strcmp(command, "TOGGLE_VALVE") == 0) {
    bool state = doc["state"];
    if (state) {
      if (currentTemps[1] > STEAM_SAFETY_THRESHOLD) { 
        manualValveOverride = true;
        steamValveOpenTime = millis();
        sendToggleConfirmation(port, "valve", true);
      } else {
        sendErrorToPort(port, "Safety Error: Steam Rod too cold");
      }
    } else {
      manualValveOverride = false;
      sendToggleConfirmation(port, "valve", false);
    }
  }

  else if (strcmp(command, "TOGGLE_LIGHT") == 0) {
    relayStates.light = doc["state"];
    applyRelayStates();
    sendToggleConfirmation(port, "light", relayStates.light);
  }
}

void sendStatusUpdate() {
  StaticJsonDocument<512> doc;
  
  String stateStr = "IDLE";
  if (currentState == PREHEATING) stateStr = "PREHEATING";
  else if (currentState == READY) stateStr = "READY";
  else if (currentState == RUNNING) stateStr = "RUNNING";
  else if (currentState == ALARM_COMPLETION) stateStr = "DONE";
  else if (currentState == AWAITING_SCHEDULE) stateStr = "SCHEDULED";

  doc["state"] = stateStr;
  
  JsonArray tempArray = doc.createNestedArray("temps");
  tempArray.add(currentTemps[0]);
  tempArray.add(currentTemps[1]);
  tempArray.add(currentTemps[2]);

  long remainingSeconds = 0;
  if (currentState == RUNNING) {
    long elapsed = (millis() - recipeStartTime) / 1000;
    remainingSeconds = (settings.recipeTimeMinutes * 60) - elapsed;
  } else if (currentState == READY) {
     long elapsed = (millis() - holdingStartTime) / 1000;
     remainingSeconds = (settings.holdingTimeMinutes * 60) - elapsed;
  } else if (currentState == AWAITING_SCHEDULE) {
     remainingSeconds = settings.scheduledUnixTime - rtc.now().unixtime();
  }
  
  doc["timer"] = remainingSeconds > 0 ? remainingSeconds : 0;
  
  JsonObject relays = doc.createNestedObject("relays");
  relays["rod1"] = relayStates.rod1;
  relays["rod2"] = relayStates.rod2;
  relays["rodSteam"] = relayStates.rodSteam;
  relays["valve"] = manualValveOverride;
  relays["light"] = relayStates.light;
  relays["alarm"] = relayStates.alarm;
  
  doc["valve"] = manualValveOverride ? "ON" : "OFF"; 
  
  DateTime now = rtc.now();
  doc["time"] = now.timestamp(DateTime::TIMESTAMP_FULL);

  String output;
  serializeJson(doc, output);
  
  if (SerialUSB) SerialUSB.println(output);
  if (Serial1) {
    digitalWrite(RS485_DE_RE_PIN, HIGH);
    Serial1.println(output);
    Serial1.flush();
    digitalWrite(RS485_DE_RE_PIN, LOW);
  }
}

void sendToPort(Stream &port, const String& message) {
  if (&port == &Serial1) { 
    digitalWrite(RS485_DE_RE_PIN, HIGH);
    port.println(message);
    port.flush(); 
    digitalWrite(RS485_DE_RE_PIN, LOW);
  } else {
    port.println(message);
  }
}

void sendToggleConfirmation(Stream &port, const char* relayName, bool newState) {
  StaticJsonDocument<128> doc;
  doc["status"] = "success";
  doc["relay"] = relayName;
  doc["state"] = newState ? "ON" : "OFF";
  
  String output;
  serializeJson(doc, output);
  sendToPort(port, output);
}

void sendErrorToPort(Stream &port, const char* errorMessage) {
  StaticJsonDocument<128> doc;
  doc["status"] = "error";
  doc["msg"] = errorMessage;
  String output;
  serializeJson(doc, output);
  sendToPort(port, output);
}

void printDebugInfo() {
  Serial.println("---[ DEBUG (Every 3s) ]---");
  
  DateTime now = rtc.now();
  Serial.print("  RTC: ");
  Serial.print(now.hour()); Serial.print(':');
  Serial.println(now.minute());

  Serial.print("  State: "); 
  Serial.println(currentState);
  
  Serial.print("  T1: "); Serial.print(currentTemps[0]);
  Serial.print(" | Setpoint: "); Serial.println(pidSetpointRod1);
  
  Serial.println("---------------------------");
}
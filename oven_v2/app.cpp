#include "app.h"
#include "hal.h"       // Needs toggleRelay(), applyRelayStates()
#include "drivers.h"   // Needs saveSettings()

// Define Timezone Offset for Pakistan Standard Time (PKT)
// PKT is UTC + 5 hours. 
// 5 hours * 3600 seconds/hour = 18000 seconds.
const long GMT_OFFSET_SEC = 18000; 

void initializeCommunication() {
  Serial.begin(9600);
  Serial.println("Arduino Due Oven Controller V3.0 (Timer Logic) Initializing...");

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
  if (SerialUSB.available() > 0) {
    processIncomingStream(SerialUSB);
  }
  if (Serial1.available() > 0) {
    processIncomingStream(Serial1);
  }
}

void processIncomingStream(Stream &port) {
  String incomingString = port.readStringUntil('\n');
  if (incomingString.length() == 0) return;

  Serial.print("Rcvd<- "); Serial.println(incomingString); 

  // Increased buffer size to handle larger payloads if necessary
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, incomingString);
  if (error) {
    sendErrorToPort(port, "Malformed JSON received.");
    return;
  }
  activePort = &port; 

  const char* command = doc["command"];
  if (command == nullptr) {
    sendErrorToPort(port, "Missing 'command' field.");
    return;
  }

  // --- Command Handling ---
  
  if (strcmp(command, "ping") == 0) {
    sendToPort(port, "{\"status\":\"ready\", \"device\":\"BecoOvenControllerV3.0\"}");
  }
  
  // NEW: Set RTC Time
  else if (strcmp(command, "set_time") == 0) {
     if (doc.containsKey("timestamp")) {
       uint32_t ts = doc["timestamp"].as<uint32_t>();
       // Apply PKT Offset (UTC + 5h)
       rtc.adjust(DateTime(ts + GMT_OFFSET_SEC));
       sendToPort(port, "{\"status\":\"success\", \"message\":\"Time updated to PKT\"}");
       sendStatusUpdate(); // Send immediate update to reflect new time
     }
  }
  
  else if (strcmp(command, "set_recipe_settings") == 0) {
    Serial.println("Received 'set_recipe_settings' command.");
    if (doc.containsKey("rod1"))     settings.thresholds.rod1     = doc["rod1"].as<int>();
    if (doc.containsKey("rod2"))     settings.thresholds.rod2     = doc["rod2"].as<int>();
    if (doc.containsKey("rodSteam")) settings.thresholds.rodSteam = doc["rodSteam"].as<int>();
    if (doc.containsKey("fan"))      settings.thresholds.fan      = doc["fan"].as<int>();
    if (doc.containsKey("siren"))    settings.thresholds.siren    = doc["siren"].as<int>();
    if (doc.containsKey("recipeTime")) settings.recipeTimeMinutes   = doc["recipeTime"].as<int>();
    
    // Handle Schedule with Offset
    if (doc.containsKey("scheduleTime")) {
      uint32_t st = doc["scheduleTime"].as<uint32_t>();
      if (st > 0) {
        settings.scheduledUnixTime = st + GMT_OFFSET_SEC;
        currentState = AWAITING_SCHEDULE;
      } else {
        settings.scheduledUnixTime = 0;
      }
    }

    manualControlActive = false;
    manualAlarmActive = false;
    saveSettings(); 

    StaticJsonDocument<256> responseDoc;
    responseDoc["status"] = "success";
    responseDoc["command_received"] = "set_recipe_settings";
    JsonObject thresholdsObj = responseDoc.createNestedObject("thresholds");
    thresholdsObj["rod1"] = settings.thresholds.rod1;
    thresholdsObj["rod2"] = settings.thresholds.rod2;
    thresholdsObj["rodSteam"] = settings.thresholds.rodSteam;
    thresholdsObj["siren"] = settings.thresholds.siren;
    responseDoc["recipeTime"] = settings.recipeTimeMinutes;
    String output;
    serializeJson(responseDoc, output);
    sendToPort(port, output);
  }

  else if (strcmp(command, "set_preheat_settings") == 0) {
    Serial.println("Received 'set_preheat_settings' command.");
    if (doc.containsKey("preheatTemp")) settings.preheatTemperature = doc["preheatTemp"].as<int>();
    
    if (doc.containsKey("time")) {
       uint32_t st = doc["time"].as<uint32_t>();
       if (st > 0) {
         settings.scheduledUnixTime = st + GMT_OFFSET_SEC;
         currentState = AWAITING_SCHEDULE;
       } else {
         settings.scheduledUnixTime = 0;
       }
    }
    
    saveSettings(); 

    StaticJsonDocument<128> responseDoc;
    responseDoc["status"] = "success";
    responseDoc["command_received"] = "set_preheat_settings";
    responseDoc["preheatTemp"] = settings.preheatTemperature;
    responseDoc["scheduledTime"] = settings.scheduledUnixTime;
    String output;
    serializeJson(responseDoc, output);
    sendToPort(port, output);
  }
  
  else if (strcmp(command, "toggle_light") == 0) {
    toggleRelay(RELAY_PIN_LIGHT, relayStates.light);
    sendToggleConfirmation(port, "light", relayStates.light);
    sendStatusUpdate();
  }

  else if (strcmp(command, "toggle_valve") == 0) {
    if (currentTemps[1] >= settings.thresholds.rodSteam) {
      
      // NEW LOGIC: Trigger the 20s timer
      Serial.println("App Steam Triggered: Valve ON for 20 seconds.");
      appSteamActive = true;           // Set the flag
      appSteamStartTime = millis();    // Start the timer
      relayStates.valve = true;        // Turn valve ON immediately
      
      applyRelayStates();              // Apply to hardware instantly
      
      // Send confirmation
      StaticJsonDocument<128> responseDoc;
      responseDoc["status"] = "success";
      responseDoc["message"] = "Valve OPEN for 20s";
      responseDoc["relay"] = "valve";
      responseDoc["state"] = true;
      String output;
      serializeJson(responseDoc, output);
      sendToPort(port, output);
      
      sendStatusUpdate();
      
    } else {
      Serial.println("Toggle Valve DENIED: Steam rod temp too low.");
      StaticJsonDocument<128> errDoc;
      errDoc["status"] = "error";
      errDoc["message"] = "Valve blocked: Steam rod too cold";
      String output;
      serializeJson(errDoc, output);
      sendToPort(port, output);
    }
  }

  else if (strcmp(command, "start_recipe") == 0) {
    Serial.println("Received 'start_recipe' command.");
    Serial.println("Moving to RUNNING state immediately.");
    currentState = RUNNING; 
    recipeStartTime = millis(); 
    preheatComplete = false;
    preheatStartTime = 0; 
    manualControlActive = false;
    manualAlarmActive = false;
    
    steamValveOpened = false;
    steamValveOpenTime = 0;
    
    sendToPort(port, "{\"status\":\"success\", \"message\":\"Recipe running immediately.\"}");
    sendStatusUpdate();
  }

  else if (strcmp(command, "stop_recipe") == 0) {
    Serial.println("Received 'stop_recipe' command.");
    Serial.println("Moving to IDLE state. All heaters off.");
    currentState = IDLE;
    settings.scheduledUnixTime = 0; 
    saveSettings(); 
    manualControlActive = true; 
    relayStates.rod1 = false;
    relayStates.rod2 = false;
    relayStates.rodSteam = false;
    relayStates.valve = false;
    relayStates.light = false;
    relayStates.alarm = false; 
    applyRelayStates();
    manualControlActive = false;
    manualAlarmActive = false;
    sendToPort(port, "{\"status\":\"success\", \"message\":\"Recipe stopped, moving to idle.\"}");
    sendStatusUpdate();
  }

  else {
    sendErrorToPort(port, "Unknown command.");
  }
}

void sendToggleConfirmation(Stream &port, const char* relayName, bool newState) {
  StaticJsonDocument<128> responseDoc;
  responseDoc["status"] = "success";
  responseDoc["command_received"] = "toggle"; 
  responseDoc["relay"] = relayName;
  responseDoc["state"] = newState;
  String output;
  serializeJson(responseDoc, output);
  sendToPort(port, output);
}

void sendStatusUpdate() {
  StaticJsonDocument<512> doc;
  doc["type"] = "status_update";
  
  switch(currentState) {
    case IDLE: doc["state"] = "IDLE"; break;
    case PREHEATING: doc["state"] = "PREHEATING"; break;
    case RUNNING: doc["state"] = "RUNNING"; break;
    case AWAITING_SCHEDULE: doc["state"] = "AWAITING_SCHEDULE"; break;
    case ALARM_COMPLETION: doc["state"] = "ALARM_COMPLETION"; break; 
  }
  
  JsonArray temps = doc.createNestedArray("temps");
  temps.add(currentTemps[0]); 
  temps.add(currentTemps[1]); 
  temps.add(currentTemps[2]); 
  
  JsonObject relays = doc.createNestedObject("relays");
  relays["rod1"]     = relayStates.rod1;
  relays["rod2"]     = relayStates.rod2;
  relays["rodSteam"] = relayStates.rodSteam;
  relays["valve"]    = relayStates.valve;
  relays["light"]    = relayStates.light;
  relays["alarm"]    = relayStates.alarm; 
  
  JsonObject recipe = doc.createNestedObject("recipe");
  recipe["preheatSet"] = settings.preheatTemperature;
  recipe["recipeTimeSet"] = settings.recipeTimeMinutes;
  recipe["preheatComplete"] = preheatComplete;
  
  if (currentState == RUNNING) {
    unsigned long elapsed = (millis() - recipeStartTime) / 1000; 
    recipe["recipeTimeElapsed"] = elapsed;
  } 
  else if (currentState == ALARM_COMPLETION) {
    recipe["recipeTimeElapsed"] = settings.recipeTimeMinutes * 60; 
  }
  else {
    recipe["recipeTimeElapsed"] = 0;
  }
  
  recipe["scheduledTime"] = settings.scheduledUnixTime;

  DateTime now = rtc.now();
  char timestamp[20];
  sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02d", 
          now.year(), now.month(), now.day(), 
          now.hour(), now.minute(), now.second());
  doc["time"] = timestamp;

  String output;
  serializeJson(doc, output);
  
  if (activePort != nullptr) {
    sendToPort(*activePort, output);
  } else {
    sendToPort(SerialUSB, output);
  }
}

void printDebugInfo() {
  Serial.println("---[ DEBUG (Every 3s) ]---");
  
  DateTime now = rtc.now();
  Serial.print("  RTC Time: ");
  Serial.print(now.year()); Serial.print('/');
  Serial.print(now.month()); Serial.print('/');
  Serial.print(now.day()); Serial.print(' ');
  Serial.print(now.hour()); Serial.print(':');
  Serial.print(now.minute()); Serial.print(':');
  Serial.println(now.second());

  Serial.print("  Current State: ");
  switch(currentState) {
    case IDLE: Serial.println("IDLE"); break;
    case PREHEATING: Serial.println("PREHEATING"); break;
    case RUNNING: Serial.println("RUNNING"); break;
    case AWAITING_SCHEDULE: Serial.println("AWAITING_SCHEDULE"); break;
    case ALARM_COMPLETION: Serial.println("ALARM_COMPLETION"); break;
  }
  
  if (currentState == RUNNING) {
     Serial.print("  Timer: ");
     Serial.print((millis() - recipeStartTime)/1000);
     Serial.print("s / ");
     Serial.print(settings.recipeTimeMinutes * 60);
     Serial.println("s");
  }
  
  Serial.println("---------------------------");
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

void sendErrorToPort(Stream &port, const char* errorMessage) {
  StaticJsonDocument<128> doc;
  doc["status"] = "error";
  doc["message"] = errorMessage;
  String output;
  serializeJson(doc, output);
  sendToPort(port, output);
}
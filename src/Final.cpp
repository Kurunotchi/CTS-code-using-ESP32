#include <Arduino.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <INA226.h>
#include <WiFi.h>
#include <Preferences.h>
#include "time.h"
#include <ESP_Google_Sheet_Client.h>
#include <ArduinoJson.h>  
// --- WIFI CONFIGURATION ---
const char* ssid = "Salamat Shopee";
const char* pass = "wednesday";  
WiFiServer tcpServer(8888);  
WiFiClient client;
bool clientConnected = false;
unsigned long lastTCPSend = 0;
int tcpSendCounter = 0;
void connectToWiFi() {
  WiFi.begin(ssid, pass);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 8000) {
    delay(100);
  }
}
// --- HARDWARE CONFIGURATION ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {15, 4, 25, 26};
byte colPins[COLS] = {27, 14, 12, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
LiquidCrystal_I2C lcd(0x27, 20, 4);
Preferences prefs;
// --- GOOGLE SHEETS CONFIGURATION ---
#define PROJECT_ID       "potent-terminal-478407-m6"
#define CLIENT_EMAIL     "randolereviewcenter@potent-terminal-478407-m6.iam.gserviceaccount.com"
const char PRIVATE_KEY[] PROGMEM =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQChtT4Z809VN3Lo\neWX4X5r1Sg4O12RNF91b7EPMmNt5HlbkggmDkb/NTEGV6e+4hZ5Id+IvSAyfM+MA\ndZCkdcZBCcdMaxnhtUS2jXW/kLnGAgnQiGDhBiIvBUcCLm+yeTlZCrJQ/J6VgIMQ\nbv4XxIMry4GVbeWfVTWUEdicjkWCIZRfbk1nJsw4YNXAeaWdz5aHTWBRwiZihe67\ngUGqM/VY95skn8YLwsHQpXaAm2HDzWW4UVHlDdKqlxlkkwjvjrm6Z7Evzui0CQn2\namXJge5FfyYfbM8tl2C8F/ISw/2n4StN4q5pdbugig1RB6KU16FILYAI6QtnxVS+\nDo5TZpJtAgMBAAECggEAMod+UMCRHRk2/EKW5O4G7zfFPcj7TAW1gzhIFUH8bpPW\n5g9mJqkf7Gg0JEKVyCxkgdOIJ2sVmpetiqKx4Fn26bLDBnN/AmLQhlScow/3pNJV\nO8apsxbmDphREHLvLy8nBtZLUvglG6UtDzEHj+i1bjVomAdflZKcK9kJvR3NxXP2\nThlcPelI4N8OHdz1T+LRnotyeYsOZzrNRVSqrUHl3CzmCJc1NLNLeTgP5HeHO9C+\nV6YOnUHtGcN1EEkhqLZMV74171fZNJxxBR4tnehnfe35ArIWJL01OADLhSHpbrto\novsqHc0ta2COV/n4LLBC41JpoEvwO2ZKcIKHMoUc9wKBgQDYzgUCa7fbdR5CfsdX\nuJ2+2S3jdGTZMzC+jQKsbTAJCVHVvkpYVA6At7FyI2hhRikZxUeC+JJAGGciwvTF\nWgI8q0BBUHZ3qEfvZ9CpMCWFHTZsERXH17KWf3Ki6sqLB5vTBkfal/Nq7oVuHRbj\nn18bAI82J4ivoo89BsDw+96o3wKBgQC+8UdcdH3s6rBTwMRyeZQ0tRqbHFzIi+5E\npnQ7a5L1BGBVB3YTx+lbIv9Knmx6htSKv8LEwBXPhFK31hA5GQ19s9WNxIihyzKf\nd013N8jp6aFD6GLOeJXqfizk0wNFzV6yxnQ9+5vDqmYoUzUhW5w+tGj3clx4Z93e\nk6tzPPzSMwKBgQCB+soAEIqS9N1malGi4tkYAWbElhScL1eK9kljDLcew8qfRc2W\ntRZYz0iAMIA0yXZ8r8zW1aYA7WBv88gBxZvPua/1OIM969Ls0iXEOUxVSRVGptuT\nC1tTZSdaSz+RKMegNYTAphbWxheS07fUUckYDDbP9dW5ztDnenQURjzQqwKBgQCx\njg/7y1+lxX7+As0qXiAQ+y+oeTFWU7jXIaoH7zqSmOUzbGLCdi1rUBnxO2xIa8SM\n2VC2QKCHfdalmGsxjThcYbP9xnn/acLDQt9IMxmjWltZmGj48m0FxxrcFdR/PkAH\nIj/Ju4TW6EdizC0lvdiG/qB1KWUPmhZY+Rx/ZoD6vQKBgBGTzXEWzB+mT643DkaX\nt83ZRDXsTPWg3Sk21MUkQjKSGQpzAijPc5Sjk9yaQfyjdIsojLDeEGvomTb49uH3\n8uOhCMv3mbUQ+PY11pgaNMuXVzk2a+6o//F1CcaAgezn83nA7PTvlff1B2s5YfuR\ngzk4lX5lyewQ+3KvIgK3QUhA\n"
"-----END PRIVATE KEY-----\n";
const float CHARGE_STOP_V = 4.10;   
const float DISCHARGE_STOP_V = 2.80; 
const unsigned long logInterval = 60000;
const unsigned long interval = 2000;
bool gsheetReadyOnce = false;
// --- GLOBAL UI STATE ---
char previousSlot = '\0';
bool inSettingScreen = false;
bool inStopPrompt = false;
bool stopMessageActive = false;
unsigned long stopMessageStart = 0;
const unsigned long stopMessageDuration = 2000; 
bool inBattNumberInput = false;
String battNumberBuffer = "";
char battNumberSlot = '\0';
bool inCycleInput = false;
String cycleBuffer = "";
char cycleSlot = '\0';
bool confirmingCycle = false;
int pendingCycleValue = 0;
unsigned long previousStatusTime = 0;
const unsigned long statusInterval = 5000;
char lastDisplayedSlot = '\0';
unsigned long lastLCDUpdate = 0;
const unsigned long lcdUpdateInterval = 500;
unsigned long lastSensorRead = 0;
const unsigned long sensorReadInterval = 1000;
int currentSensor = 0;
void formatDateTime(const char* fmt, char* result, size_t size);
void printCentered(const char* text, int row);
void showHome();
void showSlot(char slot);
// ==========================================================
// BATTERY SLOT CLASS - Encapsulates all slot logic
// ==========================================================
class BatterySlot {
public:
  char label;
  uint8_t msPin; // Main Switch Relay
  uint8_t cdPin; // Charge/Discharge Relay
  INA226 ina;
  String sheetId;
  
  float currentCal, voltageCal, voltageCalDischarge, voltageCalCharge;
  int battNum = 0;
  float voltage = 0, current = 0, capacity = 0;
  String status = "No Battery";
  
  bool isCharge = false, isDischarge = false, isCycle = false;
  int cycleTarget = 0, cycleCount = 0;
  
  bool dischargeWait = false, capacityTracking = false;
  unsigned long dischargeStart = 0;
  unsigned long elapsedMinutes = 0, lastMinuteMark = 0, lastLogTime = 0;
  unsigned long prevReadTime = 0, lastCapTime = 0;
  float lastCapCurrent = 0;
  BatterySlot(char lbl, uint8_t ms, uint8_t cd, uint8_t i2cAddr, String sId, 
              float cCal, float vCal, float vCalDis, float vCalChg) 
      : label(lbl), msPin(ms), cdPin(cd), ina(i2cAddr), sheetId(sId),
        currentCal(cCal), voltageCal(vCal), voltageCalDischarge(vCalDis), voltageCalCharge(vCalChg) {}
  void begin() {
    pinMode(msPin, OUTPUT); digitalWrite(msPin, HIGH);
    pinMode(cdPin, OUTPUT); digitalWrite(cdPin, HIGH);
    if (!ina.begin()) {
      Serial.printf("Sensor %c not detected! (check wiring)\n", label);
    } else {
      ina.setAverage(INA226_1_SAMPLE);
      ina.setBusVoltageConversionTime(INA226_1100_us);
      ina.setShuntVoltageConversionTime(INA226_1100_us);
      ina.setModeShuntBusContinuous();
      ina.setMaxCurrentShunt(5.0, 0.01);
      Serial.printf("Sensor %c initialized.\n", label);
    }
    lastMinuteMark = millis();
  }
  void resetBooleans() {
    isCharge = isDischarge = isCycle = false;
    resetCapacity();
    elapsedMinutes = 0; lastMinuteMark = millis();
  }
  void resetRelays() {
    digitalWrite(msPin, HIGH); digitalWrite(cdPin, HIGH);
  }
  void updateRelays(char action) {
    isCharge = (action == '1');
    isDischarge = (action == '2');
    digitalWrite(msPin, (isCharge || isDischarge) ? LOW : HIGH);
    digitalWrite(cdPin, isCharge ? HIGH : (isDischarge ? LOW : HIGH));
  }
  void startDischargeWithDelay() {
    elapsedMinutes = 0; lastMinuteMark = millis();
    resetCapacity();
    dischargeWait = true; dischargeStart = millis();
  }
  void startCycle() {
    isCycle = true; cycleCount = 0;
    resetCapacity(); elapsedMinutes = 0; lastMinuteMark = millis();
    isCharge = true; digitalWrite(msPin, LOW); digitalWrite(cdPin, HIGH);
    Serial.printf("Slot %c cycle started - Charging (%d cycles total)\n", label, cycleTarget);
  }
  void stopOperation(bool fromForceStop) {
    String operation = "";
    if (isCharge) operation = "CHARGING";
    else if (isDischarge) operation = "DISCHARGING";
    else if (isCycle) operation = "CYCLE";
    float finalCapacity = capacity; unsigned long finalElapsed = elapsedMinutes;
    resetBooleans(); resetRelays();
    if (battNum > 0) logToSheet("Force Stopped", finalElapsed, finalCapacity, false, "Stopped");
    lcd.clear(); lcd.setCursor(0,1);
    if (fromForceStop) lcd.print(" FORCE STOPPED!");
    else { lcd.print("   OPERATION"); lcd.setCursor(0,2); lcd.print("    STOPPED!"); }
    
    lcd.setCursor(0,3); lcd.print("Slot "); lcd.print(label);
    if (operation.length() > 0) { lcd.print(" - "); lcd.print(operation); }
    
    stopMessageStart = millis(); stopMessageActive = true; inStopPrompt = false;
    Serial.printf("Slot %c operation stopped. Was: %s\n", label, operation.c_str());
  }
  void processDeferredDischarge() {
    if (dischargeWait && millis() - dischargeStart >= 10000) {
      dischargeWait = false; isDischarge = true;
      digitalWrite(msPin, LOW); digitalWrite(cdPin, LOW);
      Serial.printf("Slot %c DISCHARGE started after delay\n", label);
      if (previousSlot == label) showSlot(label);
    }
  }
  void updateElapsedTime(unsigned long now) {
    if (isCharge || isDischarge || isCycle) {
      if (now - lastMinuteMark >= 60000) {
        elapsedMinutes += 60; lastMinuteMark = now;
        Serial.printf("Slot %c elapsed minutes: %lu\n", label, elapsedMinutes);
      }
    } else { elapsedMinutes = 0; lastMinuteMark = now; }
  }
  void resetCapacity() {
    capacity = 0; lastCapTime = 0; lastCapCurrent = 0; capacityTracking = false;
  }
  void calculateCapacity(float currentNow, unsigned long currentTime) {
    bool isActive = (isCharge || isDischarge);
    if (isActive && !capacityTracking) {
      capacityTracking = true; lastCapTime = currentTime; lastCapCurrent = abs(currentNow); capacity = 0;
    } else if (!isActive && capacityTracking) { 
      capacityTracking = false; lastCapTime = 0; 
    }
    if (capacityTracking && lastCapTime > 0 && currentTime > lastCapTime) {
      float dt = (currentTime - lastCapTime) / 3600000.0;
      float avgCurrent = (lastCapCurrent + abs(currentNow)) / 2.0;
      capacity += avgCurrent * dt;
      lastCapTime = currentTime; lastCapCurrent = abs(currentNow);
    } else if (capacityTracking) {
      lastCapTime = currentTime; lastCapCurrent = abs(currentNow);
    }
  }
  void readSensor(unsigned long now) {
    float rawV = ina.getBusVoltage(); 
    float rawC = ina.getCurrent();
    
    voltage = rawV * (isCharge ? voltageCalCharge : (isDischarge ? voltageCalDischarge : voltageCal));
    current = abs(rawC * currentCal);
    
    if (voltage < 2.0) voltage = 0.0;
    if (rawC > -5.0 && rawC < 5.0) rawC = 0.0;
    current = abs(rawC * currentCal);
    status = (voltage == 0.0 && current == 0.0) ? "No Battery" :
             (voltage > 0.0 && current == 0.0) ? "Standby" :
             (voltage > 0.0 && rawC > 0.0) ? "Discharging" :
             (voltage > 0.0 && rawC < 0.0) ? "Charging" : "Unknown";
    checkVoltageLimit(); 
    calculateCapacity(rawC * currentCal, now);
  }
  void checkVoltageLimit() {
    if (isCharge && voltage >= CHARGE_STOP_V) {
      isCharge = false;
      if (isCycle && battNum > 0) {
        float storedCap = capacity;
        logToSheet("Charge Complete", elapsedMinutes, storedCap, true, "Charge");
        logSimpsonWithCapacity(storedCap);
        
        lcd.clear(); 
        lcd.setCursor(0, 0); lcd.print("Slot "); lcd.print(label); lcd.print(" Charge Done"); 
        lcd.setCursor(0, 1); lcd.print("Simpson = "); lcd.print(storedCap, 1); lcd.print(" mAh");
        lcd.setCursor(0, 2); lcd.print("Starting Discharge"); 
        lcd.setCursor(0, 3); lcd.print("in 2 seconds...");
        delay(2000); 
        cycleCount++;
        if (cycleCount >= cycleTarget) {
          isCycle = false; resetRelays();
          logToSheet("Cycle Complete", elapsedMinutes, storedCap, false, "Complete");
          if (previousSlot == label) {
            lcd.clear(); 
            lcd.setCursor(0, 1); lcd.print("Slot "); lcd.print(label); lcd.print(" Cycle Done!");
            lcd.setCursor(0, 2); lcd.print(cycleTarget); lcd.print(" cycles completed");
            lcd.setCursor(0, 3); lcd.print("Cap: "); lcd.print(storedCap, 1); lcd.print(" mAh");
            delay(3000); showSlot(label);
          }
        } else {
          delay(1000); resetCapacity(); elapsedMinutes = 0; lastMinuteMark = millis();
          isDischarge = true; digitalWrite(msPin, LOW); digitalWrite(cdPin, LOW);
          logToSheet("Discharge Start", 0, 0, true, "Discharge");
        }
      } else { 
        resetRelays(); logToSheet("Charge Complete", elapsedMinutes, capacity, false, "Complete"); 
      }
    }
    if (isDischarge && voltage <= DISCHARGE_STOP_V) {
      isDischarge = false;
      if (isCycle && battNum > 0) {
        float storedCap = capacity;
        logToSheet("Discharge Complete", elapsedMinutes, storedCap, true, "Discharge");
        logSimpsonWithCapacity(storedCap);
        lcd.clear(); 
        lcd.setCursor(0, 0); lcd.print("Slot "); lcd.print(label); lcd.print(" Discharge Done"); 
        lcd.setCursor(0, 1); lcd.print("Simpson = "); lcd.print(storedCap, 1); lcd.print(" mAh");
        lcd.setCursor(0, 2); lcd.print("Starting Charge"); 
        lcd.setCursor(0, 3); lcd.print("in 2 seconds...");
        delay(3000);
        resetCapacity(); elapsedMinutes = 0; lastMinuteMark = millis();
        isCharge = true; digitalWrite(msPin, LOW); digitalWrite(cdPin, HIGH);
        logToSheet("Charge Start", 0, 0, true, "Charge");
      } else { 
        resetRelays(); logToSheet("Discharge Complete", elapsedMinutes, capacity, false, "Complete"); 
      }
    }
  }
  void logToSheet(String slotStatus, unsigned long elapsedMins, float cap, bool inCycleLog, const char* opMode) {
    if (!GSheet.ready()) return;
    FirebaseJson response, valueRange; char timeStamp[25];
    formatDateTime("%Y-%m-%d %H:%M:%S", timeStamp, sizeof(timeStamp));
    String finalStatus = slotStatus;
    if (strcmp(opMode, "Simpson") == 0) finalStatus = "Simpson";
    else if (strcmp(opMode, "Pre-Discharge") == 0) finalStatus = "Capacity (Pre-Discharge)";
    else if (strcmp(opMode, "Pre-Charge") == 0) finalStatus = "Capacity (Pre-Charge)";
    else if (inCycleLog) {
      if (strcmp(opMode, "Charge") == 0) finalStatus = "Cycle-Charging";
      else if (strcmp(opMode, "Discharge") == 0) finalStatus = "Cycle-Discharging";
      else if (strcmp(opMode, "Complete") == 0) finalStatus = "Cycle-Complete";
      else finalStatus = "Cycle";
    } else if (strcmp(opMode, "Charge") == 0) finalStatus = "Charging";
    else if (strcmp(opMode, "Discharge") == 0) finalStatus = "Discharging";
    else if (strcmp(opMode, "Complete") == 0) finalStatus = "Complete";
    else if (strcmp(opMode, "Stopped") == 0) finalStatus = "Force Stopped";
    valueRange.add("majorDimension", "COLUMNS");
    valueRange.set("values/[0]/[0]", timeStamp);
    valueRange.set("values/[1]/[0]", finalStatus);
    valueRange.set("values/[2]/[0]", battNum);
    valueRange.set("values/[3]/[0]", elapsedMins);
    valueRange.set("values/[4]/[0]", String(voltage, 2));
    valueRange.set("values/[5]/[0]", String(current, 0));
    valueRange.set("values/[6]/[0]", String(cap, 1));  
    GSheet.values.append(&response, sheetId.c_str(), "Sheet1!A1:G1", &valueRange);
  }
  void logSimpsonWithCapacity(float cap) {
    if (!GSheet.ready() || battNum == 0) return;
    FirebaseJson response, valueRange; char timeStamp[25];
    formatDateTime("%Y-%m-%d %H:%M:%S", timeStamp, sizeof(timeStamp));
    
    valueRange.add("majorDimension", "COLUMNS");
    valueRange.set("values/[0]/[0]", timeStamp);
    valueRange.set("values/[1]/[0]", "Simpson");
    valueRange.set("values/[2]/[0]", battNum);
    valueRange.set("values/[3]/[0]", elapsedMinutes);
    valueRange.set("values/[4]/[0]", String(voltage, 2));
    valueRange.set("values/[5]/[0]", String(current, 0));
    valueRange.set("values/[6]/[0]", String(cap, 1));
    
    GSheet.values.append(&response, sheetId.c_str(), "Sheet1!A1:G1", &valueRange);
  }
};
// ==========================================================
// SLOT DEFINITIONS
// ==========================================================
BatterySlot slots[4] = {
  BatterySlot('A', 17, 16, 0x40, "1liVQR26X5vO0VPv9nVeQRkEXtCxbicngRhdJrHw05lc", 100.3720, 0.9991, 1.0248, 0.9874),
  BatterySlot('B', 18, 5, 0x41, "1aGVDJZaUeVPIGU43q_Mu6sZrNcv4g4YG6oHHVGlfAyE", 97.8846, 1.0005, 1.0326, 0.9898),
  BatterySlot('C', 32, 33, 0x44, "1UNclOw0Z7xe073mdUaDNap8xIiTkwQHakoat5yu541k", 98.8484, 1.0005, 1.0239, 0.9948),
  BatterySlot('D', 23, 19, 0x45, "1Kpbmw2CI7hMAssOOEdyrMFwYx_6z55q2dYv9i1Dj8w8", 98.6328, 1.0008, 1.0305, 0.9875)
};
BatterySlot* getSlot(char label) {
  if (label >= 'A' && label <= 'D') return &slots[label - 'A'];
  return nullptr;
}
// ==========================================================
// HELPER FUNCTIONS & UI
// ==========================================================
void formatDateTime(const char* fmt, char* result, size_t size) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) strftime(result, size, fmt, &timeinfo);
  else strncpy(result, "1970-01-01 00:00:00", size);
}
void printCentered(const char* text, int row) {
  int col = max(0, (20 - (int)strlen(text)) / 2);
  lcd.setCursor(col, row); lcd.print(text);
}
void showHome() {
  lcd.clear(); 
  printCentered("Capacity Testing", 0); 
  printCentered("Station (CTS) V2.0", 1);
  lcd.setCursor(0,3); 
  lcd.print(WiFi.status() == WL_CONNECTED ? "  Wi-Fi: Connected  " : "Wi-Fi: Not Connected");
}
void showSlot(char slot) {
  if (slot == lastDisplayedSlot && millis() - lastLCDUpdate < lcdUpdateInterval) return;
  lastDisplayedSlot = slot; lastLCDUpdate = millis();
  inSettingScreen = inStopPrompt = stopMessageActive = inBattNumberInput = inCycleInput = confirmingCycle = false;
  BatterySlot* bs = getSlot(slot); if (!bs) return;
  lcd.clear();
  String mode = bs->isCharge ? (bs->isCycle ? "CYCLE" : "CHARGING") :
                bs->isDischarge ? (bs->isCycle ? "CYCLE" : "DISCHARGING") : "IDLE";
  String cycleInfo = bs->isCycle ? "Cyc: " + String(bs->cycleCount) + "/" + String(bs->cycleTarget) : "";
  String title = String("Slot ") + slot + (bs->battNum > 0 ? " #" + String(bs->battNum) : "");
  printCentered(title.c_str(), 0);
  
  lcd.setCursor(0, 1); lcd.print("Status: "); lcd.print(bs->status);
  lcd.setCursor(0, 2); lcd.print("Mode: "); lcd.print(mode);
  if (cycleInfo.length() > 0) { lcd.setCursor(10, 2); lcd.print(cycleInfo); }
  lcd.setCursor(0, 3);
  if (bs->isCharge || bs->isDischarge) {
    lcd.print("T:"); lcd.print(bs->elapsedMinutes); 
    lcd.print(" V:"); lcd.print(bs->voltage, 2); 
    lcd.print(" I:"); lcd.print(bs->current, 0);
  } else { 
    lcd.print("V:"); lcd.print(bs->voltage, 2); 
    lcd.print(" I:"); lcd.print(bs->current, 0); 
  }
}
void refreshSlotDisplay() { if (previousSlot != '\0') showSlot(previousSlot); }
void showSlotSetting(char slot) {
  if (slot == '\0') return;
  lcd.clear(); 
  lcd.setCursor(0,0); lcd.print("SLOT "); lcd.print(slot); lcd.print(" OPTIONS");
  lcd.setCursor(0,1); lcd.print("1.Charge 2.Discharge");
  lcd.setCursor(0,2); lcd.print("3.Cycle  4.Batt No.");
  lcd.setCursor(0,3); lcd.print("Press * to go back");
}
void showForceStopPrompt(char slot) {
  BatterySlot* bs = getSlot(slot); if (!bs) return;
  lcd.clear(); 
  lcd.setCursor(0,0); lcd.print("FORCE STOP Slot "); lcd.print(slot);
  lcd.setCursor(0,1); lcd.print("Current: ");
  lcd.print(bs->isCharge ? "CHARGING" : (bs->isDischarge ? "DISCHARGING" : (bs->isCycle ? "CYCLE" : "")));
  lcd.setCursor(0,2); lcd.print("0 = Yes, FORCE STOP");
  lcd.setCursor(0,3); lcd.print("5 = No, continue");
}
void renderBattNumberBuffer() {
  String display = "[" + battNumberBuffer + "]";
  lcd.setCursor(max(0, (20 - (int)display.length()) / 2), 1); 
  lcd.print(display);
}
void showBattNumberInput(char slot) {
  if (slot == '\0') return;
  battNumberSlot = slot; battNumberBuffer = ""; inBattNumberInput = true;
  lcd.clear(); 
  lcd.setCursor(0,0); lcd.print("Enter Batt # for "); lcd.print(slot);
  renderBattNumberBuffer();
  lcd.setCursor(0,2); lcd.print("* Cancel   # OK");
  lcd.setCursor(0,3); lcd.print(getSlot(slot)->battNum);
}
void renderCycleBuffer() {
  String display = "[" + cycleBuffer + "]";
  lcd.setCursor(max(0, (20 - (int)display.length()) / 2), 1); 
  lcd.print(display);
}
void showCycleInput(char slot) {
  if (slot == '\0') return;
  cycleSlot = slot; cycleBuffer = ""; inCycleInput = true; confirmingCycle = false;
  lcd.clear(); 
  lcd.setCursor(0,0); lcd.print("Set Cycles for "); lcd.print(slot);
  renderCycleBuffer();
  lcd.setCursor(0,2); lcd.print("Enter number (1-999)");
  lcd.setCursor(0,3); lcd.print("* Cancel   # OK");
}
void showCycleConfirmation(char slot, int cycles) {
  lcd.clear(); 
  lcd.setCursor(0,0); lcd.print("Confirm Cycle Test");
  lcd.setCursor(0,1); lcd.print("Slot "); lcd.print(slot); lcd.print(": "); lcd.print(cycles); lcd.print(" cycles");
  lcd.setCursor(0,2); lcd.print("Ends on CHARGE");
  lcd.setCursor(0,3); lcd.print("#=START *=CANCEL");
}
void saveBattNum(char slot, int num) {
  BatterySlot* bs = getSlot(slot); if (!bs) return;
  prefs.putInt((String(slot) + "_num").c_str(), num); 
  bs->battNum = num;
}
// ==========================================================
// TCP COMMUNICATION
// ==========================================================
void sendSensorDataOverTCP(BatterySlot* bs) {
  if (!clientConnected || !bs || (millis() - lastTCPSend < 2000)) return;
  lastTCPSend = millis(); tcpSendCounter++;
  
  StaticJsonDocument<300> doc;
  doc["type"] = "sensor_data"; doc["slot"] = String(bs->label);
  doc["timestamp"] = millis(); doc["battery_num"] = bs->battNum;
  doc["voltage"] = bs->voltage; doc["current"] = bs->current;
  doc["capacity"] = bs->capacity; doc["elapsed_minutes"] = bs->elapsedMinutes;
  doc["mode"] = bs->isCharge ? "CHARGING" : (bs->isDischarge ? "DISCHARGING" : (bs->isCycle ? "CYCLE" : "IDLE"));
  doc["cycle_current"] = bs->cycleCount; doc["cycle_target"] = bs->cycleTarget;
  
  String output; serializeJson(doc, output); 
  client.println(output);
}
void sendCapacityDataOverTCP(BatterySlot* bs) {
  if (!clientConnected || !bs || tcpSendCounter % 10 != 0) return;
  
  StaticJsonDocument<200> doc;
  doc["type"] = "capacity_data"; doc["slot"] = String(bs->label);
  doc["capacity"] = bs->capacity; doc["elapsed_minutes"] = bs->elapsedMinutes;
  
  String output; serializeJson(doc, output); 
  client.println(output);
}
void handleTCPCommands() {
  if (clientConnected && client.available()) {
    String command = client.readStringUntil('\n'); command.trim();
    if (command.length() > 0) {
      StaticJsonDocument<200> doc;
      if (!deserializeJson(doc, command)) {
        const char* cmd = doc["command"]; const char* slotStr = doc["slot"];
        if (cmd && slotStr) {
          char cmdChar = cmd[0], slotChar = slotStr[0];
          BatterySlot* bs = getSlot(slotChar);
          if (bs) {
            previousSlot = slotChar;
            switch(cmdChar) {
              case '1': 
                if (bs->battNum > 0) { 
                  bs->updateRelays('1'); 
                  bs->logToSheet("Charging", 0, bs->capacity, false, "Charge"); 
                } 
                break;
              case '2': 
                if (bs->battNum > 0) bs->startDischargeWithDelay(); 
                break;
              case '3': 
                if (bs->battNum > 0) { 
                  bs->cycleTarget = doc["cycles"] | 3; 
                  bs->startCycle(); 
                } 
                break;
              case '4': 
                if ((int)doc["battery_number"] > 0) saveBattNum(slotChar, doc["battery_number"]); 
                break;
              case '0': 
                bs->stopOperation(true); 
                break;
            }
          }
          client.printf("{\"type\":\"ack\",\"command\":\"%c\",\"slot\":\"%c\"}\n", cmdChar, slotChar);
        }
      }
    }
  }
}
// ==========================================================
// ARDUINO SETUP & LOOP
// ==========================================================
void setup() {
  Serial.begin(115200); Serial.println("System Starting...");
  prefs.begin("battNums", false);
  Wire.begin(); lcd.init(); lcd.backlight();
  WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.persistent(true);
  connectToWiFi();
  if (WiFi.status() == WL_CONNECTED) { 
    tcpServer.begin(); 
    Serial.printf("TCP Server started at 8888\n"); 
  }
  
  for (int i=0; i<4; i++) {
    slots[i].begin();
    slots[i].battNum = prefs.getInt((String(slots[i].label) + "_num").c_str(), 0);
  }
  showHome(); 
  configTime(0, 3600, "pool.ntp.org"); 
  GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
  
  unsigned long start = millis();
  while (!GSheet.ready() && millis() - start < 10000) delay(500);
  gsheetReadyOnce = GSheet.ready(); 
  Serial.println("Setup complete.");
}
void loop() {
  unsigned long now = millis();
  
  // Serial Monitor Prints
  static unsigned long lastSerialPrint = 0;
  if (now - lastSerialPrint >= 2000) {
    lastSerialPrint = now; 
    Serial.println("\n--- CURRENT READINGS ---");
    for (int i=0; i<4; i++) 
      Serial.printf("Slot %c - Voltage: %.2f V, Current: %.0f mA\n", slots[i].label, slots[i].voltage, slots[i].current);
    Serial.println("------------------------\n");
  }
  if (WiFi.status() != WL_CONNECTED) connectToWiFi();
  
  if (!clientConnected) {
    client = tcpServer.available();
    if (client) { 
      clientConnected = true; 
      client.println("{\"type\":\"connected\",\"message\":\"ESP32 Battery Tester Ready\"}"); 
    }
  } else if (!client.connected()) { 
    client.stop(); clientConnected = false; 
  }
  
  handleTCPCommands();
  // Tick each slot
  for (int i=0; i<4; i++) {
    slots[i].updateElapsedTime(now); 
    slots[i].processDeferredDischarge();
    
    if (i == currentSensor && now - lastSensorRead >= sensorReadInterval) {
      if (now - slots[i].prevReadTime >= interval) {
        slots[i].prevReadTime = now; 
        slots[i].readSensor(now);
        sendSensorDataOverTCP(&slots[i]); 
        sendCapacityDataOverTCP(&slots[i]);
        
        if (previousSlot == slots[i].label && !inSettingScreen && !inStopPrompt && !stopMessageActive && !inBattNumberInput && !inCycleInput && !confirmingCycle)
          showSlot(slots[i].label);
      }
      currentSensor = (currentSensor + 1) % 4; lastSensorRead = now;
    }
    if (slots[i].battNum > 0 && (slots[i].isCharge || slots[i].isDischarge || slots[i].isCycle) && (now - slots[i].lastLogTime >= logInterval) && GSheet.ready()) {
      slots[i].lastLogTime = now;
      const char* opMode = slots[i].isCycle ? (slots[i].isCharge ? "Charge" : (slots[i].isDischarge ? "Discharge" : "Cycle")) : (slots[i].isCharge ? "Charge" : (slots[i].isDischarge ? "Discharge" : "None"));
      slots[i].logToSheet(slots[i].status, slots[i].elapsedMinutes, slots[i].capacity, slots[i].isCycle, opMode);
    }
  }
  if (now - previousStatusTime >= statusInterval) {
    previousStatusTime = now;
    if (previousSlot == '\0' && !inSettingScreen && !inStopPrompt && !stopMessageActive && !inBattNumberInput && !inCycleInput && !confirmingCycle) {
      lcd.setCursor(0,3); lcd.print(WiFi.status() == WL_CONNECTED ? "  Wi-Fi: Connected  " : "Wi-Fi: Not Connected");
    }
  }
  if (stopMessageActive && now - stopMessageStart >= stopMessageDuration) { 
    stopMessageActive = false; showSlot(previousSlot); 
  }
  if (!gsheetReadyOnce && (now % 30000UL) < 200 && GSheet.ready()) 
    gsheetReadyOnce = true;
  char key = keypad.getKey(); if (!key) return;
  // UI Key Routing
  if (inBattNumberInput) {
    if (key >= '0' && key <= '9') { 
      if (!(battNumberBuffer.length() == 0 && key == '0') && battNumberBuffer.length() < 3) { 
        battNumberBuffer += key; renderBattNumberBuffer(); 
      } 
    }
    else if (key == '*') { 
      inBattNumberInput = false; battNumberSlot = '\0'; battNumberBuffer = ""; 
      showSlotSetting(previousSlot); inSettingScreen = true; 
    }
    else if (key == '#') {
      int value = battNumberBuffer.length() > 0 ? atoi(battNumberBuffer.c_str()) : 0; saveBattNum(battNumberSlot, value);
      lcd.clear(); lcd.setCursor(0,1); lcd.print(" Batt # saved!"); lcd.setCursor(0,2); lcd.print(" Slot "); lcd.print(battNumberSlot); lcd.print(" = "); lcd.print(value); delay(1500);
      inBattNumberInput = false; battNumberSlot = '\0'; battNumberBuffer = ""; showSlotSetting(previousSlot); inSettingScreen = true;
    }
    return;
  }
  
  if (inCycleInput) {
    if (key >= '0' && key <= '9') { 
      if (!(cycleBuffer.length() == 0 && key == '0') && cycleBuffer.length() < 3) { 
        cycleBuffer += key; renderCycleBuffer(); 
      } 
    }
    else if (key == '*') { 
      inCycleInput = confirmingCycle = false; cycleSlot = '\0'; cycleBuffer = ""; pendingCycleValue = 0; 
      showSlotSetting(previousSlot); inSettingScreen = true; 
    }
    else if (key == '#') { 
      pendingCycleValue = cycleBuffer.length() > 0 ? atoi(cycleBuffer.c_str()) : 1; 
      showCycleConfirmation(cycleSlot, pendingCycleValue); 
      inCycleInput = false; confirmingCycle = true; 
    }
    return;
  }
  
  if (confirmingCycle) {
    if (key == '#') {
      BatterySlot* bs = getSlot(cycleSlot);
      if (bs) { 
        bs->cycleTarget = pendingCycleValue; bs->startCycle(); bs->logToSheet("Cycle Start", 0, bs->capacity, true, "Charge"); 
        lcd.clear(); 
        lcd.setCursor(0,1); lcd.print("Starting Cycle Test"); 
        lcd.setCursor(0,2); lcd.print("Slot "); lcd.print(cycleSlot); lcd.print(" - "); lcd.print(bs->cycleTarget); lcd.print(" cycles");
        lcd.setCursor(0,3); lcd.print("Ends on CHARGE"); 
        delay(2000); 
      }
      confirmingCycle = false; cycleSlot = '\0'; cycleBuffer = ""; pendingCycleValue = 0; 
      if (previousSlot != '\0') showSlot(previousSlot);
    } 
    else if (key == '*') { 
      lcd.clear(); lcd.setCursor(0,1); lcd.print("Cycle test CANCELLED"); delay(1500); 
      confirmingCycle = false; cycleSlot = '\0'; cycleBuffer = ""; pendingCycleValue = 0; 
      showSlotSetting(previousSlot); inSettingScreen = true; 
    }
    return;
  }
  
  if (inStopPrompt) {
    if (key == '0') { 
      getSlot(previousSlot)->stopOperation(true); 
    } 
    else if (key == '5') { 
      inStopPrompt = stopMessageActive = false; refreshSlotDisplay(); 
    }
    return;
  }
  
  if (inSettingScreen) {
    BatterySlot* bs = getSlot(previousSlot); if (!bs) return;
    if (key == '1') { 
      if (bs->battNum > 0) { 
        bs->updateRelays('1'); bs->lastMinuteMark = millis(); bs->elapsedMinutes = 0; 
        bs->logToSheet("Charging", 0, bs->capacity, false, "Charge"); 
        inSettingScreen = false; refreshSlotDisplay(); 
      } else { lcd.clear(); lcd.setCursor(0,1); lcd.print(" Set batt# first!"); delay(1500); showSlotSetting(previousSlot); } 
    }
    else if (key == '2') { 
      if (bs->battNum > 0) { 
        bs->startDischargeWithDelay(); bs->logToSheet("Discharging", 0, bs->capacity, false, "Discharge"); inSettingScreen = false; 
      } else { lcd.clear(); lcd.setCursor(0,1); lcd.print(" Set batt# first!"); delay(1500); showSlotSetting(previousSlot); } 
    }
    else if (key == '3') { 
      if (bs->battNum > 0) { 
        showCycleInput(previousSlot); inSettingScreen = false; 
      } else { lcd.clear(); lcd.setCursor(0,1); lcd.print(" Set batt# first!"); delay(1500); showSlotSetting(previousSlot); } 
    }
    else if (key == '4') { 
      showBattNumberInput(previousSlot); inSettingScreen = false; 
    }
    return;
  }
  
  switch (key) {
    case '0': { 
      BatterySlot* bs = getSlot(previousSlot); 
      if (bs) { 
        if (bs->isCharge || bs->isDischarge || bs->isCycle) { 
          showForceStopPrompt(previousSlot); inStopPrompt = true; 
        } else { 
          lcd.clear(); lcd.setCursor(0,1); lcd.print("No active operation"); lcd.setCursor(0,2); lcd.print("in Slot "); lcd.print(previousSlot); delay(1500); showSlot(previousSlot); 
        } 
      } 
      break; 
    }
    case '#': showHome(); previousSlot = '\0'; inSettingScreen = inStopPrompt = inBattNumberInput = inCycleInput = confirmingCycle = false; break;
    case 'A': case 'B': case 'C': case 'D': showSlot(key); previousSlot = key; break;
    case '*': if (previousSlot != '\0') { showSlotSetting(previousSlot); inSettingScreen=true; } break;
  }
}

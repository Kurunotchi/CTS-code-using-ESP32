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

const char* ssid = "Kodicpogi21";
const char* pass = "11123232";


WiFiServer tcpServer(8888);  
WiFiClient client;
bool clientConnected = false;
unsigned long lastWiFiDataSend = 0;
const unsigned long wifiDataInterval = 3000; 
unsigned long previousStatusTime = 0;
const unsigned long statusInterval = 3000; 

void connectToWiFi() {
  WiFi.begin(ssid, pass);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 8000) {
    delay(100);
  }
}

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

const int DMS = 23;
const int DCD = 19;
const int BMS = 18;
const int BCD = 5;
const int AMS = 17;
const int ACD = 16;
const int CMS = 32;
const int CCD = 33;

#define ADDR_A 0x40
#define ADDR_B 0x41
#define ADDR_C 0x44
#define ADDR_D 0x45

INA226 inaA(ADDR_A);
INA226 inaB(ADDR_B);
INA226 inaC(ADDR_C);
INA226 inaD(ADDR_D);

float currentCalA = 100.3720;
float voltageCalA = 0.9991;
float voltageCalAdischarge = 1.0248;
float voltageCalAcharge = 0.9874;

float currentCalB = 97.8846;
float voltageCalB = 1.0005;
float voltageCalBdischarge = 1.0326;
float voltageCalBcharge = 0.9898;

float currentCalC = 98.8484;
float voltageCalC = 1.0005;
float voltageCalCdischarge = 1.0239;
float voltageCalCcharge = 0.9948;

float currentCalD = 98.6328;
float voltageCalD = 1.0008;
float voltageCalDdischarge = 1.0305;
float voltageCalDcharge = 0.9875;

unsigned long prevA = 0, prevB = 0, prevC = 0, prevD = 0;
const unsigned long interval = 3000; 

char previousSlot = '\0';
bool inSettingScreen = false;
bool inStopPrompt = false;
bool stopMessageActive = false;
unsigned long stopMessageStart = 0;
const unsigned long stopMessageDuration = 2000; 

bool inBattNumberInput = false;
String battNumberBuffer = "";
char battNumberSlot = '\0';

float voltageA = 0, currentA = 0;
float voltageB = 0, currentB = 0;
float voltageC = 0, currentC = 0;
float voltageD = 0, currentD = 0;
String statusA = "No Battery";
String statusB = "No Battery";
String statusC = "No Battery";
String statusD = "No Battery";

bool Acharge = false, Adischarge = false;
bool Bcharge = false, Bdischarge = false;
bool Ccharge = false, Cdischarge = false;
bool Dcharge = false, Ddischarge = false;

bool Acycle = false, Bcycle = false, Ccycle = false, Dcycle = false;

int cycleTargetA = 0, cycleTargetB = 0, cycleTargetC = 0, cycleTargetD = 0;
int cycleCountA = 0, cycleCountB = 0, cycleCountC = 0, cycleCountD = 0;

bool inCycleInput = false;
String cycleBuffer = "";
char cycleSlot = '\0';
bool confirmingCycle = false;
int pendingCycleValue = 0;

int battNumA = 0, battNumB = 0, battNumC = 0, battNumD = 0;

bool AdischargePending = false, BdischargePending = false;
bool CdischargePending = false, DdischargePending = false;

bool AdischargeWait = false, BdischargeWait = false;
bool CdischargeWait = false, DdischargeWait = false;

unsigned long AdischargeStart = 0, BdischargeStart = 0;
unsigned long CdischargeStart = 0, DdischargeStart = 0;

unsigned long elapsedA = 0, elapsedB = 0, elapsedC = 0, elapsedD = 0;

const float CHARGE_STOP_V = 4.10;   
const float DISCHARGE_STOP_V = 2.80; 

// SIMPSON'S RULE VARIABLES 
// Capacity tracking in milliamp-hours (mAh)
float capacityA = 0, capacityB = 0, capacityC = 0, capacityD = 0;

struct SimpsonSample {
  unsigned long time;  
  float current;     
};

#define MAX_SAMPLES 1000

SimpsonSample samplesA[MAX_SAMPLES];
SimpsonSample samplesB[MAX_SAMPLES];
SimpsonSample samplesC[MAX_SAMPLES];
SimpsonSample samplesD[MAX_SAMPLES];

int sampleCountA_cum = 0;
int sampleCountB_cum = 0;
int sampleCountC_cum = 0;
int sampleCountD_cum = 0;

bool capacityTrackingA = false, capacityTrackingB = false, capacityTrackingC = false, capacityTrackingD = false;

float calculateCumulativeSimpson(SimpsonSample* samples, int count) {
  if (count < 2) return 0.0;  
  float totalCapacity = 0.0;
  
  if (count == 2) {
    float dt = (samples[1].time - samples[0].time) / 3600000.0;  
    totalCapacity = dt * (samples[0].current + samples[1].current) / 2.0;
    return totalCapacity;
  }
  
  for (int i = 0; i < count - 2; i += 2) {
    if (i + 2 < count) {
      float dt1 = (samples[i+1].time - samples[i].time) / 3600000.0;
      float dt2 = (samples[i+2].time - samples[i+1].time) / 3600000.0;
      
      if (abs(dt1 - dt2) < 0.0001) {
        // Use Simpson's 1/3 rule
        float h = dt1;  // or dt2
        float segment = (h / 3.0) * (samples[i].current + 4 * samples[i+1].current + samples[i+2].current);
        totalCapacity += segment;
      } else {
        float seg1 = dt1 * (samples[i].current + samples[i+1].current) / 2.0;
        float seg2 = dt2 * (samples[i+1].current + samples[i+2].current) / 2.0;
        totalCapacity += (seg1 + seg2);
      }
    }
  }
  if (count % 2 == 0) {
    int last = count - 2;
    float dt = (samples[last+1].time - samples[last].time) / 3600000.0;
    float segment = dt * (samples[last].current + samples[last+1].current) / 2.0;
    totalCapacity += segment;
  }
  
  return totalCapacity;
}

Preferences prefs;

#define PROJECT_ID       "potent-terminal-478407-m6"
#define CLIENT_EMAIL     "randolereviewcenter@potent-terminal-478407-m6.iam.gserviceaccount.com"

const char PRIVATE_KEY[] PROGMEM =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQChtT4Z809VN3Lo\neWX4X5r1Sg4O12RNF91b7EPMmNt5HlbkggmDkb/NTEGV6e+4hZ5Id+IvSAyfM+MA\ndZCkdcZBCcdMaxnhtUS2jXW/kLnGAgnQiGDhBiIvBUcCLm+yeTlZCrJQ/J6VgIMQ\nbv4XxIMry4GVbeWfVTWUEdicjkWCIZRfbk1nJsw4YNXAeaWdz5aHTWBRwiZihe67\ngUGqM/VY95skn8YLwsHQpXaAm2HDzWW4UVHlDdKqlxlkkwjvjrm6Z7Evzui0CQn2\namXJge5FfyYfbM8tl2C8F/ISw/2n4StN4q5pdbugig1RB6KU16FILYAI6QtnxVS+\nDo5TZpJtAgMBAAECggEAMod+UMCRHRk2/EKW5O4G7zfFPcj7TAW1gzhIFUH8bpPW\n5g9mJqkf7Gg0JEKVyCxkgdOIJ2sVmpetiqKx4Fn26bLDBnN/AmLQhlScow/3pNJV\nO8apsxbmDphREHLvLy8nBtZLUvglG6UtDzEHj+i1bjVomAdflZKcK9kJvR3NxXP2\nThlcPelI4N8OHdz1T+LRnotyeYsOZzrNRVSqrUHl3CzmCJc1NLNLeTgP5HeHO9C+\nV6YOnUHtGcN1EEkhqLZMV74171fZNJxxBR4tnehnfe35ArIWJL01OADLhSHpbrto\novsqHc0ta2COV/n4LLBC41JpoEvwO2ZKcIKHMoUc9wKBgQDYzgUCa7fbdR5CfsdX\nuJ2+2S3jdGTZMzC+jQKsbTAJCVHVvkpYVA6At7FyI2hhRikZxUeC+JJAGGciwvTF\nWgI8q0BBUHZ3qEfvZ9CpMCWFHTZsERXH17KWf3Ki6sqLB5vTBkfal/Nq7oVuHRbj\nn18bAI82J4ivoo89BsDw+96o3wKBgQC+8UdcdH3s6rBTwMRyeZQ0tRqbHFzIi+5E\npnQ7a5L1BGBVB3YTx+lbIv9Knmx6htSKv8LEwBXPhFK31hA5GQ19s9WNxIihyzKf\nd013N8jp6aFD6GLOeJXqfizk0wNFzV6yxnQ9+5vDqmYoUzUhW5w+tGj3clx4Z93e\nk6tzPPzSMwKBgQCB+soAEIqS9N1malGi4tkYAWbElhScL1eK9kljDLcew8qfRc2W\ntRZYz0iAMIA0yXZ8r8zW1aYA7WBv88gBxZvPua/1OIM969Ls0iXEOUxVSRVGptuT\nC1tTZSdaSz+RKMegNYTAphbWxheS07fUUckYDDbP9dW5ztDnenQURjzQqwKBgQCx\njg/7y1+lxX7+As0qXiAQ+y+oeTFWU7jXIaoH7zqSmOUzbGLCdi1rUBnxO2xIa8SM\n2VC2QKCHfdalmGsxjThcYbP9xnn/acLDQt9IMxmjWltZmGj48m0FxxrcFdR/PkAH\nIj/Ju4TW6EdizC0lvdiG/qB1KWUPmhZY+Rx/ZoD6vQKBgBGTzXEWzB+mT643DkaX\nt83ZRDXsTPWg3Sk21MUkQjKSGQpzAijPc5Sjk9yaQfyjdIsojLDeEGvomTb49uH3\n8uOhCMv3mbUQ+PY11pgaNMuXVzk2a+6o//F1CcaAgezn83nA7PTvlff1B2s5YfuR\ngzk4lX5lyewQ+3KvIgK3QUhA\n"
"-----END PRIVATE KEY-----\n";

#define SHEET_ID_A "1liVQR26X5vO0VPv9nVeQRkEXtCxbicngRhdJrHw05lc"
#define SHEET_ID_B "1aGVDJZaUeVPIGU43q_Mu6sZrNcv4g4YG6oHHVGlfAyE"
#define SHEET_ID_C "1UNclOw0Z7xe073mdUaDNap8xIiTkwQHakoat5yu541k"
#define SHEET_ID_D "1Kpbmw2CI7hMAssOOEdyrMFwYx_6z55q2dYv9i1Dj8w8"

unsigned long lastRead = 0;
const unsigned long readInterval = 3000;   
unsigned long lastLogA = 0, lastLogB = 0, lastLogC = 0, lastLogD = 0;
const unsigned long logInterval = 60000;  

bool wasConnected = false;
bool gsheetReadyOnce = false;

void resetSlotBooleans(char slot);
void setupSensor(INA226 &sensor, const char* label);
void readSensor(INA226 &sensor, char label);
void checkVoltageLimit(char slot);
void updateSlotRelays(char slot, char action);
void resetSlotRelays(char slot);
void showHome();
void showSlot(char slot);
void refreshSlotDisplay();
void showSlotSetting(char slot);
void showStopPrompt(char slot);
void showForceStopPrompt(char slot);
void showBattNumberInput(char slot);
void renderBattNumberBuffer();
void handleBattNumberInputKey(char key);
void showCycleInput(char slot);
void renderCycleBuffer();
void handleCycleInputKey(char key);
void showCycleConfirmation(char slot, int cycles);
void processDeferredDischarge();
void saveBattNum(char slot, int num);
void clearBattNum(char slot);
int battNumForSlot(char slot);
void startDischargeWithDelay(char slot);
void startCycle(char slot);
void formatDateTime(const char* fmt, char* result, size_t size);
void logSlotToSheet(const char* sheetId, const String& slotStatus, int battNum, unsigned long elapsedSecs, float voltage, float current, float capacity, bool inCycle = false, const char* operationMode = "None");
void stopOperation(char slot, bool fromForceStop = false);
void calculateSimpsonCapacity(char slot, float current, unsigned long currentTime);
void resetCapacity(char slot);

void startTCPServer() {
    tcpServer.begin();
    Serial.println("=================================");
    Serial.println("TCP Server started on port 8888");
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Connect Python dashboard to this IP");
    Serial.println("=================================");
}

void checkTCPClient() {
    if (!clientConnected) {
        client = tcpServer.available();
        if (client) {
            clientConnected = true;
            Serial.println("✓ Python dashboard connected via WiFi!");
            client.println("{\"type\":\"connected\",\"message\":\"ESP32 Battery Tester Ready\"}");
        }
    }
    
    if (clientConnected && !client.connected()) {
        client.stop();
        clientConnected = false;
        Serial.println("✗ Python dashboard disconnected");
    }
}

void sendSensorDataOverTCP(char slot) {
    if (!clientConnected) return;
    
    StaticJsonDocument<300> doc;
    
    int bn = 0;
    float v = 0, c = 0, cap = 0;
    String mode = "";
    int cycleCurrent = 0, cycleTarget = 0;
    
    switch(slot) {
        case 'A':
            bn = battNumA; v = voltageA; c = currentA; cap = capacityA;
            if (Acharge) mode = "CHARGING";
            else if (Adischarge) mode = "DISCHARGING";
            else if (Acycle) mode = "CYCLE";
            else mode = "IDLE";
            cycleCurrent = cycleCountA;
            cycleTarget = cycleTargetA;
            break;
        case 'B':
            bn = battNumB; v = voltageB; c = currentB; cap = capacityB;
            if (Bcharge) mode = "CHARGING";
            else if (Bdischarge) mode = "DISCHARGING";
            else if (Bcycle) mode = "CYCLE";
            else mode = "IDLE";
            cycleCurrent = cycleCountB;
            cycleTarget = cycleTargetB;
            break;
        case 'C':
            bn = battNumC; v = voltageC; c = currentC; cap = capacityC;
            if (Ccharge) mode = "CHARGING";
            else if (Cdischarge) mode = "DISCHARGING";
            else if (Ccycle) mode = "CYCLE";
            else mode = "IDLE";
            cycleCurrent = cycleCountC;
            cycleTarget = cycleTargetC;
            break;
        case 'D':
            bn = battNumD; v = voltageD; c = currentD; cap = capacityD;
            if (Dcharge) mode = "CHARGING";
            else if (Ddischarge) mode = "DISCHARGING";
            else if (Dcycle) mode = "CYCLE";
            else mode = "IDLE";
            cycleCurrent = cycleCountD;
            cycleTarget = cycleTargetD;
            break;
    }
    
    doc["type"] = "sensor_data";
    doc["slot"] = String(slot);
    doc["timestamp"] = millis();
    doc["battery_num"] = bn;
    doc["voltage"] = v;
    doc["current"] = c;
    doc["capacity"] = cap;
    doc["mode"] = mode;
    doc["cycle_current"] = cycleCurrent;
    doc["cycle_target"] = cycleTarget;
    
    String output;
    serializeJson(doc, output);
    client.println(output);
}

void sendSimpsonDataOverTCP(char slot) {
    if (!clientConnected) return;
    
    StaticJsonDocument<400> doc;
    JsonArray samples = doc.createNestedArray("samples");
    
    switch(slot) {
        case 'A':
            if (sampleCountA_cum >= 3) {
                JsonObject s1 = samples.createNestedObject();
                s1["time"] = samplesA[sampleCountA_cum-3].time;
                s1["current"] = samplesA[sampleCountA_cum-3].current;
                JsonObject s2 = samples.createNestedObject();
                s2["time"] = samplesA[sampleCountA_cum-2].time;
                s2["current"] = samplesA[sampleCountA_cum-2].current;
                JsonObject s3 = samples.createNestedObject();
                s3["time"] = samplesA[sampleCountA_cum-1].time;
                s3["current"] = samplesA[sampleCountA_cum-1].current;
                doc["capacity"] = capacityA;
            }
            break;
        case 'B':
            if (sampleCountB_cum >= 3) {
                JsonObject s1 = samples.createNestedObject();
                s1["time"] = samplesB[sampleCountB_cum-3].time;
                s1["current"] = samplesB[sampleCountB_cum-3].current;
                JsonObject s2 = samples.createNestedObject();
                s2["time"] = samplesB[sampleCountB_cum-2].time;
                s2["current"] = samplesB[sampleCountB_cum-2].current;
                JsonObject s3 = samples.createNestedObject();
                s3["time"] = samplesB[sampleCountB_cum-1].time;
                s3["current"] = samplesB[sampleCountB_cum-1].current;
                doc["capacity"] = capacityB;
            }
            break;
        case 'C':
            if (sampleCountC_cum >= 3) {
                JsonObject s1 = samples.createNestedObject();
                s1["time"] = samplesC[sampleCountC_cum-3].time;
                s1["current"] = samplesC[sampleCountC_cum-3].current;
                JsonObject s2 = samples.createNestedObject();
                s2["time"] = samplesC[sampleCountC_cum-2].time;
                s2["current"] = samplesC[sampleCountC_cum-2].current;
                JsonObject s3 = samples.createNestedObject();
                s3["time"] = samplesC[sampleCountC_cum-1].time;
                s3["current"] = samplesC[sampleCountC_cum-1].current;
                doc["capacity"] = capacityC;
            }
            break;
        case 'D':
            if (sampleCountD_cum >= 3) {
                JsonObject s1 = samples.createNestedObject();
                s1["time"] = samplesD[sampleCountD_cum-3].time;
                s1["current"] = samplesD[sampleCountD_cum-3].current;
                JsonObject s2 = samples.createNestedObject();
                s2["time"] = samplesD[sampleCountD_cum-2].time;
                s2["current"] = samplesD[sampleCountD_cum-2].current;
                JsonObject s3 = samples.createNestedObject();
                s3["time"] = samplesD[sampleCountD_cum-1].time;
                s3["current"] = samplesD[sampleCountD_cum-1].current;
                doc["capacity"] = capacityD;
            }
            break;
    }
    
    if (samples.size() > 0) {
        doc["type"] = "simpson_samples";
        doc["slot"] = String(slot);
        
        String output;
        serializeJson(doc, output);
        client.println(output);
    }
}

void handleTCPCommands() {
    if (clientConnected && client.available()) {
        String command = client.readStringUntil('\n');
        command.trim();
        
        if (command.length() > 0) {
            Serial.print("Received from Python: ");
            Serial.println(command);
            
            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, command);
            
            if (!error) {
                const char* cmd = doc["command"];
                const char* slot = doc["slot"];
                
                if (cmd && slot) {
                    char cmdChar = cmd[0];
                    char slotChar = slot[0];
                    
                    Serial.printf("Executing command: %c for slot %c\n", cmdChar, slotChar);
                    
                    previousSlot = slotChar;
                    
                    switch(cmdChar) {
                        case '1': { // Charge
                            if (battNumForSlot(slotChar) > 0) {
                                updateSlotRelays(slotChar, '1');
                                // Log to sheets
                                char sheetId[50];
                                float v = 0, c = 0, cap = 0;
                                if (slotChar == 'A') { strcpy(sheetId, SHEET_ID_A); v = voltageA; c = currentA; cap = capacityA; }
                                else if (slotChar == 'B') { strcpy(sheetId, SHEET_ID_B); v = voltageB; c = currentB; cap = capacityB; }
                                else if (slotChar == 'C') { strcpy(sheetId, SHEET_ID_C); v = voltageC; c = currentC; cap = capacityC; }
                                else if (slotChar == 'D') { strcpy(sheetId, SHEET_ID_D); v = voltageD; c = currentD; cap = capacityD; }
                                logSlotToSheet(sheetId, "Charging", battNumForSlot(slotChar), 0, v, c, cap, false, "Charge");
                            }
                            break;
                        }
                            
                        case '2': { // Discharge
                            if (battNumForSlot(slotChar) > 0) {
                                resetCapacity(slotChar);
                                startDischargeWithDelay(slotChar);
                            }
                            break;
                        }
                            
                        case '3': { // Cycle test
                            if (battNumForSlot(slotChar) > 0) {
                                int cycles = doc["cycles"] | 3;
                                pendingCycleValue = cycles;
                                cycleSlot = slotChar;
                                switch (slotChar) {
                                    case 'A': cycleTargetA = cycles; cycleCountA = 0; resetCapacity('A'); break;
                                    case 'B': cycleTargetB = cycles; cycleCountB = 0; resetCapacity('B'); break;
                                    case 'C': cycleTargetC = cycles; cycleCountC = 0; resetCapacity('C'); break;
                                    case 'D': cycleTargetD = cycles; cycleCountD = 0; resetCapacity('D'); break;
                                }
                                startCycle(slotChar);
                            }
                            break;
                        }
                            
                        case '4': { 
                            int battNum = doc["battery_number"] | 0;
                            if (battNum > 0) {
                                saveBattNum(slotChar, battNum);
                            }
                            break;
                        }
                            
                        case '0': {
                            stopOperation(slotChar, true);
                            break;
                        }
                    }
                    client.println("{\"type\":\"ack\",\"command\":\"" + String(cmdChar) + "\",\"slot\":\"" + String(slotChar) + "\"}");
                }
            } else {
                Serial.println("Failed to parse JSON command");
            }
        }
    }
}

void setup() {
  Serial.begin(9600);
  Serial.println("System Starting...");

  prefs.begin("battNums", false);
  prefs.putInt("A_num", 0);
  prefs.putInt("B_num", 0);
  prefs.putInt("C_num", 0);
  prefs.putInt("D_num", 0);

  battNumA = 0;
  battNumB = 0;
  battNumC = 0;
  battNumD = 0;

  Wire.begin();
  lcd.init();
  lcd.backlight();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  connectToWiFi();

    if (WiFi.status() == WL_CONNECTED) {
    startTCPServer();
  }
  prefs.begin("cts", false);
  battNumA = prefs.getInt("A_num", 0);
  battNumB = prefs.getInt("B_num", 0);
  battNumC = prefs.getInt("C_num", 0);
  battNumD = prefs.getInt("D_num", 0);

  Acycle = Bcycle = Ccycle = Dcycle = false;

  showHome();

  pinMode(AMS, OUTPUT); digitalWrite(AMS, HIGH);
  pinMode(ACD, OUTPUT); digitalWrite(ACD, HIGH);
  pinMode(BMS, OUTPUT); digitalWrite(BMS, HIGH);
  pinMode(BCD, OUTPUT); digitalWrite(BCD, HIGH);
  pinMode(CMS, OUTPUT); digitalWrite(CMS, HIGH);
  pinMode(CCD, OUTPUT); digitalWrite(CCD, HIGH);
  pinMode(DMS, OUTPUT); digitalWrite(DMS, HIGH);
  pinMode(DCD, OUTPUT); digitalWrite(DCD, HIGH);

  setupSensor(inaA, "A");
  setupSensor(inaB, "B");
  setupSensor(inaC, "C");
  setupSensor(inaD, "D");

  Serial.println("Sensors initialized (A–D active).");
  Serial.print("Loaded batt numbers -- A: "); Serial.print(battNumA);
  Serial.print(" B: "); Serial.print(battNumB);
  Serial.print(" C: "); Serial.print(battNumC);
  Serial.print(" D: "); Serial.println(battNumD);

  configTime(0, 3600, "pool.ntp.org"); 

  Serial.println("Initializing GSheet client...");
  GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);

  Serial.println("Requesting token...");
  unsigned long start = millis();
  while (!GSheet.ready() && millis() - start < 10000) {
    Serial.print(".");
    delay(500);
  }

  if (GSheet.ready()) {
    Serial.println("\nGSheet Ready!");
    gsheetReadyOnce = true;
  } else {
    Serial.println("\nGSheet not ready after timeout. Will retry on logs.");
    gsheetReadyOnce = false;
  }

  Serial.println("Setup complete.");
}

void calculateSimpsonCapacity(char slot, float current, unsigned long currentTime) {
  SimpsonSample* samples;
  int* sampleCount;
  float* capacity;
  bool* tracking;

  switch (slot) {
    case 'A': 
      samples = samplesA;
      sampleCount = &sampleCountA_cum;
      capacity = &capacityA;
      tracking = &capacityTrackingA;
      break;
    case 'B':
      samples = samplesB;
      sampleCount = &sampleCountB_cum;
      capacity = &capacityB;
      tracking = &capacityTrackingB;
      break;
    case 'C':
      samples = samplesC;
      sampleCount = &sampleCountC_cum;
      capacity = &capacityC;
      tracking = &capacityTrackingC;
      break;
    case 'D':
      samples = samplesD;
      sampleCount = &sampleCountD_cum;
      capacity = &capacityD;
      tracking = &capacityTrackingD;
      break;
    default:
      return;
  }
  
  bool isActive = false;
  switch (slot) {
    case 'A': isActive = (Acharge || Adischarge); break;
    case 'B': isActive = (Bcharge || Bdischarge); break;
    case 'C': isActive = (Ccharge || Cdischarge); break;
    case 'D': isActive = (Dcharge || Ddischarge); break;
  }
  
  if (isActive && !*tracking) {
    *tracking = true;
    *sampleCount = 0;
    *capacity = 0;
    Serial.print("Slot "); Serial.print(slot); 
    Serial.println(" Cumulative capacity tracking started");
  }
  
  if (!isActive && *tracking) {
    *tracking = false;
    *capacity = calculateCumulativeSimpson(samples, *sampleCount);
    Serial.print("Slot "); Serial.print(slot); 
    Serial.print(" Cumulative tracking ended. Total: "); Serial.print(*capacity, 4); Serial.println(" mAh");
  }
  
  if (!*tracking) return;
  if (*sampleCount < MAX_SAMPLES) {
    samples[*sampleCount].time = currentTime;
    samples[*sampleCount].current = abs(current);
    (*sampleCount)++;
    
    *capacity = calculateCumulativeSimpson(samples, *sampleCount);
    
    if (*sampleCount % 5 == 0) {
      Serial.print("Slot "); Serial.print(slot);
      Serial.print(" Cumulative capacity: "); Serial.print(*capacity, 4);
      Serial.print(" mAh (samples: "); Serial.print(*sampleCount);
      Serial.println(")");
    }
  } else {
    Serial.print("Slot "); Serial.print(slot);
    Serial.println(" WARNING: Sample buffer full!");
  }
}

void resetCapacity(char slot) {
  switch (slot) {
    case 'A': 
      capacityA = 0; 
      sampleCountA_cum = 0;
      capacityTrackingA = false;
      break;
    case 'B': 
      capacityB = 0; 
      sampleCountB_cum = 0;
      capacityTrackingB = false;
      break;
    case 'C': 
      capacityC = 0; 
      sampleCountC_cum = 0;
      capacityTrackingC = false;
      break;
    case 'D': 
      capacityD = 0; 
      sampleCountD_cum = 0;
      capacityTrackingD = false;
      break;
  }
}
void resetSlotBooleans(char slot) {
  switch (slot) {
    case 'A':
      Acharge = false;
      Adischarge = false;
      Acycle = false;
      resetCapacity('A');
      break;
    case 'B':
      Bcharge = false;
      Bdischarge = false;
      Bcycle = false;
      resetCapacity('B');
      break;
    case 'C':
      Ccharge = false;
      Cdischarge = false;
      Ccycle = false;
      resetCapacity('C');
      break;
    case 'D':
      Dcharge = false;
      Ddischarge = false;
      Dcycle = false;
      resetCapacity('D');
      break;
  }
}

void loop() {
  char key = keypad.getKey();
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  checkTCPClient();
  handleTCPCommands();

  if (millis() - previousStatusTime >= statusInterval) {
    previousStatusTime = millis();
    Serial.print("Wi-Fi: ");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected");
    } else {
      Serial.println("Not Connected");
    }

    if (previousSlot == '\0' && !inSettingScreen && !inStopPrompt && !stopMessageActive && !inBattNumberInput && !inCycleInput && !confirmingCycle) {
      lcd.setCursor(0,3);
      if (WiFi.status() == WL_CONNECTED) lcd.print("  Wi-Fi: Connected  ");
      else lcd.print("Wi-Fi: Not Connected");
    }
  }

  if (now - prevA >= interval) { 
    prevA = now; 
    readSensor(inaA, 'A'); 
  }
  if (now - prevB >= interval) { 
    prevB = now; 
    readSensor(inaB, 'B'); 
  }
  if (now - prevC >= interval) { 
    prevC = now; 
    readSensor(inaC, 'C'); 
  }
  if (now - prevD >= interval) { 
    prevD = now; 
    readSensor(inaD, 'D'); 
  }
  if (stopMessageActive) {
    if (millis() - stopMessageStart >= stopMessageDuration) {
        stopMessageActive = false;

        switch (previousSlot) {
          case 'A': readSensor(inaA, 'A'); break;
          case 'B': readSensor(inaB, 'B'); break;
          case 'C': readSensor(inaC, 'C'); break;
          case 'D': readSensor(inaD, 'D'); break;
        }

        showSlot(previousSlot);
    }
  }
  processDeferredDischarge();

  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected) Serial.println("WiFi LOST!");
    wasConnected = false;
  } else {
    if (!wasConnected) Serial.println("WiFi Connected!");
    wasConnected = true;
  }

  if (!gsheetReadyOnce && (now % 30000UL) < 200) {
    if (GSheet.ready()) {
      Serial.println("GSheet Ready!");
      gsheetReadyOnce = true;
    }
  }
  if (AdischargePending && battNumA > 0) {
    logSlotToSheet(SHEET_ID_A, statusA, battNumA, elapsedA, voltageA, currentA, capacityA, Acycle, Adischarge ? "Discharge" : (Acharge ? "Charge" : "None"));
    Serial.println("Force Log: Slot A");
    AdischargePending = false;
  }
  if (BdischargePending && battNumB > 0) {
    logSlotToSheet(SHEET_ID_B, statusB, battNumB, elapsedB, voltageB, currentB, capacityB, Bcycle, Bdischarge ? "Discharge" : (Bcharge ? "Charge" : "None"));
    Serial.println("Force Log: Slot B");
    BdischargePending = false;
  }
  if (CdischargePending && battNumC > 0) {
    logSlotToSheet(SHEET_ID_C, statusC, battNumC, elapsedC, voltageC, currentC, capacityC, Ccycle, Cdischarge ? "Discharge" : (Ccharge ? "Charge" : "None"));
    Serial.println("Force Log: Slot C");
    CdischargePending = false;
  }
  if (DdischargePending && battNumD > 0) {
    logSlotToSheet(SHEET_ID_D, statusD, battNumD, elapsedD, voltageD, currentD, capacityD, Dcycle, Ddischarge ? "Discharge" : (Dcharge ? "Charge" : "None"));
    Serial.println("Force Log: Slot D");
    DdischargePending = false;
  }
  bool Aactive = (battNumA > 0) && (Acharge || Adischarge || Acycle);
  
  static bool lastAactive = false;
  if (Aactive && !lastAactive) {
    elapsedA = 0;
    lastLogA = now;
  }
  lastAactive = Aactive;

  if (Aactive && (now - lastLogA >= logInterval)) {
    lastLogA = now;

    bool present = (voltageA > 2.0);
    if (!present) elapsedA = 0;

    if (GSheet.ready()) {
      const char* opMode = "None";
      if (Acycle) {
        opMode = Acharge ? "Charge" : (Adischarge ? "Discharge" : "Cycle");
      } else if (Acharge) {
        opMode = "Charge";
      } else if (Adischarge) {
        opMode = "Discharge";
      }
      
      logSlotToSheet(SHEET_ID_A, statusA, battNumA, elapsedA, voltageA, currentA, capacityA, Acycle, opMode);
    }

    elapsedA += 60;
  } else if (!Aactive) {
    elapsedA = 0;
  }
  bool Bactive = (battNumB > 0) && (Bcharge || Bdischarge || Bcycle);
  
  static bool lastBactive = false;
  if (Bactive && !lastBactive) {
    elapsedB = 0;
    lastLogB = now;
  }
  lastBactive = Bactive;

  if (Bactive && (now - lastLogB >= logInterval)) {
    lastLogB = now;

    bool present = (voltageB > 2.0);
    if (!present) elapsedB = 0;

    if (GSheet.ready()) {
      const char* opMode = "None";
      if (Bcycle) {
        opMode = Bcharge ? "Charge" : (Bdischarge ? "Discharge" : "Cycle");
      } else if (Bcharge) {
        opMode = "Charge";
      } else if (Bdischarge) {
        opMode = "Discharge";
      }
      
      logSlotToSheet(SHEET_ID_B, statusB, battNumB, elapsedB, voltageB, currentB, capacityB, Bcycle, opMode);
    }

    elapsedB += 60;
  } else if (!Bactive) {
    elapsedB = 0;
  }
  bool Cactive = (battNumC > 0) && (Ccharge || Cdischarge || Ccycle);
  
  static bool lastCactive = false;
  if (Cactive && !lastCactive) {
    elapsedC = 0;
    lastLogC = now;
  }
  lastCactive = Cactive;

  if (Cactive && (now - lastLogC >= logInterval)) {
    lastLogC = now;

    bool present = (voltageC > 2.0);
    if (!present) elapsedC = 0;

    if (GSheet.ready()) {
      const char* opMode = "None";
      if (Ccycle) {
        opMode = Ccharge ? "Charge" : (Cdischarge ? "Discharge" : "Cycle");
      } else if (Ccharge) {
        opMode = "Charge";
      } else if (Cdischarge) {
        opMode = "Discharge";
      }
      
      logSlotToSheet(SHEET_ID_C, statusC, battNumC, elapsedC, voltageC, currentC, capacityC, Ccycle, opMode);
    }

    elapsedC += 60;
  } else if (!Cactive) {
    elapsedC = 0;
  }
  bool Dactive = (battNumD > 0) && (Dcharge || Ddischarge || Dcycle);
  
  static bool lastDactive = false;
  if (Dactive && !lastDactive) {
    elapsedD = 0;
    lastLogD = now;
  }
  lastDactive = Dactive;

  if (Dactive && (now - lastLogD >= logInterval)) {
    lastLogD = now;

    bool present = (voltageD > 2.0);
    if (!present) elapsedD = 0;

    if (GSheet.ready()) {
      const char* opMode = "None";
      if (Dcycle) {
        opMode = Dcharge ? "Charge" : (Ddischarge ? "Discharge" : "Cycle");
      } else if (Dcharge) {
        opMode = "Charge";
      } else if (Ddischarge) {
        opMode = "Discharge";
      }
      
      logSlotToSheet(SHEET_ID_D, statusD, battNumD, elapsedD, voltageD, currentD, capacityD, Dcycle, opMode);
    }

    elapsedD += 60;
  } else if (!Dactive) {
    elapsedD = 0;
  }
  if (!key) return;

  Serial.print("Key pressed: "); Serial.println(key);

  if (inBattNumberInput) {
    handleBattNumberInputKey(key);
    return;
  }

  if (inCycleInput) {
    handleCycleInputKey(key);
    return;
  }

  if (confirmingCycle) {
    if (key == '#') {
      int value = pendingCycleValue;
    
      switch (cycleSlot) {
        case 'A': 
          cycleTargetA = value; 
          cycleCountA = 0; 
          resetCapacity('A');
          Serial.print("Slot A cycle target set to: "); Serial.println(value);
          break;
        case 'B': 
          cycleTargetB = value; 
          cycleCountB = 0;
          resetCapacity('B');
          Serial.print("Slot B cycle target set to: "); Serial.println(value);
          break;
        case 'C': 
          cycleTargetC = value; 
          cycleCountC = 0;
          resetCapacity('C');
          Serial.print("Slot C cycle target set to: "); Serial.println(value);
          break;
        case 'D': 
          cycleTargetD = value; 
          cycleCountD = 0;
          resetCapacity('D');
          Serial.print("Slot D cycle target set to: "); Serial.println(value);
          break;
      }
      
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("Starting Cycle Test");
      lcd.setCursor(0,2);
      lcd.print("Slot ");
      lcd.print(cycleSlot);
      lcd.print(" - ");
      lcd.print(value);
      lcd.print(" cycles");
      lcd.setCursor(0,3);
      lcd.print("Ends on CHARGE");
      delay(2000);
      
      startCycle(cycleSlot);

      char sheetId[50]; 
      float v=0, c=0, cap=0; 
      int battNum = 0;
      
      if (cycleSlot=='A') { 
        strcpy(sheetId,SHEET_ID_A); 
        v=voltageA; 
        c=currentA; 
        cap=capacityA;
        elapsedA=0;
        battNum = battNumA;
      }
      else if (cycleSlot=='B') { 
        strcpy(sheetId,SHEET_ID_B); 
        v=voltageB; 
        c=currentB; 
        cap=capacityB;
        elapsedB=0;
        battNum = battNumB;
      }
      else if (cycleSlot=='C') { 
        strcpy(sheetId,SHEET_ID_C); 
        v=voltageC; 
        c=currentC; 
        cap=capacityC;
        elapsedC=0;
        battNum = battNumC;
      }
      else if (cycleSlot=='D') { 
        strcpy(sheetId,SHEET_ID_D); 
        v=voltageD; 
        c=currentD; 
        cap=capacityD;
        elapsedD=0;
        battNum = battNumD;
      }
      
      if (battNum > 0) {
        logSlotToSheet(sheetId, "Cycle Start", battNum, 0, v, c, cap, true, "Charge");
      }
      
      confirmingCycle = false;
      cycleSlot = '\0';
      cycleBuffer = "";
      pendingCycleValue = 0;

      if (previousSlot != '\0') {
        showSlot(previousSlot);
      }
      return;
    }
    else if (key == '*') {

      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("Cycle test CANCELLED");
      delay(1500);
      
      confirmingCycle = false;
      cycleSlot = '\0';
      cycleBuffer = "";
      pendingCycleValue = 0;
      
      showSlotSetting(previousSlot);
      inSettingScreen = true;
      return;
    }
    return;
  }
  if (inStopPrompt) {
    if (key == '0') { 
      stopOperation(previousSlot, true); 
      return;
    } else if (key == '5') {
      inStopPrompt = false;
      stopMessageActive = false;
      refreshSlotDisplay();
      return;
    }
    return;
  }

  if (inSettingScreen) {
    if (key == '1') {
      if (battNumForSlot(previousSlot) > 0) {
       
        updateSlotRelays(previousSlot, '1');
        
        char sheetId[50];
        float v = 0, c = 0, cap = 0;
        String status = "Charging";
        if (previousSlot == 'A') { strcpy(sheetId, SHEET_ID_A); v = voltageA; c = currentA; cap = capacityA; elapsedA = 0; }
        else if (previousSlot == 'B') { strcpy(sheetId, SHEET_ID_B); v = voltageB; c = currentB; cap = capacityB; elapsedB = 0; }
        else if (previousSlot == 'C') { strcpy(sheetId, SHEET_ID_C); v = voltageC; c = currentC; cap = capacityC; elapsedC = 0; }
        else if (previousSlot == 'D') { strcpy(sheetId, SHEET_ID_D); v = voltageD; c = currentD; cap = capacityD; elapsedD = 0; }
        logSlotToSheet(sheetId, status, battNumForSlot(previousSlot), 0, v, c, cap, false, "Charge");
        
        inSettingScreen = false;
        refreshSlotDisplay();
      } else {
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print(" Set batt# first!");
        delay(1500);
        showSlotSetting(previousSlot);
      }
      return;
    }
    else if (key == '2') { 
      if (battNumForSlot(previousSlot) > 0) {
        resetCapacity(previousSlot);
        startDischargeWithDelay(previousSlot);
        
        char sheetId[50];
        float v = 0, c = 0, cap = 0;
        String status = "Discharging";
        if (previousSlot == 'A') { strcpy(sheetId, SHEET_ID_A); v = voltageA; c = currentA; cap = capacityA; }
        else if (previousSlot == 'B') { strcpy(sheetId, SHEET_ID_B); v = voltageB; c = currentB; cap = capacityB; }
        else if (previousSlot == 'C') { strcpy(sheetId, SHEET_ID_C); v = voltageC; c = currentC; cap = capacityC; }
        else if (previousSlot == 'D') { strcpy(sheetId, SHEET_ID_D); v = voltageD; c = currentD; cap = capacityD; }
        logSlotToSheet(sheetId, status, battNumForSlot(previousSlot), 0, v, c, cap, false, "Discharge");
        
        inSettingScreen = false;
      } else {
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print(" Set batt# first!");
        delay(1500);
        showSlotSetting(previousSlot);
      }
      return;
    }
    else if (key == '3') { 
      if (battNumForSlot(previousSlot) > 0) {
        Serial.println("Cycle option selected - showing input screen");
        showCycleInput(previousSlot);
        inSettingScreen = false;
      } else {
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print(" Set batt# first!");
        delay(1500);
        showSlotSetting(previousSlot);
      }
      return;
    }
    else if (key == '4') { 
      showBattNumberInput(previousSlot);
      inBattNumberInput = true;
      inSettingScreen = false;
      return;
    }
    else if (key == '5') return;
  }
  switch (key) {
    case '0': 
      if (previousSlot != '\0') {
 
        bool isActive = false;
        switch (previousSlot) {
          case 'A': isActive = (Acharge || Adischarge || Acycle); break;
          case 'B': isActive = (Bcharge || Bdischarge || Bcycle); break;
          case 'C': isActive = (Ccharge || Cdischarge || Ccycle); break;
          case 'D': isActive = (Dcharge || Ddischarge || Dcycle); break;
        }
        
        if (isActive) {
          showForceStopPrompt(previousSlot);
          inStopPrompt = true;
        } else {
          
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("No active operation");
          lcd.setCursor(0,2);
          lcd.print("in Slot ");
          lcd.print(previousSlot);
          delay(1500);
          showSlot(previousSlot);
        }
      }
      break;
    case '#':
      showHome();
      previousSlot = '\0';
      inSettingScreen = false;
      inStopPrompt = false;
      inBattNumberInput = false;
      inCycleInput = false;
      confirmingCycle = false;
      break;
    case 'A': showSlot('A'); previousSlot='A'; break;
    case 'B': showSlot('B'); previousSlot='B'; break;
    case 'C': showSlot('C'); previousSlot='C'; break;
    case 'D': showSlot('D'); previousSlot='D'; break;
    case '*': 
      if (previousSlot != '\0') {
        showSlotSetting(previousSlot); 
        inSettingScreen=true;
      }
      break;
  }
}

int battNumForSlot(char slot) {
  switch (slot) {
    case 'A': return battNumA;
    case 'B': return battNumB;
    case 'C': return battNumC;
    case 'D': return battNumD;
    default: return 0;
  }
}

void startDischargeWithDelay(char slot) {
  switch (slot) {
    case 'A':
      elapsedA = 0;
      AdischargePending = true;
      AdischargeWait = true;
      AdischargeStart = millis();
      break;
    case 'B':
      elapsedB = 0;
      BdischargePending = true;
      BdischargeWait = true;
      BdischargeStart = millis();
      break;
    case 'C':
      elapsedC = 0;
      CdischargePending = true;
      CdischargeWait = true;
      CdischargeStart = millis();
      break;
    case 'D':
      elapsedD = 0;
      DdischargePending = true;
      DdischargeWait = true;
      DdischargeStart = millis();
      break;
  }
}

void startCycle(char slot) {
  switch (slot) {
    case 'A':
      Acycle = true;
      cycleCountA = 0;
      resetCapacity('A');

      Acharge = true;
      digitalWrite(AMS, LOW);
      digitalWrite(ACD, HIGH);
      Serial.print("Slot A cycle started - Charging (");
      Serial.print(cycleTargetA);
      Serial.println(" cycles total, will end on charge)");
      break;
    case 'B':
      Bcycle = true;
      cycleCountB = 0;
      resetCapacity('B');
      Bcharge = true;
      digitalWrite(BMS, LOW);
      digitalWrite(BCD, HIGH);
      Serial.print("Slot B cycle started - Charging (");
      Serial.print(cycleTargetB);
      Serial.println(" cycles total, will end on charge)");
      break;
    case 'C':
      Ccycle = true;
      cycleCountC = 0;
      resetCapacity('C');
      Ccharge = true;
      digitalWrite(CMS, LOW);
      digitalWrite(CCD, HIGH);
      Serial.print("Slot C cycle started - Charging (");
      Serial.print(cycleTargetC);
      Serial.println(" cycles total, will end on charge)");
      break;
    case 'D':
      Dcycle = true;
      cycleCountD = 0;
      resetCapacity('D');
      Dcharge = true;
      digitalWrite(DMS, LOW);
      digitalWrite(DCD, HIGH);
      Serial.print("Slot D cycle started - Charging (");
      Serial.print(cycleTargetD);
      Serial.println(" cycles total, will end on charge)");
      break;
  }
}

void setupSensor(INA226 &sensor, const char* label) {
  if (!sensor.begin()) {
    Serial.print("Sensor "); Serial.print(label); Serial.println(" not detected! (check wiring)");
    return;
  }

  sensor.setAverage(INA226_1_SAMPLE);
  sensor.setBusVoltageConversionTime(INA226_1100_us);
  sensor.setShuntVoltageConversionTime(INA226_1100_us);
  sensor.setModeShuntBusContinuous();

  sensor.setMaxCurrentShunt(5.0, 0.01);

  Serial.print("Sensor "); Serial.print(label);
  Serial.println(" initialized.");
}

void readSensor(INA226 &sensor, char label) {
  float v, c;

  switch(label) {
    case 'A':
      if (Acharge) v = sensor.getBusVoltage() * voltageCalAcharge;
      else if (Adischarge) v = sensor.getBusVoltage() * voltageCalAdischarge;
      else v = sensor.getBusVoltage() * voltageCalA;
      c = sensor.getCurrent() * currentCalA;
      break;

    case 'B':
      if (Bcharge) v = sensor.getBusVoltage() * voltageCalBcharge;
      else if (Bdischarge) v = sensor.getBusVoltage() * voltageCalBdischarge;
      else v = sensor.getBusVoltage() * voltageCalB;
      c = sensor.getCurrent() * currentCalB;
      break;

    case 'C':
      if (Ccharge) v = sensor.getBusVoltage() * voltageCalCcharge;
      else if (Cdischarge) v = sensor.getBusVoltage() * voltageCalCdischarge;
      else v = sensor.getBusVoltage() * voltageCalC;
      c = sensor.getCurrent() * currentCalC;
      break;

    case 'D':
      if (Dcharge) v = sensor.getBusVoltage() * voltageCalDcharge;
      else if (Ddischarge) v = sensor.getBusVoltage() * voltageCalDdischarge;
      else v = sensor.getBusVoltage() * voltageCalD;
      c = sensor.getCurrent() * currentCalD;
      break;

    default:
      v = 0; c = 0;
      break;
  }

  int bn = 0;
  if (label == 'A') bn = battNumA;
  else if (label == 'B') bn = battNumB;
  else if (label == 'C') bn = battNumC;
  else if (label == 'D') bn = battNumD;

  if (v < 2.0) v = 0.0;
  if (c > -5.0 && c < 5.0) c = 0.0;

  String status;
  if (v == 0.0 && c == 0.0) status = "No Battery";
  else if (v > 0.0 && c == 0.0) status = "Standby";
  else if (v > 0.0 && c > 0.0) status = "Discharging";
  else if (v > 0.0 && c < 0.0) status = "Charging";
  else status = "Unknown";

  Serial.print("Slot "); Serial.print(label);
  Serial.print(" | Status = "); Serial.print(status);
  Serial.print(" | Batt No = "); Serial.print(bn);
  Serial.print(" | Voltage = "); Serial.print(v, 2); Serial.print(" V");
  Serial.print(" | Current = "); Serial.print(c, 0); Serial.println(" mA");

  switch (label) {
    case 'A': voltageA=v; currentA=abs(c); statusA=status; checkVoltageLimit('A'); break;
    case 'B': voltageB=v; currentB=abs(c); statusB=status; checkVoltageLimit('B'); break;
    case 'C': voltageC=v; currentC=abs(c); statusC=status; checkVoltageLimit('C'); break;
    case 'D': voltageD=v; currentD=abs(c); statusD=status; checkVoltageLimit('D'); break;
  }

  calculateSimpsonCapacity(label, c, millis());  

  if (clientConnected) {
    sendSensorDataOverTCP(label);
    
    static int sampleCounter = 0;
    sampleCounter++;
    if (sampleCounter % 5 == 0) {
      sendSimpsonDataOverTCP(label);
    }
  }

  if (previousSlot == label && !inSettingScreen && !inStopPrompt && !stopMessageActive && !inBattNumberInput && !inCycleInput && !confirmingCycle) {
    showSlot(label);
  }
}

void checkVoltageLimit(char slot) {
  switch (slot) {
    case 'A': {
      if (Acharge && (voltageA >= CHARGE_STOP_V || voltageA < 1 || currentA <= 100)) {

        if (Acycle && battNumA > 0) {
   
          logSlotToSheet(SHEET_ID_A, "Charge Complete", battNumA, elapsedA, voltageA, currentA, capacityA, true, "Charge");
          Serial.print("Slot A charge complete - Cumulative Capacity: "); Serial.print(capacityA, 4); Serial.println(" mAh");
          
          logSlotToSheet(SHEET_ID_A, "Simpson Capacity", battNumA, elapsedA, voltageA, currentA, capacityA, true, "Pre-Discharge");
          Serial.println("Slot A - Logged Simpson capacity before discharge");
        }
        
        Acharge = false;
        if (Acycle) {
          if (cycleCountA >= cycleTargetA - 1) {
            Acycle = false;
            resetSlotRelays('A');
            Serial.print("Slot A cycle COMPLETE - Stopped after charge cycle ");
            Serial.println(cycleTargetA);
            
            logSlotToSheet(SHEET_ID_A, "Cycle Complete", battNumA, elapsedA, voltageA, currentA, capacityA, false, "Complete");
            
            if (previousSlot == 'A') {
              lcd.clear();
              lcd.setCursor(0,1);
              lcd.print("Slot A Cycle Done!");
              lcd.setCursor(0,2);
              lcd.print(cycleTargetA);
              lcd.print(" cycles completed");
              lcd.setCursor(0,3);
              lcd.print("Cap: "); lcd.print(capacityA, 1); lcd.print(" mAh");
              delay(3000);
              showSlot('A');
            }
          } else {
            Adischarge = true;
            digitalWrite(AMS, LOW);
            digitalWrite(ACD, LOW);
            Serial.print("Slot A cycle progress: Charge ");
            Serial.print(cycleCountA + 1);
            Serial.print("/");
            Serial.print(cycleTargetA);
            Serial.println(" complete - Starting Discharge");
            
            logSlotToSheet(SHEET_ID_A, "Cycle Phase Change", battNumA, elapsedA, voltageA, currentA, capacityA, true, "Discharge");
          }
        } else {
          resetSlotRelays('A');
        }
        Serial.println("Slot A charge completed.");
      }
      if (Adischarge && (voltageA <= DISCHARGE_STOP_V || (voltageA < 1 && currentA == 0.0))) {
        if (battNumA > 0) {
          logSlotToSheet(SHEET_ID_A, "Discharge Complete", battNumA, elapsedA, voltageA, currentA, capacityA, Acycle, "Complete");
          Serial.print("Slot A discharge complete - Cumulative Capacity: "); Serial.print(capacityA, 4); Serial.println(" mAh");
          
          logSlotToSheet(SHEET_ID_A, "Simpson Capacity", battNumA, elapsedA, voltageA, currentA, capacityA, true, "Pre-Charge");
          Serial.println("Slot A - Logged Simpson capacity before charge");
        }
        
        Adischarge = false;
        AdischargePending = false;
        if (Acycle) {
          cycleCountA++;
          
          Acharge = true;
          digitalWrite(AMS, LOW);
          digitalWrite(ACD, HIGH);
          Serial.print("Slot A discharge ");
          Serial.print(cycleCountA);
          Serial.print("/");
          Serial.print(cycleTargetA);
          Serial.println(" complete - Starting Charge");
          
          logSlotToSheet(SHEET_ID_A, "Cycle Phase Change", battNumA, elapsedA, voltageA, currentA, capacityA, true, "Charge");
        } else {
          resetSlotRelays('A');
        }
        Serial.println("Slot A discharge completed.");
      }
    } break;

    case 'B': {
      if (Bcharge && (voltageB >= CHARGE_STOP_V || voltageB < 1 || currentB <= 100)) {
        if (Bcycle && battNumB > 0) {
          logSlotToSheet(SHEET_ID_B, "Charge Complete", battNumB, elapsedB, voltageB, currentB, capacityB, true, "Charge");
          Serial.print("Slot B charge complete - Cumulative Capacity: "); Serial.print(capacityB, 4); Serial.println(" mAh");
          
          logSlotToSheet(SHEET_ID_B, "Simpson Capacity", battNumB, elapsedB, voltageB, currentB, capacityB, true, "Pre-Discharge");
          Serial.println("Slot B - Logged Simpson capacity before discharge");
        }
        
        Bcharge = false;
        if (Bcycle) {
          if (cycleCountB >= cycleTargetB - 1) {
            Bcycle = false;
            resetSlotRelays('B');
            Serial.print("Slot B cycle COMPLETE - Stopped after charge cycle ");
            Serial.println(cycleTargetB);
            
            logSlotToSheet(SHEET_ID_B, "Cycle Complete", battNumB, elapsedB, voltageB, currentB, capacityB, false, "Complete");
            
            if (previousSlot == 'B') {
              lcd.clear();
              lcd.setCursor(0,1);
              lcd.print("Slot B Cycle Done!");
              lcd.setCursor(0,2);
              lcd.print(cycleTargetB);
              lcd.print(" cycles completed");
              lcd.setCursor(0,3);
              lcd.print("Cap: "); lcd.print(capacityB, 1); lcd.print(" mAh");
              delay(3000);
              showSlot('B');
            }
          } else {
            Bdischarge = true;
            digitalWrite(BMS, LOW);
            digitalWrite(BCD, LOW);
            Serial.print("Slot B cycle progress: Charge ");
            Serial.print(cycleCountB + 1);
            Serial.print("/");
            Serial.print(cycleTargetB);
            Serial.println(" complete - Starting Discharge");
            
            logSlotToSheet(SHEET_ID_B, "Cycle Phase Change", battNumB, elapsedB, voltageB, currentB, capacityB, true, "Discharge");
          }
        } else {
          resetSlotRelays('B');
        }
        Serial.println("Slot B charge completed.");
      }
      if (Bdischarge && (voltageB <= DISCHARGE_STOP_V || (voltageB < 1 && currentB == 0.0))) {
        if (battNumB > 0) {
          logSlotToSheet(SHEET_ID_B, "Discharge Complete", battNumB, elapsedB, voltageB, currentB, capacityB, Bcycle, "Complete");
          Serial.print("Slot B discharge complete - Cumulative Capacity: "); Serial.print(capacityB, 4); Serial.println(" mAh");
          
          logSlotToSheet(SHEET_ID_B, "Simpson Capacity", battNumB, elapsedB, voltageB, currentB, capacityB, true, "Pre-Charge");
          Serial.println("Slot B - Logged Simpson capacity before charge");
        }
        
        Bdischarge = false;
        BdischargePending = false;
        if (Bcycle) {
          cycleCountB++;
          Bcharge = true;
          digitalWrite(BMS, LOW);
          digitalWrite(BCD, HIGH);
          Serial.print("Slot B discharge ");
          Serial.print(cycleCountB);
          Serial.print("/");
          Serial.print(cycleTargetB);
          Serial.println(" complete - Starting Charge");
          
          logSlotToSheet(SHEET_ID_B, "Cycle Phase Change", battNumB, elapsedB, voltageB, currentB, capacityB, true, "Charge");
        } else {
          resetSlotRelays('B');
        }
        Serial.println("Slot B discharge completed.");
      }
    } break;

    case 'C': {
      if (Ccharge && (voltageC >= CHARGE_STOP_V || voltageC < 1 || currentC <= 100)) {

        if (Ccycle && battNumC > 0) {
          logSlotToSheet(SHEET_ID_C, "Charge Complete", battNumC, elapsedC, voltageC, currentC, capacityC, true, "Charge");
          Serial.print("Slot C charge complete - Cumulative Capacity: "); Serial.print(capacityC, 4); Serial.println(" mAh");
          
          logSlotToSheet(SHEET_ID_C, "Simpson Capacity", battNumC, elapsedC, voltageC, currentC, capacityC, true, "Pre-Discharge");
          Serial.println("Slot C - Logged Simpson capacity before discharge");
        }
        
        Ccharge = false;
        if (Ccycle) {
          if (cycleCountC >= cycleTargetC - 1) {
            Ccycle = false;
            resetSlotRelays('C');
            Serial.print("Slot C cycle COMPLETE - Stopped after charge cycle ");
            Serial.println(cycleTargetC);
            
            logSlotToSheet(SHEET_ID_C, "Cycle Complete", battNumC, elapsedC, voltageC, currentC, capacityC, false, "Complete");
            
            if (previousSlot == 'C') {
              lcd.clear();
              lcd.setCursor(0,1);
              lcd.print("Slot C Cycle Done!");
              lcd.setCursor(0,2);
              lcd.print(cycleTargetC);
              lcd.print(" cycles completed");
              lcd.setCursor(0,3);
              lcd.print("Cap: "); lcd.print(capacityC, 1); lcd.print(" mAh");
              delay(3000);
              showSlot('C');
            }
          } else {
            Cdischarge = true;
            digitalWrite(CMS, LOW);
            digitalWrite(CCD, LOW);
            Serial.print("Slot C cycle progress: Charge ");
            Serial.print(cycleCountC + 1);
            Serial.print("/");
            Serial.print(cycleTargetC);
            Serial.println(" complete - Starting Discharge");
            
            logSlotToSheet(SHEET_ID_C, "Cycle Phase Change", battNumC, elapsedC, voltageC, currentC, capacityC, true, "Discharge");
          }
        } else {
          resetSlotRelays('C');
        }
        Serial.println("Slot C charge completed.");
      }
      if (Cdischarge && (voltageC <= DISCHARGE_STOP_V || (voltageC < 1 && currentC == 0.0))) {

        if (battNumC > 0) {
          logSlotToSheet(SHEET_ID_C, "Discharge Complete", battNumC, elapsedC, voltageC, currentC, capacityC, Ccycle, "Complete");
          Serial.print("Slot C discharge complete - Cumulative Capacity: "); Serial.print(capacityC, 4); Serial.println(" mAh");

          logSlotToSheet(SHEET_ID_C, "Simpson Capacity", battNumC, elapsedC, voltageC, currentC, capacityC, true, "Pre-Charge");
          Serial.println("Slot C - Logged Simpson capacity before charge");
        }
        
        Cdischarge = false;
        CdischargePending = false;
        if (Ccycle) {
          cycleCountC++;
          Ccharge = true;
          digitalWrite(CMS, LOW);
          digitalWrite(CCD, HIGH);
          Serial.print("Slot C discharge ");
          Serial.print(cycleCountC);
          Serial.print("/");
          Serial.print(cycleTargetC);
          Serial.println(" complete - Starting Charge");
          
          logSlotToSheet(SHEET_ID_C, "Cycle Phase Change", battNumC, elapsedC, voltageC, currentC, capacityC, true, "Charge");
        } else {
          resetSlotRelays('C');
        }
        Serial.println("Slot C discharge completed.");
      }
    } break;

    case 'D': {
      if (Dcharge && (voltageD >= CHARGE_STOP_V || voltageD < 1 || currentD <= 100)) {

        if (Dcycle && battNumD > 0) {
          logSlotToSheet(SHEET_ID_D, "Charge Complete", battNumD, elapsedD, voltageD, currentD, capacityD, true, "Charge");
          Serial.print("Slot D charge complete - Cumulative Capacity: "); Serial.print(capacityD, 4); Serial.println(" mAh");

          logSlotToSheet(SHEET_ID_D, "Simpson Capacity", battNumD, elapsedD, voltageD, currentD, capacityD, true, "Pre-Discharge");
          Serial.println("Slot D - Logged Simpson capacity before discharge");
        }
        
        Dcharge = false;
        if (Dcycle) {
          if (cycleCountD >= cycleTargetD - 1) {
            Dcycle = false;
            resetSlotRelays('D');
            Serial.print("Slot D cycle COMPLETE - Stopped after charge cycle ");
            Serial.println(cycleTargetD);
            
            logSlotToSheet(SHEET_ID_D, "Cycle Complete", battNumD, elapsedD, voltageD, currentD, capacityD, false, "Complete");
            
            if (previousSlot == 'D') {
              lcd.clear();
              lcd.setCursor(0,1);
              lcd.print("Slot D Cycle Done!");
              lcd.setCursor(0,2);
              lcd.print(cycleTargetD);
              lcd.print(" cycles completed");
              lcd.setCursor(0,3);
              lcd.print("Cap: "); lcd.print(capacityD, 1); lcd.print(" mAh");
              delay(3000);
              showSlot('D');
            }
          } else {
            Ddischarge = true;
            digitalWrite(DMS, LOW);
            digitalWrite(DCD, LOW);
            Serial.print("Slot D cycle progress: Charge ");
            Serial.print(cycleCountD + 1);
            Serial.print("/");
            Serial.print(cycleTargetD);
            Serial.println(" complete - Starting Discharge");
            
            logSlotToSheet(SHEET_ID_D, "Cycle Phase Change", battNumD, elapsedD, voltageD, currentD, capacityD, true, "Discharge");
          }
        } else {
          resetSlotRelays('D');
        }
        Serial.println("Slot D charge completed.");
      }
      if (Ddischarge && (voltageD <= DISCHARGE_STOP_V || (voltageD < 1 && currentD == 0.0))) {

        if (battNumD > 0) {
          logSlotToSheet(SHEET_ID_D, "Discharge Complete", battNumD, elapsedD, voltageD, currentD, capacityD, Dcycle, "Complete");
          Serial.print("Slot D discharge complete - Cumulative Capacity: "); Serial.print(capacityD, 4); Serial.println(" mAh");

          logSlotToSheet(SHEET_ID_D, "Simpson Capacity", battNumD, elapsedD, voltageD, currentD, capacityD, true, "Pre-Charge");
          Serial.println("Slot D - Logged Simpson capacity before charge");
        }
        
        Ddischarge = false;
        DdischargePending = false;
        if (Dcycle) {
          cycleCountD++;
          Dcharge = true;
          digitalWrite(DMS, LOW);
          digitalWrite(DCD, HIGH);
          Serial.print("Slot D discharge ");
          Serial.print(cycleCountD);
          Serial.print("/");
          Serial.print(cycleTargetD);
          Serial.println(" complete - Starting Charge");
          
          logSlotToSheet(SHEET_ID_D, "Cycle Phase Change", battNumD, elapsedD, voltageD, currentD, capacityD, true, "Charge");
        } else {
          resetSlotRelays('D');
        }
        Serial.println("Slot D discharge completed.");
      }
    } break;
  }
}

void updateSlotRelays(char slot, char action) {
  switch (slot) {
    case 'A':
      Acharge = (action == '1');
      Adischarge = (action == '2');
      if (Acharge){digitalWrite(AMS,LOW);digitalWrite(ACD,HIGH);}
      else if(Adischarge){digitalWrite(AMS,LOW);digitalWrite(ACD,LOW);}
      break;

    case 'B':
      Bcharge = (action == '1');
      Bdischarge = (action == '2');
      if (Bcharge){digitalWrite(BMS,LOW);digitalWrite(BCD,HIGH);}
      else if(Bdischarge){digitalWrite(BMS,LOW);digitalWrite(BCD,LOW);}
      break;

    case 'C':
      Ccharge = (action == '1');
      Cdischarge = (action == '2');
      if (Ccharge){digitalWrite(CMS,LOW);digitalWrite(CCD,HIGH);}
      else if(Cdischarge){digitalWrite(CMS,LOW);digitalWrite(CCD,LOW);}
      break;

    case 'D':
      Dcharge = (action == '1');
      Ddischarge = (action == '2');
      if (Dcharge){digitalWrite(DMS,LOW);digitalWrite(DCD,HIGH);}
      else if(Ddischarge){digitalWrite(DMS,LOW);digitalWrite(DCD,LOW);}
      break;
  }
}

void resetSlotRelays(char slot) {
  switch (slot) {
    case 'A':
      digitalWrite(AMS,HIGH);digitalWrite(ACD,HIGH);
      break;
    case 'B':
      digitalWrite(BMS,HIGH);digitalWrite(BCD,HIGH);
      break;
    case 'C':
      digitalWrite(CMS,HIGH);digitalWrite(CCD,HIGH);
      break;
    case 'D':
      digitalWrite(DMS,HIGH);digitalWrite(DCD,HIGH);
      break;
  }
}

void printCentered(const char* text, int row) {
  int len = strlen(text);
  int col = (20 - len) / 2;
  if (col < 0) col = 0;
  lcd.setCursor(col, row);
  lcd.print(text);
}

void showHome() {
  lcd.clear();
  printCentered("Capacity Testing", 0);
  printCentered("Station (CTS) V1.0", 1);
  lcd.setCursor(0,3);
  if (WiFi.status() == WL_CONNECTED) lcd.print("  Wi-Fi: Connected  ");
  else lcd.print("Wi-Fi: Not Connected");
}

void showSlot(char slot) {

  inSettingScreen = false;
  inStopPrompt = false;
  stopMessageActive = false;
  inBattNumberInput = false;
  inCycleInput = false;
  confirmingCycle = false;

  lcd.clear();

  float v, c, cap; 
  String s;
  int bn = 0;
  String mode = "";
  String cycleInfo = "";

  switch (slot) {
    case 'A': 
      v = voltageA; c = currentA; s = statusA; bn = battNumA; cap = capacityA;
      if (Acharge) mode = "CHARGING";
      else if (Adischarge) mode = "DISCHARGING";
      else if (Acycle) mode = "CYCLE";
      else mode = "IDLE";
      if (Acycle) {
        cycleInfo = "Cyc: " + String(cycleCountA) + "/" + String(cycleTargetA);
      }
      break;
    case 'B': 
      v = voltageB; c = currentB; s = statusB; bn = battNumB; cap = capacityB;
      if (Bcharge) mode = "CHARGING";
      else if (Bdischarge) mode = "DISCHARGING";
      else if (Bcycle) mode = "CYCLE";
      else mode = "IDLE";
      if (Bcycle) {
        cycleInfo = "Cyc: " + String(cycleCountB) + "/" + String(cycleTargetB);
      }
      break;
    case 'C': 
      v = voltageC; c = currentC; s = statusC; bn = battNumC; cap = capacityC;
      if (Ccharge) mode = "CHARGING";
      else if (Cdischarge) mode = "DISCHARGING";
      else if (Ccycle) mode = "CYCLE";
      else mode = "IDLE";
      if (Ccycle) {
        cycleInfo = "Cyc: " + String(cycleCountC) + "/" + String(cycleTargetC);
      }
      break;
    case 'D': 
      v = voltageD; c = currentD; s = statusD; bn = battNumD; cap = capacityD;
      if (Dcharge) mode = "CHARGING";
      else if (Ddischarge) mode = "DISCHARGING";
      else if (Dcycle) mode = "CYCLE";
      else mode = "IDLE";
      if (Dcycle) {
        cycleInfo = "Cyc: " + String(cycleCountD) + "/" + String(cycleTargetD);
      }
      break;
  }

  String title = "Slot ";
  title += slot;
  if (bn > 0) {
    title += " #";
    title += bn;
  }
  printCentered(title.c_str(), 0);

  lcd.setCursor(0, 1); lcd.print("Status: "); lcd.print(s);
  lcd.setCursor(0, 2); lcd.print("Mode: "); lcd.print(mode);
  if (cycleInfo.length() > 0) {
    lcd.setCursor(10, 2); lcd.print(cycleInfo);
  }

  if (Acharge || Adischarge || Bcharge || Bdischarge || Ccharge || Cdischarge || Dcharge || Ddischarge) {
    lcd.setCursor(0, 3); lcd.print("Cap:");
    lcd.print(cap, 1);
    lcd.print("mAh V:");
    lcd.print(v, 2);
  } else {
    lcd.setCursor(0, 3); lcd.print("V:"); lcd.print(v, 2); lcd.print(" C:"); lcd.print(c, 0);
  }
}

void refreshSlotDisplay() {
  if (previousSlot != '\0') showSlot(previousSlot);
}

void showSlotSetting(char slot) {
  if (slot == '\0') return;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("SLOT ");
  lcd.print(slot);
  lcd.print(" OPTIONS");
  lcd.setCursor(0,1);
  lcd.print("1.Charge 2.Discharge");
  lcd.setCursor(0,2);
  lcd.print("3.Cycle  4.Batt No.");
  lcd.setCursor(0,3);
  lcd.print("Press * to go back");
}

void showForceStopPrompt(char slot) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("FORCE STOP Slot ");
  lcd.print(slot);
  lcd.setCursor(0,1);

  lcd.print("Current: ");
  switch (slot) {
    case 'A': 
      if (Acharge) lcd.print("CHARGING");
      else if (Adischarge) lcd.print("DISCHARGING");
      else if (Acycle) lcd.print("CYCLE");
      break;
    case 'B': 
      if (Bcharge) lcd.print("CHARGING");
      else if (Bdischarge) lcd.print("DISCHARGING");
      else if (Bcycle) lcd.print("CYCLE");
      break;
    case 'C': 
      if (Ccharge) lcd.print("CHARGING");
      else if (Cdischarge) lcd.print("DISCHARGING");
      else if (Ccycle) lcd.print("CYCLE");
      break;
    case 'D': 
      if (Dcharge) lcd.print("CHARGING");
      else if (Ddischarge) lcd.print("DISCHARGING");
      else if (Dcycle) lcd.print("CYCLE");
      break;
  }
  
  lcd.setCursor(0,2);
  lcd.print("0 = Yes, FORCE STOP");
  lcd.setCursor(0,3);
  lcd.print("5 = No, continue");
}

void showBattNumberInput(char slot) {
  if (slot == '\0') return;
  battNumberSlot = slot;
  battNumberBuffer = "";
  inBattNumberInput = true;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Enter Batt # for ");
  lcd.print(slot);
  renderBattNumberBuffer();
  lcd.setCursor(0,2);
  lcd.print("* Cancel   # OK");
  lcd.setCursor(0,3);
  lcd.print(battNumForSlot(slot));
}

void showCycleInput(char slot) {
  if (slot == '\0') return;
  cycleSlot = slot;
  cycleBuffer = "";
  inCycleInput = true;
  confirmingCycle = false;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Set Cycles for ");
  lcd.print(slot);
  renderCycleBuffer();
  lcd.setCursor(0,2);
  lcd.print("Enter number (1-999)");
  lcd.setCursor(0,3);
  lcd.print("* Cancel   # OK");
  Serial.println("Cycle input screen shown");
}

void renderCycleBuffer() {
  String display = "[";
  display += cycleBuffer;
  display += "]";
  int col = (20 - display.length()) / 2;
  if (col < 0) col = 0;
  lcd.setCursor(col,1);
  for (int i=0;i<20;i++) lcd.print(" ");
  lcd.setCursor(col,1);
  lcd.print(display);
}

void showCycleConfirmation(char slot, int cycles) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Confirm Cycle Test");
  lcd.setCursor(0,1);
  lcd.print("Slot ");
  lcd.print(slot);
  lcd.print(": ");
  lcd.print(cycles);
  lcd.print(" cycles");
  lcd.setCursor(0,2);
  lcd.print("Ends on CHARGE");
  lcd.setCursor(0,3);
  lcd.print("#=START *=CANCEL");
  Serial.println("Cycle confirmation screen shown");
}

void handleCycleInputKey(char key) {
  Serial.print("Cycle input key: "); Serial.println(key);
  
  if (key >= '0' && key <= '9') {
    if (cycleBuffer.length() == 0 && key == '0') return;
    if (cycleBuffer.length() < 3) {
      cycleBuffer += key;
      renderCycleBuffer();
    }
    return;
  }
  if (key == '*') {
    Serial.println("Cycle input cancelled");
    inCycleInput = false;
    confirmingCycle = false;
    cycleSlot = '\0';
    cycleBuffer = "";
    pendingCycleValue = 0;
    showSlotSetting(previousSlot);
    inSettingScreen = true;
    return;
  }
  if (key == '#') {
    int value = 0;
    if (cycleBuffer.length() > 0)
      value = atoi(cycleBuffer.c_str());
    if (value <= 0) value = 1;
    
    Serial.print("Cycle value entered: "); Serial.println(value);
    
    pendingCycleValue = value;
    showCycleConfirmation(cycleSlot, value);
    inCycleInput = false;
    confirmingCycle = true;
    return;
  }
}

void renderBattNumberBuffer() {
  String display = "[";
  display += battNumberBuffer;
  display += "]";
  int col = (20 - display.length()) / 2;
  if (col < 0) col = 0;
  lcd.setCursor(col,1);
  for (int i=0;i<20;i++) lcd.print(" ");
  lcd.setCursor(col,1);
  lcd.print(display);
}

void handleBattNumberInputKey(char key) {
  if (key >= '0' && key <= '9') {
    if (battNumberBuffer.length() == 0 && key == '0') return; 
    if (battNumberBuffer.length() < 3) {
      battNumberBuffer += key;
      renderBattNumberBuffer();
    }
    return;
  }

  if (key == '*') { 
    inBattNumberInput = false;
    battNumberSlot = '\0';
    battNumberBuffer = "";
    showSlotSetting(previousSlot);
    inSettingScreen = true;
    return;
  }

  if (key == '#') { 
    int value = 0;
    if (battNumberBuffer.length() > 0)
      value = atoi(battNumberBuffer.c_str());

    saveBattNum(battNumberSlot, value);
    
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print(" Batt # saved!");
    lcd.setCursor(0,2);
    lcd.print(" Slot ");
    lcd.print(battNumberSlot);
    lcd.print(" = ");
    lcd.print(value);
    delay(1500);

    inBattNumberInput = false;
    battNumberSlot = '\0';
    battNumberBuffer = "";
    showSlotSetting(previousSlot);
    inSettingScreen = true;
    return;
  }
}

void processDeferredDischarge() {
  unsigned long now = millis();

  if (AdischargeWait && now - AdischargeStart >= 10000) {
    AdischargeWait = false; AdischargePending = false; Adischarge = true;
    digitalWrite(AMS, LOW); digitalWrite(ACD, LOW);
    Serial.println("Slot A DISCHARGE started after delay");
    showSlot('A');
  }

  if (BdischargeWait && now - BdischargeStart >= 10000) {
    BdischargeWait = false; BdischargePending = false; Bdischarge = true;
    digitalWrite(BMS, LOW); digitalWrite(BCD, LOW);
    Serial.println("Slot B DISCHARGE started after delay");
    showSlot('B');
  }

  if (CdischargeWait && now - CdischargeStart >= 10000) {
    CdischargeWait = false; CdischargePending = false; Cdischarge = true;
    digitalWrite(CMS, LOW); digitalWrite(CCD, LOW);
    Serial.println("Slot C DISCHARGE started after delay");
    showSlot('C');
  }

  if (DdischargeWait && now - DdischargeStart >= 10000) {
    DdischargeWait = false; DdischargePending = false; Ddischarge = true;
    digitalWrite(DMS, LOW); digitalWrite(DCD, LOW);
    Serial.println("Slot D DISCHARGE started after delay");
    showSlot('D');
  }
}

void saveBattNum(char slot, int num) {
  switch (slot) {
    case 'A':
      prefs.putInt("A_num", num);
      battNumA = num;
      Serial.print("Saved A_num = "); Serial.println(num);
      break;
    case 'B':
      prefs.putInt("B_num", num);
      battNumB = num;
      Serial.print("Saved B_num = "); Serial.println(num);
      break;
    case 'C':
      prefs.putInt("C_num", num);
      battNumC = num;
      Serial.print("Saved C_num = "); Serial.println(num);
      break;
    case 'D':
      prefs.putInt("D_num", num);
      battNumD = num;
      Serial.print("Saved D_num = "); Serial.println(num);
      break;
  }
}

void clearBattNum(char slot) {
  switch (slot) {
    case 'A':
      prefs.putInt("A_num", 0);
      battNumA = 0;
      Serial.println("Cleared Battery A number");
      break;
    case 'B':
      prefs.putInt("B_num", 0);
      battNumB = 0;
      Serial.println("Cleared Battery B number");
      break;
    case 'C':
      prefs.putInt("C_num", 0);
      battNumC = 0;
      Serial.println("Cleared Battery C number");
      break;
    case 'D':
      prefs.putInt("D_num", 0);
      battNumD = 0;
      Serial.println("Cleared Battery D number");
      break;
  }
}

void stopOperation(char slot, bool fromForceStop) {
  String operation = "";
  switch (slot) {
    case 'A': 
      if (Acharge) operation = "CHARGING";
      else if (Adischarge) operation = "DISCHARGING";
      else if (Acycle) operation = "CYCLE";
      break;
    case 'B': 
      if (Bcharge) operation = "CHARGING";
      else if (Bdischarge) operation = "DISCHARGING";
      else if (Bcycle) operation = "CYCLE";
      break;
    case 'C': 
      if (Ccharge) operation = "CHARGING";
      else if (Cdischarge) operation = "DISCHARGING";
      else if (Ccycle) operation = "CYCLE";
      break;
    case 'D': 
      if (Dcharge) operation = "CHARGING";
      else if (Ddischarge) operation = "DISCHARGING";
      else if (Dcycle) operation = "CYCLE";
      break;
  }

  float finalCapacity = 0;
  switch (slot) {
    case 'A': finalCapacity = capacityA; break;
    case 'B': finalCapacity = capacityB; break;
    case 'C': finalCapacity = capacityC; break;
    case 'D': finalCapacity = capacityD; break;
  }

  resetSlotBooleans(slot);
  resetSlotRelays(slot);
  
  char sheetId[50];
  float v = 0, c = 0;
  int battNum = battNumForSlot(slot);
  
  switch (slot) {
    case 'A': strcpy(sheetId, SHEET_ID_A); v = voltageA; c = currentA; break;
    case 'B': strcpy(sheetId, SHEET_ID_B); v = voltageB; c = currentB; break;
    case 'C': strcpy(sheetId, SHEET_ID_C); v = voltageC; c = currentC; break;
    case 'D': strcpy(sheetId, SHEET_ID_D); v = voltageD; c = currentD; break;
  }
  
  if (battNum > 0) {
    logSlotToSheet(sheetId, "Force Stopped", battNum, 0, v, c, finalCapacity, false, "Stopped");
  }

  lcd.clear();
  lcd.setCursor(0,1);
  if (fromForceStop) {
    lcd.print(" FORCE STOPPED!");
  } else {
    lcd.print("   OPERATION");
    lcd.setCursor(0,2);
    lcd.print("    STOPPED!");
  }
  lcd.setCursor(0,3);
  lcd.print("Slot ");
  lcd.print(slot);
  if (operation.length() > 0) {
    lcd.print(" - ");
    lcd.print(operation);
  }
  
  stopMessageStart = millis();
  stopMessageActive = true;
  inStopPrompt = false;
  
  Serial.print("Slot "); Serial.print(slot); 
  Serial.print(" operation stopped. Was: "); Serial.println(operation);
  if (finalCapacity > 0) {
    Serial.print("Final capacity: "); Serial.print(finalCapacity, 4); Serial.println(" mAh");
  }
}

void formatDateTime(const char* fmt, char* result, size_t size) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    strftime(result, size, fmt, &timeinfo);
  } else {
    strncpy(result, "1970-01-01 00:00:00", size);
  }
}

void logSlotToSheet(const char* sheetId,
                    const String& slotStatus,
                    int battNum,
                    unsigned long elapsedSecs,
                    float voltage,
                    float current,
                    float capacity,
                    bool inCycle,
                    const char* operationMode)
{
    if (!GSheet.ready()) {
        Serial.println("GSheet not ready - skipping append");
        return;
    }

    FirebaseJson response;
    FirebaseJson valueRange;
    char timeStamp[25];

    formatDateTime("%Y-%m-%d %H:%M:%S", timeStamp, sizeof(timeStamp));

    char voltageStr[10];
    char currentStr[10];
    char capacityStr[10];

    dtostrf(voltage, 5, 2, voltageStr);
    dtostrf(current, 5, 0, currentStr);
    dtostrf(capacity, 8, 4, capacityStr);

    String finalStatus = slotStatus;
    
    if (strcmp(operationMode, "Pre-Discharge") == 0) {
        finalStatus = "Simpson Capacity (Pre-Discharge)";
    } else if (strcmp(operationMode, "Pre-Charge") == 0) {
        finalStatus = "Simpson Capacity (Pre-Charge)";
    } else if (inCycle) {
        if (strcmp(operationMode, "Charge") == 0) {
            finalStatus = "Cycle-Charging";
        } else if (strcmp(operationMode, "Discharge") == 0) {
            finalStatus = "Cycle-Discharging";
        } else if (strcmp(operationMode, "Complete") == 0) {
            finalStatus = "Cycle-Complete";
        } else {
            finalStatus = "Cycle";
        }
    } else if (strcmp(operationMode, "Charge") == 0) {
        finalStatus = "Charging";
    } else if (strcmp(operationMode, "Discharge") == 0) {
        finalStatus = "Discharging";
    } else if (strcmp(operationMode, "Complete") == 0) {
        finalStatus = "Complete";
    } else if (strcmp(operationMode, "Stopped") == 0) {
        finalStatus = "Force Stopped";
    }

    valueRange.add("majorDimension", "COLUMNS");
    valueRange.set("values/[0]/[0]", timeStamp);
    valueRange.set("values/[1]/[0]", finalStatus);
    valueRange.set("values/[2]/[0]", battNum);
    valueRange.set("values/[3]/[0]", elapsedSecs);
    valueRange.set("values/[4]/[0]", voltageStr);
    valueRange.set("values/[5]/[0]", currentStr);
    valueRange.set("values/[6]/[0]", capacityStr);  

    Serial.print("Sending to Google Sheet ID: ");
    Serial.println(sheetId);
    Serial.print("Status: "); Serial.println(finalStatus);
    Serial.print("Capacity: "); Serial.print(capacity, 4); Serial.println(" mAh (cumulative)");

    bool ok = GSheet.values.append(&response, sheetId, "Sheet1!A1:G1", &valueRange);

    if (!ok) {
        Serial.println("Append FAILED!");
        String r; 
        response.toString(r, true);
        Serial.println(r);
    } else {
        Serial.println("Log SUCCESS!");
    }
}
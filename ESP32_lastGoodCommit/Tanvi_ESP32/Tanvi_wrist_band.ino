#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <NimBLEDevice.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <Preferences.h>
#include <esp_task_wdt.h>

// =========================================================================
//  SILVERCARE - WRIST BAND (NimBLE Server)
// =========================================================================

// =========================================================================
//  BLE CONFIGURATION
// =========================================================================
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_IDENTITY_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define DEVICE_NAME             "SilverCare_Wrist"

// =========================================================================
//  GSM CONFIGURATION
// =========================================================================
#define GSM_TX 16
#define GSM_RX 17
#define GSM_BAUD 9600
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);

// =========================================================================
//  WiFi & BACKEND SERVER CONFIGURATION
// =========================================================================
const char* ssid = "Pujacha Mobile";
const char* password = "ERROR 418";
const char* serverURL = "http://192.168.43.167:5002/api/sensor-data";

// =========================================================================
//  PINS
// =========================================================================
#define TEMP_PIN 4
#define BUZZER_PIN 18
#define BUTTON_PIN 19
#define MIC_BUTTON_PIN 23
#define MAX30102_INT_PIN 34

// =========================================================================
//  HARDWARE IDENTIFICATION
// =========================================================================
String deviceId = "vois_wrist";
String beltType = "Wrist Band";
double gpsLat = 18.5204;
double gpsLng = 73.8567;

// =========================================================================
//  OBJECTS
// =========================================================================
Adafruit_MPU6050 mpu;
MAX30105 maxSensor;
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

// =========================================================================
//  THRESHOLDS
// =========================================================================
#define INSTABILITY_THRESHOLD 1.15
#define SUDDEN_THRESHOLD 1.4
#define FALL_THRESHOLD 1.9
#define TEMP_WORN_THRESHOLD 26.0
#define HR_LOW 50
#define HR_HIGH 135
#define SPO2_LOW 90

// =========================================================================
//  STATES
// =========================================================================
enum SystemState {
  NORMAL,
  PREFALL,
  SUDDEN_MOVEMENT,
  FALL_DETECTED
};

SystemState currentState = NORMAL;
String lastAlertType = "";

// =========================================================================
//  BLE SERVER VARIABLES
// =========================================================================
bool deviceConnected = false;
NimBLEServer* pServer = NULL;
NimBLEService* pService = NULL;
NimBLECharacteristic* pCharacteristic = NULL;
NimBLECharacteristic* pIdentityCharacteristic = NULL;
NimBLEAdvertising* pAdvertising = NULL;

// =========================================================================
//  HEART RATE DATA
// =========================================================================
float heartRate = 0;
float spo2 = 0;
long irValue = 0;
long redValue = 0;
float bodyTemp = 0;
bool sensorInitialized = false;

// =========================================================================
//  VARIABLES
// =========================================================================
bool beltWorn = false;
unsigned long fallTime = 0;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000;
String micMessage = "";
unsigned long lastHeartRateCalc = 0;
const unsigned long HEART_RATE_INTERVAL = 100;

// Heart rate algorithm variables
const int RATE_SIZE = 25;
float rates[RATE_SIZE];
int rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 0;

// =========================================================================
//  UNIQUE DEVICE IDENTITY
// =========================================================================
String uniqueDeviceID = "";
Preferences preferences;

// =========================================================================
//  SIMPLE ENCRYPTION
// =========================================================================
class EncryptionManager {
public:
    static String encrypt(const String& plaintext) {
        if (plaintext.length() == 0 || plaintext.length() > 50) return plaintext;
        String result = "";
        result.reserve(plaintext.length() * 2 + 1);
        for (int i = 0; i < plaintext.length(); i++) {
            char c = plaintext[i] ^ 0x55;
            if (c < 16) result += "0";
            result += String(c, HEX);
        }
        return result;
    }
    
    static String decrypt(const String& encrypted) {
        if (encrypted.length() == 0 || encrypted.length() % 2 != 0) return encrypted;
        String result = "";
        result.reserve(encrypted.length() / 2);
        for (int i = 0; i < encrypted.length(); i += 2) {
            String byteStr = encrypted.substring(i, i + 2);
            char c = strtol(byteStr.c_str(), NULL, 16);
            result += (char)(c ^ 0x55);
        }
        return result;
    }
};

// =========================================================================
//  BLE SERVER CALLBACKS
// =========================================================================
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    deviceConnected = true;
    Serial.println("========================================");
    Serial.println("✅ [BLE] WRIST BAND CONNECTED!");
    Serial.print("📱 Connected to: ");
    Serial.println(connInfo.getAddress().toString().c_str());
    Serial.println("========================================");
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    deviceConnected = false;
    Serial.println("========================================");
    Serial.println("❌ [BLE] WRIST BAND DISCONNECTED");
    Serial.println("🔄 Restarting advertising...");
    Serial.println("========================================");
    delay(100);
    if (pAdvertising) {
      pAdvertising->start();
    }
  }
};

class MyCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    Serial.println("📤 [BLE] Characteristic read request");
  }
  
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    String value = pCharacteristic->getValue().c_str();
    Serial.print("📥 [BLE] Write request: ");
    Serial.println(value);
    
    if (value.startsWith("PAIR:")) {
      String response = "PAIR_ACK:" + uniqueDeviceID;
      pCharacteristic->setValue(response.c_str());
      pCharacteristic->notify();
      Serial.println("✅ Pairing acknowledged");
    }
  }
  
  void onNotify(NimBLECharacteristic* pCharacteristic) {
    Serial.println("📤 [BLE] Notification sent");
  }
};

// =========================================================================
//  SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("========================================");
  Serial.println("=== SILVERCARE WRIST BAND ===");
  Serial.println("========================================");

  // Disable watchdog
  esp_task_wdt_deinit();

  // Initialize pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MIC_BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize I2C
  Wire.begin(21, 22);
  Wire.setClock(100000);

  // Initialize Preferences
  preferences.begin("wrist_id", false);
  uniqueDeviceID = preferences.getString("unique_id", "");
  if (uniqueDeviceID.length() == 0) {
    uniqueDeviceID = "SCW_" + String(millis(), HEX);
    preferences.putString("unique_id", uniqueDeviceID);
  }
  preferences.end();
  Serial.println("🆔 Device ID: " + uniqueDeviceID);

  // Initialize BLE
  Serial.println("📱 Initializing BLE...");
  initBLEServer();
  delay(500);
  Serial.println("📱 BLE Advertising as: " + String(DEVICE_NAME));
  Serial.println("📱 Service UUID: " + String(BLE_SERVICE_UUID));

  // Initialize GSM
  Serial.println("📱 Initializing GSM...");
  gsmSerial.begin(GSM_BAUD);
  delay(1000);
  gsmSerial.println("AT");
  delay(1000);
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    if (response.indexOf("OK") != -1) {
      Serial.println("✅ GSM Module OK");
    }
  }
  gsmSerial.println("AT+CMGF=1");
  delay(1000);

  // Initialize WiFi
  Serial.println("📡 Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("📡 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi Failed - Continuing without WiFi");
  }

  // Initialize Sensors
  Serial.println("🔌 Initializing Sensors...");
  
  if (!mpu.begin()) {
    Serial.println("❌ MPU6050 NOT FOUND");
  } else {
    Serial.println("✅ MPU6050 OK");
  }

  tempSensor.begin();
  Serial.println("✅ DS18B20 OK");

  Serial.println("🔴 Initializing MAX30102...");
  bool max30102Found = false;
  for (int retry = 0; retry < 3; retry++) {
    if (maxSensor.begin(Wire, I2C_SPEED_FAST)) {
      max30102Found = true;
      break;
    }
    delay(500);
    Serial.print(".");
  }
  
  if (max30102Found) {
    byte ledBrightness = 0x7F;
    byte sampleAverage = 4;
    byte ledMode = 2;
    int sampleRate = 400;
    int pulseWidth = 411;
    int adcRange = 4096;
    
    maxSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    maxSensor.setPulseAmplitudeRed(0x7F);
    maxSensor.setPulseAmplitudeIR(0x7F);
    sensorInitialized = true;
    Serial.println("\n✅ MAX30102 OK");
  } else {
    Serial.println("\n❌ MAX30102 NOT FOUND - Check wiring");
  }

  Serial.println("========================================");
  Serial.println("✅ System Ready!");
  Serial.print("💾 Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.println("========================================");
  Serial.println("📱 Waiting for BLE connection...");
  Serial.println("📱 Device Name: " + String(DEVICE_NAME));
  Serial.println("========================================");
}

// =========================================================================
//  BLE SERVER FUNCTIONS
// =========================================================================
void initBLEServer() {
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  pService = pServer->createService(BLE_SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
    BLE_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ |
    NIMBLE_PROPERTY::WRITE |
    NIMBLE_PROPERTY::NOTIFY
  );
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  
  pIdentityCharacteristic = pService->createCharacteristic(
    BLE_IDENTITY_UUID,
    NIMBLE_PROPERTY::READ
  );
  pIdentityCharacteristic->setValue(uniqueDeviceID.c_str());
  
  pService->start();
  
  pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  
  NimBLEAdvertisementData advData;
  advData.setName(DEVICE_NAME);
  advData.setCompleteServices(NimBLEUUID(BLE_SERVICE_UUID));
  pAdvertising->setAdvertisementData(advData);
  
  NimBLEAdvertisementData scanData;
  scanData.setName(DEVICE_NAME);
  pAdvertising->setScanResponseData(scanData);
  
  pAdvertising->start();
}

// =========================================================================
//  SEND HEART DATA
// =========================================================================
void sendHeartData() {
  if (!deviceConnected || pCharacteristic == NULL) return;
  
  String plainData = String((int)heartRate) + ":" + 
                     String((int)spo2) + ":" + 
                     String(irValue) + ":" + 
                     String(bodyTemp, 1);
  
  if (plainData.length() > 0 && plainData.length() < 50) {
    String encryptedData = EncryptionManager::encrypt(plainData);
    pCharacteristic->setValue(encryptedData.c_str());
    pCharacteristic->notify();
    
    Serial.print("📤 BLE Sent - HR:");
    Serial.print((int)heartRate);
    Serial.print(" SPO2:");
    Serial.print((int)spo2);
    Serial.print(" IR:");
    Serial.print(irValue);
    Serial.print(" Temp:");
    Serial.println(bodyTemp, 1);
  }
}

// =========================================================================
//  HEART RATE CALCULATION
// =========================================================================
void calculateHeartRate() {
  if (!sensorInitialized) {
    static bool printedOnce = false;
    if (!printedOnce) {
      Serial.println("⚠️ MAX30102 not initialized - waiting for sensor...");
      printedOnce = true;
    }
    return;
  }
  
  irValue = maxSensor.getIR();
  redValue = maxSensor.getRed();
  
  static int debugCounter = 0;
  debugCounter++;
  if (debugCounter % 100 == 0) {
    Serial.print("📊 Raw - IR:");
    Serial.print(irValue);
    Serial.print(" Red:");
    Serial.println(redValue);
  }
  
  long delta = abs(irValue - redValue);
  
  if (irValue > 10000 && redValue > 10000 && delta > 5000) {
    long currentTime = millis();
    if (currentTime - lastBeat > 100) {
      long timeBetweenBeats = currentTime - lastBeat;
      if (timeBetweenBeats > 300 && timeBetweenBeats < 2000) {
        beatsPerMinute = 60000.0 / timeBetweenBeats;
        
        rates[rateSpot++] = beatsPerMinute;
        rateSpot %= RATE_SIZE;
        
        float sum = 0;
        int count = 0;
        for (int i = 0; i < RATE_SIZE; i++) {
          if (rates[i] > 0) {
            sum += rates[i];
            count++;
          }
        }
        if (count > 0) {
          heartRate = sum / count;
          Serial.print("❤️ Heart Rate: ");
          Serial.print((int)heartRate);
          Serial.println(" BPM");
        }
      }
      lastBeat = currentTime;
    }
  }
  
  if (irValue > 0 && redValue > 0) {
    float ratio = (float)redValue / (float)irValue;
    if (ratio > 0.5 && ratio < 1.5) {
      spo2 = 100 - (ratio - 0.5) * 20;
      if (spo2 > 100) spo2 = 100;
      if (spo2 < 70) spo2 = 70;
      if (debugCounter % 50 == 0) {
        Serial.print("🟦 SPO2: ");
        Serial.print((int)spo2);
        Serial.println("%");
      }
    }
  }
  
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);
  if (temp > -10 && temp < 100) {
    bodyTemp = temp;
    if (debugCounter % 50 == 0) {
      Serial.print("🌡️ Temp: ");
      Serial.print(bodyTemp, 1);
      Serial.println("°C");
    }
  }
}

// =========================================================================
//  SEND DATA TO SERVER
// =========================================================================
void sendDataToServer(SystemState state, float hr, float oxygen, float temp, bool worn, float acc, String micAudio) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<384> doc;
  doc["deviceId"] = deviceId;
  doc["beltType"] = beltType;
  doc["state"] = (int)state;
  doc["stateName"] = getStateName(state);
  doc["heartRate"] = hr;
  doc["spo2"] = oxygen;
  doc["temperature"] = temp;
  doc["beltWorn"] = worn;
  doc["acceleration"] = acc;
  doc["latitude"] = gpsLat;
  doc["longitude"] = gpsLng;
  doc["micMessageAudio"] = micAudio;
  doc["timestamp"] = millis();
  doc["uniqueDeviceID"] = uniqueDeviceID;

  String payload;
  serializeJson(doc, payload);

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    if (response.indexOf("ACKNOWLEDGED") != -1 || response.indexOf("I am Fine") != -1) {
      digitalWrite(BUZZER_PIN, LOW);
      currentState = NORMAL;
    }
  }

  http.end();
}

String getStateName(SystemState state) {
  switch(state) {
    case NORMAL: return "NORMAL";
    case PREFALL: return "PREFALL";
    case SUDDEN_MOVEMENT: return "SUDDEN_MOVEMENT";
    case FALL_DETECTED: return "FALL_DETECTED";
    default: return "UNKNOWN";
  }
}

// =========================================================================
//  SMS FUNCTIONS
// =========================================================================
void sendSmartSMSAlert(String alertType, String message) {
  static unsigned long lastSMSTime = 0;
  const unsigned long SMS_RETRY_INTERVAL = 30000;
  
  unsigned long currentTime = millis();
  if (currentTime - lastSMSTime < SMS_RETRY_INTERVAL) {
    return;
  }
  
  Serial.print("📱 Sending SMS Alert: ");
  Serial.println(alertType);
  
  if (WiFi.status() == WL_CONNECTED) {
    sendTwilioSMS(alertType, message);
  } else {
    sendGSMAlert(alertType, message);
  }
  lastSMSTime = currentTime;
}

bool sendTwilioSMS(String alertType, String message) {
  StaticJsonDocument<256> doc;
  doc["alert_type"] = alertType;
  doc["message"] = message;
  doc["device_id"] = deviceId;
  doc["belt_type"] = beltType;
  doc["timestamp"] = millis();
  doc["uniqueDeviceID"] = uniqueDeviceID;
  
  String payload;
  serializeJson(doc, payload);
  
  HTTPClient http;
  http.begin("http://192.168.43.167:5002/api/twilio-sms");
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.POST(payload);
  http.end();
  return (httpResponseCode == 200);
}

bool sendGSMAlert(String alertType, String message) {
  String guardianPhone = "+919322757538";
  
  gsmSerial.println("AT+CMGS=\"" + guardianPhone + "\"");
  delay(1000);
  
  String smsMessage = "🚨 ALERT - " + alertType + "\n" + message;
  gsmSerial.println(smsMessage);
  delay(1000);
  
  gsmSerial.write(26);
  delay(3000);
  
  while(gsmSerial.available()) {
    String response = gsmSerial.readString();
    if (response.indexOf("OK") != -1) return true;
  }
  return false;
}

// =========================================================================
//  MAIN LOOP
// =========================================================================
void loop() {
  // Heart rate calculation
  if (millis() - lastHeartRateCalc > HEART_RATE_INTERVAL) {
    calculateHeartRate();
    lastHeartRateCalc = millis();
  }

  // Send heart data via BLE
  static unsigned long lastBLESend = 0;
  if (millis() - lastBLESend > 500) {
    if (deviceConnected && heartRate > 0) {
      sendHeartData();
    } else if (deviceConnected) {
      // Send placeholder data to show connection is active
      String plainData = "0:0:" + String(irValue) + ":" + String(bodyTemp, 1);
      String encryptedData = EncryptionManager::encrypt(plainData);
      pCharacteristic->setValue(encryptedData.c_str());
      pCharacteristic->notify();
    }
    lastBLESend = millis();
  }

  // Print connection status
  static unsigned long lastStatusPrint = 0;
  if (millis() - lastStatusPrint > 10000) {
    if (deviceConnected) {
      Serial.println("📱 BLE: CONNECTED - Sending data");
    } else {
      Serial.println("📱 BLE: Waiting for connection...");
    }
    lastStatusPrint = millis();
  }

  // MPU6050 Reading
  sensors_event_t acc, gyro, temp;
  if (mpu.begin()) {
    mpu.getEvent(&acc, &gyro, &temp);
  }

  float ax = acc.acceleration.x;
  float ay = acc.acceleration.y;
  float az = acc.acceleration.z;
  float accMag = sqrt(ax * ax + ay * ay + az * az);
  float accMagG = accMag / 9.8;

  // Temperature reading
  tempSensor.requestTemperatures();
  float bodyTempLocal = tempSensor.getTempCByIndex(0);

  // Belt worn check
  #define IR_WORN_THRESHOLD 4500
  beltWorn = (irValue > IR_WORN_THRESHOLD && bodyTempLocal > TEMP_WORN_THRESHOLD);
  bool vitalsAbnormal = (heartRate < HR_LOW || heartRate > HR_HIGH || spo2 < SPO2_LOW);

  // Microphone trigger
  if (digitalRead(MIC_BUTTON_PIN) == LOW) {
    micMessage = "Help needed!";
    Serial.println("🎙️ MIC TRIGGERED");
  } else if (micMessage.length() > 0 && currentState == NORMAL) {
    micMessage = "";
  }

  // State machine
  if (currentState != FALL_DETECTED) {
    if (accMagG > INSTABILITY_THRESHOLD && accMagG <= SUDDEN_THRESHOLD && beltWorn && vitalsAbnormal) {
      currentState = PREFALL;
      micMessage = "Pre-fall detected";
      Serial.println("⚠️ PRE-FALL DETECTED");
    } else if (accMagG > SUDDEN_THRESHOLD && accMagG <= FALL_THRESHOLD) {
      currentState = SUDDEN_MOVEMENT;
      Serial.println("⚠️ SUDDEN MOVEMENT");
    } else if (accMagG > FALL_THRESHOLD && beltWorn) {
      currentState = FALL_DETECTED;
      fallTime = millis();
      micMessage = "FALL DETECTED!";
      Serial.println("🚨 FALL DETECTED!");
    } else {
      currentState = NORMAL;
    }
  }

  // Handle states
  switch (currentState) {
    case NORMAL:
      digitalWrite(BUZZER_PIN, LOW);
      break;

    case PREFALL:
      digitalWrite(BUZZER_PIN, LOW);
      sendSmartSMSAlert("PRE-FALL", "Pre-fall detected!");
      break;

    case SUDDEN_MOVEMENT:
      digitalWrite(BUZZER_PIN, LOW);
      break;

    case FALL_DETECTED:
      digitalWrite(BUZZER_PIN, HIGH);
      sendSmartSMSAlert("FALL", "EMERGENCY: Fall detected!");
      break;
  }

  // User OK button
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("👤 USER: I'M OK");
    digitalWrite(BUZZER_PIN, LOW);
    currentState = NORMAL;
    micMessage = "User pressed OK";
  }

  // Send telemetry to server
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    sendDataToServer(currentState, heartRate, spo2, bodyTempLocal, beltWorn, accMagG, micMessage);
    lastSendTime = currentTime;
  }

  delay(10);
}
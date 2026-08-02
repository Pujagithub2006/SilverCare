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

// =========================================================================
//  SILVERCARE - SENIOR SAFETY WRIST BAND (ESP32 BLE SERVER)
//  This device measures heart rate and sends it to the waist band via BLE
// =========================================================================

// ---------- BLE CONFIGURATION ----------
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DEVICE_NAME             "SilverCare_Wrist"

// ---------- GSM CONFIGURATION ----------
#define GSM_TX 16
#define GSM_RX 17
#define GSM_BAUD 9600
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);

// ---------- WiFi & BACKEND SERVER CONFIGURATION ----------
const char* ssid = "Pujacha Mobile";
const char* password = "ERROR 418";
const char* serverURL = "http://192.168.43.167:5002/api/sensor-data";

// ---------- PINS ----------
#define TEMP_PIN 4
#define BUZZER_PIN 18
#define BUTTON_PIN 19
#define MIC_BUTTON_PIN 23
#define MAX30102_INT_PIN 34

// Hardware Identification
String deviceId = "vois_wrist";
String beltType = "Wrist Band";
double gpsLat = 18.5204;
double gpsLng = 73.8567;

// ---------- OBJECTS ----------
Adafruit_MPU6050 mpu;
MAX30105 maxSensor;
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

// ---------- THRESHOLDS ----------
#define INSTABILITY_THRESHOLD 1.15
#define SUDDEN_THRESHOLD 1.4
#define FALL_THRESHOLD 1.9

#define TEMP_WORN_THRESHOLD 26.0

#define HR_LOW 50
#define HR_HIGH 135
#define SPO2_LOW 90

// ---------- STATES ----------
enum SystemState {
  NORMAL,
  PREFALL,
  SUDDEN_MOVEMENT,
  FALL_DETECTED
};

SystemState currentState = NORMAL;
String lastAlertType = "";

// ---------- BLE SERVER VARIABLES ----------
bool deviceConnected = false;
NimBLEServer* pServer = NULL;
NimBLEService* pService = NULL;
NimBLECharacteristic* pCharacteristic = NULL;
NimBLEAdvertising* pAdvertising = NULL;

// ---------- HEART RATE DATA ----------
float heartRate = 0;
float spo2 = 0;
long irValue = 0;
long redValue = 0;
float bodyTemp = 0;
bool sensorInitialized = false;

// ---------- VARIABLES ----------
bool beltWorn = false;
unsigned long fallTime = 0;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000;
String micMessage = "";
unsigned long lastHeartRateCalc = 0;
const unsigned long HEART_RATE_INTERVAL = 100; // Calculate every 100ms

// Heart rate algorithm variables
const int RATE_SIZE = 25; // Increase sample size for more stability
float rates[RATE_SIZE];
int rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 0;

// =========================================================================
//  BLE SERVER CALLBACKS
// =========================================================================
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    deviceConnected = true;
    Serial.println("✅ [BLE] Wrist band connected to Waist band");
    Serial.print("📱 [BLE] Connected to: ");
    Serial.println(connInfo.getAddress().toString().c_str());
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    deviceConnected = false;
    Serial.println("❌ [BLE] Wrist band disconnected from Waist band");
    Serial.println("🔄 [BLE] Restarting advertising...");
    // Restart advertising
    NimBLEDevice::getAdvertising()->start();
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
  delay(1000);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MIC_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(21, 22);

  Serial.println("=== SILVERCARE SENIOR SAFETY WRIST BAND (BLE SERVER) ===");

  // ========== GSM Module Initialization ==========
  gsmSerial.begin(GSM_BAUD);
  Serial.println("📱 Initializing GSM Module...");
  delay(1000);
  
  gsmSerial.println("AT");
  delay(1000);
  if (gsmSerial.available()) {
    Serial.println("✅ GSM Module Responding");
  } else {
    Serial.println("❌ GSM Module Not Responding");
  }
  
  gsmSerial.println("AT+CMGF=1");
  delay(1000);

  // ========== WiFi Connection ==========
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi Failed - Local Operation Active");
  }

  // ========== Sensors Initialization ==========
  if (!mpu.begin()) {
    Serial.println("❌ MPU6050 NOT FOUND");
    while (1);
  }
  Serial.println("✅ MPU6050 Initialized");

  tempSensor.begin();
  Serial.println("✅ DS18B20 Initialized");

  // Initialize MAX30102
  Serial.println("🔴 Initializing MAX30102...");
  if (!maxSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("❌ MAX30102 NOT FOUND");
    while (1);
  }
  
  // Configure MAX30102 for heart rate/SpO2 measurement
  byte ledBrightness = 0x7F;  // 0-255: Higher brightness for better signal
  byte sampleAverage = 4;      // 1, 2, 4, 8, 16, 32: Averaging samples
  byte ledMode = 2;            // 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  int sampleRate = 400;        // 50, 100, 200, 400, 800, 1000, 1600, 3200: Samples per second
  int pulseWidth = 411;        // 69, 118, 215, 411: Pulse width in microseconds
  int adcRange = 4096;         // 2048, 4096, 8192, 16384: ADC range
  
  maxSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  maxSensor.setPulseAmplitudeRed(0x7F);
  maxSensor.setPulseAmplitudeIR(0x7F);
  
  Serial.println("✅ MAX30102 Initialized");
  sensorInitialized = true;

  // ========== BLE Server Initialization ==========
  initBLEServer();
  
  Serial.println("✅ BLE Server initialized - Advertising as: " + String(DEVICE_NAME));
  Serial.println("=================================================");
}

// =========================================================================
//  BLE SERVER FUNCTIONS - FIXED FOR NIMBLE
// =========================================================================
void initBLEServer() {
  // Initialize NimBLE
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Max power for better range
  
  // Create server
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Create service
  pService = pServer->createService(BLE_SERVICE_UUID);
  
  // Create characteristic with notify property
  pCharacteristic = pService->createCharacteristic(
    BLE_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ |
    NIMBLE_PROPERTY::WRITE |
    NIMBLE_PROPERTY::NOTIFY
  );
  
  // FIX: Use proper NimBLE descriptor - Remove NimBLE2902 and use descriptor directly
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  
  // Start service
  pService->start();
  
  // Setup advertising
  pAdvertising = NimBLEDevice::getAdvertising();
  
  // Add service UUID
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  
  // Set advertisement data
  NimBLEAdvertisementData advData;
  advData.setName(DEVICE_NAME);
  advData.setCompleteServices(NimBLEUUID(BLE_SERVICE_UUID));
  pAdvertising->setAdvertisementData(advData);
  
  // Set scan response data
  NimBLEAdvertisementData scanData;
  scanData.setName(DEVICE_NAME);
  pAdvertising->setScanResponseData(scanData);
  
  // Start advertising
  pAdvertising->start();
  
  Serial.println("✅ BLE Server started!");
  Serial.print("📱 Service UUID: ");
  Serial.println(BLE_SERVICE_UUID);
  Serial.print("📱 Characteristic UUID: ");
  Serial.println(BLE_CHARACTERISTIC_UUID);
  Serial.print("📱 Device Name: ");
  Serial.println(DEVICE_NAME);
}

void sendHeartData() {
  if (!deviceConnected) return;
  if (pCharacteristic == NULL) return;
  
  // Format: "HR:SPO2:IR:TEMP"
  String data = String((int)heartRate) + ":" + 
                String((int)spo2) + ":" + 
                String(irValue) + ":" + 
                String(bodyTemp, 1);
  
  pCharacteristic->setValue(data.c_str());
  pCharacteristic->notify();
  
  Serial.print("📤 [BLE] Sent: ");
  Serial.println(data);
}

// =========================================================================
//  HEART RATE CALCULATION FUNCTIONS
// =========================================================================
void calculateHeartRate() {
  if (!sensorInitialized) return;
  
  // Read sensor data
  irValue = maxSensor.getIR();
  redValue = maxSensor.getRed();
  
  // Calculate heart rate using peak detection
  long delta = abs(irValue - redValue);
  
  if (irValue > 10000 && redValue > 10000) {
    // Simple heart rate detection based on IR peak
    if (delta > 5000) {
      long currentTime = millis();
      if (currentTime - lastBeat > 100) {
        long timeBetweenBeats = currentTime - lastBeat;
        if (timeBetweenBeats > 300 && timeBetweenBeats < 2000) {
          beatsPerMinute = 60000.0 / timeBetweenBeats;
          
          // Add to rate array for averaging
          rates[rateSpot++] = beatsPerMinute;
          rateSpot %= RATE_SIZE;
          
          // Calculate average
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
          }
        }
        lastBeat = currentTime;
      }
    }
  }
  
  // Estimate SpO2 using red and IR ratio
  if (irValue > 0 && redValue > 0) {
    float ratio = (float)redValue / (float)irValue;
    // Simple mapping for SpO2 (calibration needed for accuracy)
    if (ratio > 0.5 && ratio < 1.5) {
      spo2 = 100 - (ratio - 0.5) * 20;
      if (spo2 > 100) spo2 = 100;
      if (spo2 < 70) spo2 = 70;
    }
  }
  
  // Update body temperature from DS18B20
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);
  if (temp > -10 && temp < 100) {
    bodyTemp = temp;
  }
}

// =========================================================================
//  FUNCTIONS
// =========================================================================
void sendDataToServer(SystemState state, float hr, float oxygen, float temp, bool worn, float acc, String micAudio) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi not connected - skipping send");
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

  String payload;
  serializeJson(doc, payload);

  Serial.print("📤 [WRIST BELT TELEMETRY SENT]: ");
  Serial.println(payload);

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("✅ Server Response Code: ");
    Serial.println(httpResponseCode);

    if (response.indexOf("ACKNOWLEDGED") != -1 || response.indexOf("I am Fine") != -1) {
      Serial.println("💚 [GUARDIAN ACKNOWLEDGED]: Clearing local buzzer and state!");
      digitalWrite(BUZZER_PIN, LOW);
      currentState = NORMAL;
    }
  } else {
    Serial.print("❌ HTTP POST Error: ");
    Serial.println(httpResponseCode);
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

void sendSmartSMSAlert(String alertType, String message) {
  static unsigned long lastSMSTime = 0;
  static bool twilioSMSSent = false;
  static bool gsmSMSSent = false;
  const unsigned long SMS_RETRY_INTERVAL = 30000;
  
  unsigned long currentTime = millis();
  
  if (currentTime - lastSMSTime < SMS_RETRY_INTERVAL) {
    return;
  }
  
  if (alertType != lastAlertType) {
    twilioSMSSent = false;
    gsmSMSSent = false;
    lastAlertType = alertType;
  }
  
  Serial.println("📱 [SMS] Smart SMS System Activated");
  
  if (WiFi.status() == WL_CONNECTED && !twilioSMSSent) {
    Serial.println("📡 [TWILIO] Attempting SMS via WiFi...");
    bool twilioSuccess = sendTwilioSMS(alertType, message);
    if (twilioSuccess) {
      twilioSMSSent = true;
      lastSMSTime = currentTime;
      return;
    }
  }
  
  if (!gsmSMSSent) {
    Serial.println("📱 [GSM] Sending backup SMS...");
    bool gsmSuccess = sendGSMAlert(alertType, message);
    if (gsmSuccess) {
      gsmSMSSent = true;
      lastSMSTime = currentTime;
    }
  }
}

bool sendTwilioSMS(String alertType, String message) {
  StaticJsonDocument<256> doc;
  doc["alert_type"] = alertType;
  doc["message"] = message;
  doc["device_id"] = deviceId;
  doc["belt_type"] = beltType;
  doc["timestamp"] = millis();
  
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
  
  String smsMessage = "🚨 SILVERCARE ALERT - " + alertType + "\n" + message + "\nDevice: " + deviceId + " (" + beltType + ")";
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
  // ---------- HEART RATE CALCULATION ----------
  if (millis() - lastHeartRateCalc > HEART_RATE_INTERVAL) {
    calculateHeartRate();
    lastHeartRateCalc = millis();
  }

  // ---------- SEND HEART DATA VIA BLE ----------
  // Send heart data every 500ms
  static unsigned long lastBLESend = 0;
  if (millis() - lastBLESend > 500) {
    if (deviceConnected && heartRate > 0) {
      sendHeartData();
    }
    lastBLESend = millis();
  }

  // ---------- MPU6050 READING ----------
  sensors_event_t acc, gyro, temp;
  mpu.getEvent(&acc, &gyro, &temp);

  float ax = acc.acceleration.x;
  float ay = acc.acceleration.y;
  float az = acc.acceleration.z;
  float accMag = sqrt(ax * ax + ay * ay + az * az);
  float accMagG = accMag / 9.8;

  // ---------- TEMPERATURE READING (Local DS18B20) ----------
  tempSensor.requestTemperatures();
  float bodyTempLocal = tempSensor.getTempCByIndex(0);

  // ---------- BELT WORN CHECK ----------
  #define IR_WORN_THRESHOLD 4500
  beltWorn = (irValue > IR_WORN_THRESHOLD && bodyTempLocal > TEMP_WORN_THRESHOLD);
  bool vitalsAbnormal = (heartRate < HR_LOW || heartRate > HR_HIGH || spo2 < SPO2_LOW);

  // ---------- MICROPHONE TRIGGER CHECK ----------
  if (digitalRead(MIC_BUTTON_PIN) == LOW) {
    micMessage = "Senior citizen pressed Wrist Mic button: 'Help needed!'";
    Serial.println("🎙️ [MIC TRIGGERED]: " + micMessage);
  } else if (micMessage.length() > 0 && currentState == NORMAL) {
    micMessage = "";
  }

  // ---------- STATE MACHINE ----------
  if (currentState == FALL_DETECTED) {
    goto STATE_OUTPUT;
  }

  if (accMagG > INSTABILITY_THRESHOLD && accMagG <= SUDDEN_THRESHOLD && beltWorn && vitalsAbnormal) {
    currentState = PREFALL;
    micMessage = "Pre-fall instability detected on Wrist Belt. Asking: 'Are you okay?'";
  } else if (accMagG > SUDDEN_THRESHOLD && accMagG <= FALL_THRESHOLD) {
    currentState = SUDDEN_MOVEMENT;
  } else if (accMagG > FALL_THRESHOLD && beltWorn) {
    currentState = FALL_DETECTED;
    fallTime = millis();
    micMessage = "EMERGENCY: Fall impact detected on Wrist Belt! Urgent help requested.";
  } else {
    currentState = NORMAL;
  }

  STATE_OUTPUT:
  switch (currentState) {
    case NORMAL:
      digitalWrite(BUZZER_PIN, LOW);
      break;

    case PREFALL:
      digitalWrite(BUZZER_PIN, LOW);
      sendSmartSMSAlert("PRE-FALL", "Pre-fall detected on Wrist Belt! Please check senior ward.");
      break;

    case SUDDEN_MOVEMENT:
      digitalWrite(BUZZER_PIN, LOW);
      break;

    case FALL_DETECTED:
      digitalWrite(BUZZER_PIN, HIGH);
      sendSmartSMSAlert("FALL", "EMERGENCY: Fall detected on Wrist Belt! Immediate assistance required!");
      break;
  }

  // ---------- USER MANUAL OK OVERRIDE ----------
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("USER RESPONSE: I'M OK");
    digitalWrite(BUZZER_PIN, LOW);
    currentState = NORMAL;
    micMessage = "Senior pressed 'I AM OK' button on belt.";
  }

  // ---------- SEND TELEMETRY TO SPRING BOOT SERVER ----------
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    sendDataToServer(currentState, heartRate, spo2, bodyTempLocal, beltWorn, accMagG, micMessage);
    lastSendTime = currentTime;
  }

  delay(10);
}
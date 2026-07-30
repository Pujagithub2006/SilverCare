#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>

// =========================================================================
//  SILVERCARE - SENIOR SAFETY WRIST BELT (ESP32 BLE SERVER)
// =========================================================================

// ---------- BLE CONFIGURATION ----------
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DEVICE_NAME             "SilverCare_Wrist"

// ---------- GSM CONFIGURATION ----------
#define GSM_TX 17
#define GSM_RX 16
#define GSM_BAUD 9600
HardwareSerial gsmSerial(2);

// ---------- WiFi & BACKEND SERVER CONFIGURATION ----------
const char* ssid = "WiFi_SSID_Name";
const char* password = "WiFi_Password";
const char* serverURL = "http://192.168.43.167:5002/api/sensor-data";

// ---------- PINS ----------
#define TEMP_PIN 4 
#define BUZZER_PIN 5
#define PANIC_BUTTON_PIN 25
#define MIC_BUTTON_PIN 26

// Hardware Identification
String deviceId = "c3_wrist_belt";
String beltType = "Wrist Belt";
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

#define IR_WORN_THRESHOLD 4500
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

// ---------- BLE VARIABLES ----------
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// ---------- VARIABLES ----------
bool beltWorn = false;
float heartRate = 0;
float spo2 = 0;
float bodyTemp = 0;
unsigned long fallTime = 0;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000;
String micMessage = "";
unsigned long lastBLEBroadcast = 0;
const unsigned long BLE_BROADCAST_INTERVAL = 500;

// =========================================================================
//  BLE SERVER CALLBACKS
// =========================================================================
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("✅ [BLE] Wrist band connected to Waist band");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("❌ [BLE] Wrist band disconnected from Waist band");
    // Restart advertising
    pServer->getAdvertising()->start();
    Serial.println("🔄 [BLE] Advertising restarted");
  }
};

// =========================================================================
//  SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MIC_BUTTON_PIN, INPUT_PULLUP);

  // ESP32-C3 I2C Pins (SDA: 18, SCL: 19)
  Wire.begin(18, 19);

  Serial.println("=== SILVERCARE SENIOR SAFETY WRIST BELT (BLE SERVER) ===");

  // ========== GSM Module Initialization ==========
  gsmSerial.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);
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

  if (!maxSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("❌ MAX30102 NOT FOUND");
    while (1);
  }

  maxSensor.setup();
  tempSensor.begin();

  // ========== BLE Initialization ==========
  NimBLEDevice::init(DEVICE_NAME);
  
  // Create BLE Server
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Create BLE Service
  BLEService* pService = pServer->createService(BLE_SERVICE_UUID);
  
  // Create BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    BLE_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ | 
    NIMBLE_PROPERTY::NOTIFY
  );
  
  // Start Service
  pService->start();
  
  // Start Advertising
  BLEAdvertising* pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  pAdvertising->start();
  
  Serial.println("✅ BLE Server started - Waiting for Waist band to connect");
  Serial.println("=================================================");
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

void broadcastHeartData() {
  if (!deviceConnected) return;
  
  // Format: "HR:SPO2:IR:TEMP"
  String dataPacket = String(heartRate) + ":" + 
                      String(spo2) + ":" + 
                      String(maxSensor.getIR()) + ":" + 
                      String(bodyTemp);
  
  pCharacteristic->setValue(dataPacket.c_str());
  pCharacteristic->notify();
  
  Serial.println("📡 [BLE] Data sent to Waist: " + dataPacket);
}

// =========================================================================
//  MAIN LOOP
// =========================================================================
void loop() {
  // ---------- MPU6050 READING ----------
  sensors_event_t acc, gyro, temp;
  mpu.getEvent(&acc, &gyro, &temp);

  float ax = acc.acceleration.x;
  float ay = acc.acceleration.y;
  float az = acc.acceleration.z;
  float accMag = sqrt(ax * ax + ay * ay + az * az);
  float accMagG = accMag / 9.8;

  // ---------- MAX30102 READING ----------
  long irValue = maxSensor.getIR();
  long redValue = maxSensor.getRed();

  // More realistic HR/SpO2 mapping
  heartRate = map(irValue, 5000, 50000, 60, 110);
  if (heartRate < 40) heartRate = 0;
  if (heartRate > 180) heartRate = 180;
  
  spo2 = map(redValue, 5000, 50000, 88, 98);
  if (spo2 < 70) spo2 = 0;
  if (spo2 > 100) spo2 = 100;

  // ---------- TEMPERATURE READING ----------
  tempSensor.requestTemperatures();
  bodyTemp = tempSensor.getTempCByIndex(0);

  // ---------- BELT WORN CHECK ----------
  beltWorn = (irValue > IR_WORN_THRESHOLD && bodyTemp > TEMP_WORN_THRESHOLD);
  bool vitalsAbnormal = (heartRate < HR_LOW || heartRate > HR_HIGH || spo2 < SPO2_LOW);

  // ---------- MICROPHONE / PANIC BUTTON TRIGGER ----------
  if (digitalRead(MIC_BUTTON_PIN) == LOW || digitalRead(PANIC_BUTTON_PIN) == LOW) {
    micMessage = "Senior citizen pressed Wrist Belt Panic/Mic button: 'Emergency! Please assist!'";
    currentState = FALL_DETECTED;
    Serial.println("🚨 [WRIST PANIC BUTTON PRESSED]: " + micMessage);
  }

  // ---------- STATE MACHINE ----------
  if (currentState == FALL_DETECTED) {
    goto STATE_OUTPUT;
  }

  if (accMagG > INSTABILITY_THRESHOLD && accMagG <= SUDDEN_THRESHOLD && beltWorn && vitalsAbnormal) {
    currentState = PREFALL;
    micMessage = "Pre-fall instability detected on Wrist Belt. Voice prompt triggered.";
  } else if (accMagG > SUDDEN_THRESHOLD && accMagG <= FALL_THRESHOLD) {
    currentState = SUDDEN_MOVEMENT;
  } else if (accMagG > FALL_THRESHOLD && beltWorn) {
    currentState = FALL_DETECTED;
    fallTime = millis();
    micMessage = "EMERGENCY: Heavy impact detected on Wrist Belt! Immediate assistance required.";
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

  // ---------- BROADCAST HEART DATA VIA BLE ----------
  unsigned long currentMillis = millis();
  if (currentMillis - lastBLEBroadcast >= BLE_BROADCAST_INTERVAL) {
    broadcastHeartData();
    lastBLEBroadcast = currentMillis;
  }

  // ---------- SEND TELEMETRY TO SPRING BOOT SERVER ----------
  if (currentMillis - lastSendTime >= SEND_INTERVAL) {
    sendDataToServer(currentState, heartRate, spo2, bodyTemp, beltWorn, accMagG, micMessage);
    lastSendTime = currentMillis;
  }

  // ---------- BLE CONNECTION STATE HANDLING ----------
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->getAdvertising()->start();
    Serial.println("🔄 [BLE] Advertising restarted");
    oldDeviceConnected = deviceConnected;
  }
  
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  delay(100);
}
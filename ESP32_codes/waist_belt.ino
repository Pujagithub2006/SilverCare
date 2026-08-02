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

// =========================================================================
//  SILVERCARE - SENIOR SAFETY WAIST BELT (ESP32 BLE CLIENT)
// =========================================================================

// ---------- BLE CONFIGURATION ----------
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define WRIST_DEVICE_NAME       "SilverCare_Wrist"

// ---------- GSM CONFIGURATION ----------
#define GSM_TX 16
#define GSM_RX 17
#define GSM_BAUD 9600
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);

// ---------- WiFi & BACKEND SERVER CONFIGURATION ----------
const char* ssid = "WiFi_SSID_Name";
const char* password = "WiFi_Password";
const char* serverURL = "http://192.168.43.167:5002/api/sensor-data";

// ---------- PINS ----------
#define TEMP_PIN 4 
#define BUZZER_PIN 18
#define BUTTON_PIN 19
#define MIC_BUTTON_PIN 23

// Hardware Identification
String deviceId = "vois_belt";
String beltType = "Waist Belt";
double gpsLat = 18.5204;
double gpsLng = 73.8567;

// ---------- OBJECTS ----------
Adafruit_MPU6050 mpu;
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

// ---------- BLE VARIABLES ----------
bool deviceConnected = false;
bool scanning = false;
unsigned long lastScanTime = 0;
const unsigned long SCAN_INTERVAL = 5000;

// BLE Client objects
NimBLEClient* pClient = NULL;
NimBLERemoteCharacteristic* pRemoteCharacteristic = NULL;
NimBLERemoteService* pRemoteService = NULL;

// ---------- RECEIVED HEART DATA ----------
float heartRate = 0;
float spo2 = 0;
long irValue = 0;
float bodyTemp = 0;
bool heartDataReceived = false;
unsigned long lastHeartDataTime = 0;
const unsigned long HEART_DATA_TIMEOUT = 3000;

// ---------- VARIABLES ----------
bool beltWorn = false;
unsigned long fallTime = 0;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000;
String micMessage = "";
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

// =========================================================================
//  BLE CLIENT CALLBACKS
// =========================================================================
class MyClientCallback : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) {
    deviceConnected = true;
    heartDataReceived = false;
    Serial.println("✅ [BLE] Waist band connected to Wrist band");
  }

  void onDisconnect(NimBLEClient* pClient) {
    deviceConnected = false;
    pClient = NULL;
    pRemoteCharacteristic = NULL;
    pRemoteService = NULL;
    Serial.println("❌ [BLE] Waist band disconnected from Wrist band");
    Serial.println("🔄 [BLE] Will attempt to reconnect...");
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

  Serial.println("=== SILVERCARE SENIOR SAFETY WAIST BELT (BLE CLIENT) ===");

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

  tempSensor.begin();

  // ========== BLE Initialization ==========
  NimBLEDevice::init("");
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
  
  Serial.println("✅ BLE Client initialized - Searching for Wrist band");
  Serial.println("=================================================");
  
  // Start BLE scan
  startBLEScan();
}

// =========================================================================
//  BLE FUNCTIONS
// =========================================================================
void startBLEScan() {
  if (scanning) return;
  
  scanning = true;
  Serial.println("🔍 [BLE] Scanning for Wrist band...");
  
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(50);
  
  NimBLEScanResults results = pScan->start(3, false);
  
  int foundCount = results.getCount();
  Serial.print("🔍 [BLE] Found ");
  Serial.print(foundCount);
  Serial.println(" devices");
  
  for (int i = 0; i < foundCount; i++) {
    NimBLEAdvertisedDevice device = results.getDevice(i);
    String deviceName = device.getName().c_str();
    
    if (deviceName == WRIST_DEVICE_NAME) {
      Serial.print("✅ [BLE] Found Wrist band: ");
      Serial.println(deviceName);
      
      // Stop scanning and connect
      scanning = false;
      connectToWristBand(&device);
      return;
    }
  }
  
  scanning = false;
  Serial.println("⚠️ [BLE] Wrist band not found, will scan again...");
}

void connectToWristBand(NimBLEAdvertisedDevice* pDevice) {
  if (pClient != NULL) {
    // Clean up old client
    if (pClient->isConnected()) {
      pClient->disconnect();
    }
    NimBLEDevice::deleteClient(pClient);
    pClient = NULL;
  }
  
  Serial.print("🔗 [BLE] Connecting to Wrist band: ");
  Serial.println(pDevice->getName().c_str());
  
  pClient = NimBLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback(), false);
  pClient->setConnectionParams(12, 12, 0, 48);
  pClient->setConnectTimeout(5);
  
  if (!pClient->connect(pDevice)) {
    Serial.println("❌ [BLE] Failed to connect");
    NimBLEDevice::deleteClient(pClient);
    pClient = NULL;
    return;
  }
  
  Serial.println("✅ [BLE] Connected to Wrist band!");
  
  // Get service and characteristic
  pRemoteService = pClient->getService(BLE_SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("❌ [BLE] Failed to find service");
    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    pClient = NULL;
    return;
  }
  
  pRemoteCharacteristic = pRemoteService->getCharacteristic(BLE_CHARACTERISTIC_UUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("❌ [BLE] Failed to find characteristic");
    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    pClient = NULL;
    return;
  }
  
  // Subscribe to notifications
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->subscribe(true, notifyCallback);
    Serial.println("✅ [BLE] Subscribed to heart data notifications");
  }
}

void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  String data = String((char*)pData).substring(0, length);
  Serial.print("📥 [BLE] Received data: ");
  Serial.println(data);
  
  // Parse data: "HR:SPO2:IR:TEMP"
  int firstColon = data.indexOf(':');
  int secondColon = data.indexOf(':', firstColon + 1);
  int thirdColon = data.indexOf(':', secondColon + 1);
  
  if (firstColon > 0 && secondColon > 0 && thirdColon > 0) {
    heartRate = data.substring(0, firstColon).toFloat();
    spo2 = data.substring(firstColon + 1, secondColon).toFloat();
    irValue = data.substring(secondColon + 1, thirdColon).toFloat();
    bodyTemp = data.substring(thirdColon + 1).toFloat();
    
    heartDataReceived = true;
    lastHeartDataTime = millis();
    
    Serial.print("❤️ HR: ");
    Serial.print(heartRate);
    Serial.print(" | SpO2: ");
    Serial.print(spo2);
    Serial.print(" | IR: ");
    Serial.print(irValue);
    Serial.print(" | Temp: ");
    Serial.println(bodyTemp);
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

  Serial.print("📤 [WAIST BELT TELEMETRY SENT]: ");
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
  // ---------- BLE CONNECTION MANAGEMENT ----------
  if (!deviceConnected) {
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = millis();
      
      if (pClient != NULL) {
        // Try to reconnect to existing client
        if (!pClient->isConnected()) {
          NimBLEDevice::deleteClient(pClient);
          pClient = NULL;
          pRemoteCharacteristic = NULL;
          pRemoteService = NULL;
        }
      }
      
      if (pClient == NULL) {
        startBLEScan();
      }
    }
  }

  // ---------- CHECK HEART DATA TIMEOUT ----------
  if (heartDataReceived && (millis() - lastHeartDataTime > HEART_DATA_TIMEOUT)) {
    heartDataReceived = false;
    heartRate = 0;
    spo2 = 0;
    irValue = 0;
    Serial.println("⚠️ [BLE] Heart data timeout - no data received");
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
  float bodyTemp = tempSensor.getTempCByIndex(0);

  // ---------- BELT WORN CHECK (Using BLE-received IR value) ----------
  #define IR_WORN_THRESHOLD 4500
  beltWorn = (irValue > IR_WORN_THRESHOLD && bodyTemp > TEMP_WORN_THRESHOLD);
  bool vitalsAbnormal = (heartRate < HR_LOW || heartRate > HR_HIGH || spo2 < SPO2_LOW);

  // ---------- MICROPHONE TRIGGER CHECK ----------
  if (digitalRead(MIC_BUTTON_PIN) == LOW) {
    micMessage = "Senior citizen pressed Waist Mic button: 'Help needed!'";
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
    micMessage = "Pre-fall instability detected on Waist Belt. Asking: 'Are you okay?'";
  } else if (accMagG > SUDDEN_THRESHOLD && accMagG <= FALL_THRESHOLD) {
    currentState = SUDDEN_MOVEMENT;
  } else if (accMagG > FALL_THRESHOLD && beltWorn) {
    currentState = FALL_DETECTED;
    fallTime = millis();
    micMessage = "EMERGENCY: Fall impact detected on Waist Belt! Urgent help requested.";
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
      sendSmartSMSAlert("PRE-FALL", "Pre-fall detected on Waist Belt! Please check senior ward.");
      break;

    case SUDDEN_MOVEMENT:
      digitalWrite(BUZZER_PIN, LOW);
      break;

    case FALL_DETECTED:
      digitalWrite(BUZZER_PIN, HIGH);
      sendSmartSMSAlert("FALL", "EMERGENCY: Fall detected on Waist Belt! Immediate assistance required!");
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
    // Use BLE-received heart data, not local sensor
    sendDataToServer(currentState, heartRate, spo2, bodyTemp, beltWorn, accMagG, micMessage);
    lastSendTime = currentTime;
  }

  delay(100);
}
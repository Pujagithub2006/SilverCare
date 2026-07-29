#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

// ---------- SMS FALLBACK VARIABLES ----------
bool twilioSMSSent = false;
bool gsmSMSSent = false;
unsigned long lastSMSTime = 0;
const unsigned long SMS_RETRY_INTERVAL = 30000; // 30 seconds between SMS

// ---------- GSM CONFIGURATION ----------

#define GSM_TX 16  // ESP32 RX → GSM TX
#define GSM_RX 17  // ESP32 TX → GSM RX
#define GSM_BAUD 9600
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);

// ---------- WiFi CONFIGURATION ----------
const char* ssid = "...";
const char* password = "43214321";
const char* serverURL = "http://192.168.43.167:5002/api/sensor-data";

// ---------- PINS ----------
#define TEMP_PIN 4 
#define BUZZER_PIN 18
#define BUTTON_PIN 19

// Device ID - This identifies which elderly person this belt belongs to
String deviceId = "vois_belt"; // Change this to link to different elderly person
// Example: "gauri_shiv", "john_doe", etc.
// The backend will lookup guardians for this elderly person

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
SystemState lastState = NORMAL;

// ---------- VARIABLES ----------
bool beltWorn = false;
float heartRate = 0;
float spo2 = 0;
unsigned long fallTime = 0;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000; // Send every 1 second

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(21, 22);

  Serial.println("=== SENIOR SAFETY BELT SYSTEM START ===");

  // ========== GSM Module Initialization ==========
  gsmSerial.begin(GSM_BAUD);
  Serial.println("📱 Initializing GSM Module...");
  delay(1000);
  
  // Test GSM module
  gsmSerial.println("AT");
  delay(1000);
  if (gsmSerial.available()) {
    Serial.println("✅ GSM Module Responding");
  } else {
    Serial.println("❌ GSM Module Not Responding");
  }
  
  // Set SMS mode to text
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
    Serial.println("\n❌ WiFi Failed - Will continue with local operation");
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

  Serial.println("✅ ALL SENSORS INITIALIZED");
  Serial.println("=====================================");
}

void sendDataToServer(SystemState state, float hr, float oxygen, float temp, bool worn, float acc) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi not connected - skipping send");
    return;
  }

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");

  // Create JSON payload
  StaticJsonDocument<256> doc;
  doc["state"] = (int)state;
  doc["stateName"] = getStateName(state);
  doc["heartRate"] = hr;
  doc["spo2"] = oxygen;
  doc["temperature"] = temp;
  doc["beltWorn"] = worn;
  doc["acceleration"] = acc;
  doc["deviceId"] = deviceId; // Send elderly person's ID instead of hardcoded phone
  doc["timestamp"] = millis();

  String payload;
  serializeJson(doc, payload);

  Serial.print("📤 Sending: ");
  Serial.println(payload);

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    Serial.print("✅ Server Response: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println(response);
  } else {
    Serial.print("❌ HTTP Error: ");
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

// ========== SMART SMS FALLBACK FUNCTION ==========
void sendSmartSMSAlert(String alertType, String message) {
  unsigned long currentTime = millis();
  
  // Check if enough time has passed since last SMS
  if (currentTime - lastSMSTime < SMS_RETRY_INTERVAL) {
    Serial.println("📱 [SMS] Skipping - too soon since last SMS");
    return;
  }
  
  // Reset SMS flags for new alert
  if (alertType != lastAlertType) {
    twilioSMSSent = false;
    gsmSMSSent = false;
  }
  
  Serial.println("📱 [SMS] Smart SMS System Activated");
  
  // Try Twilio SMS first (if WiFi available)
  if (WiFi.status() == WL_CONNECTED && !twilioSMSSent) {
    Serial.println("📡 [TWILIO] Attempting SMS via WiFi...");
    bool twilioSuccess = sendTwilioSMS(alertType, message);
    
    if (twilioSuccess) {
      twilioSMSSent = true;
      lastSMSTime = currentTime;
      Serial.println("✅ [TWILIO] SMS sent successfully - GSM backup not needed");
      return;
    } else {
      Serial.println("❌ [TWILIO] SMS failed - Will try GSM backup");
    }
  } else if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ [WIFI] No connection - Skipping Twilio, using GSM");
  } else {
    Serial.println("📡 [TWILIO] SMS already sent - GSM backup not needed");
  }
  
  // Fallback to GSM SMS if Twilio failed or no WiFi
  if (!gsmSMSSent) {
    Serial.println("📱 [GSM] Sending backup SMS...");
    bool gsmSuccess = sendGSMAlert(alertType, message);
    
    if (gsmSuccess) {
      gsmSMSSent = true;
      lastSMSTime = currentTime;
      Serial.println("✅ [GSM] Backup SMS sent successfully");
    } else {
      Serial.println("❌ [GSM] Backup SMS also failed - No SMS sent!");
    }
  }
}

// ========== TWILIO SMS FUNCTION ==========
bool sendTwilioSMS(String alertType, String message) {
  // Create JSON payload for Twilio SMS request
  StaticJsonDocument<200> doc;
  doc["alert_type"] = alertType;
  doc["message"] = message;
  doc["device_id"] = deviceId;
  doc["timestamp"] = millis();
  
  String payload;
  serializeJson(doc, payload);
  
  HTTPClient http;
  http.begin("http://192.168.43.167:5002/api/twilio-sms");
  http.addHeader("Content-Type", "application/json");
  
  Serial.print("📡 [TWILIO] Sending SMS request: ");
  Serial.println(payload);
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.println("✅ [TWILIO] SMS request successful: " + response);
    http.end();
    return true;
  } else {
    Serial.print("❌ [TWILIO] SMS request failed: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}

// ========== GSM SMS FUNCTION ==========
bool sendGSMAlert(String alertType, String message) {
  Serial.println("📱 [GSM] Sending SMS Alert...");
  
  // Guardian phone number (can be configured)
  String guardianPhone = "+919322757538"; // Update with actual guardian number
  
  // Send SMS command
  gsmSerial.println("AT+CMGS=\"" + guardianPhone + "\"");
  delay(1000);
  
  // Send message content
  String smsMessage = "🚨 SILVERCARE ALERT - " + alertType + "\n" + message + "\nDevice: " + deviceId + "\nTime: " + millis();
  gsmSerial.println(smsMessage);
  delay(1000);
  
  // Send Ctrl+Z to finish
  gsmSerial.write(26);
  delay(3000);
  
  // Check response
  while(gsmSerial.available()) {
    String response = gsmSerial.readString();
    Serial.println("GSM Response: " + response);
    if (response.indexOf("OK") != -1) {
      Serial.println("✅ [GSM] SMS Sent Successfully!");
      return true;
    } else if (response.indexOf("ERROR") != -1) {
      Serial.println("❌ [GSM] SMS Failed");
      return false;
    }
  }
  
  Serial.println("❌ [GSM] No response from module");
  return false;
}

// Global variable to track last alert type
String lastAlertType = "";

void loop() {
  // ---------- MPU6050 ----------
  sensors_event_t acc, gyro, temp;
  mpu.getEvent(&acc, &gyro, &temp);

  float ax = acc.acceleration.x;
  float ay = acc.acceleration.y;
  float az = acc.acceleration.z;
  float accMag = sqrt(ax * ax + ay * ay + az * az);
  float accMagG = accMag / 9.8;

  // ---------- MAX30102 ----------
  long irValue = maxSensor.getIR();
  long redValue = maxSensor.getRed();

  heartRate = map(irValue, 5000, 50000, 60, 110);
  spo2 = map(redValue, 5000, 50000, 88, 98);

  // ---------- TEMPERATURE ----------
  tempSensor.requestTemperatures();
  float bodyTemp = tempSensor.getTempCByIndex(0);

  // ---------- BELT WORN ----------
  beltWorn = (irValue > IR_WORN_THRESHOLD && bodyTemp > TEMP_WORN_THRESHOLD);

  bool vitalsAbnormal =
    (heartRate < HR_LOW || heartRate > HR_HIGH || spo2 < SPO2_LOW);

  // ---------- STATE PERSISTENCE ----------
  if (currentState == FALL_DETECTED) {
    goto STATE_OUTPUT;
  }

  if (
    accMagG > INSTABILITY_THRESHOLD &&
    accMagG <= SUDDEN_THRESHOLD &&
    beltWorn &&
    vitalsAbnormal
  ) {
    currentState = PREFALL;
  }

  else if (
    accMagG > SUDDEN_THRESHOLD &&
    accMagG <= FALL_THRESHOLD
  ) {
    currentState = SUDDEN_MOVEMENT;
  }

  else if (
    accMagG > FALL_THRESHOLD &&
    beltWorn
  ) {
    currentState = FALL_DETECTED;
    fallTime = millis();
  }

  else {
    currentState = NORMAL;
  }

  // ---------- SERIAL MONITOR OUTPUT ----------
  Serial.println("\n================ SYSTEM STATUS ================");
  Serial.print("Acceleration (G): "); Serial.println(accMagG);

  Serial.print("IR Value: "); Serial.println(irValue);
  Serial.print("RED Value: "); Serial.println(redValue);

  Serial.print("Heart Rate (BPM): "); Serial.println(heartRate);
  Serial.print("SpO2 (%): "); Serial.println(spo2);

  Serial.print("Body Temperature (°C): "); Serial.println(bodyTemp);
  Serial.print("Belt Worn: "); Serial.println(beltWorn ? "YES" : "NO");

  Serial.print("FINAL STATE: ");
  STATE_OUTPUT:
  switch (currentState) {
    case NORMAL:
      Serial.println("✅ NORMAL");
      digitalWrite(BUZZER_PIN, LOW);
      break;

    case PREFALL:
      Serial.println("⚠️ PRE-FALL (INSTABILITY + ABNORMAL VITALS)");
      Serial.println("ACTION: Voice Prompt → 'Are you okay?'");
      Serial.println("ACTION: Smart SMS System Activated");
      digitalWrite(BUZZER_PIN, LOW);
      sendSmartSMSAlert("PRE-FALL", "Pre-fall detected! Please check on elderly person.");
      break;

    case SUDDEN_MOVEMENT:
      Serial.println("⚠️ SUDDEN MOVEMENT (MOTION ONLY)");
      digitalWrite(BUZZER_PIN, LOW);
      break;

    case FALL_DETECTED:
      Serial.println("🚨 FALL CONFIRMED");
      Serial.println("ACTION: CALL GUARDIAN");
      Serial.println("ACTION: SEND EMERGENCY SMS");
      Serial.println("ACTION: Smart SMS System Activated");
      digitalWrite(BUZZER_PIN, HIGH);
      sendSmartSMSAlert("FALL", "EMERGENCY: Fall detected! Immediate assistance required!");
      break;
  }

  // ---------- USER OVERRIDE ----------
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("USER RESPONSE: I'M OK");
    digitalWrite(BUZZER_PIN, LOW);
    currentState = NORMAL;
  }

  // ---------- SEND DATA TO SERVER (throttled) ----------
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    sendDataToServer(currentState, heartRate, spo2, bodyTemp, beltWorn, accMagG);
    lastSendTime = currentTime;
  }

  Serial.println("==============================================");
  delay(500);
}


//ESP32 code (change IP address according to the laptop where website is running)


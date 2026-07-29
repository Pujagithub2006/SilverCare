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

// =========================================================================
//  SILVERCARE - SENIOR SAFETY WRIST BELT (ESP32-C3 WRIST BOARD)
// =========================================================================

// ---------- SMS FALLBACK VARIABLES ----------
bool twilioSMSSent = false;
bool gsmSMSSent = false;
unsigned long lastSMSTime = 0;
const unsigned long SMS_RETRY_INTERVAL = 30000; // 30 seconds between SMS

// ---------- GSM CONFIGURATION ----------
#define GSM_TX 20  // ESP32-C3 RX → GSM TX
#define GSM_RX 21  // ESP32-C3 TX → GSM RX
#define GSM_BAUD 9600
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);

// ---------- WiFi & BACKEND SERVER CONFIGURATION ----------
const char* ssid = "WiFi_SSID_Name";
const char* password = "WiFi_Password";
// Public Server / Backend Endpoint URL (port 5002)
const char* serverURL = "http://192.168.43.167:5002/api/sensor-data";

// ---------- PINS (Tuned for ESP32-C3) ----------
#define TEMP_PIN 4 
#define BUZZER_PIN 5
#define PANIC_BUTTON_PIN 6
#define MIC_BUTTON_PIN 7

// Hardware Identification
String deviceId = "c3_wrist_belt";  // Wrist Belt Device ID
String beltType = "Wrist Belt";     // Hardware Belt Classification
double gpsLat = 18.5204;             // GPS Latitude
double gpsLng = 73.8567;             // GPS Longitude

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
String lastAlertType = "";

// ---------- VARIABLES ----------
bool beltWorn = false;
float heartRate = 0;
float spo2 = 0;
unsigned long fallTime = 0;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000; // Send every 1 second
String micMessage = "";

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MIC_BUTTON_PIN, INPUT_PULLUP);

  // ESP32-C3 I2C Pins (SDA: 8, SCL: 9)
  Wire.begin(8, 9);

  Serial.println("=== SILVERCARE SENIOR SAFETY WRIST BELT (ESP32-C3) START ===");

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

  if (!maxSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("❌ MAX30102 NOT FOUND");
    while (1);
  }

  maxSensor.setup();
  tempSensor.begin();

  Serial.println("✅ ALL WRIST BELT SENSORS INITIALIZED SUCCESSFULLY");
  Serial.println("=================================================");
}

void sendDataToServer(SystemState state, float hr, float oxygen, float temp, bool worn, float acc, String micAudio) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi not connected - skipping send");
    return;
  }

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");

  // Create JSON payload matching Spring Boot SensorDataRequest DTO
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

    // If Guardian acknowledged "I am Fine" on frontend, clear local buzzer
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

// ========== SMART SMS FALLBACK FUNCTION ==========
void sendSmartSMSAlert(String alertType, String message) {
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

  heartRate = map(irValue, 5000, 50000, 60, 110);
  spo2 = map(redValue, 5000, 50000, 88, 98);

  // ---------- TEMPERATURE READING ----------
  tempSensor.requestTemperatures();
  float bodyTemp = tempSensor.getTempCByIndex(0);

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

  // ---------- SEND TELEMETRY TO SPRING BOOT SERVER ----------
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    sendDataToServer(currentState, heartRate, spo2, bodyTemp, beltWorn, accMagG, micMessage);
    lastSendTime = currentTime;
  }

  delay(500);
}

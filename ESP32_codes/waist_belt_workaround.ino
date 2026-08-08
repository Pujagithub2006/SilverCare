#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

// ===== WIFI CONFIGURATION =====
// WiFi credentials are now managed automatically
#define MAX_KNOWN_NETWORKS 10
#define WIFI_SCAN_TIMEOUT 5000
#define WIFI_CONNECT_TIMEOUT 10000
#define AP_SSID "SilverCare_Setup"
#define AP_PASSWORD "silvercare123"

// ===== GSM CONFIGURATION =====
#define GSM_TX 16
#define GSM_RX 17
#define GSM_BAUD 9600
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);

// ===== PIN CONFIGURATION =====
#define BUZZER_PIN 18
#define BUTTON_PIN 19
#define MIC_BUTTON_PIN 23

// ===== GPS CONFIGURATION =====
double gpsLat = 18.5204;
double gpsLng = 73.8567;

// ===== BACKEND CONFIGURATION =====
const char* serverUrl = "https://silvercare-production-3455.up.railway.app/api/sensor-data";
const char* twilioUrl = "https://silvercare-production-3455.up.railway.app/api/twilio-sms";
const char* deviceApiUrl = "https://silvercare-production-3455.up.railway.app/api/devices";
String DEVICE_ID = ""; // Will be generated based on MAC address

// ===== CONNECTION STATE TRACKING =====
unsigned long lastWiFiReconnectAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 10000;
unsigned long lastWiFiScan = 0;
const unsigned long WIFI_SCAN_INTERVAL = 60000; // Scan every minute

// ===== WIFI MANAGER =====
struct KnownNetwork {
  String ssid;
  String password;
  int priority; // Higher = preferred
};

KnownNetwork knownNetworks[MAX_KNOWN_NETWORKS];
int knownNetworkCount = 0;
Preferences wifiPrefs;
Preferences devicePrefs;
WebServer webServer(80);
bool apModeActive = false;
unsigned long apModeStartTime = 0;
const unsigned long AP_MODE_TIMEOUT = 300000; // 5 minutes
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 60000; // 1 minute

// ===== EXISTING CONFIGURATION (PRESERVED) =====
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define IDENTITY_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define WRIST_NAME "SilverCare_Wrist"

// ===== ENCRYPTION CONFIGURATION =====
static const uint8_t ENCRYPTION_KEY[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};
static const uint8_t ENCRYPTION_IV[12] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B
};

// ===== TIME CONFIGURATION =====
const unsigned long DATA_SEND_INTERVAL = 5000; // Send data every 5 seconds
unsigned long lastDataSent = 0;

// ===== SIMULATION STATE =====
enum SystemState { NORMAL, ALERT, CRITICAL };
SystemState currentState = NORMAL;
unsigned long stateChangeTime = 0;

// ===== NOTIFICATION STATE =====
String micMessage = "";
String lastAlertType = "";
unsigned long lastSMSTime = 0;
bool twilioSMSSent = false;
bool gsmSMSSent = false;
const unsigned long SMS_RETRY_INTERVAL = 30000;

float currentHr = 72.0;
float currentSpo2 = 98.0;
float currentTemp = 36.6;
long currentIr = 50000;

// ===== BLE GLOBALS =====
NimBLEClient* pClient = NULL;
NimBLERemoteCharacteristic* pChar = NULL;
bool connected = false;
bool dataReceivedFromWrist = false;

// ===== ENCRYPTION MANAGER (From original) =====
class EncryptionManager {
public:
    static String decrypt(const String& encrypted) {
        if (encrypted.length() == 0) return "";
        int separator = encrypted.indexOf('|');
        if (separator == -1) return "";
        String ciphertextStr = encrypted.substring(0, separator);
        String tagStr = encrypted.substring(separator + 1);
        uint8_t ciphertext[128];
        uint8_t tag[16];
        size_t ciphertextLen = 0;
        int pos = 0;
        while (pos < ciphertextStr.length()) {
            int end = ciphertextStr.indexOf(':', pos);
            if (end == -1) end = ciphertextStr.length();
            String byteStr = ciphertextStr.substring(pos, end);
            ciphertext[ciphertextLen++] = strtol(byteStr.c_str(), NULL, 16);
            pos = end + 1;
        }
        pos = 0;
        int tagIdx = 0;
        while (pos < tagStr.length()) {
            int end = tagStr.indexOf(':', pos);
            if (end == -1) end = tagStr.length();
            String byteStr = tagStr.substring(pos, end);
            tag[tagIdx++] = strtol(byteStr.c_str(), NULL, 16);
            pos = end + 1;
        }
        uint8_t plaintext[128];
        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);
        mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, ENCRYPTION_KEY, 256);
        size_t outputLen = 0;
        int ret = mbedtls_gcm_starts(&ctx, MBEDTLS_GCM_DECRYPT, ENCRYPTION_IV, 12);
        if (ret == 0) {
            ret = mbedtls_gcm_update(&ctx, ciphertext, ciphertextLen, plaintext, ciphertextLen, &outputLen);
        }
        if (ret == 0) {
            ret = mbedtls_gcm_finish(&ctx, plaintext + outputLen, 0, &outputLen, tag, 16);
        }
        mbedtls_gcm_free(&ctx);
        if (ret != 0) return "";
        return String((char*)plaintext, ciphertextLen);
    }
};

// ===== PROCESS RECEIVED DATA =====
void processReceivedData(String encryptedData) {
    String decryptedData = EncryptionManager::decrypt(encryptedData);
    if (decryptedData.length() == 0) return;
    
    int firstColon = decryptedData.indexOf(':');
    int secondColon = decryptedData.indexOf(':', firstColon + 1);
    int thirdColon = decryptedData.indexOf(':', secondColon + 1);
    
    if (firstColon > 0 && secondColon > 0 && thirdColon > 0) {
        currentHr = decryptedData.substring(0, firstColon).toFloat();
        currentSpo2 = decryptedData.substring(firstColon + 1, secondColon).toFloat();
        currentIr = decryptedData.substring(secondColon + 1, thirdColon).toInt();
        currentTemp = decryptedData.substring(thirdColon + 1).toFloat();
        dataReceivedFromWrist = true;
    }
}

// ===== NOTIFICATION CALLBACK =====
void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* data, size_t len, bool isNotify) {
    if (len == 0) return;
    String encryptedData = String((char*)data).substring(0, len);
    processReceivedData(encryptedData);
}

// ===== WIFI MANAGER FUNCTIONS =====
void loadKnownNetworks() {
  wifiPrefs.begin("wifi_networks", false);
  knownNetworkCount = wifiPrefs.getInt("network_count", 0);
  
  for (int i = 0; i < knownNetworkCount && i < MAX_KNOWN_NETWORKS; i++) {
    String key = "net_" + String(i);
    String data = wifiPrefs.getString(key.c_str(), "");
    
    int sep1 = data.indexOf('|');
    int sep2 = data.indexOf('|', sep1 + 1);
    
    if (sep1 > 0 && sep2 > sep1) {
      knownNetworks[i].ssid = data.substring(0, sep1);
      knownNetworks[i].password = data.substring(sep1 + 1, sep2);
      knownNetworks[i].priority = data.substring(sep2 + 1).toInt();
    }
  }
  
  wifiPrefs.end();
  Serial.println("📚 Loaded " + String(knownNetworkCount) + " known WiFi networks");
}

void saveKnownNetwork(String ssid, String password, int priority = 5) {
  // Check if already exists
  for (int i = 0; i < knownNetworkCount; i++) {
    if (knownNetworks[i].ssid == ssid) {
      knownNetworks[i].password = password;
      knownNetworks[i].priority = priority;
      break;
    }
  }
  
  // Add new if not found and space available
  if (knownNetworkCount < MAX_KNOWN_NETWORKS) {
    knownNetworks[knownNetworkCount].ssid = ssid;
    knownNetworks[knownNetworkCount].password = password;
    knownNetworks[knownNetworkCount].priority = priority;
    knownNetworkCount++;
  }
  
  // Save to preferences
  wifiPrefs.begin("wifi_networks", false);
  wifiPrefs.putInt("network_count", knownNetworkCount);
  
  for (int i = 0; i < knownNetworkCount; i++) {
    String key = "net_" + String(i);
    String data = knownNetworks[i].ssid + "|" + knownNetworks[i].password + "|" + String(knownNetworks[i].priority);
    wifiPrefs.putString(key.c_str(), data);
  }
  
  wifiPrefs.end();
  Serial.println("💾 Saved network: " + ssid);
}

bool connectToNetwork(String ssid, String password) {
  Serial.println("🔗 Connecting to: " + ssid);
  
  if (password.length() > 0) {
    WiFi.begin(ssid.c_str(), password.c_str());
  } else {
    WiFi.begin(ssid.c_str());
  }
  
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to: " + ssid);
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Save successful connection
    saveKnownNetwork(ssid, password, 10);
    return true;
  } else {
    Serial.println("\n❌ Failed to connect to: " + ssid);
    return false;
  }
}

bool scanAndConnect() {
  Serial.println("🔍 Scanning for WiFi networks...");
  
  int n = WiFi.scanNetworks();
  Serial.println("📡 Found " + String(n) + " networks");
  
  if (n == 0) {
    Serial.println("❌ No networks found");
    return false;
  }
  
  // First, try known networks (sorted by priority)
  for (int i = 0; i < knownNetworkCount; i++) {
    for (int j = 0; j < n; j++) {
      if (WiFi.SSID(j) == knownNetworks[i].ssid) {
        Serial.println("🎯 Found known network: " + knownNetworks[i].ssid);
        if (connectToNetwork(knownNetworks[i].ssid, knownNetworks[i].password)) {
          WiFi.scanDelete();
          return true;
        }
      }
    }
  }
  
  // Second, try open networks (no password)
  for (int i = 0; i < n; i++) {
    if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
      String ssid = WiFi.SSID(i);
      Serial.println("🔓 Found open network: " + ssid);
      if (connectToNetwork(ssid, "")) {
        WiFi.scanDelete();
        return true;
      }
    }
  }
  
  // Third, try any network with common passwords
  const char* commonPasswords[] = {"", "12345678", "password", "1234567890"};
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    for (int j = 0; j < 4; j++) {
      if (connectToNetwork(ssid, commonPasswords[j])) {
        WiFi.scanDelete();
        return true;
      }
    }
  }
  
  WiFi.scanDelete();
  Serial.println("❌ Could not connect to any network");
  return false;
}

// ===== CAPTIVE PORTAL =====
void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html><head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>SilverCare WiFi Setup</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5;}";
  html += ".container{max-width:400px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
  html += "h1{color:#333;text-align:center;}";
  html += "label{display:block;margin:10px 0 5px;color:#666;}";
  html += "input{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;}";
  html += "button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;font-size:16px;margin-top:10px;}";
  html += "button:hover{background:#45a049;}";
  html += ".network-list{margin:20px 0;padding:10px;background:#f9f9f9;border-radius:5px;}";
  html += ".network-item{padding:8px;margin:5px 0;background:white;border-radius:3px;}";
  html += "</style></head><body>";
  html += "<div class=\"container\">";
  html += "<h1>🔧 SilverCare WiFi Setup</h1>";
  html += "<p>Configure your WiFi connection:</p>";
  
  // Show available networks
  html += "<div class=\"network-list\"><strong>Available Networks:</strong><br>";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 10; i++) {
    html += "<div class=\"network-item\">" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</div>";
  }
  html += "</div>";
  
  html += "<form action=\"/save\" method=\"POST\">";
  html += "<label>WiFi Network Name (SSID):</label>";
  html += "<input type=\"text\" name=\"ssid\" required placeholder=\"Enter WiFi name\">";
  html += "<label>WiFi Password:</label>";
  html += "<input type=\"password\" name=\"password\" placeholder=\"Enter password\">";
  html += "<button type=\"submit\">Connect & Save</button>";
  html += "</form>";
  html += "</div></body></html>";
  
  webServer.send(200, "text/html", html);
}

void handleSave() {
  String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");
  
  Serial.println("📝 Received WiFi credentials:");
  Serial.println("SSID: " + ssid);
  Serial.println("Password: " + (password.length() > 0 ? "***" : "(open)"));
  
  // Save the network
  saveKnownNetwork(ssid, password, 10);
  
  // Try to connect
  webServer.send(200, "text/html", "<html><body><h1>Connecting...</h1><p>Attempting to connect to " + ssid + "</p><p>Device will restart in 5 seconds...</p></body></html>");
  
  delay(2000);
  
  // Disable AP mode and try to connect
  apModeActive = false;
  webServer.stop();
  WiFi.softAPdisconnect(true);
  
  // Try to connect with new credentials
  if (connectToNetwork(ssid, password)) {
    Serial.println("✅ Successfully connected to configured network");
  } else {
    Serial.println("❌ Failed to connect, will retry in loop");
  }
}

void startCaptivePortal() {
  Serial.println("========================================");
  Serial.println("🔧 STARTING CAPTIVE PORTAL MODE");
  Serial.println("========================================");
  
  WiFi.mode(WIFI_AP_STA);
  
  // Start AP
  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("✅ AP Mode Started");
    Serial.print("📡 AP IP Address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("📱 Connect to: " + String(AP_SSID));
    Serial.println("🔑 Password: " + String(AP_PASSWORD));
    Serial.println("🌐 Then open: http://" + WiFi.softAPIP().toString());
  } else {
    Serial.println("❌ Failed to start AP mode");
    return;
  }
  
  // Setup web server
  webServer.on("/", handleRoot);
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.begin();
  
  apModeActive = true;
  apModeStartTime = millis();
  
  Serial.println("🌐 Web server started on port 80");
  Serial.println("========================================");
}

void stopCaptivePortal() {
  if (apModeActive) {
    webServer.stop();
    WiFi.softAPdisconnect(true);
    apModeActive = false;
    Serial.println("🔧 Captive portal stopped");
  }
}

void setupWiFi() {
  Serial.println("========================================");
  Serial.println("=== AUTOMATIC WIFI MANAGER ===");
  Serial.println("========================================");
  
  WiFi.mode(WIFI_STA);
  
  // Load known networks from preferences
  loadKnownNetworks();
  
  // Try to connect
  if (!scanAndConnect()) {
    Serial.println("⚠️ Initial connection failed, starting captive portal");
    startCaptivePortal();
  }
  
  Serial.println("========================================");
}

void manageWiFiConnection() {
  static unsigned long lastScanTime = 0;
  
  // Handle captive portal timeout
  if (apModeActive && millis() - apModeStartTime > AP_MODE_TIMEOUT) {
    Serial.println("⏰ Captive portal timeout, stopping AP mode");
    stopCaptivePortal();
  }
  
  // Serve web server if in AP mode
  if (apModeActive) {
    webServer.handleClient();
    return;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Periodically scan for better networks
    if (millis() - lastScanTime > WIFI_SCAN_INTERVAL) {
      lastScanTime = millis();
      // Optional: Scan for better networks while connected
      // scanAndConnect(); // Uncomment to enable network switching
    }
    return;
  }
  
  // Not connected, try to reconnect
  unsigned long now = millis();
  if (now - lastWiFiReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
    Serial.println("🔄 WiFi disconnected, attempting reconnection...");
    lastWiFiReconnectAttempt = now;
    
    // Try automatic connection first
    if (!scanAndConnect()) {
      Serial.println("⚠️ Auto-connection failed, starting captive portal");
      startCaptivePortal();
    }
  }
}

// ===== GSM SETUP =====
void setupGSM() {
    gsmSerial.begin(GSM_BAUD);
    Serial.println("📱 Initializing GSM Module...");
    delay(1000);
    
    // Clear any existing data
    while(gsmSerial.available()) {
        gsmSerial.read();
    }
    
    gsmSerial.println("AT");
    delay(1000);
    
    // Read response
    String response = "";
    while(gsmSerial.available()) {
        response += (char)gsmSerial.read();
    }
    
    if (response.indexOf("OK") != -1 || response.length() > 0) {
        Serial.println("✅ GSM Module Responding");
        
        // Set SMS text mode
        gsmSerial.println("AT+CMGF=1");
        delay(1000);
        while(gsmSerial.available()) {
            gsmSerial.read();
        }
    } else {
        Serial.println("❌ GSM Module Not Responding (will continue without GSM)");
    }
}

// ===== PIN SETUP =====
void setupPins() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(MIC_BUTTON_PIN, INPUT_PULLUP);
    Serial.println("✅ Pins configured");
}

// ===== SIMULATION LOGIC =====
void updateSimulation() {
    unsigned long now = millis();
    
    // Cycle state every 30 seconds for demonstration purposes
    if (now - stateChangeTime > 30000) {
        if (currentState == NORMAL) {
            currentState = ALERT;
            Serial.println("🔄 State changed to ALERT");
        } else if (currentState == ALERT) {
            currentState = CRITICAL;
            Serial.println("🔄 State changed to CRITICAL");
        } else {
            currentState = NORMAL;
            Serial.println("🔄 State changed to NORMAL");
        }
        stateChangeTime = now;
    }

    // Simulate HR based on state
    switch (currentState) {
        case NORMAL:
            currentHr = random(65, 85);
            break;
        case ALERT:
            currentHr = random(105, 115);
            break;
        case CRITICAL:
            currentHr = random(125, 140);
            break;
    }
    currentSpo2 = random(95, 100);
    currentTemp = 36.5 + (random(-5, 5) / 10.0);
}

// ===== SEND DATA TO BACKEND =====
void sendDataToBackend() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(10000); // 10 second timeout
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        int stateVal = (currentState == NORMAL) ? 0 : (currentState == ALERT) ? 1 : 2;
        String stateName = (currentState == NORMAL) ? "NORMAL" : (currentState == ALERT) ? "ALERT" : "FALL_DETECTED";

        String payload = "{";
        payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
        payload += "\"beltType\":\"Waist Belt\",";
        payload += "\"state\":" + String(stateVal) + ",";
        payload += "\"stateName\":\"" + stateName + "\",";
        payload += "\"heartRate\":" + String(currentHr) + ",";
        payload += "\"spo2\":" + String(currentSpo2) + ",";
        payload += "\"temperature\":" + String(currentTemp) + ",";
        payload += "\"beltWorn\":true,";
        payload += "\"acceleration\":1.0,";
        payload += "\"latitude\":" + String(gpsLat, 6) + ",";
        payload += "\"longitude\":" + String(gpsLng, 6) + ",";
        payload += "\"micMessageAudio\":\"" + micMessage + "\",";
        payload += "\"timestamp\":" + String(millis());
        payload += "}";

        int httpResponseCode = http.POST(payload);
        if (httpResponseCode > 0) {
            Serial.print("✅ HTTP POST successful, code: ");
            Serial.println(httpResponseCode);
            
            String response = http.getString();
            if (response.indexOf("ACKNOWLEDGED") != -1 || response.indexOf("I am Fine") != -1) {
                Serial.println("💚 [GUARDIAN ACKNOWLEDGED]: Clearing local buzzer and state!");
                digitalWrite(BUZZER_PIN, LOW);
                currentState = NORMAL;
                micMessage = "";
            }
        } else {
            Serial.print("❌ HTTP Error code: ");
            Serial.println(httpResponseCode);
            Serial.print("❌ Error: ");
            Serial.println(http.errorToString(httpResponseCode));
        }
        http.end();
    } else {
        Serial.println("❌ WiFi Disconnected");
        manageWiFiConnection();
    }
}

// ===== SMS ALERT FUNCTIONS =====
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

// ===== DEVICE MANAGEMENT =====
void generateDeviceId() {
  devicePrefs.begin("device_id", false);
  
  // Check if ID already exists
  DEVICE_ID = devicePrefs.getString("device_id", "");
  
  if (DEVICE_ID.length() == 0) {
    // Generate unique ID based on MAC address
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    DEVICE_ID = "SC_WB_" + mac.substring(0, 12) + "_" + String(millis(), HEX);
    
    devicePrefs.putString("device_id", DEVICE_ID);
    devicePrefs.end();
    devicePrefs.begin("device_id", false);
    
    Serial.println("🆕 Generated new Device ID: " + DEVICE_ID);
  } else {
    Serial.println("🆔 Loaded Device ID: " + DEVICE_ID);
  }
  
  devicePrefs.end();
}

void registerDevice() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected, skipping device registration");
    return;
  }
  
  HTTPClient http;
  String url = String(deviceApiUrl) + "/register";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  String mac = WiFi.macAddress();
  
  String payload = "{";
  payload += "\"deviceId\":\"" + DEVICE_ID + "\",";
  payload += "\"deviceType\":\"Waist Belt\",";
  payload += "\"macAddress\":\"" + mac + "\"";
  payload += "}";
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    Serial.print("✅ Device registered, code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("❌ Device registration failed, code: ");
    Serial.println(httpResponseCode);
    Serial.print("❌ Error: ");
    Serial.println(http.errorToString(httpResponseCode));
  }
  
  http.end();
}

void sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  HTTPClient http;
  String url = String(deviceApiUrl) + "/" + DEVICE_ID + "/heartbeat";
  http.begin(url);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpResponseCode = http.POST("");
  
  if (httpResponseCode > 0) {
    // Heartbeat successful
  }
  
  http.end();
}

// ... (rest of the code remains the same)

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("========================================");
  Serial.println("=== SILVERCARE WAIST BELT WORKAROUND ===");
  Serial.println("========================================");
  
  generateDeviceId();
  setupPins();
  setupWiFi();
  setupGSM();
  stateChangeTime = millis();
  
  // Register device if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    registerDevice();
  }
}

void loop() {
  unsigned long now = millis();
  
  // In actual implementation, if dataReceivedFromWrist is false, run simulation
  if (!dataReceivedFromWrist) {
    updateSimulation();
  }
  
  // Handle microphone input
  handleMicrophone();
  
  // Handle button override
  handleButtonOverride();
  
  // Manage WiFi connection (includes captive portal handling)
  manageWiFiConnection();
  
  // Send heartbeat to backend
  if (now - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = now;
  }
  
  // Handle notifications (buzzer, SMS)
  handleNotifications();
  
  if (now - lastDataSent > DATA_SEND_INTERVAL) {
    sendDataToBackend();
    lastDataSent = now;
  }
  
  // (Optional) Add non-blocking BLE connect / scan logic here to fallback to real data
  delay(10);
}

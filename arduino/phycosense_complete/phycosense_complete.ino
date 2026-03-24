#include "Arduino.h"

// --- Libraries for EC/pH/Temp ---
#include "DFRobot_ESP_EC.h"
#include "DFRobot_ESP_PH_WITH_ADC.h"
#include "Adafruit_ADS1X15.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "EEPROM.h"

// --- WiFi & HTTP Libraries ---
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <WebServer.h>

// --- WiFi Provisioning Configuration ---
Preferences preferences;
String g_deviceId = "";             // Auto-generated unique ID from MAC
String g_deviceName = "";           // User-configurable device name
String g_serverUrl = "";            // Server URL configured during provisioning
String g_backupServerUrl = "";      // Optional backup server
String g_accessKey = "";            // Access key for dashboard login

// Provisioning mode button (GPIO 0 = BOOT button)
#define PROVISION_BUTTON 0
#define PROVISION_HOLD_TIME 5000  // Hold for 5 seconds to reset

bool isProvisioned = false;

// Key portal (AP mode after registration)
WebServer keyServer(80);
bool keyPortalRunning = false;
unsigned long keyPortalStartTime = 0;
const unsigned long KEY_PORTAL_DURATION = 0; // 0 = keep portal open indefinitely

// Registration retry (when key portal is open but registration failed)
unsigned long lastRegistrationAttempt = 0;
const unsigned long REGISTRATION_RETRY_INTERVAL = 15000; // retry every 15 seconds
int g_regAttemptCount = 0;
String g_lastRegError = "Waiting...";

// Prototype mode: force provisioning on every reboot/reset.
// Set to false when you want credentials remembered across reboots.
const bool FORCE_REPROVISION_EVERY_BOOT = true;

// Production server URL (customer doesn't need to enter this)
const String PRODUCTION_SERVER = "https://algae-monitoring-final-production.up.railway.app/api/sensor-data";

// --- Hardware Definitions ---
#define ONE_WIRE_BUS 4      // DS18B20 Data Pin
#define TURBIDITY_PIN 16    // Turbidity Analog Pin
#define BATTERY_PIN 35      // Battery Voltage Monitor (ADC1_CH7)
#define PROBIOTIC_LEVEL_PIN 34  // Water Level Sensor for Probiotic Chamber (ADC1_CH6)

// --- Objects ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DFRobot_ESP_PH_WITH_ADC ph;
DFRobot_ESP_EC ec;
Adafruit_ADS1115 ads;
bool adsReady = false;

// --- Timing Variables ---
unsigned long intervals[] = {
    1000U,    // 0: Main Loop / Turbidity Read
    2000U,    // 1
    3000U,    // 2
    5000U,    // 3: Display All Data
    5000U     // 4: Send to Server
};
unsigned long last[] = {0, 0, 0, 0, 0};

// --- EC/pH/Temp Variables ---
bool ecPhCalibrationActive = false;
float ecVoltage, ecValue, phVoltage, phValue, temperature = 25.0;
float lastTemperature = 25.0;

// --- Turbidity Variables ---
float turbidityVoltage = 0.0;
float turbidityNTU = 0.0;
int turbidityRaw = 0;

// --- Probiotic Level Variables ---
float probioticLevelVoltage = 0.0;
int probioticLevelPercentage = 0;

// Turbidity Calibration (Default values)
float clearWaterVoltage = 2.8;
float turbidWaterVoltage = 1.2;
float knownTurbidNTU = 1000;
bool turbidityCalibrationMode = false;
int turbidityCalStep = 0;

// Turbidity Averaging
#define NUM_SAMPLES 10

// --- Battery Variables ---
float batteryVoltage = 0.0;
int batteryPercentage = 0;
bool isUSBPowered = false;
const float VOLTAGE_DIVIDER_RATIO = 2.0;
#define BATTERY_SAMPLES 10

// -------------------------------------------------------------------------
// --- Helper Functions ---
// -------------------------------------------------------------------------

float getWaterTemperature()
{
    sensors.requestTemperatures();
    float currentTemp = sensors.getTempCByIndex(0);

    if (currentTemp == 85.00 || currentTemp == -127.00)
    {
        currentTemp = lastTemperature;
    }
    else
    {
        lastTemperature = currentTemp;
    }
    return currentTemp;
}

float readTurbidityAvg()
{
    float sum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        int raw = analogRead(TURBIDITY_PIN);
        sum += (raw * 3.3) / 4095.0;
        delay(5);
    }
    return sum / NUM_SAMPLES;
}

float map_float(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float readProbioticLevel()
{
    int rawValue = analogRead(PROBIOTIC_LEVEL_PIN);
    float voltage = (rawValue / 4095.0) * 3.3;

    float minVoltage = 0.5;
    float maxVoltage = 2.5;

    float percentage = ((voltage - minVoltage) / (maxVoltage - minVoltage)) * 100.0;
    percentage = constrain(percentage, 0, 100);

    return percentage;
}

float readBatteryVoltage()
{
    float sum = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        int raw = analogRead(BATTERY_PIN);
        sum += (raw / 4095.0) * 3.3 * VOLTAGE_DIVIDER_RATIO;
        delay(5);
    }
    return sum / BATTERY_SAMPLES;
}

int calculateBatteryPercentage(float voltage)
{
    if (voltage > 4.5 || voltage < 0.5) {
        isUSBPowered = true;
        return 100;
    }

    isUSBPowered = false;

    if (voltage >= 4.2) return 100;
    if (voltage >= 4.0) return 80 + (int)((voltage - 4.0) * 100);
    if (voltage >= 3.8) return 60 + (int)((voltage - 3.8) * 100);
    if (voltage >= 3.6) return 40 + (int)((voltage - 3.6) * 100);
    if (voltage >= 3.4) return 20 + (int)((voltage - 3.4) * 100);
    if (voltage >= 3.0) return (int)((voltage - 3.0) * 50);
    return 0;
}

// -------------------------------------------------------------------------
// --- WiFi Provisioning Functions ---
// -------------------------------------------------------------------------

void generateDeviceId() {
    uint64_t chipid = ESP.getEfuseMac();
    char idBuf[24];
    snprintf(idBuf, sizeof(idBuf), "ESP32-%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
    g_deviceId = String(idBuf);
    g_deviceId.toUpperCase();
}

bool loadConfiguration() {
    preferences.begin("phycosense", true);

    g_serverUrl = preferences.getString("serverUrl", "");
    g_deviceName = preferences.getString("deviceName", "");
    g_backupServerUrl = preferences.getString("backupUrl", "");
    g_accessKey = preferences.getString("accessKey", "");

    preferences.end();

    if (g_serverUrl.length() > 0) {
        if (g_deviceName.length() == 0) {
            g_deviceName = g_deviceId;
        }
        return true;
    }
    return false;
}

void saveConfiguration(String server, String name, String backup = "") {
    preferences.begin("phycosense", false);

    preferences.putString("serverUrl", server);
    preferences.putString("deviceName", name);
    if (backup.length() > 0) {
        preferences.putString("backupUrl", backup);
    }
    if (g_accessKey.length() > 0) {
        preferences.putString("accessKey", g_accessKey);
    }

    preferences.end();

    Serial.println("✓ Configuration saved to flash memory");
}

void clearConfiguration() {
    preferences.begin("phycosense", false);
    preferences.clear();
    preferences.end();

    WiFiManager wm;
    wm.resetSettings();

    // Force erase saved STA credentials from NVS as an extra safety step.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(200);

    Serial.println("✓ Configuration cleared - device reset to factory defaults");
    Serial.println("✓ WiFi credentials erased");
}

bool registerDevice();
void startKeyPortal();

void startProvisioningMode() {
    Serial.println("\n=== PROVISIONING MODE ===");
    Serial.print("Connect to hotspot: PhycoSense-");
    Serial.println(g_deviceId);
    Serial.println("No password - open network");
    Serial.println("Portal will open automatically.\n");

    WiFiManager wm;

    WiFiManagerParameter custom_name("name", "Pond / Device Name", g_deviceId.c_str(), 40,
        "placeholder=\"e.g. Main Pond\"");

    String htmlContent =
        "<br/>"
        "<div style='background:#e8f5e9;border-radius:8px;padding:16px;margin:10px 0;font-size:14px;color:#1a237e'>"
        "<b>&#128273; Dashboard Access Key</b><br/>"
        "Your key will be created/retrieved from the server after you tap <b>Save</b>.<br/>"
        "<span style='font-size:0.85em;color:#555'>Then reconnect to <b>PhycoSense-" + g_deviceId + "</b> and open <b>http://192.168.4.1</b> to view the final key.</span>"
        "</div>";
    WiFiManagerParameter custom_html(htmlContent.c_str());

    wm.addParameter(&custom_html);
    wm.addParameter(&custom_name);

    wm.setConfigPortalTimeout(300);
    wm.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));

    String apName = "PhycoSense-" + g_deviceId;

    if (!wm.autoConnect(apName.c_str())) {  // Open network - no password
        Serial.println("Provisioning timed out - restarting...");
        delay(3000);
        ESP.restart();
    }

    // WiFi connected - use device name, hardcode server URL
    g_deviceName = custom_name.getValue();
    if (g_deviceName.length() == 0) g_deviceName = g_deviceId;
    g_serverUrl = PRODUCTION_SERVER;

    saveConfiguration(g_serverUrl, g_deviceName);

    Serial.println("\n✓ Provisioning complete!");
    Serial.print("   Device Name: "); Serial.println(g_deviceName);
    Serial.print("   Server URL:  "); Serial.println(g_serverUrl);

    isProvisioned = true;

    // Register in pure STA mode first (before key portal AP starts)
    Serial.println("\nRegistering device - please wait...");
    for (int i = 1; i <= 5 && g_accessKey.length() == 0; i++) {
        g_regAttemptCount = i;
        g_lastRegError = "Attempt " + String(i) + " of 5...";
        Serial.printf("  Attempt %d/5\n", i);
        registerDevice();
        if (g_accessKey.length() == 0 && i < 5) delay(3000);
    }

    if (g_accessKey.length() > 0) {
        Serial.println("\n*** Device registered successfully! ***");
    } else {
        Serial.println("\n*** Registration failed - will show error page ***");
    }

    startKeyPortal();
}

void checkProvisionButton() {
    static unsigned long buttonPressStart = 0;
    static bool buttonWasPressed = false;

    if (digitalRead(PROVISION_BUTTON) == LOW) {
        if (!buttonWasPressed) {
            buttonPressStart = millis();
            buttonWasPressed = true;
            Serial.println("⚠ Provision button pressed - hold for 5 seconds to reset...");
        } else if (millis() - buttonPressStart >= PROVISION_HOLD_TIME) {
            Serial.println("\n✓ Factory reset triggered!");
            clearConfiguration();
            delay(1000);
            ESP.restart();
        }
    } else {
        buttonWasPressed = false;
    }
}

bool registerDevice() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;
    StaticJsonDocument<256> doc;

    String baseUrl = g_serverUrl;
    baseUrl.replace("/sensor-data", "");
    if (!baseUrl.endsWith("/")) baseUrl += "/";
    String registerUrl = baseUrl + "auth/register-device";

    doc["deviceId"] = g_deviceId;
    doc["deviceName"] = g_deviceName;
    if (g_accessKey.length() > 0) {
        doc["accessKey"] = g_accessKey;
    }

    String jsonString;
    serializeJson(doc, jsonString);

    Serial.print("Registering device: ");
    Serial.println(registerUrl);

    NetworkClientSecure secureClient;
    secureClient.setInsecure(); // Skip cert verification (no CA bundle on device)
    http.begin(secureClient, registerUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000);

    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.print("✓ Registration response (");
        Serial.print(httpResponseCode);
        Serial.print("): ");
        Serial.println(response);

        StaticJsonDocument<512> resDoc;
        DeserializationError error = deserializeJson(resDoc, response);

        if (!error && resDoc.containsKey("accessKey")) {
            g_accessKey = resDoc["accessKey"].as<String>();

            preferences.begin("phycosense", false);
            preferences.putString("accessKey", g_accessKey);
            preferences.end();

            Serial.println("\n========================================");
            Serial.println("YOUR DASHBOARD ACCESS KEY");
            Serial.print("KEY: ");
            Serial.println(g_accessKey);
            Serial.println("Go to phycosense.app and enter this key");
            Serial.println("========================================\n");

            // Re-open hotspot so customer can retrieve key at 192.168.4.1
            startKeyPortal();
        } else if (httpResponseCode == 409) {
            Serial.println("Device already registered - recovering key from server.");
            StaticJsonDocument<512> resDoc409;
            if (!deserializeJson(resDoc409, response) && resDoc409.containsKey("accessKey")) {
                g_accessKey = resDoc409["accessKey"].as<String>();
                preferences.begin("phycosense", false);
                preferences.putString("accessKey", g_accessKey);
                preferences.end();
                Serial.print("Recovered access key: ");
                Serial.println(g_accessKey);
            } else if (g_accessKey.length() > 0) {
                Serial.print("Using stored key: ");
                Serial.println(g_accessKey);
            }
        }

        http.end();
        return true;
    } else {
        g_lastRegError = "HTTP error: " + http.errorToString(httpResponseCode) + " (" + String(httpResponseCode) + ")";
        Serial.print("⚠ Registration failed (will retry): ");
        Serial.println(g_lastRegError);
        http.end();
        return false;
    }
}

void startKeyPortal() {
    if (keyPortalRunning) return;

    if (g_accessKey.length() > 0) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    } else {
        WiFi.disconnect(true);
        delay(300);
        WiFi.mode(WIFI_AP);
    }

    bool apConfigOk = WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    String apName = "PhycoSense-" + g_deviceId;
    WiFi.softAP(apName.c_str());  // Open network - no password

    if (!apConfigOk) {
        Serial.println("⚠ AP static IP config failed; using default AP IP");
    }

    keyServer.on("/status", HTTP_GET, []() {
        String json = "{\"key\":\"" + g_accessKey + "\",\"attempt\":" + String(g_regAttemptCount) + ",\"error\":\"" + g_lastRegError + "\"}";
        keyServer.sendHeader("Access-Control-Allow-Origin", "*");
        keyServer.send(200, "application/json", json);
    });

    keyServer.on("/", HTTP_GET, []() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<meta charset='UTF-8'>";
        html += "<title>PhycoSense - Your Access Key</title>";
        if (g_accessKey.length() == 0) {
            html += "<meta http-equiv='refresh' content='4'>";
        }
        html += "<style>";
        html += "*{box-sizing:border-box;margin:0;padding:0}";
        html += "body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#fff;";
        html += "min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}";
        html += ".card{background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.12);";
        html += "border-radius:20px;padding:36px 28px;max-width:380px;width:100%;text-align:center}";
        html += ".check{font-size:3rem;margin-bottom:12px}";
        html += ".title{color:#4dd0e1;font-size:1.6rem;font-weight:700;margin-bottom:6px}";
        html += ".sub{color:#78909c;font-size:0.95rem;margin-bottom:28px}";
        html += ".key-box{background:rgba(77,208,225,0.1);border:2px solid rgba(77,208,225,0.4);";
        html += "border-radius:14px;padding:22px 16px;margin-bottom:24px}";
        html += ".key-label{color:#78909c;font-size:0.8rem;text-transform:uppercase;letter-spacing:1px;margin-bottom:10px}";
        html += ".key{font-size:2rem;font-weight:700;color:#4dd0e1;letter-spacing:4px;font-family:monospace}";
        html += ".copy-btn{background:#4dd0e1;color:#0a1929;border:none;border-radius:10px;";
        html += "padding:10px 24px;font-size:0.95rem;font-weight:600;cursor:pointer;margin-top:12px;width:100%}";
        html += ".steps{background:rgba(255,255,255,0.04);border-radius:12px;padding:18px;text-align:left}";
        html += ".steps p{color:#b0bec5;font-size:0.9rem;line-height:1.8;margin:0}";
        html += ".steps b{color:#4dd0e1}";
        html += ".device{color:#37474f;font-size:0.75rem;margin-top:20px}";
        html += ".waiting{color:#ffd54f;font-size:1rem;padding:20px 0}";
        html += "</style></head><body><div class='card'>";

        if (g_accessKey.length() == 0) {
            html += "<script>";
            html += "setInterval(function(){";
            html += "fetch('/status').then(r=>r.json()).then(d=>{";
            html += "if(d.key&&d.key.length>0){location.reload();}";
            html += "document.getElementById('attempt').textContent='Attempt '+d.attempt;";
            html += "document.getElementById('err').textContent=d.error;";
            html += "});},3000);";
            html += "</script>";
            html += "<div class='check'>&#9203;</div>";
            html += "<div class='title'>Almost Ready...</div>";
            html += "<div class='waiting'>Connecting to server...</div>";
            html += "<div id='attempt' style='color:#90caf9;font-size:0.9rem;margin:8px 0'>Attempt " + String(g_regAttemptCount) + "</div>";
            html += "<div id='err' style='background:rgba(255,100,100,0.1);border-radius:8px;padding:10px;margin:12px 0;font-size:0.8rem;color:#ef9a9a;word-break:break-all'>" + g_lastRegError + "</div>";
            html += "<div class='device'>Device: " + g_deviceName + " (" + g_deviceId + ")</div>";
        } else {
            html += "<div class='check'>&#10003;</div>";
            html += "<div class='title'>Setup Complete!</div>";
            html += "<div class='sub'>Your PhycoSense device is now connected</div>";
            html += "<div class='key-box'>";
            html += "<div class='key-label'>Dashboard Access Key</div>";
            html += "<div class='key' id='key'>" + g_accessKey + "</div>";
            html += "<button class='copy-btn' onclick='navigator.clipboard.writeText(\"" + g_accessKey + "\").then(()=>{this.textContent=\"Copied!\";setTimeout(()=>{this.textContent=\"Copy Key\"},2000)})'>Copy Key</button>";
            html += "</div>";
            html += "<div class='steps'><p>";
            html += "<b>1.</b> Write down or copy the key above<br>";
            html += "<b>2.</b> Reconnect your phone to your home WiFi<br>";
            html += "<b>3.</b> Open <b>phycosense.app</b> in your browser<br>";
            html += "<b>4.</b> Enter the key to view your pond dashboard";
            html += "</p></div>";
            html += "<div class='device'>Device: " + g_deviceName + " (" + g_deviceId + ")</div>";
        }
        html += "</div></body></html>";
        keyServer.send(200, "text/html", html);
    });

    keyServer.begin();
    keyPortalRunning = true;
    keyPortalStartTime = millis();

    Serial.println("\n✓ Key portal started!");
    Serial.print("  Reconnect to hotspot: PhycoSense-");
    Serial.println(g_deviceId);
    Serial.println("  (No password - open network)");
    Serial.print("  AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("  Then open the AP IP above in your browser (usually 192.168.4.1)");
    Serial.println("  (Portal stays open continuously)\n");
}

int bufIndex = 0;
bool readSerial(char result[])
{
    while (Serial.available() > 0)
    {
        char inChar = Serial.read();
        if (inChar == '\n')
        {
            result[bufIndex] = '\0';
            Serial.flush();
            bufIndex = 0;
            return true;
        }
        if (inChar != '\r')
        {
            result[bufIndex] = inChar;
            bufIndex++;
        }
        delay(1);
    }
    return false;
}

void connectWiFi() {
    if (!isProvisioned) {
        Serial.println("⚠ Device not provisioned - skipping WiFi connection");
        return;
    }

    if (keyPortalRunning) {
        Serial.println("Key portal is active - skipping STA reconnect to keep AP reachable");
        return;
    }

    Serial.println("Connecting to WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin();

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Device ID: ");
        Serial.println(g_deviceId);
        Serial.print("Device Name: ");
        Serial.println(g_deviceName);
        Serial.print("Sending data to: ");
        Serial.println(g_serverUrl);

        registerDevice();
    } else {
        Serial.println("\n✗ WiFi Connection Failed - will retry");
    }
}

void sendDataToServer(float temp, float pH, float ecVal, float turbidity) {
    if (!isProvisioned) {
        Serial.println("⚠ Device not provisioned - cannot send data");
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("✗ WiFi disconnected - reconnecting...");
        connectWiFi();
        return;
    }

    HTTPClient http;
    StaticJsonDocument<320> doc;

    doc["deviceId"] = g_deviceId;
    doc["deviceName"] = g_deviceName;
    doc["temperature"] = temp;
    doc["ph"] = pH;
    doc["ec"] = ecVal * 1000;  // Convert ms/cm to uS/cm
    doc["turbidity"] = turbidity;
    doc["dissolvedOxygen"] = 8.5; // Replace with actual DO sensor when connected
    doc["probioticLevel"] = probioticLevelPercentage;

    doc["batteryVoltage"] = round(batteryVoltage * 100) / 100.0;
    doc["batteryPercentage"] = batteryPercentage;
    doc["isUSBPowered"] = isUSBPowered;

    String jsonString;
    serializeJson(doc, jsonString);

    Serial.print(">> Sending JSON: ");
    Serial.println(jsonString);

    NetworkClientSecure dataClient;
    dataClient.setInsecure();
    http.begin(dataClient, g_serverUrl.c_str());
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0) {
        Serial.print("✓ Success! Code: ");
        Serial.println(httpResponseCode);
        http.end();
        return;
    }

    Serial.print("✗ Primary server error: ");
    Serial.println(http.errorToString(httpResponseCode));
    http.end();

    if (g_backupServerUrl.length() > 0) {
        Serial.println("Trying backup server...");
        http.begin(g_backupServerUrl.c_str());
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(10000);

        httpResponseCode = http.POST(jsonString);
        if (httpResponseCode > 0) {
            Serial.print("✓ Backup server success! Code: ");
            Serial.println(httpResponseCode);
        } else {
            Serial.println("✗ Both servers failed - data not sent");
        }
        http.end();
    }
}

// -------------------------------------------------------------------------
// --- Setup ---
// -------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(1000);
    EEPROM.begin(64);

    Serial.println(F("\n╔═══════════════════════════════════════╗"));
    Serial.println(F("║ PhycoSense Prototype (Full Sensors)   ║"));
    Serial.println(F("║   WiFi Provisioning + Live Sensors    ║"));
    Serial.println(F("╚═══════════════════════════════════════╝\n"));

    // Init provision button and identity
    pinMode(PROVISION_BUTTON, INPUT_PULLUP);
    generateDeviceId();
    Serial.print("Device ID: ");
    Serial.println(g_deviceId);

    if (FORCE_REPROVISION_EVERY_BOOT) {
        Serial.println("\n⚠ FORCE_REPROVISION_EVERY_BOOT is ON");
        Serial.println("  Clearing saved WiFi + provisioning data for this boot...");
        clearConfiguration();
        delay(200);
    }

    // Provisioning flow aligned with no-sensors firmware
    isProvisioned = loadConfiguration();
    bool justProvisioned = false;

    if (!isProvisioned) {
        Serial.println("⚠ No configuration found - entering provisioning mode\n");
        startProvisioningMode();
        justProvisioned = true;
    } else {
        Serial.println("✓ Configuration loaded from memory");
        Serial.print("  Device Name: ");
        Serial.println(g_deviceName);
        Serial.print("  Server URL: ");
        Serial.println(g_serverUrl);
        if (g_backupServerUrl.length() > 0) {
            Serial.print("  Backup URL: ");
            Serial.println(g_backupServerUrl);
        }
        Serial.println();
    }

    // Init Temperature Sensor
    sensors.begin();
    Serial.print("Found ");
    Serial.print(sensors.getDeviceCount());
    Serial.println(" DS18B20 temperature sensor(s)");
    if (sensors.getDeviceCount() == 0) {
        Serial.println("WARNING: No DS18B20 detected! Check wiring & 4.7k pull-up resistor!");
    }

    // Init ADS1115 (required for EC/pH channels); continue even if missing
    ads.setGain(GAIN_ONE);
    adsReady = ads.begin();
    if (!adsReady) {
        Serial.println("⚠ ADS1115 not detected. EC/pH sensor readings will be disabled until connected.");
    }

    // Init DFRobot objects
    ph.begin(10);
    ec.begin(20);

    // Init ADC channels
    pinMode(TURBIDITY_PIN, INPUT);
    pinMode(PROBIOTIC_LEVEL_PIN, INPUT);
    pinMode(BATTERY_PIN, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    // Initial battery reading
    batteryVoltage = readBatteryVoltage();
    batteryPercentage = calculateBatteryPercentage(batteryVoltage);
    Serial.print("Battery: ");
    Serial.print(batteryVoltage, 2);
    Serial.print("V (");
    if (isUSBPowered) {
        Serial.println("USB Powered)");
    } else {
        Serial.print(batteryPercentage);
        Serial.println("%)");
    }

    if (isProvisioned && !justProvisioned) {
        connectWiFi();
        startKeyPortal();
    }

    Serial.println(F("\nCommands:"));
    Serial.println(F("  RESET  - Factory reset device"));
    Serial.println(F("  STATUS - Show device info"));
    Serial.println(F("  KEY    - Show dashboard access key"));
    Serial.println(F("  ⚠ Or hold BOOT button for 5 seconds"));
    Serial.println(F("  EC/pH: PH, EC, EXITPH, EXITEC"));
    Serial.println(F("  Turbidity: CAL, CAL1, CAL2, DONE"));
    Serial.println(F(""));
    Serial.println(F("Starting data transmission (every 5 seconds)..."));
    Serial.println(F(""));
}

// -------------------------------------------------------------------------
// --- Main Loop ---
// -------------------------------------------------------------------------
void loop()
{
    unsigned long now = millis();

    checkProvisionButton();

    // Handle key portal (AP hotspot at 192.168.4.1)
    if (keyPortalRunning) {
        keyServer.handleClient();

        // Retry registration while waiting for key
        if (g_accessKey.length() == 0 && WiFi.status() == WL_CONNECTED && (now - lastRegistrationAttempt >= REGISTRATION_RETRY_INTERVAL)) {
            lastRegistrationAttempt = now;
            g_regAttemptCount++;
            g_lastRegError = "Retry attempt " + String(g_regAttemptCount);
            registerDevice();
        }

        if (KEY_PORTAL_DURATION > 0 && millis() - keyPortalStartTime > KEY_PORTAL_DURATION) {
            keyServer.close();
            WiFi.softAPdisconnect(true);
            keyPortalRunning = false;
            Serial.println("Key portal closed. Reconnecting WiFi...");
            connectWiFi();
        }
    }

    // Serial + calibration + utility commands
    char cmd[20];
    if (readSerial(cmd))
    {
        strupr(cmd);
        String cmdStr = String(cmd);

        if (cmdStr == "RESET") {
            Serial.println(F("\n⚠ Factory reset initiated..."));
            clearConfiguration();
            delay(1000);
            ESP.restart();
        }
        else if (cmdStr == "STATUS") {
            Serial.println(F("\n╔══════════════════════════════════════╗"));
            Serial.println(F("║         DEVICE STATUS                ║"));
            Serial.println(F("╚══════════════════════════════════════╝"));
            Serial.print("Device ID:      ");
            Serial.println(g_deviceId);
            Serial.print("Device Name:    ");
            Serial.println(g_deviceName);
            Serial.print("Provisioned:    ");
            Serial.println(isProvisioned ? "Yes" : "No");
            Serial.print("WiFi Status:    ");
            Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
            if (WiFi.status() == WL_CONNECTED) {
                Serial.print("IP Address:     ");
                Serial.println(WiFi.localIP());
                Serial.print("Signal:         ");
                Serial.print(WiFi.RSSI());
                Serial.println(" dBm");
            }
            Serial.print("Server URL:     ");
            Serial.println(g_serverUrl);
            if (g_backupServerUrl.length() > 0) {
                Serial.print("Backup URL:     ");
                Serial.println(g_backupServerUrl);
            }
            Serial.print("Battery:        ");
            if (isUSBPowered) {
                Serial.println("USB Powered");
            } else {
                Serial.print(batteryVoltage, 2);
                Serial.print("V (");
                Serial.print(batteryPercentage);
                Serial.println("%)");
            }
            if (g_accessKey.length() > 0) {
                Serial.print("Access Key:     ");
                Serial.println(g_accessKey);
                Serial.print("Key Portal:     ");
                Serial.println(keyPortalRunning ? "Open at 192.168.4.1 (reconnect to PhycoSense-" + g_deviceId + ")" : "Closed");
                if (keyPortalRunning) {
                    Serial.print("AP IP:          ");
                    Serial.println(WiFi.softAPIP());
                }
            }
            Serial.println(F("══════════════════════════════════════\n"));
        }
        else if (cmdStr == "KEY") {
            if (g_accessKey.length() > 0) {
                Serial.println("\n╔════════════════════════════════════════╗");
                Serial.println("║      YOUR DASHBOARD ACCESS KEY         ║");
                Serial.println("╠════════════════════════════════════════╣");
                Serial.print("║      ");
                Serial.print(g_accessKey);
                Serial.println("            ║");
                Serial.println("╚════════════════════════════════════════╝\n");
            } else {
                Serial.println("\n⚠ No access key yet. Device must be registered with the server first.");
            }
        }
        else if (cmdStr == "CAL") {
            turbidityCalibrationMode = true;
            turbidityCalStep = 0;
            Serial.println(F("\n[Turbidity] CAL MODE START. Step 1: Put in CLEAR water, then type CAL1"));
        }
        else if (cmdStr == "CAL1" && turbidityCalibrationMode) {
            clearWaterVoltage = turbidityVoltage;
            turbidityCalStep = 1;
            Serial.print(F("[Turbidity] Clear water calibrated at: "));
            Serial.print(clearWaterVoltage);
            Serial.println(" V");
            Serial.println(F("Step 2: Put in DIRTY water, then type CAL2"));
        }
        else if (cmdStr == "CAL2" && turbidityCalibrationMode && turbidityCalStep == 1) {
            turbidWaterVoltage = turbidityVoltage;
            turbidityCalStep = 2;
            Serial.print(F("[Turbidity] Turbid water calibrated at: "));
            Serial.print(turbidWaterVoltage);
            Serial.println(" V");
            Serial.println(F("Type DONE to finish."));
        }
        else if (cmdStr == "DONE" && turbidityCalibrationMode) {
            turbidityCalibrationMode = false;
            Serial.println(F("[Turbidity] Calibration saved (RAM only)."));
        }
        else if (!turbidityCalibrationMode) {
            if (ecPhCalibrationActive || strstr(cmd, "PH") || strstr(cmd, "EC"))
            {
                if (!adsReady) {
                    Serial.println(F("[EC/pH] ADS1115 not ready. Connect ADS1115 to use EC/pH calibration."));
                    ecPhCalibrationActive = false;
                    return;
                }

                ecPhCalibrationActive = true;
                ecVoltage = ads.readADC_SingleEnded(0) / 10;
                phVoltage = ads.readADC_SingleEnded(1) / 10;

                if (strstr(cmd, "PH")) ph.calibration(phVoltage, temperature, cmd);
                if (strstr(cmd, "EC")) ec.calibration(ecVoltage, temperature, cmd);

                if (strstr(cmd, "EXITPH") || strstr(cmd, "EXITEC")) {
                    ecPhCalibrationActive = false;
                    Serial.println(F("[EC/pH] Calibration exited."));
                }
            }
        }
    }

    // --- Task 0: 1000ms Interval (Readings) ---
    if (now - last[0] >= intervals[0])
    {
        last[0] = now;

        temperature = getWaterTemperature();
        turbidityVoltage = readTurbidityAvg();
        turbidityRaw = analogRead(TURBIDITY_PIN);
        turbidityNTU = map_float(turbidityVoltage, clearWaterVoltage, turbidWaterVoltage, 0, knownTurbidNTU);
        turbidityNTU = constrain(turbidityNTU, 0, 3000);

        probioticLevelPercentage = readProbioticLevel();
        batteryVoltage = readBatteryVoltage();
        batteryPercentage = calculateBatteryPercentage(batteryVoltage);

        if (ecPhCalibrationActive)
        {
            ecVoltage = ads.readADC_SingleEnded(0) / 10;
            phVoltage = ads.readADC_SingleEnded(1) / 10;

            Serial.print(F(">>> CAL MODE [EC/pH] <<< Temp:"));
            Serial.print(temperature);
            Serial.print(F(" | EC V:")); Serial.print(ecVoltage);
            Serial.print(F(" | pH V:")); Serial.println(phVoltage);
        }
    }

    // --- Task 3: 5000ms Interval (Display Data) ---
    if (now - last[3] >= intervals[3])
    {
        last[3] = now;

        if (!ecPhCalibrationActive && !turbidityCalibrationMode)
        {
            if (adsReady) {
                ecVoltage = ads.readADC_SingleEnded(0) / 10;
                phVoltage = ads.readADC_SingleEnded(1) / 10;
                ecValue = ec.readEC(ecVoltage, temperature);
                phValue = ph.readPH(phVoltage, temperature);
            } else {
                ecVoltage = 0;
                phVoltage = 0;
                ecValue = 0;
                phValue = 7.0;
            }

            Serial.println(F("\n--- WATER QUALITY REPORT ---"));
            Serial.print(F("Temp:      "));
            Serial.print(temperature, 1);
            Serial.println(F(" C"));

            Serial.print(F("EC:        "));
            if (adsReady) {
                Serial.print(ecValue, 2);
                Serial.print(F(" ms/cm (V:")); Serial.print(ecVoltage); Serial.println(")");
            } else {
                Serial.println(F("N/A (ADS1115 not connected)"));
            }

            Serial.print(F("pH:        "));
            if (adsReady) {
                Serial.print(phValue, 2);
                Serial.print(F(" (V:")); Serial.print(phVoltage); Serial.println(")");
            } else {
                Serial.println(F("N/A (ADS1115 not connected)"));
            }

            Serial.print(F("Turbidity: "));
            Serial.print(turbidityNTU, 1);
            Serial.print(F(" NTU (V:")); Serial.print(turbidityVoltage); Serial.println(")");

            Serial.print(F("Probiotic: "));
            Serial.print(probioticLevelPercentage);
            Serial.print(F("% "));
            if (probioticLevelPercentage < 20) Serial.println(F("(REFILL NOW!)"));
            else if (probioticLevelPercentage < 50) Serial.println(F("(Low)"));
            else Serial.println(F("(OK)"));

            Serial.print(F("Battery:   "));
            if (isUSBPowered) {
                Serial.println(F("USB Powered"));
            } else {
                Serial.print(batteryVoltage, 2);
                Serial.print(F("V ("));
                Serial.print(batteryPercentage);
                Serial.println(F("%)"));
            }

            Serial.print(F("Status:    "));
            if (turbidityNTU < 5) Serial.println(F("Clear"));
            else if (turbidityNTU < 50) Serial.println(F("Slightly Cloudy"));
            else if (turbidityNTU < 100) Serial.println(F("Turbid"));
            else Serial.println(F("Very Muddy"));
            Serial.println(F("----------------------------"));
        }
    }

    // --- Task 4: 5000ms Interval (Send Data) ---
    if (now - last[4] >= intervals[4])
    {
        last[4] = now;

        if (!ecPhCalibrationActive && !turbidityCalibrationMode && isProvisioned)
        {
            sendDataToServer(temperature, phValue, ecValue, turbidityNTU);
        }
    }

    static bool loopStartedPrinted = false;
    if (!loopStartedPrinted) {
        loopStartedPrinted = true;
        Serial.println("✓ Main loop running. Device is active.");
    }
}

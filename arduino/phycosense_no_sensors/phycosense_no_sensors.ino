// PhycoSense - WiFi Provisioning Test Version (No Sensors Required)
// For ESP32 without any sensors connected
// Only sends mock data to test WiFi provisioning & connectivity
// Install library: WiFiManager by tzapu

#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <Preferences.h>

// --- WiFi Provisioning Configuration ---
Preferences preferences;
String g_deviceId = "";             // Auto-generated unique ID from MAC
String g_deviceName = "";           // User-configurable device name
String g_serverUrl = "";            // Server URL configured during provisioning
String g_backupServerUrl = "";      // Optional backup server

// Provisioning mode button (GPIO 0 = BOOT button)
#define PROVISION_BUTTON 0
#define PROVISION_HOLD_TIME 5000  // Hold for 5 seconds to reset

// Device status
bool isProvisioned = false;
String g_accessKey = "";           // Access key for dashboard login

// Key portal (AP mode after registration)
#include <WebServer.h>
WebServer keyServer(80);
bool keyPortalRunning = false;
unsigned long keyPortalStartTime = 0;
const unsigned long KEY_PORTAL_DURATION = 600000; // 10 minutes

// Registration retry (when key portal is open but registration failed)
unsigned long lastRegistrationAttempt = 0;
const unsigned long REGISTRATION_RETRY_INTERVAL = 15000; // retry every 15 seconds
int g_regAttemptCount = 0;
String g_lastRegError = "Waiting...";

// Production server URL (customer doesn't need to enter this)
const String PRODUCTION_SERVER = "https://algae-monitoring-final-production.up.railway.app/api/sensor-data";

// --- Timing ---
unsigned long lastSend = 0;
const unsigned long sendInterval = 5000; // Send every 5 seconds
const float VOLTAGE_DIVIDER_RATIO = 2.0;
#define BATTERY_SAMPLES 10

// --- Battery Monitoring (Optional) ---
#define BATTERY_PIN 35
float batteryVoltage = 0.0;
int batteryPercentage = 0;
bool isUSBPowered = false;

// --- Demo Scenario State ---
// Phase 1 (0-15s):  Device out of water (air readings)
// Phase 2 (15s+):   Device in high-risk bloom water (temp/turbidity/EC = HIGH RISK)
unsigned long demoStartTime = 0;
bool demoStarted = false;

// High-risk bloom targets (after 15 seconds)
// Temp    > 25°C  → HIGH (we use ~27.5°C)
// Turbidity > 50 NTU → HIGH (we use ~70 NTU)
// EC      > 1000 µS/cm → HIGH (we use ~1120 µS/cm)
float bloomTemp    = 27.5;
float bloomTurb    = 70.0;
float bloomEC      = 1120.0;

// Read Battery Voltage with averaging
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

// Calculate Battery Percentage
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

// Generate unique device ID from MAC address
void generateDeviceId() {
    uint64_t chipid = ESP.getEfuseMac();
    g_deviceId = "ESP32-" + String((uint32_t)(chipid >> 24), HEX);
    g_deviceId.toUpperCase();
}

// Load saved configuration from flash memory
bool loadConfiguration() {
    preferences.begin("phycosense", true); // Read-only
    
    g_serverUrl = preferences.getString("serverUrl", "");
    g_deviceName = preferences.getString("deviceName", "");
    g_backupServerUrl = preferences.getString("backupUrl", "");
    g_accessKey = preferences.getString("accessKey", "");
    
    preferences.end();
    
    if (g_serverUrl.length() > 0) {
        if (g_deviceName.length() == 0) {
            g_deviceName = g_deviceId; // Use device ID as fallback name
        }
        return true;
    }
    return false;
}

// Save configuration to flash memory
void saveConfiguration(String server, String name, String backup = "") {
    preferences.begin("phycosense", false); // Read-write
    
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

// Clear all saved configuration (factory reset)
void clearConfiguration() {
    preferences.begin("phycosense", false);
    preferences.clear();
    preferences.end();
    
    // Also clear WiFiManager saved credentials
    WiFiManager wm;
    wm.resetSettings();
    
    Serial.println("✓ Configuration cleared - device reset to factory defaults");
    Serial.println("✓ WiFi credentials erased");
}

// Start WiFi provisioning mode (captive portal)
void startProvisioningMode() {
    Serial.println("\n=== PROVISIONING MODE ===");
    Serial.print("Connect to hotspot: PhycoSense-");
    Serial.println(g_deviceId);
    Serial.println("No password - open network");
    Serial.println("Portal will open automatically.\n");

    WiFiManager wm;

    // Only ask for device name — server URL is hardcoded
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

    if (!wm.autoConnect(apName.c_str())) {  // Open network — no password
        Serial.println("Provisioning timed out - restarting...");
        delay(3000);
        ESP.restart();
    }

    // WiFi connected — use device name, hardcode server URL
    g_deviceName = custom_name.getValue();
    if (g_deviceName.length() == 0) g_deviceName = g_deviceId;
    g_serverUrl = PRODUCTION_SERVER;

    saveConfiguration(g_serverUrl, g_deviceName);

    Serial.println("\n✓ Provisioning complete!");
    Serial.print("   Device Name: "); Serial.println(g_deviceName);
    Serial.print("   Server URL:  "); Serial.println(g_serverUrl);

    isProvisioned = true;

    // ---- Register in PURE STA mode (AP not open yet, so HTTPS works reliably) ----
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

    // Open key portal AFTER registration (hotspot appears ~15s after Save)
    startKeyPortal();
}

// Check if provision button is held (for factory reset)
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

// Register device with backend and get access key
bool registerDevice() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    HTTPClient http;
    StaticJsonDocument<256> doc;
    
    // Use the auth registration endpoint
    String baseUrl = g_serverUrl;
    // Remove /sensor-data to get base API URL, then add auth endpoint
    baseUrl.replace("/sensor-data", "");
    if (!baseUrl.endsWith("/")) baseUrl += "/";
    String registerUrl = baseUrl + "auth/register-device";
    
    doc["deviceId"] = g_deviceId;
    doc["deviceName"] = g_deviceName;
    // Use server as single source of truth for key generation/recovery.
    // If server supports deterministic recovery for existing deviceId, it will return that key.
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
        Serial.print("✓ Registration response (" + String(httpResponseCode) + "): ");
        Serial.println(response);
        
        // Parse response to get access key
        StaticJsonDocument<512> resDoc;
        DeserializationError error = deserializeJson(resDoc, response);
        
        if (!error && resDoc.containsKey("accessKey")) {
            g_accessKey = resDoc["accessKey"].as<String>();
            
            // Save the access key to flash
            preferences.begin("phycosense", false);
            preferences.putString("accessKey", g_accessKey);
            preferences.end();
            
            Serial.println("\n╔════════════════════════════════════════╗");
            Serial.println("║      YOUR DASHBOARD ACCESS KEY         ║");
            Serial.println("╠════════════════════════════════════════╣");
            Serial.print("║      ");
            Serial.print(g_accessKey);
            Serial.println("            ║");
            Serial.println("╠════════════════════════════════════════╣");
            Serial.println("║  Go to phycosense.app and enter this   ║");
            Serial.println("║  key to view your device's dashboard   ║");
            Serial.println("╚════════════════════════════════════════╝\n");
            
            // Re-open hotspot so customer can retrieve key at 192.168.4.1
            startKeyPortal();
        } else if (httpResponseCode == 409) {
            // Device already registered — server returns the key so a re-flashed device can recover it
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
        Serial.print("\u26a0 Registration failed (will retry): ");
        Serial.println(g_lastRegError);
        http.end();
        return false;
    }
}

// Open AP hotspot and serve the access key at 192.168.4.1
// Customer reconnects to PhycoSense-XXXX hotspot and opens 192.168.4.1
void startKeyPortal() {
    if (keyPortalRunning) return;

    if (g_accessKey.length() > 0) {
        // Key known: AP+STA so device keeps sending data
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    } else {
        // Key not known: pure AP only (avoids AP+STA HTTPS interference)
        WiFi.disconnect(true);
        delay(300);
        WiFi.mode(WIFI_AP);
    }

    WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    String apName = "PhycoSense-" + g_deviceId;
    WiFi.softAP(apName.c_str());  // Open network — no password

    // /status — JSON endpoint polled by the key page (avoids full page reload)
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
            // Auto-refresh every 4 seconds while waiting for registration
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
            // Waiting for registration — poll /status every 3 seconds via JS instead of full reload
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
    Serial.println("  Then open 192.168.4.1 in your browser");
    Serial.println("  (Portal stays open for 10 minutes)\n");
}

// Read Serial Command
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

// WiFi Connection (uses saved credentials from provisioning)
void connectWiFi() {
    if (!isProvisioned) {
        Serial.println("⚠ Device not provisioned - skipping WiFi connection");
        return;
    }
    
    Serial.println("Connecting to WiFi...");
    
    // WiFi credentials are already saved by WiFiManager
    WiFi.mode(WIFI_STA);
    WiFi.begin(); // Uses saved credentials
    
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
        
        // Register device with backend
        registerDevice();
    } else {
        Serial.println("\n✗ WiFi Connection Failed - will retry");
    }
}

// Send Data to Server (with fallback support)
void sendDataToServer() {
    if (!isProvisioned) {
        Serial.println("⚠ Device not provisioned - cannot send data");
        return;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("✗ WiFi disconnected - reconnecting...");
        connectWiFi();
        return;
    }

    // --- Demo scenario timing ---
    if (!demoStarted) {
        demoStartTime = millis();
        demoStarted = true;
    }
    unsigned long elapsed = millis() - demoStartTime;
    bool inWater = (elapsed >= 15000);  // Phase 2 starts at 15 seconds

    HTTPClient http;
    StaticJsonDocument<256> doc;
    float sendTemperature = 0;
    float sendTurbidity = 0;
    int sendEc = 0;
    float sendPh = 7.0;
    float sendDissolvedOxygen = 7.0;

    doc["deviceId"]   = g_deviceId;
    doc["deviceName"] = g_deviceName;

    if (!inWater) {
        // ── Phase 1: Out of water (0-15s) ────────────────────────────────
        // Air readings: ambient temp, near-zero turbidity/EC, normal pH/DO
        Serial.println("── PHASE 1: Device out of water ──");
        sendTemperature = 29.0 + random(-5, 5) * 0.1;  // ~28.5-29.5°C (air temp)
        sendTurbidity = 0.5 + random(0, 5) * 0.1;      // ~0.5-1.0 NTU (air)
        sendEc = 10 + random(-5, 5);                   // ~5-15 µS/cm (air)
        sendPh = 7.0 + random(-2, 2) * 0.1;            // normal, sensor not in water
        sendDissolvedOxygen = 7.8 + random(-3, 3) * 0.1; // normal oxygen in air
    } else {
        // ── Phase 2: High-risk algae bloom water (15s+) ──────────────────
        // Temp / Turbidity / EC  → HIGH RISK, drift minimally
        // pH / DO                → normal mock (sensors not connected)
        Serial.println("── PHASE 2: High-risk bloom water ──");

        // Drift bloom values slightly each reading to look live
        bloomTemp += (random(-2, 3) * 0.05f);   // ±0.1°C max per tick
        bloomTurb += (random(-3, 4) * 0.2f);    // ±0.6 NTU max per tick
        bloomEC   += (random(-4, 5) * 1.0f);    // ±4 µS/cm max per tick

        // Clamp to keep firmly in HIGH-RISK zone
        bloomTemp = constrain(bloomTemp, 26.5f, 28.5f);   // HIGH: > 25°C
        bloomTurb = constrain(bloomTurb, 60.0f, 80.0f);   // HIGH: > 50 NTU
        bloomEC   = constrain(bloomEC,  1050.0f, 1200.0f);// HIGH: > 1000 µS/cm

        sendTemperature = bloomTemp;
        sendTurbidity = bloomTurb;
        sendEc = (int)bloomEC;

        // pH and DO stay normal — sensors not connected to these parameters
        sendPh = 7.8 + random(-2, 2) * 0.1;               // 7.6-8.0 (normal)
        sendDissolvedOxygen = 6.5 + random(-3, 3) * 0.1; // 6.2-6.8 mg/L (normal)
    }

    doc["temperature"] = sendTemperature;
    doc["turbidity"] = sendTurbidity;
    doc["ec"] = sendEc;
    doc["ph"] = sendPh;
    doc["dissolvedOxygen"] = sendDissolvedOxygen;

    // Battery / power data (real if battery connected)
    doc["batteryVoltage"]    = round(batteryVoltage * 100) / 100.0;
    doc["batteryPercentage"] = batteryPercentage;
    doc["isUSBPowered"]      = isUSBPowered;

    String jsonString;
    serializeJson(doc, jsonString);

    Serial.println(F("\n--- SENT DATA REPORT ---"));
    Serial.print(F("Temperature: ")); Serial.print(sendTemperature, 2); Serial.println(F(" °C"));
    Serial.print(F("pH:          ")); Serial.println(sendPh, 2);
    Serial.print(F("EC:          ")); Serial.print(sendEc); Serial.println(F(" µS/cm"));
    Serial.print(F("Turbidity:   ")); Serial.print(sendTurbidity, 2); Serial.println(F(" NTU"));
    Serial.print(F("DO:          ")); Serial.print(sendDissolvedOxygen, 2); Serial.println(F(" mg/L"));
    Serial.println(F("------------------------"));

    Serial.print(">> Sending JSON: ");
    Serial.println(jsonString);
    
    // Try primary server
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
    } else {
        Serial.print("✗ Primary server error: ");
        Serial.println(http.errorToString(httpResponseCode));
        http.end();
        
        // Try backup server if configured
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
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    
    Serial.println(F("\n╔═══════════════════════════════════════╗"));
    Serial.println(F("║  PhycoSense WiFi Provisioning Test  ║"));
    Serial.println(F("║     (No Sensors - Mock Data Only)     ║"));
    Serial.println(F("╚═══════════════════════════════════════╝\n"));
    
    // Init provision button
    pinMode(PROVISION_BUTTON, INPUT_PULLUP);
    
    // Generate unique device ID
    generateDeviceId();
    Serial.print("Device ID: ");
    Serial.println(g_deviceId);
    
    // Try to load saved configuration
    isProvisioned = loadConfiguration();
    bool justProvisioned = false;

    if (!isProvisioned) {
        Serial.println("⚠ No configuration found - entering provisioning mode\n");
        startProvisioningMode(); // also calls registerDevice() + startKeyPortal() internally
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
    
    // Init Battery Monitoring
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

    // Connect to WiFi (if provisioned)
    // Skip if we just provisioned — WiFiManager already connected us and startKeyPortal() is running
    if (isProvisioned && !justProvisioned) {
        connectWiFi();
        // Open key portal (registration retries happen in loop())
        startKeyPortal();
    }
    
    Serial.println(F("\n=== Commands ==="));
    Serial.println(F("  RESET  - Factory reset device"));
    Serial.println(F("  STATUS - Show device info"));
    Serial.println(F("  KEY    - Show dashboard access key"));
    Serial.println(F("  ⚠ Or hold BOOT button for 5 seconds\n"));
    
    Serial.println(F("Starting data transmission (every 5 seconds)..."));
    Serial.println(F(""));
}

void loop()
{
    unsigned long now = millis();
    
    // Check provision button for factory reset
    checkProvisionButton();
    
    // Handle serial commands
    char cmd[20];
    if (readSerial(cmd)) {
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
    }
    
    // Send data every 5 seconds
    if (now - lastSend >= sendInterval) {
        lastSend = now;
        
        // Update battery status
        batteryVoltage = readBatteryVoltage();
        batteryPercentage = calculateBatteryPercentage(batteryVoltage);
        
        // Send to server (only if provisioned)
        if (isProvisioned) {
            sendDataToServer();
        } else {
            Serial.println("⚠ Device not provisioned - not sending data");
        }
    }
    
    delay(100); // Small delay to prevent watchdog issues

    // Handle key portal (AP hotspot at 192.168.4.1)
    if (keyPortalRunning) {
        keyServer.handleClient();

        // Auto-disable after 10 minutes, then resume normal operation
        if (millis() - keyPortalStartTime > KEY_PORTAL_DURATION) {
            keyServer.close();
            WiFi.softAPdisconnect(true);
            keyPortalRunning = false;
            Serial.println("Key portal closed. Reconnecting WiFi...");
            connectWiFi();
        }
    }
}

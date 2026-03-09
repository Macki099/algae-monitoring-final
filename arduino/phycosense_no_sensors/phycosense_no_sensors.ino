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
const unsigned long REGISTRATION_RETRY_INTERVAL = 10000; // retry every 10 seconds

// Production server URL (customer doesn't need to enter this)
const String PRODUCTION_SERVER = "https://algae-monitoring-final-production.up.railway.app/api/sensor-data";

// --- Battery Monitoring (Optional) ---
#define BATTERY_PIN 35
const float VOLTAGE_DIVIDER_RATIO = 2.0;
#define BATTERY_SAMPLES 10

float batteryVoltage = 0.0;
int batteryPercentage = 0;
bool isUSBPowered = false;

// --- Timing ---
unsigned long lastSend = 0;
const unsigned long sendInterval = 5000; // Send every 5 seconds

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
    Serial.println("Password: phyco123");
    Serial.println("Portal will open automatically.\n");

    WiFiManager wm;

    // Only ask for device name — server URL is hardcoded
    WiFiManagerParameter custom_name("name", "Pond / Device Name", g_deviceId.c_str(), 40,
        "placeholder=\"e.g. Main Pond\"");

    String htmlContent =
        "<br/>"
        "<div style='background:#e8f5e9;border-radius:8px;padding:14px;margin:10px 0;font-size:14px;color:#2e7d32'>"
        "<b>PhycoSense Setup</b><br/>"
        "Enter your WiFi password and give your device a name.<br/>"
        "After saving, reconnect to <b>PhycoSense-" + g_deviceId + "</b> and open "
        "<b>192.168.4.1</b> to get your dashboard access key."
        "</div>";
    WiFiManagerParameter custom_html(htmlContent.c_str());

    wm.addParameter(&custom_html);
    wm.addParameter(&custom_name);

    wm.setConfigPortalTimeout(300);
    wm.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));

    String apName = "PhycoSense-" + g_deviceId;

    if (!wm.autoConnect(apName.c_str(), "phyco123")) {
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

    // WiFiManager already connected us — register immediately while connection is fresh
    Serial.println("Registering device with server...");
    registerDevice();

    // Always open the key portal so customer can reconnect and see their key
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
            // Device already registered - key was already given
            Serial.println("Device already registered.");
            if (g_accessKey.length() > 0) {
                Serial.print("Saved access key: ");
                Serial.println(g_accessKey);
                startKeyPortal();
            }
        }
        
        http.end();
        return true;
    } else {
        Serial.print("⚠ Registration failed (will retry): ");
        Serial.println(http.errorToString(httpResponseCode));
        http.end();
        return false;
    }
}

// Open AP hotspot and serve the access key at 192.168.4.1
// Customer reconnects to PhycoSense-XXXX hotspot and opens 192.168.4.1
void startKeyPortal() {
    if (keyPortalRunning) return;

    // Enable AP alongside STA (dual mode — device keeps sending data)
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    String apName = "PhycoSense-" + g_deviceId;
    WiFi.softAP(apName.c_str(), "phyco123");

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
            // Registration still in progress
            html += "<div class='check'>&#9203;</div>";
            html += "<div class='title'>Almost Ready...</div>";
            html += "<div class='waiting'>Registering your device with the server.<br>This page will refresh automatically.</div>";
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
    Serial.println("  Password: phyco123");
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
    
    HTTPClient http;
    StaticJsonDocument<256> doc;
    
    // Device identification
    doc["deviceId"] = g_deviceId;
    doc["deviceName"] = g_deviceName;
    
    // Mock sensor data (since no sensors connected)
    doc["temperature"] = 25.0 + random(-5, 5) * 0.1;  // 24.5-25.5°C
    doc["ph"] = 7.0 + random(-2, 2) * 0.1;            // 6.8-7.2 pH
    doc["ec"] = 500 + random(-50, 50);                // 450-550 µS/cm
    doc["turbidity"] = 5.0 + random(0, 10) * 0.1;     // 5.0-6.0 NTU
    doc["dissolvedOxygen"] = 8.5 + random(-3, 3) * 0.1;  // 8.2-8.8 mg/L
    
    // Battery/Power data (real if battery connected)
    doc["batteryVoltage"] = round(batteryVoltage * 100) / 100.0;
    doc["batteryPercentage"] = batteryPercentage;
    doc["isUSBPowered"] = isUSBPowered;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
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

        // Re-open the key portal on every reboot so customer can always retrieve their key
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
        
        // Display mock data
        Serial.println(F("\n--- MOCK DATA REPORT ---"));
        Serial.println(F("Temperature: 25.0 °C (Mock)"));
        Serial.println(F("pH: 7.0 (Mock)"));
        Serial.println(F("EC: 500 µS/cm (Mock)"));
        Serial.println(F("Turbidity: 5.0 NTU (Mock)"));
        Serial.print(F("Battery: "));
        if (isUSBPowered) {
            Serial.println(F("USB Powered"));
        } else {
            Serial.print(batteryVoltage, 2);
            Serial.print(F("V ("));
            Serial.print(batteryPercentage);
            Serial.println(F("%)"));
        }
        Serial.println(F("------------------------"));
        
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

        // Retry registration if key not yet received
        if (g_accessKey.length() == 0 && WiFi.status() == WL_CONNECTED) {
            if (millis() - lastRegistrationAttempt > REGISTRATION_RETRY_INTERVAL) {
                lastRegistrationAttempt = millis();
                Serial.println("Retrying device registration...");
                registerDevice();
            }
        }

        // Auto-disable AP after 10 minutes
        if (millis() - keyPortalStartTime > KEY_PORTAL_DURATION) {
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            keyPortalRunning = false;
            Serial.println("Key portal closed. Device operating normally.");
        }
    }
}

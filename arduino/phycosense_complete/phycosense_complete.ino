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
#include <ArduinoJson.h>
// #include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
// #include <Preferences.h>

// --- WiFi Provisioning Configuration ---
// NOTE: Full WiFi provisioning coming soon. For now, use hardcoded credentials.
// For testing WiFi provisioning, use phycosense_no_sensors.ino

// Temporary hardcoded credentials (will be replaced with provisioning)
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD"; 
String serverUrl = "https://algae-monitoring-final-production.up.railway.app/api/sensor-data";
String deviceId = "ESP32-SENSOR-01";
String deviceName = "Sensor Device";

/* WiFi Provisioning Variables (disabled for now)
Preferences preferences;
String deviceId;             // Auto-generated unique ID from MAC
String deviceName;           // User-configurable device name
String serverUrl;            // Server URL configured during provisioning
String backupServerUrl;      // Optional local backup server

// Provisioning mode button (Optional - use GPIO 0 / BOOT button)
#define PROVISION_BUTTON 0
#define PROVISION_HOLD_TIME 5000  // Hold for 5 seconds to reset

// Device status
bool isProvisioned = false;
*/

// --- Hardware Definitions ---
#define ONE_WIRE_BUS 4      // DS18B20 Data Pin
#define TURBIDITY_PIN 16     // Turbidity Analog Pin
#define BATTERY_PIN 35       // Battery Voltage Monitor (ADC1_CH7)
#define PROBIOTIC_LEVEL_PIN 34  // Water Level Sensor for Probiotic Chamber (ADC1_CH6)
// --- Objects ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DFRobot_ESP_PH_WITH_ADC ph;
DFRobot_ESP_EC ec;
Adafruit_ADS1115 ads;

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
const float VOLTAGE_DIVIDER_RATIO = 2.0; // Adjust based on your voltage divider (R1=R2=100k)
#define BATTERY_SAMPLES 10 

// -------------------------------------------------------------------------
// --- Helper Functions ---
// -------------------------------------------------------------------------

// Read DS18B20 Temperature
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

// Read and Average Turbidity Voltage
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

// Map Float function
float map_float(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Read Probiotic Chamber Water Level
float readProbioticLevel()
{
    int rawValue = analogRead(PROBIOTIC_LEVEL_PIN);
    float voltage = (rawValue / 4095.0) * 3.3;  // Convert to voltage
    
    // Water level sensors typically output:
    // - Min voltage (~0.5V) = Empty (0%)
    // - Max voltage (~2.5V) = Full (100%)
    // Adjust these values based on your specific sensor calibration
    float minVoltage = 0.5;
    float maxVoltage = 2.5;
    
    // Map voltage to percentage
    float percentage = ((voltage - minVoltage) / (maxVoltage - minVoltage)) * 100.0;
    percentage = constrain(percentage, 0, 100);
    
    return percentage;
}

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

// Calculate Battery Percentage from Voltage (LiPo 3.0V-4.2V)
int calculateBatteryPercentage(float voltage)
{
    // USB/AC Power detection
    // Case 1: Voltage > 4.5V means USB powered through voltage divider
    // Case 2: Voltage < 0.5V means no battery connected, likely USB powered
    if (voltage > 4.5 || voltage < 0.5) {
        isUSBPowered = true;
        return 100; // Show 100% when USB powered
    }
    
    isUSBPowered = false;
    
    // LiPo battery voltage curve (approximate)
    if (voltage >= 4.2) return 100;
    if (voltage >= 4.0) return 80 + (int)((voltage - 4.0) * 100);
    if (voltage >= 3.8) return 60 + (int)((voltage - 3.8) * 100);
    if (voltage >= 3.6) return 40 + (int)((voltage - 3.6) * 100);
    if (voltage >= 3.4) return 20 + (int)((voltage - 3.4) * 100);
    if (voltage >= 3.0) return (int)((voltage - 3.0) * 50);
    return 0; // Below 3.0V = dead battery
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

// WiFi Connection
void connectWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
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
        Serial.println(deviceId);
        Serial.print("Device Name: ");
        Serial.println(deviceName);
        Serial.print("Sending data to: ");
        Serial.println(serverUrl);
    } else {
        Serial.println("\n✗ WiFi Connection Failed - will retry");
    }
}

// Send Data to Server
void sendDataToServer(float temp, float pH, float ecVal, float turbidity) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("✗ WiFi disconnected - reconnecting...");
        connectWiFi();
        return;
    }
    
    HTTPClient http;
    StaticJsonDocument<256> doc;
    
    // Device identification
    doc["deviceId"] = deviceId;
    doc["deviceName"] = deviceName;
    
    // Sensor data
    doc["temperature"] = temp;
    doc["ph"] = 7.0;  // Placeholder: neutral pH (sensor not installed yet)
    doc["ec"] = ecVal * 1000;  // Convert ms/cm to µS/cm (9.7 ms/cm → 9700 µS/cm)
    doc["turbidity"] = turbidity;
    doc["dissolvedOxygen"] = 8.5; // Default
    doc["probioticLevel"] = probioticLevelPercentage;
    
    // Battery/Power data
    doc["batteryVoltage"] = round(batteryVoltage * 100) / 100.0;
    doc["batteryPercentage"] = batteryPercentage;
    doc["isUSBPowered"] = isUSBPowered;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Debug: Print the EXACT JSON being sent
    Serial.print(">> Sending JSON: ");
    Serial.println(jsonString);
    
    http.begin(serverUrl.c_str());
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
        Serial.print("✓ Success! Code: ");
        Serial.println(httpResponseCode);
    } else {
        Serial.print("✗ Error! Code: ");
        Serial.print(httpResponseCode);
        Serial.print(" (");
        Serial.print(http.errorToString(httpResponseCode));
        Serial.println(")");
    }
    
    http.end();
}

// -------------------------------------------------------------------------
// --- Setup ---
// -------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(1000);
    EEPROM.begin(64);
    
    Serial.println(F("\n=== PhycoSense Algae Monitoring System ==="));
    
    // Init Temperature Sensor (DS18B20) - CRITICAL FIX!
    sensors.begin();
    Serial.print("Found ");
    Serial.print(sensors.getDeviceCount());
    Serial.println(" DS18B20 temperature sensor(s)");
    if (sensors.getDeviceCount() == 0) {
        Serial.println("⚠ WARNING: No DS18B20 detected! Check wiring & 4.7k pull-up resistor!");
    }
    
    // Init ADS1115
    ads.setGain(GAIN_ONE);
    ads.begin();
    
    // Init DFRobot Objects
    ph.begin(10); // Store 2 values starting at address 10
    ec.begin(20); // Store 2 values starting at address 20
    
    // Init Turbidity
    pinMode(TURBIDITY_PIN, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    
    // Init Probiotic Level Sensor
    pinMode(PROBIOTIC_LEVEL_PIN, INPUT);
    
    // Init Battery Monitoring
    pinMode(BATTERY_PIN, INPUT);
    
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

    // Connect to WiFi
    connectWiFi();
    
    Serial.println(F("\nSensor Commands:"));
    Serial.println(F("  EC/pH:     PH, EC, EXITPH, EXITEC"));
    Serial.println(F("  Turbidity: CAL, CAL1 (Clear), CAL2 (Turbid), DONE"));
    Serial.println(F(""));
}

// -------------------------------------------------------------------------
// --- Main Loop ---
// -------------------------------------------------------------------------
void loop()
{
    unsigned long now = millis();

    // --- Task 0: 1000ms Interval (Readings & Calibration Handling) ---
    if (now - last[0] >= intervals[0]) 
    {
        last[0] = now;

        // 1. Always update Temperature
        temperature = getWaterTemperature();

        // 2. Always update Turbidity Raw Data
        turbidityVoltage = readTurbidityAvg();
        turbidityRaw = analogRead(TURBIDITY_PIN);
        turbidityNTU = map_float(turbidityVoltage, clearWaterVoltage, turbidWaterVoltage, 0, knownTurbidNTU);
        turbidityNTU = constrain(turbidityNTU, 0, 3000);
        
        // 2b. Update Probiotic Level
        probioticLevelPercentage = readProbioticLevel();
        
        // 2b. Update Battery Status
        batteryVoltage = readBatteryVoltage();
        batteryPercentage = calculateBatteryPercentage(batteryVoltage);

        // 3. Handle Serial Commands
        char cmd[20];
        if (readSerial(cmd))
        {
            strupr(cmd);
            String cmdStr = String(cmd);

            // --- TURBIDITY COMMANDS ---
            if (cmdStr == "CAL") {
                turbidityCalibrationMode = true;
                turbidityCalStep = 0;
                Serial.println(F("\n[Turbidity] CAL MODE START. Step 1: Put in CLEAR water, wait, then type CAL1"));
            }
            else if (cmdStr == "CAL1" && turbidityCalibrationMode) {
                clearWaterVoltage = turbidityVoltage;
                turbidityCalStep = 1;
                Serial.print(F("[Turbidity] Clear Water Calibrated at: ")); Serial.print(clearWaterVoltage); Serial.println(" V");
                Serial.println(F("Step 2: Put in DIRTY water, wait, then type CAL2"));
            }
            else if (cmdStr == "CAL2" && turbidityCalibrationMode && turbidityCalStep == 1) {
                turbidWaterVoltage = turbidityVoltage;
                turbidityCalStep = 2;
                Serial.print(F("[Turbidity] Turbid Water Calibrated at: ")); Serial.print(turbidWaterVoltage); Serial.println(" V");
                Serial.println(F("Type DONE to finish."));
            }
            else if (cmdStr == "DONE" && turbidityCalibrationMode) {
                turbidityCalibrationMode = false;
                Serial.println(F("[Turbidity] Calibration Saved (RAM only)."));
            }
            
            // --- EC / PH COMMANDS ---
            else if (!turbidityCalibrationMode) {
                 if (ecPhCalibrationActive || strstr(cmd, "PH") || strstr(cmd, "EC"))
                {
                    ecPhCalibrationActive = true;
                    ecVoltage = ads.readADC_SingleEnded(0) / 10;
                    phVoltage = ads.readADC_SingleEnded(1) / 10;

                    if (strstr(cmd, "PH")) ph.calibration(phVoltage, temperature, cmd);
                    if (strstr(cmd, "EC")) ec.calibration(ecVoltage, temperature, cmd);
                    
                    if (strstr(cmd, "EXITPH") || strstr(cmd, "EXITEC")) {
                        ecPhCalibrationActive = false;
                        Serial.println(F("[EC/pH] Calibration Exited."));
                    }
                }
            }
        }

        // 4. EC/pH Calibration Debug
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

    // --- Task 3: 5000ms Interval (Display All Data) ---
    if (now - last[3] >= intervals[3]) 
    {
        last[3] = now;
        
        if (!ecPhCalibrationActive && !turbidityCalibrationMode)
        {
            // Update ADC readings for display
            ecVoltage = ads.readADC_SingleEnded(0) / 10;
            phVoltage = ads.readADC_SingleEnded(1) / 10;
            
            ecValue = ec.readEC(ecVoltage, temperature);
            phValue = ph.readPH(phVoltage, temperature);

            Serial.println(F("\n--- WATER QUALITY REPORT ---"));
            
            Serial.print(F("Temp:      ")); 
            Serial.print(temperature, 1); 
            Serial.println(F(" °C"));
            
            Serial.print(F("EC:        ")); 
            Serial.print(ecValue, 2);  // Use calibrated ecValue
            Serial.print(F(" ms/cm (V:")); Serial.print(ecVoltage); Serial.println(")");

            Serial.print(F("pH:        ")); 
            Serial.print(phValue, 2); 
            Serial.print(F(" (V:")); Serial.print(phVoltage); Serial.println(")");

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
    
    // --- Task 4: 5000ms Interval (Send Data to Server) ---
    if (now - last[4] >= intervals[4]) 
    {
        last[4] = now;
        
        // Only send if not calibrating
        if (!ecPhCalibrationActive && !turbidityCalibrationMode)
        {
            // Reuse the SAME readings from Task 3 display
            // No need to read again - ecValue and phValue are already fresh
            
            // Send to dashboard - this will send the EXACT value shown in the report
            sendDataToServer(temperature, phValue, ecValue, turbidityNTU);
        }
    }
}

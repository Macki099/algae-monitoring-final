# PhycoSense — User Manual

## What is PhycoSense?

PhycoSense is a **smart algae monitoring system** that uses an ESP32 microcontroller with physical sensors to monitor water quality in real time. The data is sent to the cloud and displayed on a live web dashboard at [www.phycosense.app](https://www.phycosense.app).

---

## System Flow Overview

```
[ Sensors in Tank ]
        |
        v
[ ESP32 Microcontroller ]
  - Reads: Temperature, pH, EC, Turbidity, Dissolved Oxygen, Probiotic Level
  - Every 5 seconds: sends a POST request to the backend server
        |
        v
[ Railway Backend Server ]  (Node.js / Express)
  - Receives sensor data
  - Forwards to ML Service for risk prediction
  - Saves everything to MongoDB database
        |
        +--> [ ML Service ]  (Python / Flask)
               - Predicts risk level: Normal / Moderate / High
               - Returns prediction + recommended action
        |
        v
[ MongoDB Database ]
  - Stores all historical sensor readings
  - Each reading tagged with deviceId (e.g. TANK-01)
        |
        v
[ Vercel Frontend ]  (React)  →  www.phycosense.app
  - User enters their Access Key from the device manual
  - Dashboard shows live readings, charts, risk assessment
```

---

## Step-by-Step: First-Time Setup for a New Device

### Step 1 — Power on the device
Plug in your PhycoSense unit. The ESP32 will automatically create a WiFi hotspot.

### Step 2 — Connect to the device hotspot
On your phone or laptop, connect to the WiFi network named:
```
PhycoSense-ESP32-XXXXXX
```
No password required — it is an open network.

### Step 3 — Open the setup portal
Once connected, open a browser and go to:
```
http://192.168.4.1
```
A setup page will load automatically.

### Step 4 — Enter your WiFi credentials
On the setup page:
- Enter your **WiFi network name** (SSID)
- Enter your **WiFi password**
- Enter a **device name** (e.g. "Main Tank", "Pond 1")
- Press **Save**

### Step 5 — Get your Access Key
After saving, reconnect to the `PhycoSense-ESP32-XXXXXX` hotspot and open `http://192.168.4.1` again. Your unique access key will be displayed:
```
PHY-XXXX-XXXX
```
**Write this key down** and keep it safe — it is your password to access the dashboard.

### Step 6 — Open the dashboard
Go to [www.phycosense.app](https://www.phycosense.app) on any browser.

Enter your access key (`PHY-XXXX-XXXX`) and press **Access Dashboard**.

### Step 7 — View your data
The dashboard will show:
- Live readings for all sensors (Temperature, pH, EC, Turbidity, DO, Probiotic Level)
- Historical charts for each parameter
- Risk assessment (Normal / Moderate / High) powered by the ML model
- Recommended actions based on current risk level

---

## Returning Users

If you have already logged in before, the dashboard will remember your key and show a **"Welcome back"** screen. Click **Open Dashboard** to resume.

To log out or switch devices, click **Clear saved session**.

---

## Dashboard Features

| Feature | Description |
|---|---|
| Live Readings | Updates every 5 seconds from your ESP32 |
| Parameter Cards | Shows current value, unit, and trend chart for each sensor |
| Risk Assessment | ML model classifies water quality as Normal / Moderate / High |
| Recommended Action | Suggests what to do based on the current risk (e.g. "Increase aeration") |
| Device Selector | Switch between multiple registered tanks |
| Export Data | Download historical sensor data as a CSV/Excel file by date range |

---

## Monitored Parameters

| Parameter | Unit | Normal Range (Algae) |
|---|---|---|
| Temperature | °C | 20 – 30 |
| pH | — | 7.0 – 9.0 |
| Dissolved Oxygen | mg/L | 5.0 – 8.0 |
| Electrical Conductivity | µS/cm | 500 – 1500 |
| Turbidity | NTU | < 100 |
| Probiotic Level | % | 50 – 100 |

---

## Factory Reset (Start Over)

If you need to change WiFi or reconfigure the device:
1. Hold the **BOOT button** on the ESP32 for **5 seconds**
2. The device will reset all saved settings
3. It will create the hotspot again — follow Step 2 onwards

---

## Troubleshooting

| Problem | Solution |
|---|---|
| Hotspot not appearing | Wait 30 seconds after powering on |
| Setup page won't load | Make sure you are connected to the PhycoSense hotspot, not your home WiFi |
| Access key not working | Make sure you enter it exactly as shown (e.g. `PHY-A3F2-KX91`) |
| Dashboard shows no data | Check that the ESP32 is powered on and connected to WiFi |
| "Unable to connect to server" | The Railway backend may be restarting — wait 1 minute and retry |
| Lost your access key | Hold BOOT button 5 sec to factory reset, then re-provision to get a new key |

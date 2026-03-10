# PhycoSense — GitHub Copilot Project Context

This file gives Copilot full context about this project so it can assist without needing re-introduction on any device.

---

## Project Overview

**PhycoSense** is a smart algae water quality monitoring system built for research/commercial use. An ESP32 microcontroller reads physical sensors in a tank and sends data to a cloud backend every 5 seconds. A React dashboard displays live readings and ML-powered risk predictions.

**Live URLs:**
- Frontend: https://www.phycosense.app (Vercel)
- Backend API: https://algae-monitoring-final-production.up.railway.app (Railway)
- GitHub repo: https://github.com/copilot302/algae-monitoring-final

---

## Architecture

```
ESP32 (Arduino C++)
  → POST /api/sensor-data  every 5s
  → Railway Backend (Node.js / Express)
      → ML Service (Python / Flask)  for risk prediction
      → MongoDB Atlas  for persistence
  ← React Frontend (Vercel)  polls GET /api/sensor-data/:deviceId
```

---

## Tech Stack

| Layer | Technology |
|---|---|
| Firmware | Arduino C++ (ESP32), WiFiManager, HTTPClient, ArduinoJson |
| Backend | Node.js, Express, Mongoose, dotenv, axios |
| Database | MongoDB Atlas |
| ML Service | Python, Flask, scikit-learn (Random Forest), joblib |
| Frontend | React 18, Webpack 5, Chart.js, Lucide React |
| Hosting | Vercel (frontend), Railway (backend), Render (ML service optional) |

---

## Repository Structure

```
algae-monitoring-final/
├── src/                          # React frontend source
│   ├── App.js                    # Root component, auth state
│   ├── components/
│   │   ├── LandingPage.js        # Auth screen — PHY-XXXX-XXXX key entry
│   │   ├── Header.js
│   │   ├── DeviceSelector.js     # Switch between authenticated devices
│   │   ├── ParameterCard.js      # Sensor value card with chart
│   │   ├── RiskAssessment.js     # ML risk display
│   │   ├── CircularGauge.js
│   │   ├── TankGauge.js          # Probiotic level gauge
│   │   ├── LineChart.js
│   │   └── dialogs/DateRangeDialog.js
│   ├── hooks/
│   │   ├── useSensorData.js      # Fetches live + historical data from backend
│   │   └── useRiskAssessment.js  # Derives risk color/label from sensor values
│   ├── styles/
│   └── utils/
│       ├── dataExport.js         # Export to CSV/Excel
│       └── trendAnalysis.js
│
├── server/                       # Node.js Express backend
│   ├── server.js                 # Entry point, Express setup
│   ├── routes/
│   │   ├── auth.js               # Key verification — reads devices.json (NO MongoDB for auth)
│   │   └── sensorData.js         # GET/POST sensor readings, calls ML service
│   ├── models/
│   │   └── SensorData.js         # Mongoose schema for sensor readings
│   ├── config/
│   │   ├── db.js                 # MongoDB Atlas connection
│   │   └── devices.json          # Key registry: { "PHY-XXXX-XXXX": { deviceId, deviceName } }
│   └── generate-keys.js          # CLI script: node generate-keys.js TANK-04 "Tank 4"
│
├── ml-service/                   # Python Flask ML service (port 5001)
│   ├── ml_service.py             # Flask app, Random Forest predict endpoint, PID controller
│   ├── rf_model.joblib           # Trained model (do not delete)
│   ├── label_encoder.joblib      # Label encoder (do not delete)
│   ├── train_model.py            # Retrain model from CSV data
│   └── requirements.txt
│
├── arduino/
│   ├── phycosense_complete/      # Full firmware with all sensors
│   ├── phycosense_no_sensors/    # WiFi provisioning test (no sensors)
│   └── esp32_sensor_integration/ # Basic integration test
│
├── railway.toml                  # MUST be at root — tells Railway: cd server && npm start
├── vercel.json                   # Tells Vercel: npm run build → dist/
└── webpack.config.js             # Injects REACT_APP_API_URL at build time
```

---

## Authentication System

**Approach: ISP modem-style pre-generated keys** (no user accounts, no JWT)

- Each device gets a unique `PHY-XXXX-XXXX` key
- Keys are stored in `server/config/devices.json` as plain JSON
- On first power-on, ESP32 runs WiFi provisioning (creates hotspot `PhycoSense-XXXXXX`)
- User connects, enters WiFi password + device name
- ESP32 calls `POST /api/auth/register-device` → server generates key → saves to `devices.json` → returns key
- Key is shown at `http://192.168.4.1` on the ESP32's hotspot portal
- User writes down the key, enters it at www.phycosense.app to access dashboard
- Key is saved in browser `localStorage` for auto-resume

**Auth routes** (`server/routes/auth.js`):
- `POST /api/auth/verify-key` — single key login
- `POST /api/auth/verify-keys` — bulk verify saved keys (returning users)
- `POST /api/auth/register-device` — called by ESP32 during provisioning

**No MongoDB for auth** — devices.json is the complete auth source of truth.

> ⚠ Railway has ephemeral filesystem. Keys registered at runtime are lost on redeploy.
> Fix: Always add new device keys to `devices.json` in the repo before deploying, OR implement MongoDB persistence for the key store.

---

## Sensor Data Flow

1. ESP32 reads sensors every 5 seconds
2. Sends `POST /api/sensor-data` with:
   ```json
   { "deviceId": "TANK-01", "deviceName": "Tank 1", "temperature": 25.3, "ph": 7.8, "ec": 920, "turbidity": 45, "dissolvedOxygen": 6.2, "probioticLevel": 80 }
   ```
3. Backend forwards to ML service `POST http://localhost:5001/predict`
4. ML service returns `{ "risk": "Normal", "confidence": 0.92, "action": "Maintain current conditions" }`
5. Backend saves full reading + prediction to MongoDB
6. Frontend polls `GET /api/sensor-data?deviceId=TANK-01&limit=1` every 5s for live data
7. Frontend polls `GET /api/sensor-data?deviceId=TANK-01&limit=100` for chart history

---

## ML Service

- **Model:** Random Forest Classifier (`rf_model.joblib`)
- **Input features:** temperature, pH, dissolvedOxygen, ec, turbidity
- **Output:** risk label (`Normal` / `Moderate` / `High`) + confidence + recommended action
- **PID Controller:** controls aerator pump speed based on dissolved oxygen level
- **Endpoint:** `POST /predict` with sensor JSON body
- **Retrain:** `python train_model.py` then restart the service

---

## Environment Variables

### `server/.env`
```env
MONGODB_URI=mongodb+srv://...
PORT=5000
ML_SERVICE_URL=http://localhost:5001
```

### Root `.env` (frontend)
```env
REACT_APP_API_URL=http://localhost:5000/api/sensor-data
```

### Vercel (set in dashboard)
```
REACT_APP_API_URL=https://algae-monitoring-final-production.up.railway.app/api/sensor-data
```

---

## Deployment Notes

### Vercel (Frontend)
- Deploy command: `npx vercel --prod` from project root
- Build command: `npm run build` → outputs to `dist/`
- **Does NOT auto-deploy from GitHub** — must run `npx vercel --prod` manually or connect GitHub in Vercel dashboard settings
- After every deploy, confirm `REACT_APP_API_URL` is set in Vercel environment variables

### Railway (Backend)
- Config file: `railway.toml` at repo root (CRITICAL — must be at root, not in `config/`)
- Build: `cd server && npm install`
- Start: `cd server && npm start`
- Railway project name: `zippy-healing`, service name: `nuwendo`
- **Does NOT reliably auto-deploy from GitHub** — use `npx @railway/cli up` from `server/` folder
- CLI link command: `npx @railway/cli link` → select `zippy-healing` → `nuwendo`

### Common Deployment Issues
- `Cannot POST /api/auth/verify-key` → Railway is running stale code. Run `npx @railway/cli up` from `server/`
- `Unexpected token 'export'` in browser console → Frontend is hitting the wrong URL (not the Express backend)
- Auth page not showing → Clear `localStorage` in browser: `localStorage.removeItem('phycosense_keys')`
- 404 on all API routes → `railway.toml` missing from repo root, or Railway running frontend instead of backend

---

## Local Development

Start all 3 services in separate terminals:
```powershell
# Terminal 1 — Frontend
npm start

# Terminal 2 — Backend
cd server ; npm run dev

# Terminal 3 — ML Service
cd ml-service ; python ml_service.py
```

Test keys for local dev (already in `devices.json`):
- `PHY-A3F2-KX91` → TANK-01
- `PHY-B7YQ-MN44` → TANK-02
- `PHY-C5TR-PZ83` → TANK-03

Add a new device key:
```powershell
cd server
node generate-keys.js TANK-04 "Tank 4"
```

---

## Key Design Decisions

1. **No JWT / no user accounts** — Auth is key-based (like a serial number). One key = one device = one user.
2. **devices.json instead of MongoDB for auth** — Simpler, no DB dependency for login. Trade-off: keys registered at runtime are lost on Railway redeploy.
3. **ML service is optional** — Backend gracefully continues saving data if ML service is down.
4. **ESP32 deviceId is MAC-based** — Auto-generated as `ESP32-XXXXXX`, not hardcoded. This means you cannot pre-generate keys before knowing the device MAC.
5. **Webpack DefinePlugin** injects `REACT_APP_API_URL` at build time — changing the env var requires a rebuild + redeploy.

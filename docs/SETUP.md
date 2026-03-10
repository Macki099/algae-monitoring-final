# PhycoSense — New Device Setup Guide

Run these steps in order when setting up this project on a new machine.

---

## Prerequisites

Make sure these are installed on your machine before starting:

| Tool | Version | Download |
|---|---|---|
| Node.js | v18+ | https://nodejs.org |
| Python | 3.10+ | https://python.org |
| Git | any | https://git-scm.com |

---

## Step 1 — Clone the repository

```powershell
git clone https://github.com/copilot302/algae-monitoring-final.git
cd algae-monitoring-final
```

---

## Step 2 — Install frontend dependencies

```powershell
npm install
```

---

## Step 3 — Install backend dependencies

```powershell
cd server
npm install
cd ..
```

---

## Step 4 — Install ML service dependencies

```powershell
cd ml-service
pip install -r requirements.txt
cd ..
```

---

## Step 5 — Set up environment variables

### Backend (`server/.env`)
Create `server/.env` with:
```env
MONGODB_URI=mongodb+srv://<user>:<password>@<cluster>.mongodb.net/phycosense
PORT=5000
ML_SERVICE_URL=http://localhost:5001
```
> Get the MongoDB URI from MongoDB Atlas. Ask the project owner for credentials.

### Frontend (`.env` at root)
Create `.env` at the project root with:
```env
REACT_APP_API_URL=http://localhost:5000/api/sensor-data
```
> For production, replace with the Railway URL:
> `REACT_APP_API_URL=https://algae-monitoring-final-production.up.railway.app/api/sensor-data`

---

## Step 6 — Start all services

Open **3 separate terminals** and run one command in each:

**Terminal 1 — Frontend (React)**
```powershell
cd c:\path\to\algae-monitoring-final
npm start
```
Opens at: http://localhost:3000

**Terminal 2 — Backend (Node/Express)**
```powershell
cd c:\path\to\algae-monitoring-final\server
npm run dev
```
Runs at: http://localhost:5000

**Terminal 3 — ML Service (Python/Flask)**
```powershell
cd c:\path\to\algae-monitoring-final\ml-service
python ml_service.py
```
Runs at: http://localhost:5001

---

## One-Command Start (PowerShell)

Alternatively, use the included script to start everything at once:

```powershell
cd c:\path\to\algae-monitoring-final
.\scripts\start-all.ps1
```

---

## Step 7 — Verify everything is running

| Service | URL | Expected Response |
|---|---|---|
| Frontend | http://localhost:3000 | PhycoSense login page |
| Backend health | http://localhost:5000/api/health | `{"status":"OK"}` |
| ML service health | http://localhost:5001/health | `{"status":"healthy"}` |

---

## Step 8 — Test authentication

On the login page at http://localhost:3000, enter one of the pre-loaded test keys:

```
PHY-A3F2-KX91   →  Tank 1
PHY-B7YQ-MN44   →  Tank 2
PHY-C5TR-PZ83   →  Tank 3
```

---

## Adding a new device key (before shipping a unit)

```powershell
cd server
node generate-keys.js TANK-04 "Tank 4"
```

The key is printed to the console and saved to `server/config/devices.json`.

---

## Deployment

| Service | Platform | Trigger |
|---|---|---|
| Frontend (React) | Vercel | `npx vercel --prod` |
| Backend (Node) | Railway | `git push` (if GitHub connected) or `npx @railway/cli up` |
| ML Service | Render / Railway | Manual or GitHub auto-deploy |

### Vercel environment variable (REQUIRED after deploy)
In Vercel → Project → Settings → Environment Variables:
```
REACT_APP_API_URL = https://algae-monitoring-final-production.up.railway.app/api/sensor-data
```

### Railway login
```powershell
npx @railway/cli login
npx @railway/cli link   # select: zippy-healing → nuwendo
npx @railway/cli up
```

---

## Project Structure Summary

```
algae-monitoring-final/
├── src/                    # React frontend
├── public/                 # index.html
├── server/                 # Node.js Express backend
│   ├── routes/
│   │   ├── auth.js         # Key verification (reads devices.json)
│   │   └── sensorData.js   # Sensor data CRUD
│   ├── models/
│   │   └── SensorData.js   # MongoDB schema
│   ├── config/
│   │   ├── db.js           # MongoDB connection
│   │   └── devices.json    # Device key registry (PHY-XXXX-XXXX → deviceId)
│   └── generate-keys.js    # Script to generate new device keys
├── ml-service/             # Python Flask ML service
│   ├── ml_service.py       # Flask app + PID controller
│   ├── rf_model.joblib     # Trained Random Forest model
│   └── train_model.py      # Retrain the model
├── arduino/                # ESP32 firmware (.ino files)
├── railway.toml            # Railway deployment config (root level)
├── vercel.json             # Vercel deployment config
└── webpack.config.js       # Frontend build config
```

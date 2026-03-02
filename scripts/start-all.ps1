# PhycoSense - Start All Services
Write-Host "Starting PhycoSense Complete System..." -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# Get the project root directory (parent of scripts folder)
$projectRoot = Split-Path -Parent $PSScriptRoot

# Start Backend Server
Write-Host "Starting Backend Server (Port 5000)..." -ForegroundColor Yellow
Start-Process powershell -ArgumentList "-NoExit", "-Command", "cd '$projectRoot\server'; npm start"

# Wait a bit for backend to initialize
Start-Sleep -Seconds 2

# Start ML Service
Write-Host "Starting ML Service (Port 5001)..." -ForegroundColor Yellow
Start-Process powershell -ArgumentList "-NoExit", "-Command", "cd '$projectRoot\ml-service'; python ml_service.py"

# Wait a bit for ML service to initialize
Start-Sleep -Seconds 2

# Start Frontend
Write-Host "Starting Frontend (Port 3000)..." -ForegroundColor Yellow
Start-Process powershell -ArgumentList "-NoExit", "-Command", "cd '$projectRoot'; npm start"

Write-Host ""
Write-Host "All services starting in separate windows!" -ForegroundColor Green
Write-Host ""
Write-Host "Services:" -ForegroundColor Cyan
Write-Host "  - Backend:  http://localhost:5000" -ForegroundColor White
Write-Host "  - ML API:   http://localhost:5001" -ForegroundColor White
Write-Host "  - Frontend: http://localhost:3000" -ForegroundColor White
Write-Host ""
Write-Host "Close each PowerShell window to stop the respective service." -ForegroundColor Gray

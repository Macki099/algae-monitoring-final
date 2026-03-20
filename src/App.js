import React, { useMemo, useState } from 'react';
import LandingPage from './components/LandingPage';
import Header from './components/Header';
import DeviceSelector from './components/DeviceSelector';
import ParameterCard from './components/ParameterCard';
import RiskAssessment from './components/RiskAssessment';
import Footer from './components/Footer';
import DateRangeDialog from './components/dialogs/DateRangeDialog';
import SettingsDialog from './components/dialogs/SettingsDialog';
import { useSensorData } from './hooks/useSensorData';
import { exportDataByDateRange, getExportPreview } from './utils/dataExport';
import './styles/App.css';

const SETTINGS_STORAGE_KEY = 'phycosense_settings';

const DEFAULT_SETTINGS = {
  device: {
    defaultDeviceId: '',
    aliases: {}
  },
  alerts: {
    sensitivity: 'balanced',
    cooldownMinutes: 10,
    temperatureNormalMax: 20,
    temperatureModerateMax: 25,
    dissolvedOxygenNormalMin: 5,
    dissolvedOxygenModerateMin: 2,
    phNormalMax: 8.5,
    phModerateMax: 9,
    electricalConductivityNormalMax: 800,
    electricalConductivityModerateMax: 1000,
    turbidityNormalMax: 10,
    turbidityModerateMax: 50
  },
  data: {
    pollIntervalSeconds: 5,
    historyWindowHours: 24,
    autoExport: 'off',
    exportFormat: 'csv'
  },
  display: {
    temperatureUnit: 'C',
    chartDensity: 'standard',
    riskColors: 'default',
    showSensors: {
      temperature: true,
      dissolvedOxygen: true,
      ph: true,
      electricalConductivity: true,
      turbidity: true,
      probioticLevel: true
    }
  }
};

const mergeSettings = (baseSettings, overrides = {}) => ({
  device: {
    ...baseSettings.device,
    ...(overrides.device || {}),
    aliases: {
      ...baseSettings.device.aliases,
      ...(overrides.device?.aliases || {})
    }
  },
  alerts: {
    ...baseSettings.alerts,
    ...(overrides.alerts || {})
  },
  data: {
    ...baseSettings.data,
    ...(overrides.data || {})
  },
  display: {
    ...baseSettings.display,
    ...(overrides.display || {}),
    showSensors: {
      ...baseSettings.display.showSensors,
      ...(overrides.display?.showSensors || {})
    }
  }
});

const loadSettings = () => {
  try {
    const stored = JSON.parse(localStorage.getItem(SETTINGS_STORAGE_KEY) || '{}');
    return mergeSettings(DEFAULT_SETTINGS, stored);
  } catch {
    return DEFAULT_SETTINGS;
  }
};

const App = () => {
  const [authenticatedDevices, setAuthenticatedDevices] = useState(null);
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [isExportModalOpen, setIsExportModalOpen] = useState(false);
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);
  const [settings, setSettings] = useState(loadSettings);

  const pollIntervalSeconds = Math.max(5, Number(settings.data.pollIntervalSeconds) || 5);
  const historyWindowHours = Math.max(1, Number(settings.data.historyWindowHours) || 24);
  const historyLimit = Math.min(
    500,
    Math.max(10, Math.round((historyWindowHours * 3600) / pollIntervalSeconds))
  );

  const { sensorData, dataHistory, isConnected, mlPrediction, mlServiceStatus } = useSensorData(selectedDevice, {
    pollIntervalSeconds,
    historyLimit
  });

  const mlOverallRisk = typeof mlPrediction?.risk === 'string'
    ? mlPrediction.risk.toLowerCase()
    : null;
  const isMlDriven = Boolean(mlOverallRisk);
  const displayOverallRisk = mlOverallRisk || 'unknown';
  const displayRiskLevels = mlOverallRisk
    ? {
        temperature: mlOverallRisk,
        dissolvedOxygen: mlOverallRisk,
        ph: mlOverallRisk,
        electricalConductivity: mlOverallRisk,
        turbidity: mlOverallRisk,
        probioticLevel: mlOverallRisk
      }
    : {
        temperature: 'unknown',
        dissolvedOxygen: 'unknown',
        ph: 'unknown',
        electricalConductivity: 'unknown',
        turbidity: 'unknown',
        probioticLevel: 'unknown'
      };

  const handleAuthenticated = (devices) => {
    setAuthenticatedDevices(devices);
    if (devices.length > 0) {
      const defaultId = settings.device.defaultDeviceId;
      const isAllowed = devices.some((device) => device.deviceId === defaultId);
      setSelectedDevice(isAllowed ? defaultId : devices[0].deviceId);
    }
  };

  const handleLogout = () => {
    setAuthenticatedDevices(null);
    setSelectedDevice(null);
    localStorage.removeItem('phycosense_keys');
  };

  const chartPointCount = useMemo(() => {
    switch (settings.display.chartDensity) {
      case 'compact':
        return 10;
      case 'detailed':
        return 40;
      default:
        return 20;
    }
  }, [settings.display.chartDensity]);

  const sliceHistory = (values) => values.slice(-chartPointCount);
  const showSensors = settings.display.showSensors;
  const temperatureValue = settings.display.temperatureUnit === 'F'
    ? (sensorData.temperature * 9) / 5 + 32
    : sensorData.temperature;
  const temperatureUnit = settings.display.temperatureUnit === 'F' ? '°F' : '°C';

  const appClassName = settings.display.riskColors === 'high-contrast'
    ? 'app risk-high-contrast'
    : 'app';

  // If not authenticated, show landing page
  if (!authenticatedDevices) {
    return <LandingPage onAuthenticated={handleAuthenticated} />;
  }

  // Note: Data logging removed - ESP32 sends data directly to backend
  // Frontend only displays data, doesn't generate it

  const handleExportData = () => {
    setIsExportModalOpen(true);
  };

  const handleExportWithDateRange = async (exportParams) => {
    await exportDataByDateRange({
      ...exportParams,
      format: settings.data.exportFormat,
      deviceId: selectedDevice,
      context: {
        deviceId: selectedDevice,
        sensorData,
        riskLevels: displayRiskLevels,
        overallRisk: displayOverallRisk,
        mlPrediction,
        isMlDriven
      }
    });
  };

  const handlePreviewWithDateRange = async (previewParams) => getExportPreview({
    ...previewParams,
    format: settings.data.exportFormat,
    deviceId: selectedDevice
  });

  const handleSaveSettings = (nextSettings) => {
    const merged = mergeSettings(DEFAULT_SETTINGS, nextSettings);
    setSettings(merged);
    localStorage.setItem(SETTINGS_STORAGE_KEY, JSON.stringify(merged));

    if (merged.device.defaultDeviceId && authenticatedDevices) {
      const isAllowed = authenticatedDevices.some(
        (device) => device.deviceId === merged.device.defaultDeviceId
      );
      if (isAllowed) {
        setSelectedDevice(merged.device.defaultDeviceId);
      }
    }
  };

  return (
    <div className={appClassName}>
      <div className="container">
        <Header 
          isConnected={isConnected}
          overallRisk={displayOverallRisk}
          onExportData={handleExportData}
          onOpenSettings={() => setIsSettingsOpen(true)}
          onLogout={handleLogout}
          exportFormat={settings.data.exportFormat}
        />
        
        <DeviceSelector
          selectedDevice={selectedDevice}
          onDeviceChange={setSelectedDevice}
          allowedDevices={authenticatedDevices}
          deviceAliases={settings.device.aliases}
        />
        
        <main className="dashboard">
          <div className="parameter-grid">
            {showSensors.temperature && (
              <ParameterCard
                title="Temperature"
                icon="thermometer"
                value={temperatureValue}
                unit={temperatureUnit}
                data={sliceHistory(dataHistory.temperature)}
                riskLevel={displayRiskLevels.temperature}
                type="chart"
              />
            )}
            
            {showSensors.dissolvedOxygen && (
              <ParameterCard
                title="Dissolved Oxygen"
                icon="droplets"
                value={sensorData.dissolvedOxygen}
                unit="mg/L"
                data={sliceHistory(dataHistory.dissolvedOxygen)}
                riskLevel={displayRiskLevels.dissolvedOxygen}
                type="chart"
              />
            )}
            
            {showSensors.ph && (
              <ParameterCard
                title="pH Level"
                icon="flask"
                value={sensorData.ph}
                unit=""
                data={sliceHistory(dataHistory.ph)}
                riskLevel={displayRiskLevels.ph}
                type="gauge"
                min={0}
                max={14}
              />
            )}
            
            {showSensors.electricalConductivity && (
              <ParameterCard
                title="Electrical Conductivity"
                icon="zap"
                value={sensorData.electricalConductivity}
                unit="µS/cm"
                data={sliceHistory(dataHistory.electricalConductivity)}
                riskLevel={displayRiskLevels.electricalConductivity}
                type="gauge"
                min={0}
                max={1200}
              />
            )}
            
            {showSensors.turbidity && (
              <ParameterCard
                title="Turbidity"
                icon="eye"
                value={sensorData.turbidity}
                unit="NTU"
                data={sliceHistory(dataHistory.turbidity)}
                riskLevel={displayRiskLevels.turbidity}
                type="chart"
              />
            )}
            
            {showSensors.probioticLevel && (
              <ParameterCard
                title="Probiotic Chamber"
                icon="droplet"
                value={sensorData.probioticLevel}
                unit="%"
                data={sliceHistory(dataHistory.probioticLevel)}
                riskLevel={displayRiskLevels.probioticLevel}
                type="tank"
                min={0}
                max={100}
              />
            )}
            
            <RiskAssessment
              overallRisk={displayOverallRisk}
              riskLevels={displayRiskLevels}
              sensorData={sensorData}
              mlPrediction={mlPrediction}
              mlServiceStatus={mlServiceStatus}
              isMlDriven={isMlDriven}
            />
          </div>
        </main>
        
        <Footer />
        
        <DateRangeDialog
          isOpen={isExportModalOpen}
          onClose={() => setIsExportModalOpen(false)}
          onExport={handleExportWithDateRange}
          onPreview={handlePreviewWithDateRange}
          exportFormat={settings.data.exportFormat}
        />

        <SettingsDialog
          isOpen={isSettingsOpen}
          onClose={() => setIsSettingsOpen(false)}
          onSave={handleSaveSettings}
          settings={settings}
          defaultSettings={DEFAULT_SETTINGS}
          devices={authenticatedDevices}
          selectedDevice={selectedDevice}
        />
      </div>
    </div>
  );
};

export default App;
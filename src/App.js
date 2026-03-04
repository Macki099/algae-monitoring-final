import React, { useState, useEffect } from 'react';
import Header from './components/Header';
import DeviceSelector from './components/DeviceSelector';
import ParameterCard from './components/ParameterCard';
import RiskAssessment from './components/RiskAssessment';
import Footer from './components/Footer';
import DateRangeDialog from './components/dialogs/DateRangeDialog';
import { useSensorData } from './hooks/useSensorData';
import { useRiskAssessment } from './hooks/useRiskAssessment';
import { exportDataByDateRange } from './utils/dataExport';
import './styles/App.css';

const App = () => {
  const [selectedDevice, setSelectedDevice] = useState(null);
  const { sensorData, dataHistory, isConnected, mlPrediction } = useSensorData(selectedDevice);
  const { riskLevels, overallRisk } = useRiskAssessment(sensorData);
  const [isExportModalOpen, setIsExportModalOpen] = useState(false);

  // Note: Data logging removed - ESP32 sends data directly to backend
  // Frontend only displays data, doesn't generate it

  const handleExportData = () => {
    setIsExportModalOpen(true);
  };

  const handleExportWithDateRange = async (exportParams) => {
    await exportDataByDateRange(exportParams);
  };

  return (
    <div className="app">
      <div className="container">
        <Header 
          isConnected={isConnected}
          overallRisk={overallRisk}
        />
        
        <DeviceSelector
          selectedDevice={selectedDevice}
          onDeviceChange={setSelectedDevice}
        />
        
        <main className="dashboard">
          <div className="parameter-grid">
            <ParameterCard
              title="Temperature"
              icon="thermometer"
              value={sensorData.temperature}
              unit="°C"
              data={dataHistory.temperature}
              riskLevel={riskLevels.temperature}
              type="chart"
            />
            
            <ParameterCard
              title="Dissolved Oxygen"
              icon="droplets"
              value={sensorData.dissolvedOxygen}
              unit="mg/L"
              data={dataHistory.dissolvedOxygen}
              riskLevel={riskLevels.dissolvedOxygen}
              type="chart"
            />
            
            <ParameterCard
              title="pH Level"
              icon="flask"
              value={sensorData.ph}
              unit=""
              data={dataHistory.ph}
              riskLevel={riskLevels.ph}
              type="gauge"
              min={0}
              max={14}
            />
            
            <ParameterCard
              title="Electrical Conductivity"
              icon="zap"
              value={sensorData.electricalConductivity}
              unit="µS/cm"
              data={dataHistory.electricalConductivity}
              riskLevel={riskLevels.electricalConductivity}
              type="gauge"
              min={0}
              max={1200}
            />
            
            <ParameterCard
              title="Turbidity"
              icon="eye"
              value={sensorData.turbidity}
              unit="NTU"
              data={dataHistory.turbidity}
              riskLevel={riskLevels.turbidity}
              type="chart"
            />
            
            <ParameterCard
              title="Probiotic Chamber"
              icon="droplet"
              value={sensorData.probioticLevel}
              unit="%"
              data={dataHistory.probioticLevel}
              riskLevel={riskLevels.probioticLevel}
              type="gauge"
              min={0}
              max={100}
            />
            
            <RiskAssessment
              overallRisk={overallRisk}
              riskLevels={riskLevels}
              sensorData={sensorData}
              mlPrediction={mlPrediction}
            />
          </div>
        </main>
        
        <Footer onExportData={handleExportData} />
        
        <DateRangeDialog
          isOpen={isExportModalOpen}
          onClose={() => setIsExportModalOpen(false)}
          onExport={handleExportWithDateRange}
        />
      </div>
    </div>
  );
};

export default App;
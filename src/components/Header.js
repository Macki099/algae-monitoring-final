import React from 'react';
import Icon from './Icon';
import { Download, Settings, LogOut } from 'lucide-react';

const Header = ({ isConnected, overallRisk, onExportData, onOpenSettings, onLogout, exportFormat }) => {
  const getRiskMessage = (risk) => {
    switch (risk) {
      case 'normal':
        return 'System Monitoring Active - All Parameters Normal';
      case 'moderate':
        return 'Caution: Potential Algae Bloom Formation Detected';
      case 'high':
        return 'Alert: High Risk Bloom Conditions Present';
      case 'unknown':
        return 'ML Prediction Unavailable - Waiting for Model Output';
      default:
        return 'ML Prediction Unavailable';
    }
  };

  const getRiskAlertClass = (risk) => {
    switch (risk) {
      case 'moderate':
        return 'risk-alert warning';
      case 'high':
        return 'risk-alert danger';
      default:
        return 'risk-alert';
    }
  };

  const exportLabel = exportFormat === 'bundle'
    ? 'Bundle ZIP'
    : exportFormat === 'excel'
      ? 'Excel'
      : exportFormat === 'csv'
        ? 'CSV'
        : 'JSON';

  return (
    <header className="header">
      <div className="header-top">
        <div className="header-content">
          <h1 className="title">
            <Icon name="droplets" size={48} className="title-icon" />
            PhycoSense
          </h1>
          <p className="subtitle">Real-time Algae Monitoring Dashboard</p>
          <div className="status-indicator">
            <span className={`status-dot ${isConnected ? 'connected' : 'disconnected'}`}></span>
            <span>Last updated: {new Date().toLocaleTimeString()}</span>
          </div>
        </div>

        <div className="header-actions">
          <div className="header-chip">Default export: {exportLabel}</div>
          <button className="btn-header-action" onClick={onExportData}>
            <Download size={16} />
            Export
          </button>
          <button className="btn-header-action" onClick={onOpenSettings}>
            <Settings size={16} />
            Settings
          </button>
          <button className="btn-header-action danger" onClick={onLogout}>
            <LogOut size={16} />
            Sign Out
          </button>
        </div>
      </div>

      <div className={getRiskAlertClass(overallRisk)}>
        <Icon name="alertTriangle" size={20} />
        <span>{getRiskMessage(overallRisk)}</span>
      </div>
    </header>
  );
};

export default Header;
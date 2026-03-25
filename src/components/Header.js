import React from 'react';
import Icon from './Icon';
import { Download, Settings, LogOut } from 'lucide-react';

const Header = ({
  isConnected,
  overallRisk,
  onExportData,
  onOpenSettings,
  onLogout,
  exportFormat,
  showRiskDetails,
  onToggleRiskDetails,
  latestPredictionConfidence,
  alertCount,
  isMlDriven,
  mlServiceStatus,
  recommendedAction
}) => {
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
            <img src="/phycosense-logo-mark.svg" alt="PhycoSense logo" className="title-logo" />
            <span>PhycoSense</span>
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

      <section className="risk-summary-panel" aria-label="Risk summary details">
        <div className="risk-summary-head">
          <div>
            <p className="risk-summary-label">Risk overview</p>
            <p className={`risk-summary-value ${overallRisk}`}>{String(overallRisk).toUpperCase()}</p>
          </div>
          <button className="risk-summary-toggle" onClick={onToggleRiskDetails}>
            {showRiskDetails ? 'Hide details' : 'Show details'}
          </button>
        </div>

        {showRiskDetails && (
          <div className="risk-summary-body">
            <p><strong>ML source:</strong> {isMlDriven ? 'Machine Learning prediction' : mlServiceStatus}</p>
            <p><strong>Confidence:</strong> {latestPredictionConfidence}</p>
            <p><strong>Alerts in sensors:</strong> {alertCount}</p>
            {recommendedAction && (
              <p><strong>Recommended action:</strong> {recommendedAction}</p>
            )}
          </div>
        )}
      </section>
    </header>
  );
};

export default Header;
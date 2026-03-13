import React from 'react';
import { Download, Settings, LogOut } from 'lucide-react';

const Footer = ({ onExportData, onOpenSettings, onLogout }) => {
  const handleSettingsClick = () => {
    if (onOpenSettings) {
      onOpenSettings();
    }
  };

  return (
    <footer className="footer">
      <div className="footer-content">
        <div className="export-controls">
          <button className="btn-export" onClick={onExportData}>
            <Download size={16} />
            Export Data
          </button>
          <button className="btn-settings" onClick={handleSettingsClick}>
            <Settings size={16} />
            Settings
          </button>
          {onLogout && (
            <button className="btn-settings" onClick={onLogout} title="Sign out">
              <LogOut size={16} />
              Sign Out
            </button>
          )}
        </div>
        <div className="footer-info">
          <p>&copy; 2025 PhycoSense - Algae Monitoring System</p>
        </div>
      </div>
    </footer>
  );
};

export default Footer;
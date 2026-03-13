import React, { useEffect, useMemo, useState } from 'react';
import '../../styles/Modal.css';

const SettingsDialog = ({
  isOpen,
  onClose,
  onSave,
  settings,
  devices,
  selectedDevice
}) => {
  const [draft, setDraft] = useState(settings);
  const [savedKeys, setSavedKeys] = useState([]);
  const [newKey, setNewKey] = useState('');

  useEffect(() => {
    if (!isOpen) return;
    setDraft(settings);
    try {
      const keys = JSON.parse(localStorage.getItem('phycosense_keys') || '[]');
      setSavedKeys(Array.isArray(keys) ? keys : []);
    } catch {
      setSavedKeys([]);
    }
  }, [isOpen, settings]);

  const deviceOptions = useMemo(() => devices || [], [devices]);

  const updateSection = (section, key, value) => {
    setDraft((prev) => ({
      ...prev,
      [section]: {
        ...prev[section],
        [key]: value
      }
    }));
  };

  const updateSensorVisibility = (key, value) => {
    setDraft((prev) => ({
      ...prev,
      display: {
        ...prev.display,
        showSensors: {
          ...prev.display.showSensors,
          [key]: value
        }
      }
    }));
  };

  const updateDeviceAlias = (deviceId, value) => {
    setDraft((prev) => ({
      ...prev,
      device: {
        ...prev.device,
        aliases: {
          ...prev.device.aliases,
          [deviceId]: value
        }
      }
    }));
  };

  const handleAddKey = () => {
    const trimmed = newKey.trim().toUpperCase();
    if (!trimmed) return;

    const nextKeys = savedKeys.includes(trimmed)
      ? savedKeys
      : [...savedKeys, trimmed];

    localStorage.setItem('phycosense_keys', JSON.stringify(nextKeys));
    setSavedKeys(nextKeys);
    setNewKey('');
  };

  const handleRemoveKey = (key) => {
    const nextKeys = savedKeys.filter((saved) => saved !== key);
    localStorage.setItem('phycosense_keys', JSON.stringify(nextKeys));
    setSavedKeys(nextKeys);
  };

  const handleSave = () => {
    onSave(draft);
    onClose();
  };

  if (!isOpen) return null;

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal" onClick={(event) => event.stopPropagation()}>
        <div className="modal-header">
          <h2>Settings</h2>
          <button className="modal-close" onClick={onClose}>×</button>
        </div>

        <div className="modal-body settings-body">
          <section className="settings-section">
            <h3>Device</h3>
            <div className="settings-grid">
              <label className="settings-field">
                <span>Default device</span>
                <select
                  value={draft.device.defaultDeviceId}
                  onChange={(event) => updateSection('device', 'defaultDeviceId', event.target.value)}
                >
                  <option value="">Auto-select first device</option>
                  {deviceOptions.map((device) => (
                    <option key={device.deviceId} value={device.deviceId}>
                      {device.deviceName} ({device.deviceId})
                    </option>
                  ))}
                </select>
              </label>

              {selectedDevice && (
                <label className="settings-field">
                  <span>Display name override</span>
                  <input
                    type="text"
                    value={draft.device.aliases[selectedDevice] || ''}
                    onChange={(event) => updateDeviceAlias(selectedDevice, event.target.value)}
                    placeholder="Optional nickname"
                  />
                </label>
              )}
            </div>

            <div className="settings-keys">
              <div className="settings-keys-header">
                <span>Saved access keys (this browser)</span>
              </div>
              <div className="settings-keys-list">
                {savedKeys.length === 0 ? (
                  <p className="settings-muted">No saved keys yet.</p>
                ) : (
                  savedKeys.map((key) => (
                    <div key={key} className="settings-key-item">
                      <span>{key}</span>
                      <button
                        type="button"
                        className="settings-link"
                        onClick={() => handleRemoveKey(key)}
                      >
                        Remove
                      </button>
                    </div>
                  ))
                )}
              </div>
              <div className="settings-key-add">
                <input
                  type="text"
                  value={newKey}
                  onChange={(event) => setNewKey(event.target.value)}
                  placeholder="PHY-XXXX-XXXX"
                />
                <button type="button" className="btn-secondary" onClick={handleAddKey}>
                  Add key
                </button>
              </div>
              <p className="settings-muted">Changes apply on the next login.</p>
            </div>
          </section>

          <section className="settings-section">
            <h3>Alerts & Risk</h3>
            <div className="settings-grid">
              <label className="settings-field">
                <span>Alert sensitivity</span>
                <select
                  value={draft.alerts.sensitivity}
                  onChange={(event) => updateSection('alerts', 'sensitivity', event.target.value)}
                >
                  <option value="relaxed">Relaxed</option>
                  <option value="balanced">Balanced</option>
                  <option value="strict">Strict</option>
                </select>
              </label>
              <label className="settings-field">
                <span>Cooldown (minutes)</span>
                <input
                  type="number"
                  min="1"
                  value={draft.alerts.cooldownMinutes}
                  onChange={(event) => updateSection('alerts', 'cooldownMinutes', Number(event.target.value))}
                />
              </label>
            </div>

            <div className="settings-grid settings-grid-wide">
              <label className="settings-field">
                <span>Temperature normal max</span>
                <input
                  type="number"
                  step="0.1"
                  value={draft.alerts.temperatureNormalMax}
                  onChange={(event) => updateSection('alerts', 'temperatureNormalMax', Number(event.target.value))}
                />
              </label>
              <label className="settings-field">
                <span>Temperature moderate max</span>
                <input
                  type="number"
                  step="0.1"
                  value={draft.alerts.temperatureModerateMax}
                  onChange={(event) => updateSection('alerts', 'temperatureModerateMax', Number(event.target.value))}
                />
              </label>
              <label className="settings-field">
                <span>DO normal min</span>
                <input
                  type="number"
                  step="0.1"
                  value={draft.alerts.dissolvedOxygenNormalMin}
                  onChange={(event) => updateSection('alerts', 'dissolvedOxygenNormalMin', Number(event.target.value))}
                />
              </label>
              <label className="settings-field">
                <span>DO moderate min</span>
                <input
                  type="number"
                  step="0.1"
                  value={draft.alerts.dissolvedOxygenModerateMin}
                  onChange={(event) => updateSection('alerts', 'dissolvedOxygenModerateMin', Number(event.target.value))}
                />
              </label>
              <label className="settings-field">
                <span>pH normal max</span>
                <input
                  type="number"
                  step="0.1"
                  value={draft.alerts.phNormalMax}
                  onChange={(event) => updateSection('alerts', 'phNormalMax', Number(event.target.value))}
                />
              </label>
              <label className="settings-field">
                <span>pH moderate max</span>
                <input
                  type="number"
                  step="0.1"
                  value={draft.alerts.phModerateMax}
                  onChange={(event) => updateSection('alerts', 'phModerateMax', Number(event.target.value))}
                />
              </label>
              <label className="settings-field">
                <span>EC normal max</span>
                <input
                  type="number"
                  step="1"
                  value={draft.alerts.electricalConductivityNormalMax}
                  onChange={(event) => updateSection('alerts', 'electricalConductivityNormalMax', Number(event.target.value))}
                />
              </label>
              <label className="settings-field">
                <span>EC moderate max</span>
                <input
                  type="number"
                  step="1"
                  value={draft.alerts.electricalConductivityModerateMax}
                  onChange={(event) => updateSection('alerts', 'electricalConductivityModerateMax', Number(event.target.value))}
                />
              </label>
              <label className="settings-field">
                <span>Turbidity normal max</span>
                <input
                  type="number"
                  step="1"
                  value={draft.alerts.turbidityNormalMax}
                  onChange={(event) => updateSection('alerts', 'turbidityNormalMax', Number(event.target.value))}
                />
              </label>
              <label className="settings-field">
                <span>Turbidity moderate max</span>
                <input
                  type="number"
                  step="1"
                  value={draft.alerts.turbidityModerateMax}
                  onChange={(event) => updateSection('alerts', 'turbidityModerateMax', Number(event.target.value))}
                />
              </label>
            </div>
          </section>

          <section className="settings-section">
            <h3>Data & Sampling</h3>
            <div className="settings-grid">
              <label className="settings-field">
                <span>Poll interval (seconds)</span>
                <select
                  value={draft.data.pollIntervalSeconds}
                  onChange={(event) => updateSection('data', 'pollIntervalSeconds', Number(event.target.value))}
                >
                  <option value={5}>5</option>
                  <option value={10}>10</option>
                  <option value={30}>30</option>
                  <option value={60}>60</option>
                </select>
              </label>
              <label className="settings-field">
                <span>History window</span>
                <select
                  value={draft.data.historyWindowHours}
                  onChange={(event) => updateSection('data', 'historyWindowHours', Number(event.target.value))}
                >
                  <option value={1}>Last hour</option>
                  <option value={6}>Last 6 hours</option>
                  <option value={24}>Last 24 hours</option>
                  <option value={72}>Last 3 days</option>
                </select>
              </label>
              <label className="settings-field">
                <span>Auto-export</span>
                <select
                  value={draft.data.autoExport}
                  onChange={(event) => updateSection('data', 'autoExport', event.target.value)}
                >
                  <option value="off">Off</option>
                  <option value="daily">Daily</option>
                  <option value="weekly">Weekly</option>
                </select>
              </label>
              <label className="settings-field">
                <span>Export format</span>
                <select
                  value={draft.data.exportFormat}
                  onChange={(event) => updateSection('data', 'exportFormat', event.target.value)}
                >
                  <option value="csv">CSV</option>
                  <option value="excel">Excel</option>
                </select>
              </label>
            </div>
          </section>

          <section className="settings-section">
            <h3>Display</h3>
            <div className="settings-grid">
              <label className="settings-field">
                <span>Temperature unit</span>
                <select
                  value={draft.display.temperatureUnit}
                  onChange={(event) => updateSection('display', 'temperatureUnit', event.target.value)}
                >
                  <option value="C">Celsius</option>
                  <option value="F">Fahrenheit</option>
                </select>
              </label>
              <label className="settings-field">
                <span>Chart density</span>
                <select
                  value={draft.display.chartDensity}
                  onChange={(event) => updateSection('display', 'chartDensity', event.target.value)}
                >
                  <option value="compact">Compact</option>
                  <option value="standard">Standard</option>
                  <option value="detailed">Detailed</option>
                </select>
              </label>
              <label className="settings-field">
                <span>Risk colors</span>
                <select
                  value={draft.display.riskColors}
                  onChange={(event) => updateSection('display', 'riskColors', event.target.value)}
                >
                  <option value="default">Default</option>
                  <option value="high-contrast">High contrast</option>
                </select>
              </label>
            </div>

            <div className="settings-grid settings-grid-wide">
              <label className="settings-toggle">
                <input
                  type="checkbox"
                  checked={draft.display.showSensors.temperature}
                  onChange={(event) => updateSensorVisibility('temperature', event.target.checked)}
                />
                <span>Show temperature</span>
              </label>
              <label className="settings-toggle">
                <input
                  type="checkbox"
                  checked={draft.display.showSensors.dissolvedOxygen}
                  onChange={(event) => updateSensorVisibility('dissolvedOxygen', event.target.checked)}
                />
                <span>Show dissolved oxygen</span>
              </label>
              <label className="settings-toggle">
                <input
                  type="checkbox"
                  checked={draft.display.showSensors.ph}
                  onChange={(event) => updateSensorVisibility('ph', event.target.checked)}
                />
                <span>Show pH</span>
              </label>
              <label className="settings-toggle">
                <input
                  type="checkbox"
                  checked={draft.display.showSensors.electricalConductivity}
                  onChange={(event) => updateSensorVisibility('electricalConductivity', event.target.checked)}
                />
                <span>Show electrical conductivity</span>
              </label>
              <label className="settings-toggle">
                <input
                  type="checkbox"
                  checked={draft.display.showSensors.turbidity}
                  onChange={(event) => updateSensorVisibility('turbidity', event.target.checked)}
                />
                <span>Show turbidity</span>
              </label>
              <label className="settings-toggle">
                <input
                  type="checkbox"
                  checked={draft.display.showSensors.probioticLevel}
                  onChange={(event) => updateSensorVisibility('probioticLevel', event.target.checked)}
                />
                <span>Show probiotic chamber</span>
              </label>
            </div>
          </section>
        </div>

        <div className="modal-actions">
          <button className="btn-secondary" onClick={onClose}>Cancel</button>
          <button className="btn-primary" onClick={handleSave}>Save settings</button>
        </div>
      </div>
    </div>
  );
};

export default SettingsDialog;

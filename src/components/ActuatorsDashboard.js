import React, { useState } from 'react';
import Icon from './Icon';

const RAW_API_URL = process.env.REACT_APP_API_URL || 'http://localhost:5000/api/sensor-data';
const normalizeSensorDataUrl = (url) => {
  let base = (url || '').trim();
  if (base.endsWith('/')) base = base.slice(0, -1);
  if (base.endsWith('/sensor-data')) return base;
  if (base.endsWith('/api')) return `${base}/sensor-data`;
  return `${base}/api/sensor-data`;
};
const API_URL = normalizeSensorDataUrl(RAW_API_URL).trim();

const ActuatorsDashboard = ({
  lastAction,
  overallRisk,
  alertCount,
  mlServiceStatus,
  isConnected,
  latestPredictionConfidence
  ,
  selectedDevice
}) => {
  const actionText = (lastAction?.text || '').toLowerCase();

  const aeratorState = actionText.includes('aerator activated')
    ? 'Active'
    : actionText.includes('aerator deactivated')
      ? 'Inactive'
      : overallRisk === 'high'
        ? 'Standby High'
        : 'Standby';

  const probioticState = actionText.includes('probiotic')
    ? 'Dispensing'
    : overallRisk === 'moderate'
      ? 'Ready'
      : 'Idle';

  const [actuatorPower, setActuatorPower] = useState({
    aerationValve: aeratorState !== 'Inactive',
    probioticValve: probioticState !== 'Idle'
  });
  const [aeratorIntensity, setAeratorIntensity] = useState(aeratorState === 'Active' ? 75 : 45);
  const [isSendingCommand, setIsSendingCommand] = useState(false);
  const [commandStatus, setCommandStatus] = useState('');

  const actuatorCards = [
    {
      key: 'aerationValve',
      name: 'Aeration Valve',
      icon: 'zap',
      state: aeratorState,
      detail: `Source: ${lastAction?.source || 'Rule-Based'}`
    },
    {
      key: 'probioticValve',
      name: 'Probiotic/Algaecide Valve',
      icon: 'droplets',
      state: probioticState,
      detail: `Risk mode: ${String(overallRisk).toUpperCase()}`
    }
  ];

  const getManualStateLabel = (key, fallbackState) => {
    if (key === 'aerationValve') {
      return actuatorPower.aerationValve ? `On • ${aeratorIntensity}%` : 'Off';
    }

    if (key in actuatorPower) {
      return actuatorPower[key] ? 'On' : 'Off';
    }

    return fallbackState;
  };

  const sendActuatorCommand = async (actuator, state, pwm) => {
    if (!selectedDevice) {
      setCommandStatus('No device selected');
      return;
    }

    setIsSendingCommand(true);
    try {
      const response = await fetch(`${API_URL}/actuator-command`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          deviceId: selectedDevice,
          actuator,
          state,
          pwm: actuator === 'aerator' ? Math.round((Number(pwm) / 100) * 255) : 0,
          durationMs: 3000
        })
      });

      if (!response.ok) {
        throw new Error(`Command failed (${response.status})`);
      }

      setCommandStatus(`Sent: ${actuator} ${state ? 'ON' : 'OFF'}`);
    } catch (error) {
      setCommandStatus(`Error: ${error.message}`);
    } finally {
      setIsSendingCommand(false);
    }
  };

  const toggleActuator = (key, nextValue) => {
    setActuatorPower((previous) => ({
      ...previous,
      [key]: nextValue
    }));

    if (key === 'aerationValve') {
      sendActuatorCommand('aerator', nextValue, aeratorIntensity);
    }

    if (key === 'probioticValve') {
      sendActuatorCommand('probiotic', nextValue, 0);
    }
  };

  return (
    <main className="dashboard actuators-page" aria-label="Actuators dashboard">
      <section className="actuators-header">
        <h2>Actuators Dashboard</h2>
        <p>Live status of control outputs for this device.</p>
        <p className="actuator-connection">Connection: {isConnected ? 'Online' : 'Offline'}</p>
        <p className="actuator-connection">Manual command: {isSendingCommand ? 'Sending...' : (commandStatus || 'Idle')}</p>
      </section>

      <section className="actuators-grid">
        {actuatorCards.map((actuator) => (
          <article className="actuator-card" key={actuator.key}>
            <div className="actuator-card-head">
              <h3>
                <Icon name={actuator.icon} size={18} />
                {actuator.name}
              </h3>
              <span className="actuator-state">{getManualStateLabel(actuator.key, actuator.state)}</span>
            </div>
            <p className="actuator-detail">{actuator.detail}</p>

            <div className="actuator-controls">
              <button
                className={`actuator-toggle on ${actuatorPower[actuator.key] ? 'active' : ''}`}
                onClick={() => toggleActuator(actuator.key, true)}
                disabled={!isConnected}
              >
                ON
              </button>
              <button
                className={`actuator-toggle off ${!actuatorPower[actuator.key] ? 'active' : ''}`}
                onClick={() => toggleActuator(actuator.key, false)}
                disabled={!isConnected}
              >
                OFF
              </button>
            </div>

            {actuator.key === 'aerationValve' && (
              <div className="actuator-slider-wrap">
                <div className="actuator-slider-head">
                  <span>Aerator Intensity</span>
                  <strong>{aeratorIntensity}%</strong>
                </div>
                <input
                  className="actuator-slider"
                  type="range"
                  min="0"
                  max="100"
                  step="5"
                  value={aeratorIntensity}
                  onChange={(event) => {
                    const nextValue = Number(event.target.value);
                    setAeratorIntensity(nextValue);
                    if (actuatorPower.aerationValve) {
                      sendActuatorCommand('aerator', true, nextValue);
                    }
                  }}
                  disabled={!isConnected || !actuatorPower.aerationValve}
                />
              </div>
            )}
          </article>
        ))}
      </section>
    </main>
  );
};

export default ActuatorsDashboard;

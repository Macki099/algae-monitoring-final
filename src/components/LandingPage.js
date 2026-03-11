import React, { useState } from 'react';
import { Droplets, KeyRound, ArrowRight, Activity, Shield, Wifi, ChevronRight, AlertCircle, Plus, X } from 'lucide-react';
import '../styles/LandingPage.css';

const LandingPage = ({ onAuthenticated }) => {
  const [accessKey, setAccessKey] = useState('');
  const [error, setError] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [showAddKey, setShowAddKey] = useState(false);

  // Derive base API URL (strip /sensor-data if present in env)
  const rawUrl = process.env.REACT_APP_API_URL || 'http://localhost:5000/api/sensor-data';
  const API_URL = rawUrl.replace(/\/sensor-data\/?$/, '');

  // Load saved keys from localStorage
  const getSavedKeys = () => {
    try {
      return JSON.parse(localStorage.getItem('phycosense_keys') || '[]');
    } catch {
      return [];
    }
  };

  const formatKeyInput = (value) => {
    // Auto-format: PHY-XXXX-XXXX
    const cleaned = value.toUpperCase().replace(/[^A-Z0-9-]/g, '');
    
    // If user is typing naturally, auto-insert dashes
    const noDash = cleaned.replace(/-/g, '');
    if (noDash.length <= 3) return noDash;
    if (noDash.length <= 7) return `${noDash.slice(0, 3)}-${noDash.slice(3)}`;
    return `${noDash.slice(0, 3)}-${noDash.slice(3, 7)}-${noDash.slice(7, 11)}`;
  };

  const handleKeyChange = (e) => {
    setError('');
    setAccessKey(formatKeyInput(e.target.value));
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    
    const trimmedKey = accessKey.trim();
    if (!trimmedKey) {
      setError('Please enter your device access key');
      return;
    }

    setIsLoading(true);
    setError('');

    try {
      const response = await fetch(`${API_URL}/auth/verify-key`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ accessKey: trimmedKey })
      });

      let data;
      try {
        data = await response.json();
      } catch {
        setError('Server returned an unexpected response. Please try again.');
        return;
      }

      if (response.ok && data.authenticated) {
        // Save key to localStorage
        const savedKeys = getSavedKeys();
        if (!savedKeys.includes(trimmedKey)) {
          savedKeys.push(trimmedKey);
          localStorage.setItem('phycosense_keys', JSON.stringify(savedKeys));
        }
        onAuthenticated([{ deviceId: data.deviceId, deviceName: data.deviceName }]);
      } else {
        setError(data.message || 'Invalid access key. Check your key and try again.');
      }
    } catch (err) {
      setError('Cannot reach the server. Check your internet connection or try again later.');
    } finally {
      setIsLoading(false);
    }
  };

  const savedKeys = getSavedKeys();
  const hasSavedKeys = savedKeys.length > 0;

  const handleResumeSession = async () => {
    setIsLoading(true);
    setError('');

    try {
      const response = await fetch(`${API_URL}/auth/verify-keys`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ accessKeys: savedKeys })
      });

      let data;
      try {
        data = await response.json();
      } catch {
        setError('Server returned an unexpected response. Please enter your key manually.');
        return;
      }

      if (response.ok && data.authenticated) {
        onAuthenticated(data.devices);
      } else {
        // Keys are no longer valid, clear them
        localStorage.removeItem('phycosense_keys');
        setError('Saved session expired. Please enter your access key again.');
      }
    } catch (err) {
      setError('Cannot reach the server. Check your internet connection or try again later.');
    } finally {
      setIsLoading(false);
    }
  };

  const handleClearSavedKeys = () => {
    localStorage.removeItem('phycosense_keys');
    setShowAddKey(false);
    setError('');
  };

  return (
    <div className="landing-page">
      {/* Animated background elements */}
      <div className="bg-effects">
        <div className="bg-orb bg-orb-1"></div>
        <div className="bg-orb bg-orb-2"></div>
        <div className="bg-orb bg-orb-3"></div>
        <div className="bg-grid"></div>
      </div>

      <div className="landing-content">
        {/* Logo & Brand */}
        <div className="landing-brand fade-in">
          <div className="brand-icon">
            <Droplets size={48} />
            <div className="brand-icon-ring"></div>
          </div>
          <h1 className="brand-title">PhycoSense</h1>
          <p className="brand-tagline">Smart Algae Monitoring System</p>
        </div>

        {/* Auth Card */}
        <div className="auth-card fade-in">
          {hasSavedKeys && !showAddKey ? (
            // Returning user — quick resume
            <div className="auth-resume">
              <h2>Welcome back</h2>
              <p className="auth-subtitle">
                {savedKeys.length} device{savedKeys.length > 1 ? 's' : ''} linked to this browser
              </p>
              
              <button 
                className="btn-resume" 
                onClick={handleResumeSession}
                disabled={isLoading}
              >
                {isLoading ? (
                  <span className="btn-loading">Connecting...</span>
                ) : (
                  <>
                    <span>Open Dashboard</span>
                    <ArrowRight size={20} />
                  </>
                )}
              </button>

              <div className="auth-divider">
                <span>or</span>
              </div>

              <button 
                className="btn-add-device" 
                onClick={() => setShowAddKey(true)}
              >
                <Plus size={16} />
                Add Another Device
              </button>

              <button 
                className="btn-clear-session" 
                onClick={handleClearSavedKeys}
              >
                <X size={14} />
                Clear saved session
              </button>
            </div>
          ) : (
            // New user or adding device — enter key
            <div className="auth-form-container">
              <div className="auth-header">
                <KeyRound size={24} className="auth-icon" />
                <h2>{showAddKey ? 'Add Device' : 'Access Your Dashboard'}</h2>
                <p className="auth-subtitle">
                  Enter the access key included with your PhycoSense device
                </p>
              </div>

              <form onSubmit={handleSubmit} className="auth-form">
                <div className={`input-group ${error ? 'input-error' : ''}`}>
                  <input
                    type="text"
                    value={accessKey}
                    onChange={handleKeyChange}
                    placeholder="PHY-XXXX-XXXX"
                    maxLength={14}
                    autoFocus
                    spellCheck={false}
                    autoComplete="off"
                  />
                </div>

                {error && (
                  <div className="error-message">
                    <AlertCircle size={16} />
                    <span>{error}</span>
                  </div>
                )}

                <button 
                  type="submit" 
                  className="btn-authenticate"
                  disabled={isLoading || accessKey.length < 12}
                >
                  {isLoading ? (
                    <span className="btn-loading">Verifying...</span>
                  ) : (
                    <>
                      <span>Connect Device</span>
                      <ChevronRight size={20} />
                    </>
                  )}
                </button>
              </form>

              {showAddKey && (
                <button 
                  className="btn-back" 
                  onClick={() => { setShowAddKey(false); setError(''); setAccessKey(''); }}
                >
                  Back to dashboard
                </button>
              )}

              <p className="auth-help">
                Your access key is printed on your device or included in your welcome kit
              </p>
            </div>
          )}
        </div>

        {/* Feature highlights */}
        <div className="features fade-in">
          <div className="feature">
            <div className="feature-icon">
              <Activity size={22} />
            </div>
            <div className="feature-text">
              <h3>Real-time Monitoring</h3>
              <p>Live sensor data from your pond</p>
            </div>
          </div>
          <div className="feature">
            <div className="feature-icon">
              <Shield size={22} />
            </div>
            <div className="feature-text">
              <h3>ML Risk Prediction</h3>
              <p>AI-powered algae bloom alerts</p>
            </div>
          </div>
          <div className="feature">
            <div className="feature-icon">
              <Wifi size={22} />
            </div>
            <div className="feature-text">
              <h3>IoT Connected</h3>
              <p>Wireless ESP32 sensor integration</p>
            </div>
          </div>
        </div>

        <p className="landing-footer">&copy; 2025 PhycoSense</p>
      </div>
    </div>
  );
};

export default LandingPage;

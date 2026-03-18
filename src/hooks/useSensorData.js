import { useState, useEffect, useRef } from 'react';

const RAW_API_URL = process.env.REACT_APP_API_URL || 'http://localhost:5000/api/sensor-data';
const normalizeSensorDataUrl = (url) => {
  let base = (url || '').trim();
  if (base.endsWith('/')) base = base.slice(0, -1);
  if (base.endsWith('/sensor-data')) return base;
  if (base.endsWith('/api')) return `${base}/sensor-data`;
  return `${base}/api/sensor-data`;
};
const API_URL = normalizeSensorDataUrl(RAW_API_URL).trim();

export const useSensorData = (deviceId, options = {}) => {
  const pollIntervalSeconds = Math.max(1, Number(options.pollIntervalSeconds) || 5);
  const historyLimit = Math.max(10, Number(options.historyLimit) || 20);
  const [sensorData, setSensorData] = useState({
    temperature: 22.5,
    dissolvedOxygen: 3.8,
    ph: 8.7,
    electricalConductivity: 850,
    turbidity: 35.2,
    probioticLevel: 75
  });

  const [mlPrediction, setMlPrediction] = useState(null);
  const [mlServiceStatus, setMlServiceStatus] = useState('unknown');

  const [dataHistory, setDataHistory] = useState({
    temperature: [],
    dissolvedOxygen: [],
    ph: [],
    electricalConductivity: [],
    turbidity: [],
    probioticLevel: []
  });

  const [isConnected, setIsConnected] = useState(true);
  const intervalRef = useRef();
  const mlHealthIntervalRef = useRef();

  const fetchMlHealth = async () => {
    try {
      const response = await fetch(`${API_URL}/ml-health`);
      if (!response.ok) {
        setMlServiceStatus('unavailable');
        return;
      }

      const data = await response.json();
      setMlServiceStatus(data?.status === 'available' ? 'available' : 'unavailable');
    } catch (error) {
      console.error('Error fetching ML health:', error);
      setMlServiceStatus('unavailable');
    }
  };

  // Function to log data to MongoDB
  const logDataToDatabase = async (data, riskLevel) => {
    try {
      const response = await fetch(API_URL, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          temperature: data.temperature,
          dissolvedOxygen: data.dissolvedOxygen,
          ph: data.ph,
          ec: data.electricalConductivity,
          turbidity: data.turbidity,
          probioticLevel: data.probioticLevel,
          riskLevel: riskLevel
        }),
      });

      if (!response.ok) {
        console.error('Failed to log data to database');
      }
    } catch (error) {
      console.error('Error logging data:', error);
      setIsConnected(false);
    }
  };

  // Function to fetch historical data from MongoDB
  const fetchHistoricalData = async () => {
    // Don't fetch if no device is selected
    if (!deviceId) return;
    
    try {
      const url = `${API_URL}?limit=${historyLimit}&deviceId=${deviceId}`;
      const response = await fetch(url);
      if (response.ok) {
        const data = await response.json();
        
        if (data.length > 0) {
          const history = {
            temperature: [],
            dissolvedOxygen: [],
            ph: [],
            electricalConductivity: [],
            turbidity: [],
            probioticLevel: []
          };

          data.reverse().forEach(entry => {
            history.temperature.push(entry.temperature || 0);
            history.dissolvedOxygen.push(entry.dissolvedOxygen || 0);
            history.ph.push(entry.ph || 7);
            history.electricalConductivity.push(entry.ec || entry.electricalConductivity || 0);
            history.turbidity.push(entry.turbidity || 0);
            history.probioticLevel.push(entry.probioticLevel || 0);
          });

          setDataHistory(history);
          
          // Set the latest data as current
          const latest = data[data.length - 1];
          setSensorData({
            temperature: latest.temperature || 0,
            dissolvedOxygen: latest.dissolvedOxygen || 0,
            ph: latest.ph || 7,
            electricalConductivity: latest.ec || latest.electricalConductivity || 0,
            turbidity: latest.turbidity || 0,
            probioticLevel: latest.probioticLevel || 0
          });
          
          // Extract ML prediction if available
          if (latest.risk) {
            setMlPrediction({
              risk: latest.risk,
              confidence: latest.riskConfidence || null,
              action: latest.action || null
            });
          }
          
          setIsConnected(true);
        }
      }
    } catch (error) {
      console.error('Error fetching historical data:', error);
      // If database is not available, use simulated data
      initializeSimulatedHistory();
    }
  };

  // Initialize simulated data history
  const initializeSimulatedHistory = () => {
    const initialHistory = {};
    Object.keys(sensorData).forEach(key => {
      initialHistory[key] = Array.from({ length: 20 }, () => 
        generateRealisticValue(key, sensorData[key])
      );
    });
    setDataHistory(initialHistory);
  };

  // Generate realistic sensor data variations with occasional risk conditions
  const generateRealisticValue = (parameter, currentValue) => {
    const variations = {
      temperature: 0.5,           // °C
      dissolvedOxygen: 0.3,       // mg/L
      ph: 0.1,                    // pH units
      electricalConductivity: 25, // µS/cm
      turbidity: 2.0,             // NTU
      probioticLevel: 1.5         // % level
    };

    const bounds = {
      temperature: { min: 15, max: 30 },        // °C
      dissolvedOxygen: { min: 1, max: 12 },     // mg/L
      ph: { min: 6, max: 10 },                  // pH units
      electricalConductivity: { min: 200, max: 1200 },  // µS/cm
      turbidity: { min: 0, max: 80 },           // NTU
      probioticLevel: { min: 0, max: 100 }      // % level
    };

    // Occasionally introduce more dramatic changes to trigger risk conditions
    let variation = (Math.random() - 0.5) * 2 * variations[parameter];
    
    // 15% chance of a larger variation that might push into moderate/high risk zones
    if (Math.random() < 0.15) {
      variation *= 3;
    }
    
    const newValue = currentValue + variation;
    
    return Math.max(
      bounds[parameter].min,
      Math.min(bounds[parameter].max, newValue)
    );
  };

  // Initialize data on mount and when device changes
  useEffect(() => {
    if (deviceId) {
      fetchHistoricalData();
    }
  }, [deviceId, historyLimit]);

  useEffect(() => {
    fetchMlHealth();
    mlHealthIntervalRef.current = setInterval(fetchMlHealth, 30000);

    return () => {
      if (mlHealthIntervalRef.current) {
        clearInterval(mlHealthIntervalRef.current);
      }
    };
  }, []);

  // Fetch real-time data from backend
  useEffect(() => {
    if (!deviceId) return;
    
    intervalRef.current = setInterval(() => {
      fetchHistoricalData(); // Fetch latest data from backend
    }, pollIntervalSeconds * 1000); // Update based on settings

    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, [deviceId, pollIntervalSeconds, historyLimit]);

  return {
    sensorData,
    dataHistory,
    isConnected,
    mlPrediction,
    mlServiceStatus,
    logDataToDatabase,
    fetchHistoricalData
  };
};
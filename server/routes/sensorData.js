const express = require('express');
const router = express.Router();
const SensorData = require('../models/SensorData');
const axios = require('axios');

// ML Service configuration
const ML_SERVICE_URL = process.env.ML_SERVICE_URL || 'http://localhost:5001';
const ML_REQUIRED = (process.env.ML_REQUIRED || 'false').toLowerCase() === 'true';

const getMlErrorMessage = (error) => {
  if (!error) return 'Unknown ML error';
  if (error.response?.data?.error) return error.response.data.error;
  if (error.response?.status) return `ML service returned status ${error.response.status}`;
  return error.message || 'Unknown ML error';
};

// @route   POST /api/sensor-data
// @desc    Add new sensor reading with ML risk prediction
// @access  Public
router.post('/', async (req, res) => {
  try {
    const sensorData = req.body;
    const responseMeta = {
      mlStatus: 'unavailable',
      mlRequired: ML_REQUIRED
    };
    
    // Call ML service for risk prediction
    try {
      const mlResponse = await axios.post(`${ML_SERVICE_URL}/predict`, sensorData, {
        timeout: 5000
      });
      
      if (mlResponse.data && mlResponse.data.risk) {
        sensorData.risk = mlResponse.data.risk;
        sensorData.riskConfidence = mlResponse.data.confidence;
        sensorData.riskProbabilities = mlResponse.data.probabilities || null;
        sensorData.action = mlResponse.data.action;
        responseMeta.mlStatus = 'available';
        
        console.log(`ML Prediction: ${mlResponse.data.risk} (${(mlResponse.data.confidence * 100).toFixed(1)}%)`);
      } else {
        responseMeta.mlStatus = 'no-prediction';
      }
    } catch (mlError) {
      const mlErrorMessage = getMlErrorMessage(mlError);
      responseMeta.mlError = mlErrorMessage;

      if (ML_REQUIRED) {
        console.error('ML required and unavailable. Rejecting sensor data:', mlErrorMessage);
        return res.status(503).json({
          message: 'ML prediction unavailable. Data rejected because ML_REQUIRED=true.',
          mlStatus: 'unavailable',
          mlRequired: true,
          error: mlErrorMessage
        });
      }

      console.warn('ML service unavailable. Continuing without prediction:', mlErrorMessage);
    }
    
    const newSensorData = new SensorData(sensorData);
    const savedData = await newSensorData.save();
    res.status(201).json({
      ...savedData.toObject(),
      ...responseMeta
    });
  } catch (error) {
    console.error('ERROR saving sensor data:', error.message);
    console.error('Data received:', JSON.stringify(req.body, null, 2));
    res.status(400).json({ message: error.message });
  }
});

// @route   GET /api/sensor-data
// @desc    Get sensor readings with optional filters
// @access  Public
router.get('/', async (req, res) => {
  try {
    const { limit = 100, startDate, endDate, deviceId } = req.query;
    
    let query = {};
    
    // Filter by device if specified
    if (deviceId) {
      query.deviceId = deviceId;
    }
    
    if (startDate || endDate) {
      query.timestamp = {};
      if (startDate) query.timestamp.$gte = new Date(startDate);
      if (endDate) query.timestamp.$lte = new Date(endDate);
    }
    
    const data = await SensorData.find(query)
      .sort({ timestamp: -1 })
      .limit(parseInt(limit));
    
    res.json(data);
  } catch (error) {
    res.status(500).json({ message: error.message });
  }
});

// @route   GET /api/sensor-data/latest
// @desc    Get the most recent sensor reading
// @access  Public
router.get('/latest', async (req, res) => {
  try {
    const { deviceId } = req.query;
    let query = {};
    
    // Filter by device if specified
    if (deviceId) {
      query.deviceId = deviceId;
    }
    
    const latestData = await SensorData.findOne(query).sort({ timestamp: -1 });
    
    if (!latestData) {
      return res.status(404).json({ message: 'No sensor data found' });
    }
    
    res.json(latestData);
  } catch (error) {
    res.status(500).json({ message: error.message });
  }
});

// @route   GET /api/sensor-data/ml-health
// @desc    Check ML service availability from backend
// @access  Public
router.get('/ml-health', async (req, res) => {
  try {
    const mlResponse = await axios.get(`${ML_SERVICE_URL}/health`, {
      timeout: 3000
    });

    return res.json({
      status: 'available',
      mlServiceUrl: ML_SERVICE_URL,
      mlRequired: ML_REQUIRED,
      ml: mlResponse.data
    });
  } catch (mlError) {
    const mlErrorMessage = getMlErrorMessage(mlError);

    return res.status(503).json({
      status: 'unavailable',
      mlServiceUrl: ML_SERVICE_URL,
      mlRequired: ML_REQUIRED,
      error: mlErrorMessage
    });
  }
});

// @route   POST /api/sensor-data/devices/register
// @desc    Register a new device (ESP32 compatibility)
// @access  Public
router.post('/devices/register', async (req, res) => {
  try {
    const { deviceId, deviceName, firmwareVersion } = req.body;
    
    // Check if device already exists
    const existingDevice = await SensorData.findOne({ deviceId }).sort({ timestamp: -1 });
    
    if (existingDevice) {
      return res.status(200).json({
        message: 'Device already registered',
        deviceId,
        deviceName: existingDevice.deviceName || deviceName
      });
    }
    
    // Return success - device will be registered when first data is sent
    res.status(201).json({
      message: 'Device registration acknowledged',
      deviceId,
      deviceName,
      note: 'Device will be fully registered upon first data transmission'
    });
  } catch (error) {
    console.error('Device registration error:', error.message);
    res.status(400).json({ message: error.message });
  }
});

// @route   GET /api/sensor-data/devices
// @desc    Get list of all devices
// @access  Public
router.get('/devices', async (req, res) => {
  try {
    const devices = await SensorData.distinct('deviceId');
    
    // Get device names and last update for each device
    const deviceDetails = await Promise.all(
      devices.map(async (deviceId) => {
        const latestData = await SensorData.findOne({ deviceId })
          .sort({ timestamp: -1 })
          .select('deviceId deviceName timestamp batteryVoltage batteryPercentage isUSBPowered');
        
        return {
          deviceId: latestData.deviceId,
          deviceName: latestData.deviceName || deviceId,
          lastUpdate: latestData.timestamp,
          status: (new Date() - new Date(latestData.timestamp)) < 60000 ? 'online' : 'offline',
          batteryVoltage: latestData.batteryVoltage,
          batteryPercentage: latestData.batteryPercentage,
          isUSBPowered: latestData.isUSBPowered
        };
      })
    );
    
    res.json(deviceDetails);
  } catch (error) {
    res.status(500).json({ message: error.message });
  }
});

// @route   DELETE /api/sensor-data/:id
// @desc    Delete a sensor reading
// @access  Public
router.delete('/:id', async (req, res) => {
  try {
    const deletedData = await SensorData.findByIdAndDelete(req.params.id);
    
    if (!deletedData) {
      return res.status(404).json({ message: 'Sensor data not found' });
    }
    
    res.json({ message: 'Sensor data deleted successfully' });
  } catch (error) {
    res.status(500).json({ message: error.message });
  }
});

module.exports = router;

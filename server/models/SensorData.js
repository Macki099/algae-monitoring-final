const mongoose = require('mongoose');

const sensorDataSchema = new mongoose.Schema({
  deviceId: {
    type: String,
    required: true,
    default: 'DEVICE-01'
  },
  deviceName: {
    type: String,
    required: false,
    default: 'Main Device'
  },
  temperature: {
    type: Number,
    required: false,
    default: 0
  },
  ph: {
    type: Number,
    required: false,
    default: 7
  },
  turbidity: {
    type: Number,
    required: false,
    default: 0
  },
  dissolvedOxygen: {
    type: Number,
    required: false,
    default: 0
  },
  ec: {
    type: Number,
    required: false,
    default: 0
  },
  probioticLevel: {
    type: Number,
    required: false,
    default: null,
    min: 0,
    max: 100
  },
  risk: {
    type: String,
    enum: ['Normal', 'Moderate', 'High'],
    required: false
  },
  riskConfidence: {
    type: Number,
    required: false,
    min: 0,
    max: 1
  },
  riskProbabilities: {
    type: mongoose.Schema.Types.Mixed,
    required: false,
    default: null
  },
  action: {
    type: mongoose.Schema.Types.Mixed,
    required: false
  },
  batteryVoltage: {
    type: Number,
    required: false,
    default: null
  },
  batteryPercentage: {
    type: Number,
    required: false,
    default: null,
    min: 0,
    max: 100
  },
  isUSBPowered: {
    type: Boolean,
    required: false,
    default: false
  },
  timestamp: {
    type: Date,
    default: Date.now
  }
}, {
  timestamps: true
});

module.exports = mongoose.model('SensorData', sensorDataSchema);

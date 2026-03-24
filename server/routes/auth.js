const express = require('express');
const router = express.Router();
const crypto = require('crypto');
const Device = require('../models/Device');

const KEY_PATTERN = /^PHY-[A-HJ-NP-Z2-9]{4}-[A-HJ-NP-Z2-9]{4}$/;

// Generate a unique PHY-XXXX-XXXX key
const generateKey = () => {
  const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  const segment = (len) => {
    const bytes = crypto.randomBytes(len);
    return Array.from(bytes).map(b => chars[b % chars.length]).join('');
  };
  return `PHY-${segment(4)}-${segment(4)}`;
};

const hashKey = (key) => crypto.createHash('sha256').update(key).digest('hex');

// @route   POST /api/auth/verify-key
// @desc    Verify a device access key
// @access  Public
router.post('/verify-key', async (req, res) => {
  const { accessKey } = req.body;

  if (!accessKey || typeof accessKey !== 'string') {
    return res.status(400).json({ message: 'Access key is required' });
  }

  const key = accessKey.trim().toUpperCase();

  try {
    const device = await Device.findOne({ accessKeyHash: hashKey(key) });

    if (!device) {
      return res.status(401).json({ message: 'Invalid access key' });
    }

    res.json({
      authenticated: true,
      deviceId: device.deviceId,
      deviceName: device.deviceName
    });
  } catch (err) {
    res.status(500).json({ message: 'Server error' });
  }
});

// @route   POST /api/auth/register-device
// @desc    Called by ESP32 during WiFi provisioning to register its access key.
//          The ESP32 pre-generates the key locally and shows it to the user on
//          its setup portal (192.168.4.1) *before* WiFi credentials are entered,
//          so the user writes it down in a single setup step.
//          If no key is provided, the server generates one (backward compat).
// @access  Public
router.post('/register-device', async (req, res) => {
  const { deviceId, deviceName, accessKey: clientKey } = req.body;

  if (!deviceId) {
    return res.status(400).json({ message: 'deviceId is required' });
  }

  if (clientKey && !KEY_PATTERN.test(clientKey)) {
    return res.status(400).json({ message: 'Invalid access key format' });
  }

  try {
    // If this exact deviceId is already registered, return its existing key
    const existing = await Device.findOne({ deviceId });
    if (existing) {
      if (deviceName && deviceName.trim() && existing.deviceName !== deviceName.trim()) {
        existing.deviceName = deviceName.trim();
        await existing.save();
      }

      return res.status(409).json({
        message: 'Device already registered',
        deviceId,
        deviceName: existing.deviceName,
        accessKey: existing.accessKey
      });
    }

    let key;
    if (clientKey) {
      const existingKeyOwner = await Device.findOne({ accessKeyHash: hashKey(clientKey) });

      if (!existingKeyOwner) {
        // Key is completely free — use it
        key = clientKey;
      } else {
        // Key already exists under a different deviceId (e.g. seeded with placeholder).
        // The device pre-generated this key so it rightfully owns it — update the record.
        existingKeyOwner.deviceId = deviceId;
        if (deviceName) existingKeyOwner.deviceName = deviceName;
        await existingKeyOwner.save();

        console.log(`Device claimed key: ${deviceId} → ${clientKey}`);
        return res.status(200).json({
          message: 'Device registered successfully',
          deviceId,
          deviceName: existingKeyOwner.deviceName,
          accessKey: clientKey
        });
      }
    } else {
      let attempts = 0;
      do {
        key = generateKey();
        if (++attempts > 20) throw new Error('Key generation failed');
      } while (await Device.findOne({ accessKeyHash: hashKey(key) }));
    }

    const device = await Device.create({
      deviceId,
      deviceName: deviceName || 'PhycoSense Device',
      accessKey: key,
      accessKeyHash: hashKey(key)
    });

    console.log(`Device registered: ${deviceId} → ${key}`);
    res.status(201).json({
      message: 'Device registered successfully',
      deviceId,
      deviceName: device.deviceName,
      accessKey: key
    });
  } catch (err) {
    console.error('Registration error:', err.message);
    res.status(500).json({ message: 'Failed to register device' });
  }
});

// @route   POST /api/auth/verify-keys
// @desc    Verify multiple saved access keys (for returning users)
// @access  Public
router.post('/verify-keys', async (req, res) => {
  const { accessKeys } = req.body;

  if (!Array.isArray(accessKeys) || accessKeys.length === 0) {
    return res.status(400).json({ message: 'Access keys array is required' });
  }

  if (accessKeys.length > 20) {
    return res.status(400).json({ message: 'Too many keys (max 20)' });
  }

  try {
    const hashes = accessKeys.map(k => hashKey(k.trim().toUpperCase()));
    const devices = await Device.find({ accessKeyHash: { $in: hashes } });

    res.json({
      authenticated: devices.length > 0,
      devices: devices.map(d => ({ deviceId: d.deviceId, deviceName: d.deviceName }))
    });
  } catch (err) {
    res.status(500).json({ message: 'Server error' });
  }
});

module.exports = router;

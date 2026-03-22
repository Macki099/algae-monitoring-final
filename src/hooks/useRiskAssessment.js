import { useMemo } from 'react';

// Risk thresholds for algae bloom prediction (based on Random Forest algorithm)
const DEFAULT_THRESHOLDS = {
  temperature: {
    normal: { max: 20 },      // Normal: < 20°C
    moderate: { max: 25 },    // Moderate: 20-25°C, High: > 25°C
  },
  dissolvedOxygen: {
    normal: { min: 5 },       // Normal: > 5 mg/L
    moderate: { min: 2 },     // Moderate: 2-5 mg/L, High: < 2 mg/L
  },
  ph: {
    normal: { max: 8.5 },     // Normal: < 8.5
    moderate: { max: 9 },     // Moderate: 8.5-9, High: > 9
  },
  electricalConductivity: {
    normal: { max: 800 },     // Normal: < 800 µS/cm
    moderate: { max: 1000 },  // Moderate: 800-1000 µS/cm, High: > 1000 µS/cm
  },
  turbidity: {
    normal: { max: 10 },      // Normal: < 10 NTU
    moderate: { max: 50 },    // Moderate: 10-50 NTU, High: > 50 NTU
  }
};

const getSensitivityScale = (sensitivity) => {
  switch (sensitivity) {
    case 'strict':
      return 1.1;
    case 'relaxed':
      return 0.9;
    default:
      return 1;
  }
};

const buildThresholds = (overrides = {}) => {
  const scale = getSensitivityScale(overrides.sensitivity);

  const scaleMax = (value) => value * scale;
  const scaleMin = (value) => value * scale;

  return {
    temperature: {
      normal: { max: scaleMax(overrides.temperatureNormalMax ?? DEFAULT_THRESHOLDS.temperature.normal.max) },
      moderate: { max: scaleMax(overrides.temperatureModerateMax ?? DEFAULT_THRESHOLDS.temperature.moderate.max) }
    },
    dissolvedOxygen: {
      normal: { min: scaleMin(overrides.dissolvedOxygenNormalMin ?? DEFAULT_THRESHOLDS.dissolvedOxygen.normal.min) },
      moderate: { min: scaleMin(overrides.dissolvedOxygenModerateMin ?? DEFAULT_THRESHOLDS.dissolvedOxygen.moderate.min) }
    },
    ph: {
      normal: { max: scaleMax(overrides.phNormalMax ?? DEFAULT_THRESHOLDS.ph.normal.max) },
      moderate: { max: scaleMax(overrides.phModerateMax ?? DEFAULT_THRESHOLDS.ph.moderate.max) }
    },
    electricalConductivity: {
      normal: { max: scaleMax(overrides.electricalConductivityNormalMax ?? DEFAULT_THRESHOLDS.electricalConductivity.normal.max) },
      moderate: { max: scaleMax(overrides.electricalConductivityModerateMax ?? DEFAULT_THRESHOLDS.electricalConductivity.moderate.max) }
    },
    turbidity: {
      normal: { max: scaleMax(overrides.turbidityNormalMax ?? DEFAULT_THRESHOLDS.turbidity.normal.max) },
      moderate: { max: scaleMax(overrides.turbidityModerateMax ?? DEFAULT_THRESHOLDS.turbidity.moderate.max) }
    }
  };
};

export const useRiskAssessment = (sensorData, overrides = {}) => {
  const riskLevels = useMemo(() => {
    const THRESHOLDS = buildThresholds(overrides);
    const risks = {};
    
    // Temperature: Higher = Higher Risk
    const temp = sensorData.temperature;
    if (temp < THRESHOLDS.temperature.normal.max) {
      risks.temperature = 'normal';
    } else if (temp < THRESHOLDS.temperature.moderate.max) {
      risks.temperature = 'moderate';
    } else {
      risks.temperature = 'high';
    }
    
    // Dissolved Oxygen: Lower = Higher Risk (inverted logic)
    const do_val = sensorData.dissolvedOxygen;
    if (do_val > THRESHOLDS.dissolvedOxygen.normal.min) {
      risks.dissolvedOxygen = 'normal';
    } else if (do_val > THRESHOLDS.dissolvedOxygen.moderate.min) {
      risks.dissolvedOxygen = 'moderate';
    } else {
      risks.dissolvedOxygen = 'high';
    }
    
    // pH: Higher = Higher Risk
    const ph = sensorData.ph;
    if (ph < THRESHOLDS.ph.normal.max) {
      risks.ph = 'normal';
    } else if (ph < THRESHOLDS.ph.moderate.max) {
      risks.ph = 'moderate';
    } else {
      risks.ph = 'high';
    }
    
    // Electrical Conductivity: Higher = Higher Risk
    const ec = sensorData.electricalConductivity;
    if (ec < THRESHOLDS.electricalConductivity.normal.max) {
      risks.electricalConductivity = 'normal';
    } else if (ec < THRESHOLDS.electricalConductivity.moderate.max) {
      risks.electricalConductivity = 'moderate';
    } else {
      risks.electricalConductivity = 'high';
    }
    
    // Turbidity: Higher = Higher Risk
    const turbidity = sensorData.turbidity;
    if (turbidity < THRESHOLDS.turbidity.normal.max) {
      risks.turbidity = 'normal';
    } else if (turbidity < THRESHOLDS.turbidity.moderate.max) {
      risks.turbidity = 'moderate';
    } else {
      risks.turbidity = 'high';
    }
    
    // Probiotic chamber is a dosing-level monitor, not an algae-risk input.
    risks.probioticLevel = 'monitor';
    
    return risks;
  }, [sensorData, overrides]);

  const overallRisk = useMemo(() => {
    const riskValues = Object.entries(riskLevels)
      .filter(([key]) => key !== 'probioticLevel')
      .map(([, value]) => value);
    const highCount = riskValues.filter(risk => risk === 'high').length;
    const moderateCount = riskValues.filter(risk => risk === 'moderate').length;
    
    if (highCount > 0) {
      return 'high';
    } else if (moderateCount >= 2) {
      return 'moderate';
    } else if (moderateCount >= 1) {
      return 'moderate';
    }
    
    return 'normal';
  }, [riskLevels]);

  return {
    riskLevels,
    overallRisk
  };
};
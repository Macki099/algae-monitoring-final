import React from 'react';
import Icon from './Icon';

const RiskAssessment = ({ overallRisk, riskLevels, sensorData, mlPrediction, mlServiceStatus, isMlDriven }) => {
  const mlStatusText = mlPrediction?.risk
    ? 'Machine Learning (latest prediction received)'
    : mlServiceStatus === 'available'
      ? 'ML service online (waiting for sensor data)'
      : mlServiceStatus === 'unavailable'
        ? 'ML service unavailable'
        : 'Checking ML service status';
  const getRiskLevelText = (risk) => {
    const normalizedRisk = typeof risk === 'string' ? risk.toLowerCase() : risk;
    
    switch (normalizedRisk) {
      case 'normal': return 'Normal';
      case 'moderate': return 'Early Bloom Formation';
      case 'high': return 'High Risk - Bloom Conditions';
      default: return 'ML Unavailable';
    }
  };

  const getRiskDescription = (risk) => {
    const normalizedRisk = typeof risk === 'string' ? risk.toLowerCase() : risk;
    
    switch (normalizedRisk) {
      case 'normal': return 'No immediate algae bloom risk detected. All parameters within normal ranges.';
      case 'moderate': return 'Moderate risk of algae bloom formation. Monitor conditions closely.';
      case 'high': return 'High probability of algae bloom conditions. Immediate action recommended.';
      default: return 'No ML prediction received yet. Ensure backend can reach the ML service.';
    }
  };

  const getActionRecommendation = (action) => {
    if (!action) return null;
    
    return (
      <div className="action-recommendation">
        <h4><Icon name="activity" size={16} /> Recommended Actions</h4>
        <div className="action-details">
          {action.aerator_state && (
            <div className="action-item">
              <strong>Aerator:</strong> {action.aerator_state}
              {action.aerator_pwm > 0 && ` (PWM: ${action.aerator_pwm})`}
            </div>
          )}
          {action.valve_state && (
            <div className="action-item">
              <strong>Valve:</strong> {action.valve_state}
            </div>
          )}
          {action.reason && (
            <div className="action-reason">
              <Icon name="info" size={14} /> {action.reason}
            </div>
          )}
          {action.alert && (
            <div className="action-alert">
              <Icon name="alert-triangle" size={14} /> 
              <strong>ALERT:</strong> Critical intervention required!
            </div>
          )}
        </div>
      </div>
    );
  };

  const parameters = [
    { key: 'temperature', name: 'Temperature', value: sensorData.temperature, unit: '°C' },
    { key: 'dissolvedOxygen', name: 'Dissolved Oxygen', value: sensorData.dissolvedOxygen, unit: 'mg/L' },
    { key: 'ph', name: 'pH Level', value: sensorData.ph, unit: '' },
    { key: 'electricalConductivity', name: 'Conductivity', value: sensorData.electricalConductivity, unit: 'µS/cm' },
    { key: 'turbidity', name: 'Turbidity', value: sensorData.turbidity, unit: 'NTU' }
  ];

  const displayRisk = mlPrediction?.risk ? mlPrediction.risk.toLowerCase() : overallRisk;
  const probabilityRows = mlPrediction?.probabilities
    ? Object.entries(mlPrediction.probabilities)
        .sort((a, b) => b[1] - a[1])
        .map(([label, score]) => ({
          label,
          score: Number(score)
        }))
    : [];

  return (
    <div className="risk-assessment-card">
      <div className="card-header">
        <h3>
          <Icon name="shield" size={20} />
          Risk Assessment
          {mlPrediction?.risk && (
            <span className="ml-badge" title="Machine Learning Prediction">
              <Icon name="cpu" size={14} /> ML
            </span>
          )}
        </h3>
      </div>
      
      <div className="card-content">
        <div className="overall-risk">
          <div className="ml-confidence" style={{ marginBottom: 8 }}>
            <Icon name="cpu" size={14} />
            Risk Source: {isMlDriven ? 'Machine Learning (prediction mode)' : mlStatusText}
          </div>
          <div className={`risk-level ${displayRisk}`}>
            {getRiskLevelText(displayRisk)}
          </div>
          <div className="risk-description">
            {getRiskDescription(displayRisk)}
          </div>
          
          {mlPrediction?.confidence && (
            <div className="ml-confidence">
              <Icon name="trending-up" size={14} />
              Prediction Confidence: {(mlPrediction.confidence * 100).toFixed(1)}%
            </div>
          )}

          {probabilityRows.length > 0 && (
            <div className="ml-confidence">
              <Icon name="barChart" size={14} />
              Class Votes: {probabilityRows.map((row) => `${row.label} ${(row.score * 100).toFixed(1)}%`).join(' • ')}
            </div>
          )}
        </div>
        
        {mlPrediction?.action && getActionRecommendation(mlPrediction.action)}
        
        <div className="risk-parameters">
          {parameters.map((param) => (
            <div key={param.key} className={`risk-param ${riskLevels[param.key] || 'unknown'}`}>
              <div className="param-info">
                <span className="param-name">{param.name}:</span>
                <span className="param-value">
                  {param.value.toFixed(1)}{param.unit}
                </span>
              </div>
              <span className={`param-status ${riskLevels[param.key] || 'unknown'}`}>
                {riskLevels[param.key] === 'normal' ? 'Normal' : 
                 riskLevels[param.key] === 'moderate' ? 'Moderate' :
                 riskLevels[param.key] === 'high' ? 'High Risk' : 'No ML'}
              </span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};

export default RiskAssessment;
import React from 'react';
import Icon from './Icon';
import LineChart from './LineChart';
import CircularGauge from './CircularGauge';
import TankGauge from './TankGauge';
import { calculateTrend } from '../utils/trendAnalysis';

const ParameterCard = ({ 
  title, 
  icon, 
  value, 
  unit, 
  data, 
  riskLevel, 
  type = 'chart',
  thresholds,
  min = 0,
  max = 100 
}) => {
  const trend = calculateTrend(data);
  
  const getRiskBadgeText = (risk) => {
    switch (risk) {
      case 'normal': return 'Normal';
      case 'moderate': return 'Moderate Risk';
      case 'high': return 'High Risk';
      case 'monitor': return 'Level Monitor';
      default: return 'No ML';
    }
  };

  const getTrendIcon = (trendDirection) => {
    switch (trendDirection) {
      case 'up': return '↗';
      case 'down': return '↘';
      default: return '→';
    }
  };

  const getTrendText = (trendDirection) => {
    switch (trendDirection) {
      case 'up': return 'Rising';
      case 'down': return 'Decreasing';
      default: return 'Stable';
    }
  };

  const safeRangeText = thresholds
    ? `${thresholds.low.toFixed(1)} - ${thresholds.high.toFixed(1)}${unit}`
    : null;

  return (
    <div className={`parameter-card ${riskLevel}`}>
      <div className="card-header">
        <h3>
          <Icon name={icon} size={20} />
          {title}
        </h3>
        <div className={`risk-badge ${riskLevel}`}>
          {getRiskBadgeText(riskLevel)}
        </div>
      </div>
      
      <div className="card-content">
        {type === 'tank' ? (
          <div className="tank-container-wrapper">
            <TankGauge 
              value={value} 
              unit={unit}
              title={title}
            />
          </div>
        ) : type === 'gauge' ? (
          <div className="gauge-container">
            <CircularGauge 
              value={value} 
              min={min} 
              max={max} 
              unit={unit}
              title={title}
            />
          </div>
        ) : (
          <>
            <div className="chart-controls">
              <div className="chart-pill active">Live</div>
              <div className="chart-pill">6h</div>
              <div className="chart-pill">24h</div>
            </div>
            <div className="value-display">
              <span className="value">{value.toFixed(1)}</span>
              <span className="unit">{unit}</span>
            </div>
            {safeRangeText && <p className="safe-range-label">Target range: {safeRangeText}</p>}
            <div className="chart-container">
              <LineChart data={data} color="#4dd0e1" unit={unit} thresholds={thresholds} />
            </div>
          </>
        )}
        
        <div className="trend-indicator">
          <span className={`trend-icon ${trend}`}>
            {getTrendIcon(trend)}
          </span>
          <span className="trend-text">{getTrendText(trend)}</span>
        </div>
      </div>
    </div>
  );
};

export default ParameterCard;
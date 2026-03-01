from flask import Flask, request, jsonify
from flask_cors import CORS
import joblib
import pandas as pd
import numpy as np
import os
import time

app = Flask(__name__)
CORS(app)

# Load trained Random Forest model and label encoder
MODEL_PATH = os.path.join(os.path.dirname(__file__), 'rf_model.joblib')
ENCODER_PATH = os.path.join(os.path.dirname(__file__), 'label_encoder.joblib')

rf_model = None
label_encoder = None

def load_models():
    """Load the trained RF model and label encoder"""
    global rf_model, label_encoder
    
    if os.path.exists(MODEL_PATH) and os.path.exists(ENCODER_PATH):
        rf_model = joblib.load(MODEL_PATH)
        label_encoder = joblib.load(ENCODER_PATH)
        print(f"✓ Models loaded successfully from {os.path.dirname(__file__)}")
        return True
    else:
        print(f"⚠ Model files not found. Please train the model first.")
        print(f"  Expected: {MODEL_PATH}")
        print(f"  Expected: {ENCODER_PATH}")
        return False

# PID Controller for aerator control
class PIDController:
    def __init__(self, Kp=40.0, Ki=0.0, Kd=0.0, setpoint=5.0, out_min=0, out_max=255):
        self.Kp = Kp
        self.Ki = Ki
        self.Kd = Kd
        self.setpoint = setpoint
        self.prev_error = 0.0
        self.integral = 0.0
        self.out_min = out_min
        self.out_max = out_max
        self.prev_time = None

    def update(self, measurement):
        """Calculate PID output based on DO measurement"""
        now = time.time()
        
        if self.prev_time is None:
            dt = 1.0
        else:
            dt = max(1e-3, now - self.prev_time)
        
        error = self.setpoint - measurement
        self.integral += error * dt
        derivative = (error - self.prev_error) / dt if dt > 0 else 0.0
        
        output = self.Kp * error + self.Ki * self.integral + self.Kd * derivative
        output = max(self.out_min, min(self.out_max, int(output)))
        
        self.prev_error = error
        self.prev_time = now
        
        return output

# Global PID controller instance
pid_controller = PIDController(Kp=40.0, Ki=0.0, Kd=0.0, setpoint=5.0)

@app.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({
        'status': 'healthy',
        'model_loaded': rf_model is not None,
        'encoder_loaded': label_encoder is not None
    })

@app.route('/predict', methods=['POST'])
def predict_risk():
    """
    Predict algae bloom risk based on sensor data
    
    Expected JSON payload:
    {
        "temperature": 22.5,
        "ph": 8.2,
        "ec": 450.0,
        "turbidity": 35.0,
        "dissolvedOxygen": 4.5
    }
    
    Returns:
    {
        "risk": "Moderate",
        "confidence": 0.85,
        "action": {
            "aerator_pwm": 120,
            "aerator_state": "ON",
            "reason": "DO below setpoint (5.0 mg/L)"
        }
    }
    """
    if rf_model is None or label_encoder is None:
        return jsonify({
            'error': 'Model not loaded. Please train the model first.'
        }), 500
    
    try:
        data = request.get_json()
        
        # Map frontend field names to ML model feature names
        sample = {
            "DO": float(data.get('dissolvedOxygen', 0)),
            "Turbidity": float(data.get('turbidity', 0)),
            "pH": float(data.get('ph', 0)),
            "EC": float(data.get('ec', 0)),
            "Temp": float(data.get('temperature', 0))
        }
        
        # Create DataFrame with correct feature order
        sample_df = pd.DataFrame([sample], columns=["DO", "Turbidity", "pH", "EC", "Temp"])
        
        # Predict risk level
        pred_encoded = rf_model.predict(sample_df)[0]
        pred_label = label_encoder.inverse_transform([pred_encoded])[0]
        
        # Get prediction probabilities for confidence score
        pred_proba = rf_model.predict_proba(sample_df)[0]
        confidence = float(np.max(pred_proba))
        
        # Determine action based on risk level and PID control
        action = determine_action(pred_label, sample["DO"])
        
        return jsonify({
            'risk': pred_label,
            'confidence': round(confidence, 3),
            'action': action,
            'features': sample
        })
    
    except Exception as e:
        return jsonify({
            'error': f'Prediction failed: {str(e)}'
        }), 400

def determine_action(risk_level, dissolved_oxygen):
    """
    Determine actuator control action based on risk level and DO
    
    Uses PID controller for aerator PWM when intervention needed
    """
    if risk_level == "Normal":
        return {
            "aerator_pwm": 0,
            "aerator_state": "OFF",
            "valve_state": "CLOSED",
            "reason": "Water quality is normal. No intervention needed."
        }
    
    elif risk_level == "Moderate":
        pwm = pid_controller.update(dissolved_oxygen)
        return {
            "aerator_pwm": pwm,
            "aerator_state": "ON" if pwm > 0 else "OFF",
            "valve_state": "MICRODOSING",
            "reason": f"Moderate risk detected. DO: {dissolved_oxygen:.2f} mg/L, Target: 5.0 mg/L"
        }
    
    elif risk_level == "High":
        # Maximum intervention
        return {
            "aerator_pwm": 255,
            "aerator_state": "MAX_POWER",
            "valve_state": "FULL_DOSING",
            "alert": True,
            "reason": "CRITICAL: High algae bloom risk! Immediate mitigation required."
        }
    
    return {
        "aerator_pwm": 0,
        "aerator_state": "OFF",
        "valve_state": "CLOSED",
        "reason": "Unknown risk level"
    }

@app.route('/retrain', methods=['POST'])
def retrain_model():
    """
    Endpoint to retrain the model with new data
    Expects Excel file path in JSON payload
    """
    try:
        data = request.get_json()
        # This would trigger model retraining
        # Implementation would call train_model.py
        return jsonify({
            'message': 'Model retraining initiated',
            'status': 'pending'
        })
    except Exception as e:
        return jsonify({
            'error': f'Retraining failed: {str(e)}'
        }), 400

if __name__ == '__main__':
    print("=" * 50)
    print("PhycoSense ML Service - Random Forest Risk Prediction")
    print("=" * 50)
    
    # Load models on startup
    if load_models():
        print("\n✓ ML Service ready to accept predictions")
    else:
        print("\n⚠ Warning: Models not loaded. Train model first using train_model.py")
    
    print("\nStarting Flask server on http://localhost:5001")
    print("=" * 50)
    
    app.run(host='0.0.0.0', port=5001, debug=True)

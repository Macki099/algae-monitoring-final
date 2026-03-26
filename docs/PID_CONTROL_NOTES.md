# PID Control Notes (From Attached Documents)

## Source Summary

The provided documents define a control loop with two actuator actions:
- Aerator control using PID based on dissolved oxygen (DO) error from a risk-based setpoint.
- Probiotic dosing based on ML/risk category.

Risk-to-action mapping from documents:
- NORMAL: DO setpoint 5.0 mg/L, probiotic dose 0 mL
- MODERATE: DO setpoint 5.5 mg/L, probiotic dose 3 mL
- HIGH: DO setpoint 6.0 mg/L, probiotic dose 8 mL

PID equation from documents:
- error = setpoint - DO
- integral = integral + error
- derivative = error - previous_error
- output = (Kp * error) + (Ki * integral) + (Kd * derivative)

Reference constants in attached sketch:
- Kp = 2.0
- Ki = 0.1
- Kd = 0.5
- flow_rate = 1 mL/s

## Implemented in PhycoSense Complete Firmware

File updated:
- arduino/phycosense_complete/phycosense_complete.ino

### Pin Mapping (optional)
- The current sketch includes one sample mapping for testing.
- Pin assignments are configurable and can be changed based on final wiring.

Current sample sensor pin adjustments for ADC compatibility:
- Turbidity moved to GPIO 32
- Dissolved oxygen (DO) sensor on GPIO 33

### Control Logic Added
- DO reading function with simple voltage-to-mg/L mapping (placeholder calibration).
- Local risk classification function (`NORMAL`, `MODERATE`, `HIGH`) from sensor values.
- Risk-based DO setpoint and probiotic dose selection.
- PID-based aerator PWM control (`0-255`) using:
  - Integral windup clamp
  - Derivative by elapsed time
- Non-blocking probiotic dosing scheduler:
  - Dosing duration = dose_mL / flow_rate
  - Pump auto-stops using millis timing
  - Cooldown between doses (2 minutes)
  - Skip dose when probiotic level is critically low

### Telemetry Added to Payload
The firmware now sends additional runtime control fields with sensor data:
- dissolvedOxygen
- localRiskLevel
- aeratorPwm

## Important Calibration Note

Current DO conversion is a generic linear map and must be calibrated to your exact DO sensor module for accurate mg/L readings in real pond operation.

Suggested next calibration steps:
1. Capture raw voltage in known reference solutions.
2. Update `doVoltageAtZero`, `doVoltageAtFull`, and scaling range.
3. Validate PID stability in controlled tests before field dosing.

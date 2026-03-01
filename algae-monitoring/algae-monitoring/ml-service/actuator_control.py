"""
PhycoSense Actuator Control Script

This script implements PID-based aerator control based on ML risk predictions.
It can be run independently or integrated with the ESP32 system.

Features:
- PID control for aerator PWM based on dissolved oxygen
- Automatic valve control for chemical dosing
- Emergency alerts for high-risk conditions
- Serial/HTTP communication with ESP32

Usage:
    python actuator_control.py --port COM3 --mode serial
    python actuator_control.py --esp32-ip 192.168.1.100 --mode http
"""

import argparse
import time
import json
import requests
try:
    import serial
except ImportError:
    serial = None

class PIDController:
    """
    PID Controller for dissolved oxygen regulation via aerator
    
    Parameters:
        Kp: Proportional gain
        Ki: Integral gain  
        Kd: Derivative gain
        setpoint: Target DO level (mg/L)
        out_min: Minimum PWM output (0)
        out_max: Maximum PWM output (255)
    """
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
        
        # Anti-windup: prevent integral from growing unbounded
        self.integral = max(-100, min(100, self.integral))
        
        derivative = (error - self.prev_error) / dt if dt > 0 else 0.0
        
        output = self.Kp * error + self.Ki * self.integral + self.Kd * derivative
        output = max(self.out_min, min(self.out_max, int(output)))
        
        self.prev_error = error
        self.prev_time = now
        
        return output

    def reset(self):
        """Reset PID controller state"""
        self.prev_error = 0.0
        self.integral = 0.0
        self.prev_time = None


class ActuatorController:
    """Control aerators and valves based on ML risk predictions"""
    
    def __init__(self, pid_controller, esp32_ip=None, serial_port=None):
        self.pid = pid_controller
        self.esp32_ip = esp32_ip
        self.serial_port = serial_port
        self.serial_conn = None
        
        if serial_port and serial:
            try:
                self.serial_conn = serial.Serial(serial_port, 115200, timeout=1)
                print(f"✓ Serial connection established on {serial_port}")
            except Exception as e:
                print(f"⚠ Serial connection failed: {e}")
    
    def determine_action(self, risk_level, dissolved_oxygen):
        """
        Determine actuator control action based on risk level and DO
        
        Returns control command dict with:
            - aerator_pwm: PWM value 0-255
            - aerator_state: ON/OFF/MAX_POWER
            - valve_state: CLOSED/MICRODOSING/FULL_DOSING
            - alert: True if critical condition
        """
        if risk_level == "Normal":
            return {
                "aerator_pwm": 0,
                "aerator_state": "OFF",
                "valve_state": "CLOSED",
                "alert": False,
                "reason": "Water quality normal"
            }
        
        elif risk_level == "Moderate":
            pwm = self.pid.update(dissolved_oxygen)
            return {
                "aerator_pwm": pwm,
                "aerator_state": "ON" if pwm > 0 else "OFF",
                "valve_state": "MICRODOSING",
                "alert": False,
                "reason": f"Moderate risk - DO: {dissolved_oxygen:.2f} mg/L"
            }
        
        elif risk_level == "High":
            return {
                "aerator_pwm": 255,
                "aerator_state": "MAX_POWER",
                "valve_state": "FULL_DOSING",
                "alert": True,
                "reason": "CRITICAL: High algae bloom risk!"
            }
        
        return {
            "aerator_pwm": 0,
            "aerator_state": "OFF",
            "valve_state": "CLOSED",
            "alert": False,
            "reason": "Unknown risk level"
        }
    
    def send_command_serial(self, command):
        """Send control command via serial to ESP32"""
        if not self.serial_conn:
            print("⚠ Serial not connected")
            return False
        
        try:
            cmd_json = json.dumps(command) + '\n'
            self.serial_conn.write(cmd_json.encode())
            print(f"→ Serial: {cmd_json.strip()}")
            return True
        except Exception as e:
            print(f"⚠ Serial send error: {e}")
            return False
    
    def send_command_http(self, command):
        """Send control command via HTTP to ESP32"""
        if not self.esp32_ip:
            print("⚠ ESP32 IP not configured")
            return False
        
        try:
            url = f"http://{self.esp32_ip}/actuator"
            response = requests.post(url, json=command, timeout=2)
            
            if response.status_code == 200:
                print(f"→ HTTP: {command}")
                return True
            else:
                print(f"⚠ HTTP error: {response.status_code}")
                return False
        except Exception as e:
            print(f"⚠ HTTP send error: {e}")
            return False
    
    def execute_action(self, action):
        """Execute control action on ESP32 actuators"""
        print(f"\n{'='*50}")
        print(f"ACTUATOR CONTROL ACTION")
        print(f"{'='*50}")
        print(f"Aerator: {action['aerator_state']} (PWM: {action['aerator_pwm']})")
        print(f"Valve: {action['valve_state']}")
        print(f"Reason: {action['reason']}")
        
        if action.get('alert'):
            print(f"\n🚨 ALERT: {action['reason']}")
        
        # Send via configured method
        if self.serial_conn:
            self.send_command_serial(action)
        elif self.esp32_ip:
            self.send_command_http(action)
        else:
            print("⚠ No communication method configured")
        
        print(f"{'='*50}\n")
    
    def close(self):
        """Close connections"""
        if self.serial_conn:
            self.serial_conn.close()


def main():
    parser = argparse.ArgumentParser(description='PhycoSense Actuator Control')
    parser.add_argument('--mode', choices=['serial', 'http'], default='http',
                       help='Communication mode with ESP32')
    parser.add_argument('--port', type=str, default='COM3',
                       help='Serial port for ESP32 communication')
    parser.add_argument('--esp32-ip', type=str, default='192.168.1.128',
                       help='ESP32 IP address for HTTP communication')
    parser.add_argument('--ml-service', type=str, default='http://localhost:5001',
                       help='ML service URL')
    parser.add_argument('--interval', type=int, default=10,
                       help='Update interval in seconds')
    
    args = parser.parse_args()
    
    print("=" * 50)
    print("PhycoSense Actuator Control System")
    print("=" * 50)
    print(f"Mode: {args.mode.upper()}")
    print(f"ML Service: {args.ml_service}")
    print(f"Update Interval: {args.interval}s")
    print("=" * 50)
    
    # Initialize PID controller
    pid = PIDController(Kp=40.0, Ki=0.0, Kd=0.0, setpoint=5.0)
    
    # Initialize actuator controller
    serial_port = args.port if args.mode == 'serial' else None
    esp32_ip = args.esp32_ip if args.mode == 'http' else None
    controller = ActuatorController(pid, esp32_ip=esp32_ip, serial_port=serial_port)
    
    print("\n✓ Actuator controller ready")
    print("Press Ctrl+C to stop\n")
    
    try:
        while True:
            # In real implementation, get sensor data from backend API
            # For demo, using placeholder values
            # sensor_data = get_latest_sensor_data()
            
            # Example: Get risk prediction from ML service
            # ml_response = requests.get(f"{args.ml_service}/predict", json=sensor_data)
            # risk_level = ml_response.json()['risk']
            # dissolved_oxygen = sensor_data['dissolvedOxygen']
            
            # Placeholder for demonstration
            print("Waiting for sensor data...")
            print("(In production, this would query the backend API)")
            
            time.sleep(args.interval)
            
    except KeyboardInterrupt:
        print("\n\nShutting down actuator control...")
        controller.close()
        print("✓ Shutdown complete")


if __name__ == '__main__':
    main()

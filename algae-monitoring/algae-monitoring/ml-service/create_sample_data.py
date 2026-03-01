"""
Create sample training data for PhycoSense ML model
This generates realistic sensor data with varied risk levels
"""

import pandas as pd
import numpy as np
from datetime import datetime, timedelta

np.random.seed(42)

# Generate 110 samples (matching your original dataset)
n_samples = 110

# Create timestamp range
start_date = datetime.now() - timedelta(hours=3)
timestamps = [start_date + timedelta(seconds=i*30) for i in range(n_samples)]

data = []

for i in range(n_samples):
    # Vary the risk levels - 40% Normal, 40% Moderate, 20% High
    risk_type = np.random.choice(['normal', 'moderate', 'high'], p=[0.4, 0.4, 0.2])
    
    if risk_type == 'normal':
        # Normal conditions
        temp = np.random.uniform(18, 20)
        do = np.random.uniform(6, 10)
        ph = np.random.uniform(7, 8)
        ec = np.random.uniform(300, 600)
        turbidity = np.random.uniform(5, 9)
    
    elif risk_type == 'moderate':
        # Moderate risk conditions
        temp = np.random.uniform(20, 24)
        do = np.random.uniform(3, 5)
        ph = np.random.uniform(8, 8.8)
        ec = np.random.uniform(600, 900)
        turbidity = np.random.uniform(10, 45)
    
    else:  # high
        # High risk conditions
        temp = np.random.uniform(24, 28)
        do = np.random.uniform(1, 3)
        ph = np.random.uniform(8.5, 9.5)
        ec = np.random.uniform(850, 1100)
        turbidity = np.random.uniform(45, 70)
    
    data.append({
        'Timestamp': timestamps[i].strftime('%m/%d/%Y, %H:%M:%S'),
        'Temperature (°C)': temp,
        'Dissolved Oxygen (mg/L)': do,
        'pH Level': ph,
        'Electrical Conductivity (µS/cm)': ec,
        'Turbidity (NTU)': turbidity
    })

df = pd.DataFrame(data)

# Save to Excel
output_file = 'phycosense-data.xlsx'
df.to_excel(output_file, index=False)
print(f"✓ Created sample training data: {output_file}")
print(f"  Total samples: {len(df)}")
print(f"\nRisk distribution (will be auto-labeled during training):")
print(f"  Temperature range: {df['Temperature (°C)'].min():.1f} - {df['Temperature (°C)'].max():.1f} °C")
print(f"  DO range: {df['Dissolved Oxygen (mg/L)'].min():.1f} - {df['Dissolved Oxygen (mg/L)'].max():.1f} mg/L")
print(f"  pH range: {df['pH Level'].min():.1f} - {df['pH Level'].max():.1f}")
print(f"  EC range: {df['Electrical Conductivity (µS/cm)'].min():.1f} - {df['Electrical Conductivity (µS/cm)'].max():.1f} µS/cm")
print(f"  Turbidity range: {df['Turbidity (NTU)'].min():.1f} - {df['Turbidity (NTU)'].max():.1f} NTU")

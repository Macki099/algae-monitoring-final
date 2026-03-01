"""
PhycoSense Random Forest Model Training Script

This script trains the Random Forest classifier for algae bloom risk prediction
based on water quality sensor data (Temperature, pH, EC, Turbidity, DO).

Usage:
    python train_model.py --data path/to/phycosense-data.xlsx
"""

import pandas as pd
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.preprocessing import LabelEncoder
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix, accuracy_score
import joblib
import argparse
import os

def label_row(row):
    """
    Apply risk labeling based on water quality thresholds
    
    Risk Levels:
    - High: DO < 2 OR Turbidity > 50 OR pH > 9 OR EC > 1000 OR Temp > 25
    - Moderate: DO < 5 OR Turbidity > 10 OR pH > 8.5 OR EC > 800 OR Temp > 20
    - Normal: Otherwise
    
    Note: pH = 7.0 is treated as neutral placeholder (sensor not installed)
    """
    # Ignore pH in risk assessment if it's exactly 7.0 (placeholder value)
    ph_high = (row["pH"] > 9) if row["pH"] != 7.0 else False
    ph_moderate = (row["pH"] > 8.5) if row["pH"] != 7.0 else False
    
    if (row["DO"] < 2 or 
        row["Turbidity"] > 50 or 
        ph_high or 
        row["EC"] > 1000 or 
        row["Temp"] > 25):
        return "High"
    elif (row["DO"] < 5 or 
          row["Turbidity"] > 10 or 
          ph_moderate or 
          row["EC"] > 800 or 
          row["Temp"] > 20):
        return "Moderate"
    else:
        return "Normal"

def load_and_prepare_data(file_path):
    """Load dataset and prepare features"""
    print(f"Loading dataset from {file_path}...")
    
    # Try reading Excel file
    try:
        df = pd.read_excel(file_path)
    except Exception as e:
        print(f"Error reading Excel file: {e}")
        print("Trying CSV format...")
        df = pd.read_csv(file_path)
    
    print(f"✓ Dataset loaded: {len(df)} samples")
    
    # Rename columns to match model features
    column_mapping = {
        "Temperature (°C)": "Temp",
        "Dissolved Oxygen (mg/L)": "DO",
        "pH Level": "pH",
        "Electrical Conductivity (µS/cm)": "EC",
        "Turbidity (NTU)": "Turbidity"
    }
    
    df = df.rename(columns=column_mapping)
    
    # Check for required columns
    required_cols = ["Temp", "DO", "pH", "EC", "Turbidity"]
    missing = [c for c in required_cols if c not in df.columns]
    
    if missing:
        print(f"⚠ Missing required columns: {missing}")
        print(f"Available columns: {list(df.columns)}")
        raise ValueError(f"Missing columns: {missing}")
    
    print("✓ All required columns found")
    
    # Generate risk labels
    print("Generating risk labels based on thresholds...")
    df["Risk"] = df.apply(label_row, axis=1)
    
    # Display risk distribution
    print("\nRisk Distribution:")
    print(df["Risk"].value_counts())
    
    return df

def train_random_forest(X_train, y_train):
    """Train Random Forest classifier"""
    print("\nTraining Random Forest Classifier...")
    print(f"  - n_estimators: 100")
    print(f"  - max_depth: 99")
    print(f"  - class_weight: balanced")
    
    rf = RandomForestClassifier(
        n_estimators=100,
        max_depth=99,
        random_state=42,
        class_weight='balanced'
    )
    
    rf.fit(X_train, y_train)
    print("✓ Model training completed")
    
    return rf

def evaluate_model(rf, X_test, y_test, encoder):
    """Evaluate model performance"""
    print("\n" + "=" * 50)
    print("MODEL EVALUATION")
    print("=" * 50)
    
    y_pred = rf.predict(X_test)
    accuracy = accuracy_score(y_test, y_pred)
    
    print(f"\nAccuracy: {accuracy:.3f} ({accuracy*100:.1f}%)")
    
    print("\nClassification Report:")
    target_names = encoder.classes_
    print(classification_report(y_test, y_pred, target_names=target_names))
    
    print("\nConfusion Matrix:")
    cm = confusion_matrix(y_test, y_pred)
    print(cm)
    
    # Feature importance
    feature_names = ["DO", "Turbidity", "pH", "EC", "Temp"]
    importances = rf.feature_importances_
    
    print("\nFeature Importances:")
    for name, importance in sorted(zip(feature_names, importances), 
                                   key=lambda x: x[1], reverse=True):
        print(f"  {name:15s}: {importance:.4f}")
    
    return accuracy

def save_models(rf, encoder, output_dir):
    """Save trained model and encoder"""
    os.makedirs(output_dir, exist_ok=True)
    
    model_path = os.path.join(output_dir, 'rf_model.joblib')
    encoder_path = os.path.join(output_dir, 'label_encoder.joblib')
    
    joblib.dump(rf, model_path)
    joblib.dump(encoder, encoder_path)
    
    print(f"\n✓ Model saved to: {model_path}")
    print(f"✓ Encoder saved to: {encoder_path}")

def main():
    parser = argparse.ArgumentParser(description='Train Random Forest model for algae bloom prediction')
    parser.add_argument('--data', type=str, 
                       default='phycosense-data.xlsx',
                       help='Path to training data (Excel or CSV)')
    parser.add_argument('--output', type=str,
                       default='.',
                       help='Output directory for model files')
    
    args = parser.parse_args()
    
    print("=" * 50)
    print("PhycoSense Random Forest Model Training")
    print("=" * 50)
    
    # Load and prepare data
    df = load_and_prepare_data(args.data)
    
    # Prepare features and labels
    X = df[["DO", "Turbidity", "pH", "EC", "Temp"]]
    y = df["Risk"]
    
    # Encode labels
    encoder = LabelEncoder()
    y_encoded = encoder.fit_transform(y)
    
    print(f"\nLabel Encoding:")
    for idx, label in enumerate(encoder.classes_):
        print(f"  {label}: {idx}")
    
    # Train-test split
    X_train, X_test, y_train, y_test = train_test_split(
        X, y_encoded, test_size=0.2, random_state=42, stratify=y_encoded
    )
    
    print(f"\nDataset Split:")
    print(f"  Training samples: {len(X_train)}")
    print(f"  Testing samples: {len(X_test)}")
    
    # Train model
    rf = train_random_forest(X_train, y_train)
    
    # Evaluate model
    accuracy = evaluate_model(rf, X_test, y_test, encoder)
    
    # Save models
    save_models(rf, encoder, args.output)
    
    print("\n" + "=" * 50)
    print(f"✓ Training Complete - Accuracy: {accuracy:.3f}")
    print("=" * 50)
    print("\nNext steps:")
    print("1. Start ML service: python ml_service.py")
    print("2. ML service will be available at http://localhost:5001")
    print("3. Backend will call /predict endpoint for risk predictions")

if __name__ == '__main__':
    main()

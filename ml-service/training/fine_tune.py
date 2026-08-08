"""
Fine-tuning script for personalizing the fall detection model
Takes calibration data from a specific user and fine-tunes the model
"""

import torch
import numpy as np
import os
import sys
from torch.utils.data import DataLoader
from datetime import datetime

# Add parent directory to path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from models.multimodal_fall_detector import create_model
from training.trainer import Trainer, FallDetectionDataset


def load_calibration_data(calibration_file: str):
    """Load calibration data from file"""
    import json
    
    with open(calibration_file, 'r') as f:
        data = json.load(f)
    
    # Parse sensor data
    sensor_data = []
    vitals_data = []
    labels = []
    user_ids = []
    
    for sample in data['samples']:
        # Combine accelerometer and gyroscope
        accel = np.array(sample['accelerometer'])
        gyro = np.array(sample['gyroscope'])
        combined = np.concatenate([accel, gyro], axis=1)
        
        # Ensure window size
        window_size = 50
        if combined.shape[0] < window_size:
            padding = np.zeros((window_size - combined.shape[0], 6))
            combined = np.vstack([combined, padding])
        elif combined.shape[0] > window_size:
            combined = combined[:window_size]
        
        sensor_data.append(combined)
        
        # Vitals
        vitals = np.array([
            sample['vitals']['heartRate'],
            sample['vitals']['spo2'],
            sample['vitals']['temperature']
        ])
        vitals_data.append(vitals)
        
        # Labels (all calibration data is NORMAL)
        labels.append(0)
        
        # User ID
        user_id = hash(data['elderlyId']) % 1000
        user_ids.append(user_id)
    
    return {
        'sensor_data': np.array(sensor_data),
        'vitals_data': np.array(vitals_data),
        'user_ids': np.array(user_ids),
        'labels': np.array(labels),
        'elderly_id': data['elderlyId']
    }


def fine_tune_user_model(
    elderly_id: str,
    calibration_data: dict,
    base_model_path: str = 'models/checkpoints/best_model.pth',
    output_dir: str = 'models/personalized',
    device: str = 'cpu'
):
    """Fine-tune model on user's calibration data"""
    print(f"Fine-tuning model for user: {elderly_id}")
    
    # Create dataset
    dataset = FallDetectionDataset(calibration_data)
    
    if len(dataset) < 10:
        print(f"Warning: Only {len(dataset)} samples available for fine-tuning")
    
    # Create dataloader
    dataloader = DataLoader(dataset, batch_size=8, shuffle=True)
    
    # Load base model
    model = create_model(num_users=1000, device=device)
    
    if os.path.exists(base_model_path):
        checkpoint = torch.load(base_model_path, map_location=device)
        model.load_state_dict(checkpoint['model_state_dict'])
        print(f"Loaded base model from {base_model_path}")
    else:
        print("No base model found, using initialized model")
    
    # Create trainer
    trainer = Trainer(model, device=device, learning_rate=0.0001)
    
    # Fine-tune
    print("Starting fine-tuning...")
    history = trainer.fine_tune(dataloader, epochs=10, learning_rate=0.0001)
    
    # Save personalized model
    os.makedirs(output_dir, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    save_path = os.path.join(output_dir, f"{elderly_id}_model_{timestamp}.pth")
    
    torch.save({
        'model_state_dict': model.state_dict(),
        'history': history,
        'elderly_id': elderly_id,
        'timestamp': timestamp,
        'base_model': base_model_path
    }, save_path)
    
    print(f"Personalized model saved to {save_path}")
    
    return save_path, history


def main():
    """Main fine-tuning pipeline"""
    import argparse
    
    parser = argparse.ArgumentParser(description='Fine-tune fall detection model for a user')
    parser.add_argument('--elderly_id', type=str, required=True, help='Elderly ID')
    parser.add_argument('--calibration_file', type=str, required=True, help='Path to calibration data file')
    parser.add_argument('--base_model', type=str, default='models/checkpoints/best_model.pth', help='Path to base model')
    parser.add_argument('--output_dir', type=str, default='models/personalized', help='Output directory')
    parser.add_argument('--device', type=str, default='cpu', help='Device to use')
    
    args = parser.parse_args()
    
    # Load calibration data
    calibration_data = load_calibration_data(args.calibration_file)
    
    # Fine-tune
    model_path, history = fine_tune_user_model(
        elderly_id=args.elderly_id,
        calibration_data=calibration_data,
        base_model_path=args.base_model,
        output_dir=args.output_dir,
        device=args.device
    )
    
    print(f"\nFine-tuning complete!")
    print(f"Model saved to: {model_path}")
    print(f"Final accuracy: {history['train_acc'][-1]:.2f}%")


if __name__ == "__main__":
    main()

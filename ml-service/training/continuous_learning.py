"""
Continuous learning system for model adaptation
Periodically updates the model with new user data to improve accuracy over time
"""

import torch
import numpy as np
import os
import json
from datetime import datetime, timedelta
from typing import Dict, List
from collections import deque

import sys
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from models.multimodal_fall_detector import create_model
from training.trainer import Trainer, FallDetectionDataset
from torch.utils.data import DataLoader


class ContinuousLearningSystem:
    """Manages continuous learning and model adaptation"""
    
    def __init__(
        self,
        elderly_id: str,
        model_path: str,
        buffer_size: int = 1000,
        min_samples_for_update: int = 50,
        update_interval_hours: int = 24,
        device: str = 'cpu'
    ):
        self.elderly_id = elderly_id
        self.model_path = model_path
        self.buffer_size = buffer_size
        self.min_samples_for_update = min_samples_for_update
        self.update_interval_hours = update_interval_hours
        self.device = device
        
        # Data buffer for collecting new samples
        self.data_buffer = deque(maxlen=buffer_size)
        self.last_update_time = datetime.now()
        
        # Load model
        self.model = self._load_model()
        
        # Adaptive thresholds
        self.thresholds = {
            'normal_confidence': 0.7,
            'prefall_confidence': 0.6,
            'fall_confidence': 0.8,
            'sudden_movement_confidence': 0.65
        }
        
        # User statistics for adaptive thresholds
        self.user_stats = {
            'avg_acceleration': 0.0,
            'avg_heart_rate': 75.0,
            'movement_variance': 0.0,
            'sample_count': 0
        }
    
    def _load_model(self):
        """Load the personalized model"""
        model = create_model(num_users=1000, device=self.device)
        
        if os.path.exists(self.model_path):
            checkpoint = torch.load(self.model_path, map_location=self.device)
            model.load_state_dict(checkpoint['model_state_dict'])
            print(f"Loaded model from {self.model_path}")
        else:
            print(f"Model not found at {self.model_path}, using initialized model")
        
        model.eval()
        return model
    
    def add_sample(self, sensor_data: np.ndarray, vitals: np.ndarray, label: int):
        """Add a new sample to the data buffer"""
        sample = {
            'sensor_data': sensor_data,
            'vitals_data': vitals,
            'label': label,
            'timestamp': datetime.now().isoformat()
        }
        
        self.data_buffer.append(sample)
        
        # Update user statistics
        self._update_user_stats(sensor_data, vitals)
        
        # Check if update is needed
        if self._should_update():
            self.update_model()
    
    def _update_user_stats(self, sensor_data: np.ndarray, vitals: np.ndarray):
        """Update user statistics for adaptive thresholds"""
        # Calculate acceleration magnitude
        accel_magnitude = np.mean(np.sqrt(np.sum(sensor_data[:, :3]**2, axis=1)))
        
        # Update running averages
        alpha = 0.1  # Smoothing factor
        self.user_stats['avg_acceleration'] = (
            alpha * accel_magnitude + 
            (1 - alpha) * self.user_stats['avg_acceleration']
        )
        self.user_stats['avg_heart_rate'] = (
            alpha * vitals[0] + 
            (1 - alpha) * self.user_stats['avg_heart_rate']
        )
        
        # Calculate movement variance
        if len(self.data_buffer) > 10:
            recent_accels = [s['sensor_data'][:, :3] for s in list(self.data_buffer)[-10:]]
            variance = np.var([np.mean(np.sqrt(np.sum(a**2, axis=1))) for a in recent_accels])
            self.user_stats['movement_variance'] = variance
        
        self.user_stats['sample_count'] += 1
    
    def _should_update(self) -> bool:
        """Check if model should be updated"""
        # Check minimum samples
        if len(self.data_buffer) < self.min_samples_for_update:
            return False
        
        # Check time interval
        time_since_update = datetime.now() - self.last_update_time
        if time_since_update < timedelta(hours=self.update_interval_hours):
            return False
        
        return True
    
    def update_model(self):
        """Update the model with new data"""
        print(f"Updating model for {self.elderly_id} with {len(self.data_buffer)} new samples")
        
        # Convert buffer to dataset
        sensor_data = np.array([s['sensor_data'] for s in self.data_buffer])
        vitals_data = np.array([s['vitals_data'] for s in self.data_buffer])
        labels = np.array([s['label'] for s in self.data_buffer])
        user_ids = np.array([hash(self.elderly_id) % 1000] * len(self.data_buffer))
        
        dataset = FallDetectionDataset({
            'sensor_data': sensor_data,
            'vitals_data': vitals_data,
            'user_ids': user_ids,
            'labels': labels
        })
        
        dataloader = DataLoader(dataset, batch_size=16, shuffle=True)
        
        # Create trainer and fine-tune
        trainer = Trainer(self.model, device=self.device, learning_rate=0.0001)
        history = trainer.fine_tune(dataloader, epochs=5, learning_rate=0.0001)
        
        # Save updated model
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_path = self.model_path.replace('.pth', f'_backup_{timestamp}.pth')
        torch.save({
            'model_state_dict': self.model.state_dict(),
            'timestamp': timestamp
        }, backup_path)
        
        # Update main model
        torch.save({
            'model_state_dict': self.model.state_dict(),
            'history': history,
            'elderly_id': self.elderly_id,
            'last_update': timestamp,
            'user_stats': self.user_stats
        }, self.model_path)
        
        self.last_update_time = datetime.now()
        
        # Update adaptive thresholds based on new statistics
        self._update_adaptive_thresholds()
        
        print(f"Model updated successfully. Backup saved to {backup_path}")
    
    def _update_adaptive_thresholds(self):
        """Update adaptive thresholds based on user statistics"""
        # Adjust thresholds based on movement patterns
        if self.user_stats['movement_variance'] > 2.0:
            # High variance user - lower thresholds for sensitivity
            self.thresholds['normal_confidence'] = 0.6
            self.thresholds['prefall_confidence'] = 0.5
            self.thresholds['fall_confidence'] = 0.7
        elif self.user_stats['movement_variance'] < 0.5:
            # Low variance user - higher thresholds to reduce false positives
            self.thresholds['normal_confidence'] = 0.8
            self.thresholds['prefall_confidence'] = 0.7
            self.thresholds['fall_confidence'] = 0.85
        
        # Adjust based on heart rate patterns
        if self.user_stats['avg_heart_rate'] > 90:
            # Elevated heart rate - more sensitive to sudden changes
            self.thresholds['sudden_movement_confidence'] = 0.55
        else:
            self.thresholds['sudden_movement_confidence'] = 0.65
    
    def predict(self, sensor_data: np.ndarray, vitals: np.ndarray) -> Dict:
        """Make prediction with adaptive thresholds"""
        self.model.eval()
        
        with torch.no_grad():
            # Prepare input
            sensor_tensor = torch.FloatTensor(sensor_data).unsqueeze(0).transpose(1, 2).to(self.device)
            vitals_tensor = torch.FloatTensor(vitals).unsqueeze(0).to(self.device)
            user_id = hash(self.elderly_id) % 1000
            user_id_tensor = torch.LongTensor([user_id]).to(self.device)
            
            # Get logits
            logits = self.model(sensor_tensor, vitals_tensor, user_id_tensor)
            probabilities = torch.softmax(logits, dim=1)
            
            # Apply adaptive thresholds
            confidence, predicted = torch.max(probabilities, 1)
            
            # Check if confidence meets adaptive threshold
            class_names = ["NORMAL", "SUDDEN_MOVEMENT", "PREFALL", "FALL"]
            threshold_key = f"{class_names[predicted.item()].lower()}_confidence"
            adaptive_threshold = self.thresholds.get(threshold_key, 0.7)
            
            if confidence.item() < adaptive_threshold:
                # If confidence is below threshold, default to NORMAL
                predicted = torch.tensor([0])
                confidence = torch.tensor([probabilities[0][0]])
            
            return {
                'prediction': class_names[predicted.item()],
                'confidence': confidence.item(),
                'probabilities': {
                    class_names[i]: probabilities[0][i].item()
                    for i in range(len(class_names))
                },
                'adaptive_thresholds': self.thresholds,
                'user_stats': self.user_stats
            }
    
    def get_status(self) -> Dict:
        """Get system status"""
        return {
            'elderly_id': self.elderly_id,
            'buffer_size': len(self.data_buffer),
            'last_update': self.last_update_time.isoformat(),
            'adaptive_thresholds': self.thresholds,
            'user_stats': self.user_stats,
            'should_update': self._should_update()
        }


if __name__ == "__main__":
    # Test continuous learning system
    system = ContinuousLearningSystem(
        elderly_id="test_user",
        model_path="models/personalized/test_user_model.pth",
        device="cpu"
    )
    
    # Add some test samples
    for i in range(100):
        sensor_data = np.random.randn(50, 6)
        vitals = np.array([75 + np.random.randn(), 98 + np.random.randn(), 36.5 + np.random.randn() * 0.1])
        system.add_sample(sensor_data, vitals, label=0)
    
    print("System status:", system.get_status())
    
    # Test prediction
    test_sensor = np.random.randn(50, 6)
    test_vitals = np.array([75, 98, 36.5])
    prediction = system.predict(test_sensor, test_vitals)
    print("Prediction:", prediction)

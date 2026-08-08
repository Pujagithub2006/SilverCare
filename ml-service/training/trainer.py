"""
Training pipeline for multimodal fall detection model
Supports training on global datasets and fine-tuning for individual users
"""

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import numpy as np
from typing import Dict, List, Tuple
import os
import json
from datetime import datetime

from models.multimodal_fall_detector import MultimodalFallDetector, create_model
from data.preprocessing import DataPipeline


class FallDetectionDataset(Dataset):
    """Dataset for fall detection training"""
    
    def __init__(self, data_dict: Dict):
        self.sensor_data = data_dict['sensor_data']
        self.vitals_data = data_dict['vitals_data']
        self.user_ids = data_dict['user_ids']
        self.labels = data_dict['labels']
        
    def __len__(self) -> int:
        return len(self.labels)
    
    def __getitem__(self, idx: int) -> Dict[str, torch.Tensor]:
        # Transpose sensor data from (timesteps, channels) to (channels, timesteps)
        sensor = torch.FloatTensor(self.sensor_data[idx]).transpose(0, 1)
        return {
            'sensor_data': sensor,
            'vitals_data': torch.FloatTensor(self.vitals_data[idx]),
            'user_id': torch.LongTensor([self.user_ids[idx]]),
            'label': torch.LongTensor([self.labels[idx]])
        }


class Trainer:
    """Trainer for multimodal fall detection model"""
    
    def __init__(
        self,
        model: MultimodalFallDetector,
        device: str = 'cpu',
        learning_rate: float = 0.001,
        num_classes: int = 4
    ):
        self.model = model.to(device)
        self.device = device
        self.num_classes = num_classes
        
        # Loss function (class weights for imbalanced data)
        self.criterion = nn.CrossEntropyLoss(weight=torch.tensor([1.0, 2.0, 3.0, 5.0]).to(device))
        
        # Optimizer
        self.optimizer = optim.Adam(model.parameters(), lr=learning_rate)
        
        # Learning rate scheduler
        self.scheduler = optim.lr_scheduler.ReduceLROnPlateau(
            self.optimizer, mode='min', factor=0.5, patience=5
        )
        
        # Training history
        self.history = {
            'train_loss': [],
            'train_acc': [],
            'val_loss': [],
            'val_acc': []
        }
    
    def train_epoch(self, dataloader: DataLoader) -> Tuple[float, float]:
        """Train for one epoch"""
        self.model.train()
        total_loss = 0.0
        correct = 0
        total = 0
        
        for batch in dataloader:
            sensor_data = batch['sensor_data'].to(self.device)
            vitals_data = batch['vitals_data'].to(self.device)
            user_ids = batch['user_id'].squeeze().to(self.device)
            labels = batch['label'].squeeze().to(self.device)
            
            # Forward pass
            self.optimizer.zero_grad()
            outputs = self.model(sensor_data, vitals_data, user_ids)
            loss = self.criterion(outputs, labels)
            
            # Backward pass
            loss.backward()
            self.optimizer.step()
            
            # Statistics
            total_loss += loss.item()
            _, predicted = torch.max(outputs.data, 1)
            total += labels.size(0)
            correct += (predicted == labels).sum().item()
        
        avg_loss = total_loss / len(dataloader)
        accuracy = 100.0 * correct / total
        
        return avg_loss, accuracy
    
    def validate(self, dataloader: DataLoader) -> Tuple[float, float]:
        """Validate the model"""
        self.model.eval()
        total_loss = 0.0
        correct = 0
        total = 0
        
        with torch.no_grad():
            for batch in dataloader:
                sensor_data = batch['sensor_data'].to(self.device)
                vitals_data = batch['vitals_data'].to(self.device)
                user_ids = batch['user_id'].squeeze().to(self.device)
                labels = batch['label'].squeeze().to(self.device)
                
                outputs = self.model(sensor_data, vitals_data, user_ids)
                loss = self.criterion(outputs, labels)
                
                total_loss += loss.item()
                _, predicted = torch.max(outputs.data, 1)
                total += labels.size(0)
                correct += (predicted == labels).sum().item()
        
        avg_loss = total_loss / len(dataloader)
        accuracy = 100.0 * correct / total
        
        return avg_loss, accuracy
    
    def train(
        self,
        train_loader: DataLoader,
        val_loader: DataLoader,
        epochs: int = 50,
        save_path: str = 'models/checkpoints'
    ) -> Dict:
        """Full training loop"""
        os.makedirs(save_path, exist_ok=True)
        best_val_acc = 0.0
        
        for epoch in range(epochs):
            train_loss, train_acc = self.train_epoch(train_loader)
            val_loss, val_acc = self.validate(val_loader)
            
            # Update history
            self.history['train_loss'].append(train_loss)
            self.history['train_acc'].append(train_acc)
            self.history['val_loss'].append(val_loss)
            self.history['val_acc'].append(val_acc)
            
            # Learning rate scheduling
            self.scheduler.step(val_loss)
            
            # Save best model
            if val_acc > best_val_acc:
                best_val_acc = val_acc
                torch.save({
                    'epoch': epoch,
                    'model_state_dict': self.model.state_dict(),
                    'optimizer_state_dict': self.optimizer.state_dict(),
                    'val_acc': val_acc,
                }, os.path.join(save_path, 'best_model.pth'))
            
            print(f"Epoch {epoch+1}/{epochs}")
            print(f"Train Loss: {train_loss:.4f}, Train Acc: {train_acc:.2f}%")
            print(f"Val Loss: {val_loss:.4f}, Val Acc: {val_acc:.2f}%")
            print("-" * 50)
        
        return self.history
    
    def fine_tune(
        self,
        user_data_loader: DataLoader,
        epochs: int = 10,
        learning_rate: float = 0.0001
    ) -> Dict:
        """Fine-tune model on specific user data"""
        # Lower learning rate for fine-tuning
        for param_group in self.optimizer.param_groups:
            param_group['lr'] = learning_rate
        
        history = {'train_loss': [], 'train_acc': []}
        
        for epoch in range(epochs):
            train_loss, train_acc = self.train_epoch(user_data_loader)
            history['train_loss'].append(train_loss)
            history['train_acc'].append(train_acc)
            
            print(f"Fine-tune Epoch {epoch+1}/{epochs}")
            print(f"Loss: {train_loss:.4f}, Acc: {train_acc:.2f}%")
        
        # Save personalized model
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        save_path = f"models/personalized/user_model_{timestamp}.pth"
        os.makedirs(os.path.dirname(save_path), exist_ok=True)
        torch.save({
            'model_state_dict': self.model.state_dict(),
            'history': history
        }, save_path)
        
        return history
    
    def save_model(self, path: str):
        """Save model checkpoint"""
        torch.save({
            'model_state_dict': self.model.state_dict(),
            'optimizer_state_dict': self.optimizer.state_dict(),
            'history': self.history
        }, path)
    
    def load_model(self, path: str):
        """Load model checkpoint"""
        checkpoint = torch.load(path, map_location=self.device)
        self.model.load_state_dict(checkpoint['model_state_dict'])
        self.optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        self.history = checkpoint.get('history', self.history)


def generate_dummy_data(num_samples: int = 1000) -> Dict:
    """Generate dummy training data for testing"""
    # Sensor data: (samples, channels=6, time_steps=50)
    sensor_data = np.random.randn(num_samples, 6, 50)
    
    # Vitals data: (samples, features=3)
    vitals_data = np.random.randn(num_samples, 3)
    
    # User IDs: (samples,)
    user_ids = np.random.randint(0, 100, num_samples)
    
    # Labels: (samples,) - 0: NORMAL, 1: SUDDEN_MOVEMENT, 2: PREFALL, 3: FALL
    labels = np.random.randint(0, 4, num_samples)
    
    return {
        'sensor_data': sensor_data,
        'vitals_data': vitals_data,
        'user_ids': user_ids,
        'labels': labels
    }


if __name__ == "__main__":
    # Test training pipeline
    device = 'cuda' if torch.cuda.is_available() else 'cpu'
    print(f"Using device: {device}")
    
    # Create model
    model = create_model(num_users=100, device=device)
    
    # Generate dummy data
    train_data = generate_dummy_data(800)
    val_data = generate_dummy_data(200)
    
    # Create datasets and dataloaders
    train_dataset = FallDetectionDataset(train_data)
    val_dataset = FallDetectionDataset(val_data)
    
    train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=32, shuffle=False)
    
    # Train
    trainer = Trainer(model, device=device)
    history = trainer.train(train_loader, val_loader, epochs=5)
    
    print("Training completed!")
    print(f"Best validation accuracy: {max(history['val_acc']):.2f}%")

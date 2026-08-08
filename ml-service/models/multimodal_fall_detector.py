"""
Multimodal Fall Detection Model
Combines accelerometer, gyroscope, heart rate, SpO2, and temperature data
with personalization layer for user-specific adaptation
"""

import torch
import torch.nn as nn
import torch.nn.functional as F
from typing import Dict, Tuple


class SensorEncoder(nn.Module):
    """CNN encoder for accelerometer and gyroscope data"""
    def __init__(self, input_channels: int = 6, hidden_dim: int = 128):
        super().__init__()
        self.conv1 = nn.Conv1d(input_channels, 64, kernel_size=3, padding=1)
        self.conv2 = nn.Conv1d(64, 128, kernel_size=3, padding=1)
        self.adaptive_pool = nn.AdaptiveAvgPool1d(1)
        self.dropout = nn.Dropout(0.2)
        
    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x shape: (batch, channels, time_steps)
        x = F.relu(self.conv1(x))
        x = F.relu(self.conv2(x))
        x = self.adaptive_pool(x)  # (batch, 128, 1)
        x = x.squeeze(2)  # (batch, 128)
        x = self.dropout(x)
        return x


class VitalsEncoder(nn.Module):
    """Dense encoder for vitals (heart rate, SpO2, temperature)"""
    def __init__(self, input_dim: int = 3, hidden_dim: int = 32):
        super().__init__()
        self.fc1 = nn.Linear(input_dim, hidden_dim)
        self.fc2 = nn.Linear(hidden_dim, hidden_dim)
        self.dropout = nn.Dropout(0.2)
        
    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x shape: (batch, input_dim)
        x = F.relu(self.fc1(x))
        x = self.dropout(x)
        x = F.relu(self.fc2(x))
        return x


class AttentionFusion(nn.Module):
    """Simple fusion of sensor and vitals features"""
    def __init__(self, sensor_dim: int = 128, vitals_dim: int = 32, output_dim: int = 128):
        super().__init__()
        self.sensor_proj = nn.Linear(sensor_dim, output_dim // 2)
        self.vitals_proj = nn.Linear(vitals_dim, output_dim // 2)
        self.fusion = nn.Linear(output_dim, output_dim)
        
    def forward(self, sensor_features: torch.Tensor, vitals_features: torch.Tensor) -> torch.Tensor:
        # Project to same dimension
        sensor_proj = self.sensor_proj(sensor_features)
        vitals_proj = self.vitals_proj(vitals_features)
        
        # Concatenate and fuse
        combined = torch.cat([sensor_proj, vitals_proj], dim=1)
        fused = self.fusion(combined)
        return fused


class TemporalModel(nn.Module):
    """Simple MLP for feature processing"""
    def __init__(self, input_dim: int = 128, hidden_dim: int = 128):
        super().__init__()
        self.fc1 = nn.Linear(input_dim, hidden_dim)
        self.fc2 = nn.Linear(hidden_dim, hidden_dim)
        self.dropout = nn.Dropout(0.2)
        
    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x shape: (batch, seq_len, input_dim) or (batch, input_dim)
        if x.dim() == 3:
            x = x.squeeze(1)  # Remove sequence dimension if present
        x = F.relu(self.fc1(x))
        x = self.dropout(x)
        x = F.relu(self.fc2(x))
        return x  # Return last hidden state


class PersonalizationLayer(nn.Module):
    """User-specific personalization layer"""
    def __init__(self, input_dim: int = 128, output_dim: int = 128, num_users: int = 1000):
        super().__init__()
        # User embedding
        self.user_embedding = nn.Embedding(num_users, input_dim)
        # Personalization weights
        self.personalization_fc = nn.Linear(input_dim * 2, output_dim)
        
    def forward(self, features: torch.Tensor, user_id: torch.Tensor) -> torch.Tensor:
        # Get user embedding
        user_emb = self.user_embedding(user_id)
        # Concatenate with features
        combined = torch.cat([features, user_emb], dim=-1)
        # Apply personalization
        personalized = self.personalization_fc(combined)
        return personalized


class MultimodalFallDetector(nn.Module):
    """Complete multimodal fall detection model"""
    def __init__(
        self,
        sensor_channels: int = 6,  # 3 accel + 3 gyro
        vitals_dim: int = 3,  # HR, SpO2, temp
        hidden_dim: int = 128,
        num_classes: int = 4,  # NORMAL, SUDDEN_MOVEMENT, PREFALL, FALL
        num_users: int = 1000
    ):
        super().__init__()
        self.sensor_encoder = SensorEncoder(sensor_channels, hidden_dim)
        self.vitals_encoder = VitalsEncoder(vitals_dim, hidden_dim // 4)
        self.fusion = AttentionFusion(hidden_dim, hidden_dim // 4, hidden_dim)
        self.temporal = TemporalModel(hidden_dim, hidden_dim)
        self.personalization = PersonalizationLayer(hidden_dim, hidden_dim, num_users)
        self.classifier = nn.Linear(hidden_dim, num_classes)
        
    def forward(
        self,
        sensor_data: torch.Tensor,
        vitals_data: torch.Tensor,
        user_id: torch.Tensor
    ) -> torch.Tensor:
        # Encode sensor data
        sensor_features = self.sensor_encoder(sensor_data)
        
        # Encode vitals
        vitals_features = self.vitals_encoder(vitals_data)
        
        # Fuse features
        fused_features = self.fusion(sensor_features, vitals_features)
        
        # Process through temporal model
        temporal_features = self.temporal(fused_features)
        
        # Personalize
        personalized_features = self.personalization(temporal_features, user_id)
        
        # Classify
        logits = self.classifier(personalized_features)
        return logits


def create_model(num_users: int = 1000, device: str = 'cpu') -> MultimodalFallDetector:
    """Create and initialize the model"""
    model = MultimodalFallDetector(num_users=num_users)
    model.to(device)
    return model


if __name__ == "__main__":
    # Test model
    model = create_model(num_users=10)
    
    # Dummy data
    batch_size = 4
    time_steps = 50
    sensor_data = torch.randn(batch_size, 6, time_steps)
    vitals_data = torch.randn(batch_size, 3)
    user_ids = torch.randint(0, 10, (batch_size,))
    
    # Forward pass
    output = model(sensor_data, vitals_data, user_ids)
    print(f"Model output shape: {output.shape}")
    print(f"Number of parameters: {sum(p.numel() for p in model.parameters())}")

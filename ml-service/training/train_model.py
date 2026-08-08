"""
Main training script for multimodal fall detection model
Loads datasets, preprocesses, trains base model, and saves checkpoints
"""

import torch
import numpy as np
import os
import sys
from torch.utils.data import DataLoader, random_split

# Add parent directory to path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from data.data_loader import DatasetLoader
from data.preprocessing import DataPipeline
from models.multimodal_fall_detector import create_model
from training.trainer import Trainer, FallDetectionDataset


def prepare_training_data(data_dir: str = 'data'):
    """Load and prepare training data from all datasets"""
    print("=" * 60)
    print("PREPARING TRAINING DATA")
    print("=" * 60)
    
    # Initialize loader and pipeline
    loader = DatasetLoader(data_dir=data_dir)
    pipeline = DataPipeline(window_size=50, sampling_rate=50)
    
    # Load all datasets
    uci_data = loader.load_uci_har()
    mobiact_data = loader.load_mobiact()
    sisfall_data = loader.load_sisfall()
    
    # Combine datasets
    combined = loader.combine_datasets([uci_data, mobiact_data, sisfall_data])
    
    if len(combined['sensor_data']) == 0:
        print("ERROR: No training data loaded!")
        return None, None, None
    
    print(f"\nCombined dataset statistics:")
    print(f"Total samples: {len(combined['sensor_data'])}")
    print(f"Label distribution: {np.bincount(combined['labels'])}")
    
    # Preprocess sensor data
    print("\nPreprocessing sensor data...")
    sensor_data_normalized = pipeline.sensor_preprocessor.normalize(combined['sensor_data'])
    
    # Generate synthetic vitals (will be replaced with real data later)
    print("Generating synthetic vitals data...")
    vitals_data = pipeline.generate_synthetic_vitals(
        len(combined['labels']), 
        combined['labels']
    )
    
    # Create dataset
    dataset = FallDetectionDataset({
        'sensor_data': sensor_data_normalized,
        'vitals_data': vitals_data,
        'user_ids': combined['user_ids'],
        'labels': combined['labels']
    })
    
    # Split into train/val/test (70/15/15)
    total_size = len(dataset)
    train_size = int(0.7 * total_size)
    val_size = int(0.15 * total_size)
    test_size = total_size - train_size - val_size
    
    train_dataset, val_dataset, test_dataset = random_split(
        dataset, [train_size, val_size, test_size],
        generator=torch.Generator().manual_seed(42)
    )
    
    print(f"\nData split:")
    print(f"Train: {len(train_dataset)} samples")
    print(f"Validation: {len(val_dataset)} samples")
    print(f"Test: {len(test_dataset)} samples")
    
    return train_dataset, val_dataset, test_dataset


def train_base_model(
    train_dataset,
    val_dataset,
    epochs: int = 50,
    batch_size: int = 32,
    device: str = 'cpu'
):
    """Train the base model on global datasets"""
    print("\n" + "=" * 60)
    print("TRAINING BASE MODEL")
    print("=" * 60)
    
    # Create dataloaders
    train_loader = DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=batch_size, shuffle=False)
    
    # Create model
    model = create_model(num_users=1000, device=device)
    
    # Create trainer
    trainer = Trainer(model, device=device, learning_rate=0.001)
    
    # Train
    print(f"\nStarting training for {epochs} epochs...")
    history = trainer.train(train_loader, val_loader, epochs=epochs)
    
    print("\n" + "=" * 60)
    print("TRAINING COMPLETED")
    print("=" * 60)
    print(f"Best validation accuracy: {max(history['val_acc']):.2f}%")
    print(f"Final training accuracy: {history['train_acc'][-1]:.2f}%")
    
    return model, history


def main():
    """Main training pipeline"""
    # Set device
    device = 'cuda' if torch.cuda.is_available() else 'cpu'
    print(f"Using device: {device}")
    
    # Prepare data
    train_dataset, val_dataset, test_dataset = prepare_training_data(data_dir='data')
    
    if train_dataset is None:
        print("Failed to prepare training data. Exiting.")
        return
    
    # Train model
    model, history = train_base_model(
        train_dataset,
        val_dataset,
        epochs=50,
        batch_size=32,
        device=device
    )
    
    # Save final model
    save_dir = 'models/checkpoints'
    os.makedirs(save_dir, exist_ok=True)
    torch.save({
        'model_state_dict': model.state_dict(),
        'history': history,
        'device': device
    }, os.path.join(save_dir, 'final_model.pth'))
    
    print(f"\nModel saved to {save_dir}/final_model.pth")
    print("Training pipeline complete!")


if __name__ == "__main__":
    main()

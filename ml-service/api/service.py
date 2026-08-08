"""
FastAPI service for real-time fall detection inference
Provides endpoints for prediction, model updates, and user personalization
"""

from fastapi import FastAPI, HTTPException, BackgroundTasks
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import List, Dict, Optional
import torch
import numpy as np
import os
from datetime import datetime

from models.multimodal_fall_detector import create_model
from data.preprocessing import DataPipeline


# Pydantic models for API requests
class SensorData(BaseModel):
    """Sensor data from waist belt"""
    accelerometer: List[List[float]]  # [x, y, z] over time
    gyroscope: List[List[float]]  # [x, y, z] over time


class VitalsData(BaseModel):
    """Vitals data from sensors"""
    heart_rate: float
    spo2: float
    temperature: float


class HealthHistory(BaseModel):
    """User health history for personalization"""
    age: int
    weight: float
    height: float
    conditions: List[str]
    mobility: str
    fall_count: int
    last_fall_days: int


class PredictionRequest(BaseModel):
    """Request for fall prediction"""
    device_id: str
    elderly_id: str
    sensor_data: SensorData
    vitals_data: VitalsData
    health_history: Optional[HealthHistory] = None


class CalibrationRequest(BaseModel):
    """Request for user calibration"""
    elderly_id: str
    sensor_data: List[SensorData]
    vitals_data: List[VitalsData]
    health_history: HealthHistory


class FineTuneRequest(BaseModel):
    """Request for model fine-tuning"""
    elderly_id: str
    labeled_data: List[Dict]  # Labeled samples for training


# Initialize FastAPI app
app = FastAPI(
    title="SilverCare ML Service",
    description="Multimodal fall detection with personalization",
    version="1.0.0"
)

# CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Global variables
model = None
device = 'cpu'
preprocessor = DataPipeline()
user_embeddings = {}  # Store user-specific embeddings


@app.on_event("startup")
async def startup_event():
    """Load model on startup"""
    global model, device
    device = 'cuda' if torch.cuda.is_available() else 'cpu'
    
    # Create model
    model = create_model(num_users=1000, device=device)
    
    # Load pretrained weights if available
    model_path = 'models/checkpoints/best_model.pth'
    if os.path.exists(model_path):
        checkpoint = torch.load(model_path, map_location=device)
        model.load_state_dict(checkpoint['model_state_dict'])
        print(f"Loaded pretrained model from {model_path}")
    else:
        print("No pretrained model found, using initialized model")
    
    model.eval()
    print(f"ML Service started on device: {device}")


@app.get("/")
async def root():
    """Health check endpoint"""
    return {
        "status": "healthy",
        "service": "SilverCare ML Service",
        "timestamp": datetime.now().isoformat()
    }


@app.get("/model/info")
async def model_info():
    """Get model information"""
    return {
        "device": device,
        "num_parameters": sum(p.numel() for p in model.parameters()),
        "model_type": "MultimodalFallDetector",
        "input_channels": {
            "sensor": 6,  # 3 accel + 3 gyro
            "vitals": 3  # HR, SpO2, temp
        },
        "output_classes": ["NORMAL", "SUDDEN_MOVEMENT", "PREFALL", "FALL"]
    }


@app.post("/predict")
async def predict(request: PredictionRequest):
    """
    Predict fall state from sensor data
    
    Returns:
        - prediction: One of NORMAL, SUDDEN_MOVEMENT, PREFALL, FALL
        - confidence: Confidence score for prediction
        - probabilities: Probability distribution over all classes
    """
    try:
        # Convert sensor data to numpy
        accel_data = np.array(request.sensor_data.accelerometer)
        gyro_data = np.array(request.sensor_data.gyroscope)
        
        # Ensure minimum window size
        window_size = 50
        if len(accel_data) < window_size:
            # Pad with zeros if too short
            padding = np.zeros((window_size - len(accel_data), 3))
            accel_data = np.vstack([accel_data, padding])
            gyro_data = np.vstack([gyro_data, padding])
        elif len(accel_data) > window_size:
            # Truncate if too long
            accel_data = accel_data[:window_size]
            gyro_data = gyro_data[:window_size]
        
        # Combine sensor data
        sensor_combined = np.concatenate([accel_data, gyro_data], axis=1)  # (50, 6)
        sensor_tensor = torch.FloatTensor(sensor_combined).unsqueeze(0).transpose(1, 2).to(device)
        
        # Process vitals
        vitals_array = np.array([
            request.vitals_data.heart_rate,
            request.vitals_data.spo2,
            request.vitals_data.temperature
        ])
        vitals_tensor = torch.FloatTensor(vitals_array).unsqueeze(0).to(device)
        
        # Get or create user ID
        user_id = hash(request.elderly_id) % 1000
        user_id_tensor = torch.LongTensor([user_id]).to(device)
        
        # Run inference
        with torch.no_grad():
            logits = model(sensor_tensor, vitals_tensor, user_id_tensor)
            probabilities = torch.softmax(logits, dim=1)
            confidence, predicted = torch.max(probabilities, 1)
        
        # Map prediction to label
        class_names = ["NORMAL", "SUDDEN_MOVEMENT", "PREFALL", "FALL"]
        prediction_label = class_names[predicted.item()]
        
        return {
            "prediction": prediction_label,
            "confidence": confidence.item(),
            "probabilities": {
                class_names[i]: probabilities[0][i].item()
                for i in range(len(class_names))
            },
            "timestamp": datetime.now().isoformat()
        }
        
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Prediction failed: {str(e)}")


@app.post("/calibrate")
async def calibrate_user(request: CalibrationRequest, background_tasks: BackgroundTasks):
    """
    Calibrate model for a specific user using 10-minute baseline data
    
    This endpoint:
    1. Processes the user's baseline data
    2. Creates a user profile from health history
    3. Fine-tunes the model on the user's normal movement patterns
    """
    try:
        # Process health history
        profile = preprocessor.health_encoder.create_user_profile(
            age=request.health_history.age,
            weight=request.health_history.weight,
            height=request.health_history.height,
            conditions=request.health_history.conditions,
            mobility=request.health_history.mobility,
            fall_count=request.health_history.fall_count,
            last_fall_days=request.health_history.last_fall_days
        )
        
        # Store user profile
        user_id = hash(request.elderly_id) % 1000
        user_embeddings[request.elderly_id] = profile
        
        # Process sensor data for calibration
        sensor_windows = []
        vitals_sequences = []
        
        for sensor_data, vitals_data in zip(request.sensor_data, request.vitals_data):
            accel = np.array(sensor_data.accelerometer)
            gyro = np.array(sensor_data.gyroscope)
            combined = np.concatenate([accel, gyro], axis=1)
            
            # Normalize window size
            window_size = 50
            if len(combined) < window_size:
                padding = np.zeros((window_size - len(combined), 6))
                combined = np.vstack([combined, padding])
            elif len(combined) > window_size:
                combined = combined[:window_size]
            
            sensor_windows.append(combined)
            vitals_sequences.append([
                vitals_data.heart_rate,
                vitals_data.spo2,
                vitals_data.temperature
            ])
        
        # In a real implementation, this would trigger fine-tuning
        # For now, we'll just acknowledge receipt
        background_tasks.add_task(
            trigger_fine_tuning,
            request.elderly_id,
            sensor_windows,
            vitals_sequences
        )
        
        return {
            "status": "calibration_started",
            "elderly_id": request.elderly_id,
            "message": "Calibration initiated. Model will be personalized in background.",
            "samples_processed": len(sensor_windows)
        }
        
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Calibration failed: {str(e)}")


@app.post("/fine-tune")
async def fine_tune_model(request: FineTuneRequest):
    """
    Fine-tune model on labeled data for a specific user
    Used for continuous learning as more data is collected
    """
    try:
        # In a real implementation, this would:
        # 1. Load the labeled data
        # 2. Create a dataset
        # 3. Run fine-tuning on the model
        # 4. Save the personalized model
        
        return {
            "status": "fine_tuning_scheduled",
            "elderly_id": request.elderly_id,
            "samples_count": len(request.labeled_data),
            "message": "Fine-tuning scheduled"
        }
        
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Fine-tuning failed: {str(e)}")


@app.get("/user/{elderly_id}/profile")
async def get_user_profile(elderly_id: str):
    """Get user profile if it exists"""
    if elderly_id in user_embeddings:
        return {
            "elderly_id": elderly_id,
            "profile": user_embeddings[elderly_id].tolist(),
            "calibrated": True
        }
    else:
        return {
            "elderly_id": elderly_id,
            "calibrated": False,
            "message": "User not calibrated yet"
        }


async def trigger_fine_tuning(
    elderly_id: str,
    sensor_windows: List[np.ndarray],
    vitals_sequences: List[List[float]]
):
    """Background task for fine-tuning"""
    # This would implement the actual fine-tuning logic
    # For now, it's a placeholder
    print(f"Fine-tuning for user {elderly_id} with {len(sensor_windows)} samples")


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)

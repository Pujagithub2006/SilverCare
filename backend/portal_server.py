from flask import Flask, request, jsonify
from flask_cors import CORS
import json
import threading
import time
from datetime import datetime
import sys
import os

# Add current directory to path for imports
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

# Initialize Flask app
portal_app = Flask(__name__)
CORS(portal_app)

# Global storage for latest sensor data
latest_sensor_data = {
    "status": "no_data",
    "data": None,
    "last_update": None,
    "device_connected": False
}

# Device connection tracking
connected_devices = {}

# Hardware-Software Integration Server
@portal_app.route("/api/sensor-data", methods=["POST"])
def receive_sensor_data():
    """Receive sensor data from ESP32 hardware"""
    try:
        # Get JSON data from ESP32
        data = request.get_json()
        
        if not data:
            return jsonify({"status": "error", "message": "No data received"}), 400
        
        # Extract sensor values
        device_id = data.get("deviceId", "unknown")
        state = data.get("state", 0)
        state_name = data.get("stateName", "UNKNOWN")
        heart_rate = data.get("heartRate", 0)
        spo2 = data.get("spo2", 0)
        temperature = data.get("temperature", 0)
        belt_worn = data.get("beltWorn", False)
        acceleration = data.get("acceleration", 0)
        timestamp = data.get("timestamp", int(time.time() * 1000))
        
        # Store latest data
        sensor_data = {
            "deviceId": device_id,
            "state": state,
            "stateName": state_name,
            "heartRate": heart_rate,
            "spo2": spo2,
            "temperature": temperature,
            "beltWorn": belt_worn,
            "acceleration": acceleration,
            "timestamp": timestamp,
            "received_at": datetime.now().isoformat()
        }
        
        # Update global storage
        latest_sensor_data["status"] = "success"
        latest_sensor_data["data"] = sensor_data
        latest_sensor_data["last_update"] = datetime.now().isoformat()
        latest_sensor_data["device_connected"] = True
        
        # Track device connection
        connected_devices[device_id] = {
            "last_seen": datetime.now().isoformat(),
            "state": state_name,
            "belt_worn": belt_worn
        }
        
        # Log received data
        print(f"📡 [HARDWARE] Data received from {device_id}:")
        print(f"   State: {state_name}")
        print(f"   Heart Rate: {heart_rate} BPM")
        print(f"   SpO2: {spo2}%")
        print(f"   Temperature: {temperature}°C")
        print(f"   Belt Worn: {belt_worn}")
        print(f"   Acceleration: {acceleration}G")
        
        # Check for fall detection and integrate with main system
        if state_name == "FALL_DETECTED":
            print(f"🚨 [FALL DETECTED] from {device_id}!")
            # Import and trigger fall detection from main system
            try:
                from fall_detection import notify_guardian_fall
                # Trigger fall detection workflow
                notify_guardian_fall(device_id, sensor_data)
                print(f"📞 [ALERT] Fall detection system notified for {device_id}")
            except ImportError:
                print("⚠️ Fall detection module not available - basic alert only")
            except Exception as e:
                print(f"❌ Error in fall detection: {e}")
        
        # Return success response
        response = {
            "status": "success",
            "message": "Data received successfully",
            "timestamp": datetime.now().isoformat()
        }
        
        print(f"✅ [SERVER] Response sent to {device_id}")
        return jsonify(response), 200
        
    except Exception as e:
        print(f"❌ [ERROR] receiving sensor data: {e}")
        return jsonify({
            "status": "error", 
            "message": str(e)
        }), 500

@portal_app.route("/api/sensor-data", methods=["GET"])
def get_sensor_data():
    """Provide latest sensor data to frontend"""
    try:
        # Check if device is still connected (within last 10 seconds)
        if latest_sensor_data["last_update"]:
            last_update = datetime.fromisoformat(latest_sensor_data["last_update"])
            time_diff = (datetime.now() - last_update).total_seconds()
            
            if time_diff > 10:  # Device considered disconnected after 10 seconds
                latest_sensor_data["device_connected"] = False
                print(f"⚠️ [CONNECTION] Device timeout after {time_diff:.1f} seconds")
        
        return jsonify(latest_sensor_data)
        
    except Exception as e:
        print(f"❌ [ERROR] getting sensor data: {e}")
        return jsonify({
            "status": "error",
            "message": str(e),
            "data": None
        }), 500

@portal_app.route("/api/device-status", methods=["GET"])
def get_device_status():
    """Get status of all connected devices"""
    try:
        # Clean up old devices (not seen for 30 seconds)
        current_time = datetime.now()
        active_devices = {}
        
        for device_id, device_info in connected_devices.items():
            last_seen = datetime.fromisoformat(device_info["last_seen"])
            time_diff = (current_time - last_seen).total_seconds()
            
            if time_diff <= 30:  # Device considered active for 30 seconds
                active_devices[device_id] = {
                    **device_info,
                    "connection_status": "online" if time_diff <= 10 else "stale",
                    "last_seen_seconds": int(time_diff)
                }
        
        return jsonify({
            "status": "success",
            "devices": active_devices,
            "total_devices": len(active_devices)
        })
        
    except Exception as e:
        print(f"❌ [ERROR] getting device status: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@portal_app.route("/api/twilio-sms", methods=["POST"])
def handle_twilio_sms():
    """Handle Twilio SMS request from ESP32"""
    try:
        data = request.get_json()
        alert_type = data.get("alert_type", "UNKNOWN")
        message = data.get("message", "")
        device_id = data.get("device_id", "unknown")
        
        print(f"📡 [TWILIO] SMS request from {device_id}")
        print(f"   Alert Type: {alert_type}")
        print(f"   Message: {message}")
        
        # Import and use Twilio service
        try:
            from twilio_service import TwilioService
            twilio_service = TwilioService()
            
            # Get guardian phone number
            from fall_detection import get_guardian_phone_for_elderly
            guardian_phone = get_guardian_phone_for_elderly(device_id)
            
            if guardian_phone:
                # Send SMS via Twilio
                success = twilio_service.send_fall_alert_sms(
                    guardian_phone, 
                    device_id, 
                    "Home", 
                    message
                )
                
                if success:
                    print(f"✅ [TWILIO] SMS sent to {guardian_phone}")
                    return jsonify({
                        "status": "success",
                        "message": "Twilio SMS sent successfully",
                        "phone": guardian_phone
                    }), 200
                else:
                    print(f"❌ [TWILIO] SMS failed to {guardian_phone}")
                    return jsonify({
                        "status": "error",
                        "message": "Twilio SMS failed"
                    }), 500
            else:
                print(f"❌ [TWILIO] No guardian phone found for {device_id}")
                return jsonify({
                    "status": "error",
                    "message": "No guardian phone found"
                }), 404
                
        except ImportError:
            print("❌ [TWILIO] Twilio service not available")
            return jsonify({
                "status": "error",
                "message": "Twilio service not available"
            }), 503
        except Exception as e:
            print(f"❌ [TWILIO] Error: {e}")
            return jsonify({
                "status": "error",
                "message": str(e)
            }), 500
            
    except Exception as e:
        print(f"❌ [TWILIO] Request error: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 400

@portal_app.route("/api/fall-alert", methods=["POST"])
def trigger_fall_alert_endpoint():
    """Manual fall alert trigger for testing"""
    try:
        data = request.get_json()
        device_id = data.get("deviceId", "test_device")
        
        # Create test fall data
        test_fall_data = {
            "deviceId": device_id,
            "state": 3,
            "stateName": "FALL_DETECTED",
            "heartRate": 75,
            "spo2": 98,
            "temperature": 36.5,
            "beltWorn": True,
            "acceleration": 2.5,
            "timestamp": int(time.time() * 1000),
            "received_at": datetime.now().isoformat(),
            "test_mode": True
        }
        
        trigger_fall_alert(device_id, test_fall_data)
        
        return jsonify({
            "status": "success",
            "message": f"Fall alert triggered for {device_id}"
        }), 200
        
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

def trigger_fall_alert(device_id, sensor_data):
    """Trigger fall detection workflow"""
    try:
        print(f"🚨 [FALL ALERT] Processing fall for {device_id}")
        
        # Here you can integrate with existing fall detection system
        # For now, we'll just log and could send notifications
        
        # Update sensor data with fall flag
        if latest_sensor_data["data"]:
            latest_sensor_data["data"]["fall_detected"] = True
            latest_sensor_data["data"]["fall_time"] = datetime.now().isoformat()
        
        print(f"📞 [ALERT] Fall detection workflow triggered for {device_id}")
        print(f"   Location: Home (could be enhanced with GPS)")
        print(f"   Guardian: Will be notified (integrate with existing system)")
        
        # You could integrate with existing fall_detection.py here
        # For example: import fall_detection; fall_detection.notify_guardian_fall(device_id)
        
    except Exception as e:
        print(f"❌ [ERROR] in fall alert: {e}")

@portal_app.route("/", methods=["GET"])
def home():
    """Server info and status"""
    return jsonify({
        "status": "success",
        "message": "SilverCare Hardware Integration Server",
        "version": "1.0.0",
        "endpoints": {
            "sensor_data_post": "POST /api/sensor-data (ESP32 sends data)",
            "sensor_data_get": "GET /api/sensor-data (Frontend gets data)",
            "device_status": "GET /api/device-status (Device connection status)",
            "fall_alert": "POST /api/fall-alert (Manual fall trigger)"
        },
        "hardware_integration": {
            "esp32_support": "✅",
            "real_time_data": "✅",
            "fall_detection": "✅",
            "device_tracking": "✅"
        },
        "current_status": latest_sensor_data
    }), 200

def start_background_monitoring():
    """Background thread to monitor device connections"""
    def monitor():
        while True:
            try:
                # Check device connections every 5 seconds
                current_time = datetime.now()
                
                for device_id in list(connected_devices.keys()):
                    device_info = connected_devices[device_id]
                    last_seen = datetime.fromisoformat(device_info["last_seen"])
                    time_diff = (current_time - last_seen).total_seconds()
                    
                    if time_diff > 30:  # Remove device after 30 seconds of inactivity
                        del connected_devices[device_id]
                        print(f"📱 [DEVICE] {device_id} removed (inactive for {time_diff:.1f}s)")
                
                time.sleep(5)
                
            except Exception as e:
                print(f"❌ [MONITOR] Error: {e}")
                time.sleep(5)
    
    # Start background thread
    monitor_thread = threading.Thread(target=monitor, daemon=True)
    monitor_thread.start()
    print("🔄 [MONITOR] Background device monitoring started")

if __name__ == "__main__":
    print("🚀 [SERVER] Starting SilverCare Hardware Integration Server")
    print("📡 [HARDWARE] Ready to receive ESP32 sensor data")
    print("🌐 [WEB] Frontend can fetch real-time data")
    print("🔗 [INTEGRATION] Hardware-Software bridge active")
    
    # Start background monitoring
    start_background_monitoring()
    
    # Run server
    portal_app.run(
        host="0.0.0.0", 
        port=5002, 
        debug=True,
        threaded=True
    )

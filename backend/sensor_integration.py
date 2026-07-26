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

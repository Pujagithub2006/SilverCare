from flask import Flask, Blueprint, request, jsonify, send_from_directory
import json
import os
from datetime import datetime
from flask_cors import CORS

from fall_detection import fall_detection_bp
from guardian_auth import guardian_auth_bp
from elderly_management import elderly_management_bp
from medicine_management import medicine_bp
from medicine_notifications import start_medicine_notifications
from medicine_reminder_system import start_medicine_reminder_system, handle_medicine_response
from elderly_notifications import register_elderly_session, unregister_elderly_session, send_elderly_notification

main_app = Flask(__name__)
CORS(main_app)

main_app.register_blueprint(fall_detection_bp)
main_app.register_blueprint(guardian_auth_bp)
main_app.register_blueprint(elderly_management_bp)
main_app.register_blueprint(medicine_bp)

# Try to import genai, but don't fail if not available
try:
    import google.generativeai as genai
    API_KEY = "AIzaSyBSdKD5Flk7zfOgnjOuw9cWgtKIISePz5Y"
    genai.configure(api_key=API_KEY)
    GENAI_AVAILABLE = True
    print("✅ Google Generative AI available")
except ImportError:
    GENAI_AVAILABLE = False
    print("⚠️ Google Generative AI not available - chatbot will use fallback")
except Exception as e:
    GENAI_AVAILABLE = False
    print(f"⚠️ Google Generative AI error: {e} - chatbot will use fallback")


@main_app.route("/chat", methods=["POST"])
def chat():
    print("--- MESSAGE RECEIVED ---")
    data = request.get_json()
    user_message = data.get("message", "")
    print(f"User said: {user_message}")

    if not GENAI_AVAILABLE:
        return jsonify({"reply": "I'm here to help! How can I assist you today?"})

    model_options = ["gemini-2.5-flash", "gemini-2.0-flash", "gemini-1.5-flash-8b"]
    for model_name in model_options:
        try:
            print(f"Trying model: {model_name}...")
            model = genai.GenerativeModel(model_name)
            response = model.generate_content(user_message)
            print(f"Success with {model_name}!")
            return jsonify({"reply": response.text})
        except Exception as e:
            print(f"Failed {model_name}: {e}")
            continue 

    return jsonify({"reply": "I'm right here. I'm just organizing my thoughts. How can I help you feel better?"})


@main_app.route("/", methods=["GET"])
def home():
    print("🚀 Starting SilverCare Backend")
    return {
        "status": "success",
        "message": "SilverCare Main Backend",
        "endpoints": {
            "guardian": {
                "register": "POST /guardian-register",
                "login": "POST /guardian-login",
                "info": "GET /guardian-info/<username>",
                "update": "POST /guardian-update",
                "elderly": "GET /guardian-elderly/<username>"
            },
            "elderly": {
                "register": "POST /elderly-register",
                "info": "GET /elderly-info/<elderly_id>",
                "update": "POST /elderly-update",
                "by_guardian": "GET /guardian-elderly/<username>"
            },
            "fall_detection": {
                "detect": "POST /detect-fall",
                "status": "GET /fall-status",
                "clear": "POST /clear-fall",
                "notify_fall": "POST /notify-guardian-fall",
                "notify_no_response": "POST /notify-guardian-no-response",
                "notify_safe": "POST /notify-guardian-safe"
            },
            "chatbot": {
                "chat": "POST /chat"
            },
            "medicine_management": {
                "add_medicine": "POST /medicine/add",
                "get_medicines": "GET /medicines/<elderly_id>",
                "confirm_medicine": "POST /medicine/confirm",
                "manage_suggestions": "GET/POST /medicine/suggestions/<elderly_id>"
            }
        }
    }, 200

@main_app.route("/frontend/<path:filename>")
def serve_frontend(filename):
    """Serve frontend files"""
    return send_from_directory('../frontend', filename)

@main_app.route("/hardware-data/<elderly_id>", methods=["GET"])
def get_hardware_data(elderly_id):
    """Get hardware data for elderly member"""
    try:
        # Return real data structure - currently no hardware connected
        return jsonify({
            "status": "success",
            "data": {
                "heartRate": 0,
                "oxygenLevel": 0,
                "temperature": 0,
                "beltConnected": False,
                "beltLastSeen": None,
                "lastUpdate": datetime.now().isoformat(),
                "message": "No hardware connected"
            }
        }), 200
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@main_app.route("/sensor-data", methods=["GET"])
def get_sensor_data():
    """Get sensor data - compatibility endpoint"""
    try:
        return jsonify({
            "status": "success",
            "data": {
                "deviceId": "no_device",
                "heartRate": 0,
                "spo2": 0,
                "temperature": 0,
                "beltWorn": False,
                "stateName": "normal",
                "message": "No hardware connected"
            }
        }), 200
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@main_app.route("/medicine-reminder-response", methods=["POST"])
def medicine_reminder_response():
    """Handle response to medicine reminder"""
    try:
        data = request.get_json()
        elderly_id = data.get('elderly_id')
        medicine_id = data.get('medicine_id')
        response = data.get('response')  # 'taken', 'snooze', 'not_taken'
        
        if not all([elderly_id, medicine_id, response]):
            return jsonify({
                "status": "error",
                "message": "Missing required fields"
            }), 400
        
        # Handle the response
        handle_medicine_response(elderly_id, medicine_id, response)
        
        return jsonify({
            "status": "success",
            "message": f"Response '{response}' recorded successfully"
        }), 200
        
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@main_app.route("/active-notifications", methods=["GET"])
def get_active_notifications():
    """Get active medicine notifications"""
    try:
        notifications_file = 'data/active_notifications.json'
        
        if os.path.exists(notifications_file):
            with open(notifications_file, 'r') as f:
                notifications = json.load(f)
        else:
            notifications = {}
        
        return jsonify({
            "status": "success",
            "notifications": notifications
        }), 200
        
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@main_app.route("/manifest.json")
def serve_manifest():
    """Serve PWA manifest file"""
    try:
        return send_from_directory('../frontend', 'manifest.json'), 200
    except Exception as e:
        return jsonify({"error": str(e)}), 404

@main_app.route("/elderly/login", methods=["POST"])
def elderly_login():
    """Login elderly user by phone number"""
    try:
        data = request.get_json()
        phone = data.get('phone', '').strip()
        name = data.get('name', '').strip()
        
        if not phone or not name:
            return jsonify({
                "status": "error",
                "message": "Phone number and name are required"
            }), 400
        
        # Load elderly data
        from utils.auth import load_elderly
        elderly_data = load_elderly()
        
        # Find elderly by phone and name
        elderly_found = None
        for elderly_id, elderly_info in elderly_data.items():
            if (elderly_info.get('phone') == phone and 
                elderly_info.get('name', '').lower() == name.lower()):
                elderly_found = elderly_info
                elderly_found['elderly_id'] = elderly_id
                break
        
        if not elderly_found:
            return jsonify({
                "status": "error",
                "message": "Elderly not found. Please check your name and phone number."
            }), 404
        
        return jsonify({
            "status": "success",
            "message": "Login successful",
            "elderly_id": elderly_found['elderly_id'],
            "name": elderly_found['name'],
            "guardian_username": elderly_found['guardian_username']
        }), 200
        
    except Exception as e:
        return jsonify({
            "status": "error", 
            "message": str(e)
        }), 500

@main_app.route("/elderly/register-session", methods=["POST"])
def register_elderly_session_endpoint():
    """Register elderly user session"""
    try:
        data = request.get_json()
        elderly_id = data.get('elderly_id')
        device_info = data.get('device_info', 'unknown_device')
        
        if not elderly_id:
            return jsonify({
                "status": "error",
                "message": "Missing elderly_id"
            }), 400
        
        # Use global instance directly
        from elderly_notifications import elderly_notification_system
        elderly_notification_system.register_elderly_session(elderly_id, device_info)
        
        return jsonify({
            "status": "success",
            "message": "Session registered successfully"
        }), 200
        
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@main_app.route("/elderly/unregister-session", methods=["POST"])
def unregister_elderly_session_endpoint():
    """Unregister elderly user session"""
    try:
        data = request.get_json()
        elderly_id = data.get('elderly_id')
        
        if not elderly_id:
            return jsonify({
                "status": "error",
                "message": "Missing elderly_id"
            }), 400
        
        # Use global instance directly
        from elderly_notifications import elderly_notification_system
        elderly_notification_system.unregister_elderly_session(elderly_id)
        
        return jsonify({
            "status": "success",
            "message": "Session unregistered successfully"
        }), 200
        
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@main_app.route("/elderly/notifications/<elderly_id>", methods=["GET"])
def get_elderly_notifications(elderly_id):
    """Get notifications for elderly user"""
    try:
        # Use global instance directly
        from elderly_notifications import elderly_notification_system
        notifications = elderly_notification_system.get_elderly_notifications(elderly_id)
        
        return jsonify({
            "status": "success",
            "notifications": notifications
        }), 200
        
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@main_app.route("/elderly/clear-notification", methods=["POST"])
def clear_elderly_notification():
    """Clear elderly notification after response"""
    try:
        data = request.get_json()
        elderly_id = data.get('elderly_id')
        medicine_id = data.get('medicine_id')
        response = data.get('response')  # 'taken', 'snooze', 'not_taken'
        
        if not all([elderly_id, medicine_id, response]):
            return jsonify({
                "status": "error",
                "message": "Missing required fields"
            }), 400
        
        from elderly_notifications import elderly_notification_system
        
        # Handle medicine response logic first
        handle_medicine_response(elderly_id, medicine_id, response)
        
        # Only clear notification for taken/not_taken, NOT snooze
        if response != 'snooze':
            from elderly_notifications import elderly_notification_system
            elderly_notification_system.clear_elderly_notification(elderly_id, medicine_id)
        
        return jsonify({
            "status": "success",
            "message": f"Response '{response}' recorded successfully"
        }), 200
        
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

if __name__ == "__main__":
    # Start medicine notification systems
    start_medicine_notifications()
    start_medicine_reminder_system()
    main_app.run(host='0.0.0.0', port=5001, debug=True)

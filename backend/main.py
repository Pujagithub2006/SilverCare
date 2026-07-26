from flask import Flask, Blueprint, request, jsonify, send_from_directory, session
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
from twilio_service import twilio_service

main_app = Flask(__name__)
main_app.secret_key = "vois_senior_safety_2024"  # Required for session persistence
CORS(main_app)

# Log ALL incoming requests
@main_app.before_request
def log_request():
    print(f"🌐 [REQUEST] {request.method} {request.path} from {request.remote_addr}")
    if request.is_json:
        print(f"📦 [REQUEST BODY] {request.get_json()}")

main_app.register_blueprint(fall_detection_bp)
main_app.register_blueprint(guardian_auth_bp)
main_app.register_blueprint(elderly_management_bp)
main_app.register_blueprint(medicine_bp)

# Try to import genai, but don't fail if not available
try:
    import google.generativeai as genai
    API_KEY = "AIzaSyAFlAqPTl8lX3wL7tTQ5hMMANDQApxqrV0"
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

# @main_app.route("/hardware-data/<elderly_id>", methods=["GET"])
# def get_hardware_data(elderly_id):
#     """Get hardware data for elderly member"""
#     try:
#         # Return real data structure - currently no hardware connected
#         return jsonify({
#             "status": "success",
#             "data": {
#                 "heartRate": 0,
#                 "oxygenLevel": 0,
#                 "temperature": 0,
#                 "beltConnected": False,
#                 "beltLastSeen": None,
#                 "lastUpdate": datetime.now().isoformat(),
#                 "message": "No hardware connected"
#             }
#         }), 200
#     except Exception as e:
#         return jsonify({
#             "status": "error",
#             "message": str(e)
#         }), 500

@main_app.route("/hardware-data/<elderly_id>", methods=["GET"])
def get_hardware_data(elderly_id):
    """Return real hardware data for elderly"""

    device_id_to_use = "isha_amit"
    # In your system device_id == elderly_id
    data = latest_sensor_data.get(device_id_to_use)

    if not data:
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

    return jsonify({
        "status": "success",
        "data": {
            "heartRate": data.get("heartRate"),
            "oxygenLevel": data.get("spo2"),
            "temperature": data.get("temperature"),
            "beltConnected": data.get("beltWorn"),
            "beltLastSeen": data.get("timestamp"),
            "lastUpdate": datetime.now().isoformat()
        }
    }), 200


@main_app.route("/sensor-data", methods=["GET"])
def get_sensor_data():
    """Get sensor data - compatibility endpoint"""
    try:
        return jsonify({
            "status": "success",
            "data": {
                "heartRate": latest_sensor_data.get("heartRate"),
                "oxygenLevel": latest_sensor_data.get("spo2"),
                "temperature": latest_sensor_data.get("temperature"),
                "beltConnected": latest_sensor_data.get("beltWorn"),
                "beltLastSeen": latest_sensor_data.get("timestamp")
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

# Store latest hardware data in memory
latest_sensor_data = {}

@main_app.route("/api/sensor-data", methods=["POST"])
def receive_sensor_data():
    try:
        data = request.get_json()

        device_id = data.get("deviceId")

        if not device_id:
            return jsonify({
                "status": "error",
                "message": "deviceId required"
            }), 400

        latest_sensor_data[device_id] = {
            "state": data.get("state"),
            "stateName": data.get("stateName"),
            "heartRate": data.get("heartRate"),
            "spo2": data.get("spo2"),
            "temperature": data.get("temperature"),
            "beltWorn": data.get("beltWorn"),
            "acceleration": data.get("acceleration"),
            "timestamp": datetime.now().isoformat()
        }

        print("📡 Arduino Data Received:")
        print(latest_sensor_data[device_id])

        return jsonify({
            "status": "success",
            "message": "Data received"
        }), 200

    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@main_app.route("/api/sensor-data/<device_id>", methods=["GET"])
def get_latest_sensor_data(device_id):
    data = latest_sensor_data.get(device_id)

    if not data:
        return jsonify({
            "status": "success",
            "data": None
        }), 200

    return jsonify({
        "status": "success",
        "data": data
    }), 200

# SMS and Call Alert Endpoints
@main_app.route("/debug-session", methods=["GET"])
def debug_session():
    """Debug endpoint to check session data"""
    return jsonify({
        "guardian_phone": session.get('guardian_phone'),
        "guardian_username": session.get('guardian_username'),
        "session_data": dict(session)
    })

@main_app.route("/test-session", methods=["GET"])
def test_session():
    """Temporary test endpoint to set session"""
    session['guardian_phone'] = "9011443024"
    session['guardian_username'] = "vaishn"
    session['guardian_name'] = "Vaishnavi"
    return jsonify({
        "status": "success",
        "message": "Test session created",
        "session_data": dict(session)
    })

@main_app.route("/test-twilio", methods=["GET"])
def test_twilio():
    """Direct Twilio test without session"""
    try:
        # Test Twilio directly
        result = twilio_service.send_prefall_alert_sms(
            guardian_phone="9011443024",
            elderly_name="Test User",
            location="Test Location", 
            device_id="test_device"
        )
        return jsonify({
            "status": "success",
            "message": "Twilio test completed",
            "result": result
        })
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": str(e)
        })

@main_app.route("/send-prefall-sms", methods=["POST"])
def send_prefall_sms():
    """Send SMS alert for prefall detection"""
    print("🚨 [PREFALL] send_prefall_sms() called!")
    
    try:
        data = request.get_json()
        elderly_name = data.get('elderly_name', 'Elderly Person')
        location = data.get('location', 'Unknown Location')
        
        print(f"🚨 [PREFALL] Elderly: {elderly_name}")
        print(f"📍 [PREFALL] Location: {location}")
        
        # Get latest guardian phone instead of session
        print("🔍 [PREFALL] Getting latest guardian...")
        from fall_detection import get_guardian_phone_for_elderly
        guardian_phone = get_guardian_phone_for_elderly()
        
        if not guardian_phone:
            print("❌ [PREFALL] No guardian found!")
            return jsonify({
                "status": "error",
                "message": "No guardian found"
            }), 401
        
        print(f"📱 [PREFALL] Guardian phone: {guardian_phone}")
        
        # Send prefall SMS using Twilio
        print("📤 [PREFALL] Sending prefall SMS...")
        success = twilio_service.send_prefall_alert_sms(
            guardian_phone, 
            elderly_name, 
            location, 
            device_id="vois_belt"
        )
        
        if success:
            print(f"✅ [PREFALL] Prefall alert sent to {guardian_phone}")
            return jsonify({
                "status": "success",
                "message": "Prefall SMS sent successfully"
            }), 200
        else:
            print(f"❌ [PREFALL] Failed to send prefall SMS to {guardian_phone}")
            return jsonify({
                "status": "error",
                "message": "Failed to send prefall SMS"
            }), 500
            
    except Exception as e:
        print(f"❌ [PREFALL] Error sending prefall SMS: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@main_app.route("/send-fall-alert", methods=["POST"])
def send_fall_alert():
    """Send SMS and make call for fall detection"""
    try:
        data = request.get_json()
        elderly_name = data.get('elderly_name', 'Elderly Person')
        location = data.get('location', 'Unknown Location')
        device_id = data.get('device_id', 'Unknown Device')
        
        # Get logged-in guardian phone from session
        guardian_phone = session.get('guardian_phone')
        if not guardian_phone:
            return jsonify({
                "status": "error",
                "message": "No guardian logged in"
            }), 401
        
        # Send fall alert SMS
        sms_success = twilio_service.send_fall_alert_sms(
            guardian_phone, 
            elderly_name, 
            location, 
            device_id
        )
        
        # Make emergency call
        call_success = twilio_service.make_emergency_call(
            guardian_phone, 
            elderly_name, 
            location
        )
        
        if sms_success and call_success:
            print(f"✅ [ALERT] Fall alert (SMS + Call) sent to {guardian_phone}")
            return jsonify({
                "status": "success",
                "message": "Fall alert SMS and call sent successfully"
            }), 200
        else:
            return jsonify({
                "status": "error",
                "message": "Failed to send fall alert"
            }), 500
            
    except Exception as e:
        print(f"❌ [ALERT] Error sending fall alert: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

if __name__ == '__main__':
    main_app.run(host='0.0.0.0', port=5001, debug=True)

@main_app.route("/send-urgent-fall-alert", methods=["POST"])
def send_urgent_fall_alert():
    """Send urgent SMS and call when no response to fall alert"""
    try:
        data = request.get_json()
        guardian_phone = data.get('guardian_phone')
        elderly_name = data.get('elderly_name', 'Elderly Person')
        location = data.get('location', 'Unknown Location')
        device_id = data.get('device_id', 'Unknown Device')
        
        if not guardian_phone:
            return jsonify({
                "status": "error",
                "message": "Guardian phone number required"
            }), 400
        
        # Send urgent SMS
        sms_success = twilio_service.send_urgent_alert_sms(
            guardian_phone, 
            elderly_name, 
            location, 
            device_id
        )
        
        # Make urgent call with siren
        call_success = twilio_service.make_no_response_alert_call(
            guardian_phone, 
            elderly_name, 
            location
        )
        
        if sms_success and call_success:
            print(f"🚨 [URGENT] Urgent fall alert sent to {guardian_phone}")
            return jsonify({
                "status": "success",
                "message": "Urgent fall alert sent successfully"
            }), 200
        else:
            return jsonify({
                "status": "error",
                "message": "Failed to send urgent fall alert"
            }), 500
            
    except Exception as e:
        print(f"❌ [URGENT] Error sending urgent fall alert: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@main_app.route("/send-safe-confirmation", methods=["POST"])
def send_safe_confirmation():
    """Send confirmation that elderly is safe (false alarm)"""
    try:
        data = request.get_json()
        guardian_phone = data.get('guardian_phone')
        elderly_name = data.get('elderly_name', 'Elderly Person')
        
        if not guardian_phone:
            return jsonify({
                "status": "error",
                "message": "Guardian phone number required"
            }), 400
        
        # Make safe confirmation call
        success = twilio_service.make_safe_confirmation_call(
            guardian_phone, 
            elderly_name
        )
        
        if success:
            print(f"✅ [SAFE] Safe confirmation sent to {guardian_phone}")
            return jsonify({
                "status": "success",
                "message": "Safe confirmation sent successfully"
            }), 200
        else:
            return jsonify({
                "status": "error",
                "message": "Failed to send safe confirmation"
            }), 500
            
    except Exception as e:
        print(f"❌ [SAFE] Error sending safe confirmation: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500


if __name__ == "__main__":
    # Start medicine notification systems
    start_medicine_notifications()
    start_medicine_reminder_system()
    main_app.run(host='0.0.0.0', port=5001, debug=True, threaded=True)

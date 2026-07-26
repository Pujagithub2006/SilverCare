
from flask import Blueprint, request, jsonify
from twilio_service import twilio_service
from utils.auth import get_guardian
import json
import os

# Create Blueprint for fall detection routes
fall_detection_bp = Blueprint('fall_detection', __name__)

# Store fall detection state
fall_detected = False

def get_guardian_phone_for_elderly():
    """Get phone number of latest registered guardian (simple approach)"""
    print(" [DEBUG] get_guardian_phone_for_elderly() called")
    try:
        # Load guardians data
        guardians_file = 'data/guardians.json'
        print(f" [DEBUG] Loading guardians from: {guardians_file}")
        
        if os.path.exists(guardians_file):
            with open(guardians_file, 'r') as f:
                guardians_data = json.load(f)
            print(f" [DEBUG] Loaded {len(guardians_data)} guardians")
        
        # Find latest registered guardian (simple: just get first one)
        if guardians_data:
            # Get most recently created guardian
            latest_guardian = None
            latest_time = None
            
            for username, guardian_info in guardians_data.items():
                created_at = guardian_info.get('created_at', '')
                print(f" [DEBUG] Checking guardian: {username}, created: {created_at}")
                
                if created_at and (latest_time is None or created_at > latest_time):
                    latest_time = created_at
                    latest_guardian = guardian_info
            
            if latest_guardian:
                phone = latest_guardian.get('phone')
                guardian_name = latest_guardian.get('name', 'Unknown')
                print(f" [SUCCESS] Latest guardian found: {guardian_name} ({phone})")
                print(f" [SUCCESS] Created at: {latest_time}")
                return phone
            else:
                print(" [ERROR] No latest guardian found!")
        
        print(" [ERROR] No guardians data found!")
        return None
    except Exception as e:
        print(f" [ERROR] Error getting latest guardian phone: {e}")
        return None


@fall_detection_bp.route("/detect-fall", methods=["POST"])
def detect_fall():
    """Endpoint for hardware belt to report a fall detection.
    
    Expected JSON payload:
    {
        "device_id": "belt_001",
        "timestamp": "2026-01-05T10:30:00Z",
        "confidence": 0.95
    }
    """
    global fall_detected
    
    try:
        data = request.get_json()
        device_id = data.get("device_id", "unknown")
        confidence = data.get("confidence", 1.0)
        
        print("🚨 [FALL] detect_fall() called!")
        print(f"📊 [FALL] Device: {device_id}, Confidence: {confidence}")
        fall_detected = True
        
        return jsonify({
            "status": "success",
            "message": "Fall detected and alert triggered",
            "device_id": device_id
        }), 200
    except Exception as e:
        print(f"❌ [FALL] Error processing fall detection: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 400


@fall_detection_bp.route("/fall-status", methods=["GET"])
def get_fall_status():
    """Check if a fall has been detected."""
    global fall_detected
    return jsonify({"fall_detected": fall_detected}), 200


@fall_detection_bp.route("/clear-fall", methods=["POST"])
def clear_fall():
    """Clear the fall detection flag after user confirms safe or guardian is contacted."""
    global fall_detected
    fall_detected = False
    return jsonify({"status": "fall_cleared"}), 200


@fall_detection_bp.route("/notify-guardian-fall", methods=["POST"])
def notify_guardian_fall():
    """Notify guardian that a fall has been detected.
    This is called immediately when fall is detected.
    
    Expected JSON payload:
    {
        "elderly_name": "John",
        "device_id": "belt_001",
        "location": "Home"
    }
    """
    try:
        data = request.get_json()
        elderly_name = data.get("elderly_name", "User")
        location = data.get("location", "Unknown location")
        
        print("🚨 [NOTIFY] notify_guardian_fall() called!")
        print(f"🚨 [NOTIFY] Elderly: {elderly_name}")
        print(f"📍 [NOTIFY] Location: {location}")
        
        # Send SMS alert to guardian
        try:
            print("🔍 [NOTIFY] Getting latest guardian...")
            # Find guardian linked to this elderly person
            guardian_phone = get_guardian_phone_for_elderly()
            
            if guardian_phone:
                print(f"📱 [NOTIFY] Guardian phone found: {guardian_phone}")
                
                # Send SMS
                print("📤 [NOTIFY] Sending fall alert SMS...")
                sms_success = twilio_service.send_fall_alert_sms(
                    guardian_phone=guardian_phone,
                    elderly_name=elderly_name,
                    location=location,
                    device_id="vois_belt"
                )
                
                # Make emergency call
                print("📞 [NOTIFY] Making emergency call...")
                call_success = twilio_service.make_emergency_call(
                    guardian_phone=guardian_phone,
                    elderly_name=elderly_name,
                    location=location
                )
                
                if sms_success:
                    print(f"✅ [SUCCESS] Fall alert SMS sent to {guardian_phone}")
                else:
                    print(f"❌ [ERROR] Failed to send fall alert SMS to {guardian_phone}")
                    
                if call_success:
                    print(f"✅ [SUCCESS] Emergency call initiated to {guardian_phone}")
                else:
                    print(f"❌ [ERROR] Failed to initiate emergency call to {guardian_phone}")
            else:
                print("❌ [ERROR] No guardian found!")
                
        except Exception as sms_error:
            print(f"❌ [ERROR] SMS/Call error: {sms_error}")
        
        return jsonify({
            "status": "success",
            "message": "Guardian notified of potential fall",
            "elderly_name": elderly_name
        }), 200
    except Exception as e:
        print(f"❌ [ERROR] notify_guardian_fall error: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 400


@fall_detection_bp.route("/switch-user", methods=["POST"])
def switch_user():
    """Switch the current user using the vois_belt device"""
    try:
        data = request.get_json()
        new_user = data.get("elderly_id")
        
        if not new_user:
            return jsonify({"status": "error", "message": "elderly_id required"}), 400
        
        # Load elderly data
        elderly_file = 'data/elderly.json'
        with open(elderly_file, 'r') as f:
            elderly_data = json.load(f)
        
        # Update current_user for vois_belt device
        if "vois_belt" in elderly_data:
            elderly_data["vois_belt"]["current_user"] = new_user
            
            # Save updated data
            with open(elderly_file, 'w') as f:
                json.dump(elderly_data, f, indent=2)
            
            return jsonify({
                "status": "success",
                "message": f"Device switched to {new_user}",
                "current_user": new_user
            }), 200
        else:
            return jsonify({"status": "error", "message": "vois_belt device not found"}), 404
            
    except Exception as e:
        print(f"Error switching user: {e}")
        return jsonify({"status": "error", "message": str(e)}), 400


@fall_detection_bp.route("/current-user", methods=["GET"])
def get_current_user():
    """Get the current user using the vois_belt device"""
    try:
        # Load elderly data
        elderly_file = 'data/elderly.json'
        with open(elderly_file, 'r') as f:
            elderly_data = json.load(f)
        
        # Get current_user for vois_belt device
        if "vois_belt" in elderly_data:
            current_user = elderly_data["vois_belt"].get("current_user")
            available_users = elderly_data["vois_belt"].get("available_users", [])
            
            return jsonify({
                "status": "success",
                "current_user": current_user,
                "available_users": available_users
            }), 200
        else:
            return jsonify({"status": "error", "message": "vois_belt device not found"}), 404
            
    except Exception as e:
        print(f"Error getting current user: {e}")
        return jsonify({"status": "error", "message": str(e)}), 400
def notify_guardian_no_response():
    """Notify guardian with SOUND ALERT that elderly didn't respond to fall alert.
    This is called after 10 seconds if elderly hasn't responded.
    
    Expected JSON payload:
    {
        "elderly_name": "John",
        "device_id": "belt_001",
        "location": "Home"
    }
    """
    try:
        data = request.get_json()
        elderly_name = data.get("elderly_name", "User")
        device_id = data.get("device_id", "unknown")
        location = data.get("location", "Unknown location")
        
        print(f"[GUARDIAN URGENT ALERT] {elderly_name} DID NOT RESPOND TO FALL ALERT!")
        print(f"[GUARDIAN URGENT ALERT] Device: {device_id}, Location: {location}")
        
        # Send URGENT SMS alert to guardian
        try:
            # Find guardian linked to this elderly person
            guardian_phone = get_guardian_phone_for_elderly()
            
            if guardian_phone:
                sms_success = twilio_service.send_urgent_alert_sms(
                    guardian_phone=guardian_phone,
                    elderly_name=elderly_name,
                    location=location,
                    device_id="vois_belt"
                )
                
                # Make emergency call for urgent situation
                call_success = twilio_service.make_emergency_call(
                    guardian_phone=guardian_phone,
                    elderly_name=elderly_name,
                    location=location
                )
                
                if sms_success:
                    print(f"[SMS] ✅ URGENT alert SMS sent to guardian at {guardian_phone}")
                else:
                    print(f"[SMS] ❌ Failed to send urgent alert SMS to {guardian_phone}")
                    
                if call_success:
                    print(f"[CALL] ✅ URGENT emergency call initiated to guardian at {guardian_phone}")
                else:
                    print(f"[CALL] ❌ Failed to initiate urgent emergency call to {guardian_phone}")
            else:
                print(f"[SMS] ❌ No guardian found for elderly device: {device_id}")
                
        except Exception as sms_error:
            print(f"[SMS] Error sending urgent SMS/call: {sms_error}")
        
        return jsonify({
            "status": "success",
            "message": "Guardian notified with urgent sound alert",
            "elderly_name": elderly_name
        }), 200
    except Exception as e:
        print(f"Error sending urgent alert to guardian: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 400


@fall_detection_bp.route("/notify-guardian-safe", methods=["POST"])
def notify_guardian_safe():
    """Notify guardian that elderly person confirmed they are safe.
    
    Expected JSON payload:
    {
        "elderly_name": "John",
        "device_id": "belt_001"
    }
    """
    try:
        data = request.get_json()
        elderly_name = data.get("elderly_name", "User")
        device_id = data.get("device_id", "unknown")
        
        print(f"[GUARDIAN INFO] {elderly_name} confirmed they are safe (Fall was false alarm)")
        print(f"[GUARDIAN INFO] Device: {device_id}")
        
        # TODO: Send info notification to guardian
        # Example: Send SMS/Email confirming false alarm
        # from twilio.rest import Client
        # client = Client(account_sid, auth_token)
        # message = client.messages.create(
        #     body=f"All clear: {elderly_name} confirmed they are safe.",
        #     from_="+1234567890",
        #     to="+guardian_number"
        # )
        
        return jsonify({
            "status": "success",
            "message": "Guardian notified - false alarm",
            "elderly_name": elderly_name
        }), 200
    except Exception as e:
        print(f"Error notifying guardian (safe): {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 400


@fall_detection_bp.route("/emergency-call", methods=["POST"])
def emergency_call():
    """Make an emergency phone call to guardian
    
    Called when elderly clicks "Need Help Now" button
    
    Expected JSON:
    {
        "elderly_id": "john_guardian_harsh",
        "elderly_name": "Harsh",
        "guardian_username": "john_guardian",
        "location": "Home"
    }
    """
    try:
        data = request.get_json()
        elderly_name = data.get("elderly_name", "User")
        guardian_username = data.get("guardian_username", "")
        location = data.get("location", "Unknown location")
        
        # Get guardian info to fetch phone number
        guardian = get_guardian(guardian_username)
        
        if not guardian:
            return jsonify({
                "status": "error",
                "message": "Guardian not found"
            }), 404
        
        guardian_phone = guardian.get("phone")
        
        if not guardian_phone:
            return jsonify({
                "status": "error",
                "message": "Guardian phone number not found"
            }), 400
        
        print(f"[EMERGENCY CALL] Initiating call to {guardian_username}")
        print(f"[EMERGENCY CALL] Guardian: {guardian.get('name')}")
        print(f"[EMERGENCY CALL] Phone: {guardian_phone}")
        print(f"[EMERGENCY CALL] Elderly: {elderly_name}")
        print(f"[EMERGENCY CALL] Location: {location}")
        
        # Make the actual call using Twilio
        call_success = twilio_service.make_emergency_call(
            guardian_phone=guardian_phone,
            elderly_name=elderly_name,
            location=location
        )
        
        if call_success:
            return jsonify({
                "status": "success",
                "message": "Emergency call initiated to guardian",
                "guardian_phone": guardian_phone,
                "guardian_name": guardian.get("name")
            }), 200
        else:
            return jsonify({
                "status": "error",
                "message": "Failed to initiate emergency call"
            }), 500
            
    except Exception as e:
        print(f"Error making emergency call: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500


@fall_detection_bp.route("/urgent-call-no-response", methods=["POST"])
def urgent_call_no_response():
    """Make URGENT call with SIREN when elderly doesn't respond
    
    Called after 10 seconds if elderly hasn't clicked anything
    
    Expected JSON:
    {
        "elderly_id": "john_guardian_harsh",
        "elderly_name": "Harsh",
        "guardian_username": "john_guardian",
        "location": "Home"
    }
    """
    try:
        data = request.get_json()
        elderly_name = data.get("elderly_name", "User")
        guardian_username = data.get("guardian_username", "")
        location = data.get("location", "Unknown location")
        
        # Get guardian info
        guardian = get_guardian(guardian_username)
        
        if not guardian:
            return jsonify({
                "status": "error",
                "message": "Guardian not found"
            }), 404
        
        guardian_phone = guardian.get("phone")
        
        if not guardian_phone:
            return jsonify({
                "status": "error",
                "message": "Guardian phone number not found"
            }), 400
        
        print(f"[URGENT CALL] 🚨 Making URGENT call with SIREN!")
        print(f"[URGENT CALL] Guardian: {guardian.get('name')}")
        print(f"[URGENT CALL] Phone: {guardian_phone}")
        print(f"[URGENT CALL] Elderly did not respond: {elderly_name}")
        
        # Make the urgent call with siren
        call_success = twilio_service.make_no_response_alert_call(
            guardian_phone=guardian_phone,
            elderly_name=elderly_name,
            location=location
        )
        
        if call_success:
            return jsonify({
                "status": "success",
                "message": "URGENT call with siren initiated",
                "guardian_phone": guardian_phone,
                "alert_type": "NO_RESPONSE"
            }), 200
        else:
            return jsonify({
                "status": "error",
                "message": "Failed to initiate urgent call"
            }), 500
            
    except Exception as e:
        print(f"Error making urgent call: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 400

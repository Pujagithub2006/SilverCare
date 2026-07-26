"""
Elderly Management Module
Handles elderly registration, linking to guardians, and profile management
"""

from flask import Blueprint, request, jsonify
from datetime import datetime
from utils.auth import (
    load_guardians, save_guardians, load_elderly, save_elderly,
    guardian_exists, get_guardian, verify_password, elderly_exists
)

# Create Blueprint
elderly_management_bp = Blueprint('elderly_management', __name__)


@elderly_management_bp.route("/elderly-register", methods=["POST"])
def elderly_register():
    """Register elderly and link to guardian
    
    Expected JSON:
    {
        "name": "Harsh",
        "age": 65,
        "medical_history": "Diabetes, High BP",
        "phone": "+919987654321",
        "location": "Home",
        "guardian_username": "john_guardian",
        "guardian_password": "secure123"
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({
                "status": "error",
                "message": "No JSON data received"
            }), 400
        
        print(f"[ELDERLY REGISTER] Received data: {data}")
        
        # Validate input
        required_fields = ["name", "age", "guardian_username", "guardian_password"]
        if not all(field in data for field in required_fields):
            return jsonify({
                "status": "error",
                "message": "Missing required fields"
            }), 400
        
        guardian_username = data.get("guardian_username", "").strip()
        guardian_password = data.get("guardian_password", "")
        
        # Verify guardian credentials
        guardian = get_guardian(guardian_username)
        
        if not guardian:
            return jsonify({
                "status": "error",
                "message": "Guardian username not found"
            }), 404
        
        if not verify_password(guardian.get("password_hash"), guardian_password):
            return jsonify({
                "status": "error",
                "message": "Invalid guardian credentials"
            }), 401
        
        # Check if guardian already has an elderly linked (1:1 relationship)
        existing_elderly = load_elderly()
        guardian_existing_elderly = [
            elderly for elderly in existing_elderly.values()
            if elderly.get("guardian_username") == guardian_username
        ]
        
        if guardian_existing_elderly:
            return jsonify({
                "status": "error",
                "message": f"Guardian '{guardian_username}' is already linked to '{guardian_existing_elderly[0]['name']}'. One guardian can only link to one elderly person."
            }), 400
        
        # Create elderly record
        elderly_id = f"{guardian_username}_{data.get('name').lower().replace(' ', '_')}"
        
        elderly = load_elderly()
        elderly[elderly_id] = {
            "elderly_id": elderly_id,
            "name": data.get("name"),
            "age": data.get("age"),
            "medical_history": data.get("medical_history", ""),
            "phone": data.get("phone", ""),
            "location": data.get("location", "Home"),
            "guardian_username": guardian_username,
            "created_at": datetime.now().isoformat()
        }
        
        save_elderly(elderly)
        
        # Update guardian's linked elderly list
        if elderly_id not in guardian.get("elderly_linked", []):
            guardian["elderly_linked"].append(elderly_id)
            guardians = load_guardians()
            guardians[guardian_username] = guardian
            save_guardians(guardians)
        
        print(f"[ELDERLY REGISTRATION] New elderly registered: {elderly_id}")
        print(f"[ELDERLY LINK] Linked to guardian: {guardian_username}")
        
        return jsonify({
            "status": "success",
            "message": "Elderly registered and linked to guardian successfully",
            "elderly_id": elderly_id,
            "guardian_name": guardian.get("name")
        }), 201
        
    except Exception as e:
        print(f"Error registering elderly: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500


@elderly_management_bp.route("/elderly-info/<elderly_id>", methods=["GET"])
def elderly_info(elderly_id):
    """Get elderly information
    
    URL: /elderly-info/john_guardian_harsh
    """
    try:
        elderly = load_elderly()
        
        if elderly_id not in elderly:
            return jsonify({
                "status": "error",
                "message": "Elderly profile not found"
            }), 404
        
        elderly_data = elderly[elderly_id]
        
        return jsonify({
            "status": "success",
            "data": elderly_data
        }), 200
        
    except Exception as e:
        print(f"Error fetching elderly info: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500


@elderly_management_bp.route("/elderly-update", methods=["POST"])
def elderly_update():
    """Update elderly information
    
    Expected JSON:
    {
        "elderly_id": "john_guardian_harsh",
        "medical_history": "Updated history",
        "location": "Hospital"
    }
    """
    try:
        data = request.get_json()
        elderly_id = data.get("elderly_id", "").strip()
        
        elderly = load_elderly()
        
        if elderly_id not in elderly:
            return jsonify({
                "status": "error",
                "message": "Elderly profile not found"
            }), 404
        
        elderly_data = elderly[elderly_id]
        
        # Update allowed fields
        updatable_fields = ["medical_history", "phone", "location", "age"]
        for field in updatable_fields:
            if field in data:
                elderly_data[field] = data.get(field)
        
        elderly[elderly_id] = elderly_data
        save_elderly(elderly)
        
        print(f"[ELDERLY UPDATE] {elderly_id} information updated")
        
        return jsonify({
            "status": "success",
            "message": "Elderly information updated"
        }), 200
        
    except Exception as e:
        print(f"Error updating elderly info: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500


@elderly_management_bp.route("/guardian-elderly/<guardian_username>", methods=["GET"])
def guardian_elderly(guardian_username):
    """Get all elderly linked to guardian
    
    URL: /guardian-elderly/john_guardian
    """
    try:
        guardian = get_guardian(guardian_username)
        
        if not guardian:
            return jsonify({
                "status": "error",
                "message": "Guardian not found"
            }), 404
        
        elderly_ids = guardian.get("elderly_linked", [])
        elderly = load_elderly()
        
        elderly_list = []
        for elderly_id in elderly_ids:
            if elderly_id in elderly:
                elderly_list.append(elderly[elderly_id])
        
        return jsonify({
            "status": "success",
            "data": elderly_list,
            "count": len(elderly_list)
        }), 200
        
    except Exception as e:
        print(f"Error fetching guardian's elderly: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

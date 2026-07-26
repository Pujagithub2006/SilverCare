"""
Guardian Authentication Module
Handles guardian registration, login, and profile management
"""

from flask import Blueprint, request, jsonify
from datetime import datetime
from utils.auth import (
    hash_password, verify_password, load_guardians, save_guardians,
    guardian_exists, get_guardian
)

# Create Blueprint
guardian_auth_bp = Blueprint('guardian_auth', __name__)


@guardian_auth_bp.route("/guardian-register", methods=["POST"])
def guardian_register():
    """Register a new guardian
    
    Expected JSON:
    {
        "fullName": "John Sharma",
        "username": "john_guardian",
        "password": "secure123",
        "phone": "+919876543210",
        "email": "john@example.com",
        "address": "123 Main St, City",
        "emergencyContact": "+919876543211"
    }
    """
    try:
        data = request.get_json()
        
        # Validate input
        required_fields = ["name", "username", "password", "phone", "email"]
        if not all(field in data for field in required_fields):
            return jsonify({
                "status": "error",
                "message": "Missing required fields"
            }), 400
        
        username = data.get("username", "").strip()
        
        # Check if guardian already exists
        if guardian_exists(username):
            return jsonify({
                "status": "error",
                "message": "Username already exists"
            }), 409  # 409 Conflict
            
        # Validate password strength
        if len(data["password"]) < 6:
            return jsonify({
                "status": "error",
                "message": "Password must be at least 6 characters long"
            }), 400
        
        # Create guardian record
        guardians = load_guardians()
        guardians[username] = {
            "name": data.get("name"),
            "username": username,
            "password_hash": hash_password(data.get("password")),
            "phone": data.get("phone"),
            "email": data.get("email"),
            "elderly_linked": [],
            "created_at": datetime.utcnow().isoformat()
        }
        
        save_guardians(guardians)
        
        print(f"[GUARDIAN REGISTRATION] New guardian registered: {username}")
        
        return jsonify({
            "status": "success",
            "message": "Guardian registered successfully",
            "username": username
        }), 201
        
    except Exception as e:
        print(f"Error registering guardian: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500


@guardian_auth_bp.route("/guardian-login", methods=["POST"])
def guardian_login():
    """Login guardian
    
    Expected JSON:
    {
        "username": "john_guardian",
        "password": "secure123"
    }
    """
    try:
        data = request.get_json()
        username = data.get("username", "").strip()
        password = data.get("password", "")
        
        if not username or not password:
            return jsonify({
                "status": "error",
                "message": "Username and password required"
            }), 400
        
        guardian = get_guardian(username)
        
        if not guardian:
            return jsonify({
                "status": "error",
                "message": "Invalid username or password"
            }), 401
        
        # Verify password
        if not verify_password(guardian.get("password_hash"), password):
            return jsonify({
                "status": "error",
                "message": "Invalid username or password"
            }), 401
        
        print(f"[GUARDIAN LOGIN] {username} logged in successfully")
        
        return jsonify({
            "status": "success",
            "message": "Login successful",
            "username": username,
            "name": guardian.get("name"),
            "phone": guardian.get("phone"),
            "email": guardian.get("email"),
            "elderly_linked": guardian.get("elderly_linked", [])
        }), 200
        
    except Exception as e:
        print(f"Error during guardian login: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500


@guardian_auth_bp.route("/guardian-info/<username>", methods=["GET"])
def guardian_info(username):
    """Get guardian information
    
    URL: /guardian-info/john_guardian
    """
    try:
        guardian = get_guardian(username)
        
        if not guardian:
            return jsonify({
                "status": "error",
                "message": "Guardian not found"
            }), 404
        
        # Don't send password hash
        guardian_info = {
            "name": guardian.get("name"),
            "username": guardian.get("username"),
            "phone": guardian.get("phone"),
            "email": guardian.get("email"),
            "elderly_linked": guardian.get("elderly_linked", []),
            "created_at": guardian.get("created_at")
        }
        
        return jsonify({
            "status": "success",
            "data": guardian_info
        }), 200
        
    except Exception as e:
        print(f"Error fetching guardian info: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500


@guardian_auth_bp.route("/guardian-update", methods=["POST"])
def guardian_update():
    """Update guardian information
    
    Expected JSON:
    {
        "username": "john_guardian",
        "phone": "+919876543210",
        "email": "newemail@example.com"
    }
    """
    try:
        data = request.get_json()
        username = data.get("username", "").strip()
        
        guardian = get_guardian(username)
        
        if not guardian:
            return jsonify({
                "status": "error",
                "message": "Guardian not found"
            }), 404
        
        # Update fields
        if "phone" in data:
            guardian["phone"] = data.get("phone")
        if "email" in data:
            guardian["email"] = data.get("email")
        if "name" in data:
            guardian["name"] = data.get("name")
        
        guardians = load_guardians()
        guardians[username] = guardian
        save_guardians(guardians)
        
        print(f"[GUARDIAN UPDATE] {username} information updated")
        
        return jsonify({
            "status": "success",
            "message": "Guardian information updated"
        }), 200
        
    except Exception as e:
        print(f"Error updating guardian info: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

"""
Medicine Management Blueprint
Handles medicine schedules, reminders, and guardian suggestions
"""

from flask import Blueprint, request, jsonify
import json
import os
from datetime import datetime, time
import threading

medicine_bp = Blueprint('medicine_management', __name__)

# Data files
MEDICINES_FILE = 'data/medicines.json'
ELDERLY_FILE = 'data/elderly.json'

def load_medicines():
    """Load medicines data"""
    if os.path.exists(MEDICINES_FILE):
        try:
            with open(MEDICINES_FILE, 'r') as f:
                content = f.read().strip()
                if not content:
                    return {}
                return json.loads(content)
        except (json.JSONDecodeError, IOError):
            # File corrupted, create new empty file
            with open(MEDICINES_FILE, 'w') as f:
                json.dump({}, f)
            return {}
    return {}

def save_medicines(medicines):
    """Save medicines data"""
    with open(MEDICINES_FILE, 'w') as f:
        json.dump(medicines, f, indent=2)

def load_elderly():
    """Load elderly data"""
    if os.path.exists(ELDERLY_FILE):
        with open(ELDERLY_FILE, 'r') as f:
            return json.load(f)
    return {}

# ====================
# MEDICINE MANAGEMENT ENDPOINTS
# ====================

@medicine_bp.route("/medicine/add", methods=["POST"])
def add_medicine():
    """Add medicine schedule for elderly person"""
    try:
        data = request.get_json()
        guardian_username = data.get('guardian_username')
        elderly_id = data.get('elderly_id')
        medicine_name = data.get('medicine_name')
        dosage = data.get('dosage')
        times = data.get('times')
        instructions = data.get('instructions', '')
        start_date = data.get('start_date')
        end_date = data.get('end_date')
        
        if not all([guardian_username, elderly_id, medicine_name, dosage, times, start_date, end_date]):
            return jsonify({
                "status": "error",
                "message": "Missing required fields"
            }), 400
        
        # Validate guardian access
        elderly_data = load_elderly()
        if elderly_id not in elderly_data:
            return jsonify({"error": "Elderly person not found"}), 404
            
        if elderly_data[elderly_id].get('guardian_username') != guardian_username:
            return jsonify({"error": "Unauthorized access"}), 403
        
        # Load and update medicines
        medicines = load_medicines()
        
        if elderly_id not in medicines:
            medicines[elderly_id] = []
        
        medicine_entry = {
            "id": len(medicines[elderly_id]) + 1,
            "medicine_name": medicine_name,
            "dosage": dosage,
            "times": times,
            "instructions": instructions,
            "start_date": start_date,
            "end_date": end_date,
            "created_at": datetime.now().isoformat(),
            "active": True
        }
        
        medicines[elderly_id].append(medicine_entry)
        save_medicines(medicines)
        
        return jsonify({
            "status": "success",
            "message": "Medicine schedule added successfully",
            "medicine": medicine_entry
        }), 200
        
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@medicine_bp.route("/medicine/delete/<medicine_id>", methods=["DELETE"])
def delete_medicine(medicine_id):
    """Delete medicine for elderly person"""
    try:
        data = request.get_json()
        guardian_username = data.get('guardian_username')
        elderly_id = data.get('elderly_id')
        
        # Validate guardian access
        elderly_data = load_elderly()
        if elderly_id not in elderly_data:
            return jsonify({"error": "Elderly person not found"}), 404
            
        if elderly_data[elderly_id].get('guardian_username') != guardian_username:
            return jsonify({"error": "Unauthorized access"}), 403
        
        # Load medicines and delete
        medicines = load_medicines()
        
        if elderly_id not in medicines:
            return jsonify({"error": "No medicines found for this elderly person"}), 404
        
        # Find and remove medicine
        medicine_found = False
        medicines[elderly_id] = [m for m in medicines[elderly_id] if m['id'] != int(medicine_id)]
        
        save_medicines(medicines)
        
        return jsonify({
            "status": "success",
            "message": "Medicine deleted successfully"
        }), 200
        
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@medicine_bp.route("/medicines", methods=["POST"])
def add_medicine_old():
    """Add medicine schedule for elderly person (legacy endpoint)"""
    return add_medicine()

@medicine_bp.route("/medicines/<elderly_id>", methods=["GET"])
def get_medicines(elderly_id):
    """Get all medicines for an elderly person"""
    try:
        medicines = load_medicines()
        
        if elderly_id not in medicines:
            return jsonify({"medicines": []}), 200
        
        # Filter only active medicines
        active_medicines = [m for m in medicines[elderly_id] if m.get('active', True)]
        
        return jsonify({"medicines": active_medicines}), 200
        
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@medicine_bp.route("/medicine/confirm", methods=["POST"])
def confirm_medicine_taken():
    """Confirm medicine taken by elderly person"""
    try:
        data = request.get_json()
        elderly_id = data.get('elderly_id')
        medicine_id = data.get('medicine_id')
        time_taken = data.get('time_taken')
        taken = data.get('taken')  # boolean
        
        # Load medicines
        medicines = load_medicines()
        
        if elderly_id not in medicines:
            return jsonify({"error": "No medicines found for this elderly person"}), 404
        
        # Find the medicine and add confirmation record
        medicine_found = False
        for medicine in medicines[elderly_id]:
            if medicine['id'] == medicine_id:
                if 'confirmation_history' not in medicine:
                    medicine['confirmation_history'] = []
                
                confirmation = {
                    "time_taken": time_taken,
                    "taken": taken,
                    "timestamp": datetime.now().isoformat()
                }
                
                medicine['confirmation_history'].append(confirmation)
                medicine_found = True
                break
        
        if not medicine_found:
            return jsonify({"error": "Medicine not found"}), 404
        
        save_medicines(medicines)
        
        status = "taken" if taken else "not taken"
        return jsonify({
            "message": f"Medicine marked as {status}",
            "status": status
        }), 200
        
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@medicine_bp.route("/medicine/suggestions/<elderly_id>", methods=["GET", "POST"])
def manage_suggestions(elderly_id):
    """Get or add guardian suggestions for elderly person"""
    try:
        if request.method == "GET":
            # Get suggestions
            elderly_data = load_elderly()
            
            if elderly_id not in elderly_data:
                return jsonify({"error": "Elderly person not found"}), 404
            
            suggestions = elderly_data[elderly_id].get('guardian_suggestions', [])
            
            return jsonify({"suggestions": suggestions}), 200
            
        else:  # POST
            # Add suggestion
            data = request.get_json()
            guardian_username = data.get('guardian_username')
            suggestion = data.get('suggestion')
            
            # Validate access
            elderly_data = load_elderly()
            
            if elderly_id not in elderly_data:
                return jsonify({"error": "Elderly person not found"}), 404
                
            if elderly_data[elderly_id].get('guardian_username') != guardian_username:
                return jsonify({"error": "Unauthorized access"}), 403
            
            # Add suggestion
            if 'guardian_suggestions' not in elderly_data[elderly_id]:
                elderly_data[elderly_id]['guardian_suggestions'] = []
            
            suggestion_entry = {
                "id": len(elderly_data[elderly_id]['guardian_suggestions']) + 1,
                "suggestion": suggestion,
                "created_at": datetime.now().isoformat(),
                "active": True
            }
            
            elderly_data[elderly_id]['guardian_suggestions'].append(suggestion_entry)
            
            # Save updated data
            with open(ELDERLY_FILE, 'w') as f:
                json.dump(elderly_data, f, indent=2)
            
            return jsonify({
                "message": "Suggestion added successfully",
                "suggestion": suggestion_entry
            }), 200
        
    except Exception as e:
        return jsonify({"error": str(e)}), 500
"""
Elderly Person Notification System
Sends voice alerts and notifications directly to elderly person's device
"""

import json
import os
import time
import threading
from datetime import datetime, timedelta
from medicine_management import load_medicines, load_elderly
import winsound
import platform
from flask import request, jsonify

class ElderlyNotificationSystem:
    def __init__(self):
        self.active_notifications = {}  # Track active notifications for elderly
        self.elderly_sessions = {}  # Track logged-in elderly users
        
    def register_elderly_session(self, elderly_id, device_info):
        """Register elderly user session when they log in"""
        self.elderly_sessions[elderly_id] = {
            'device_info': device_info,
            'last_active': datetime.now(),
            'notifications_sent': []
        }
        print(f"👤 [ELDERLY] {elderly_id} logged in on {device_info}")
        
    def unregister_elderly_session(self, elderly_id):
        """Unregister elderly user session when they log out"""
        if elderly_id in self.elderly_sessions:
            del self.elderly_sessions[elderly_id]
            print(f"👋 [ELDERLY] {elderly_id} logged out")
    
    def send_elderly_notification(self, elderly_id, medicine, notification_type="medicine_reminder"):
        """Send notification to elderly person's device"""
        if elderly_id not in self.elderly_sessions:
            print(f"⚠️ [ELDERLY] {elderly_id} not logged in - skipping notification")
            return False
            
        session = self.elderly_sessions[elderly_id]
        
        # Create notification data
        notification_data = {
            "elderly_id": elderly_id,
            "medicine": medicine,
            "message": f"Time to take your medicine: {medicine.get('medicine_name', 'Unknown')} - {medicine.get('dosage', 'Unknown')}",
            "type": notification_type,
            "timestamp": datetime.now().isoformat(),
            "voice_message": f"Time to take your medicine. Please take {medicine.get('dosage', 'Unknown')} of {medicine.get('medicine_name', 'Unknown')}",
            "options": ["taken", "snooze"],
            "device_info": session['device_info']
        }
        
        # Store for elderly device to fetch
        self.active_notifications[f"{elderly_id}_{medicine['id']}"] = notification_data
        
        print(f"📱 [ELDERLY NOTIFICATION] Sent to {elderly_id}")
        print(f"   Device: {session['device_info']}")
        print(f"   Medicine: {medicine.get('medicine_name', 'Unknown')}")
        print(f"   Voice: {notification_data['voice_message']}")
        
        return True
    
    def get_elderly_notifications(self, elderly_id):
        """Get notifications for specific elderly user"""
        user_notifications = {}
        
        for key, notification in self.active_notifications.items():
            if notification['elderly_id'] == elderly_id:
                user_notifications[key] = notification
                
        return user_notifications
    
    def clear_elderly_notification(self, elderly_id, medicine_id):
        """Clear notification after elderly responds"""
        key = f"{elderly_id}_{medicine_id}"
        if key in self.active_notifications:
            del self.active_notifications[key]
            print(f"✅ [ELDERLY] Notification cleared for {elderly_id}")

# Global instance - shared across all imports
elderly_notification_system = ElderlyNotificationSystem()

def register_elderly_session(elderly_id, device_info):
    """Register elderly user session"""
    elderly_notification_system.register_elderly_session(elderly_id, device_info)

def unregister_elderly_session(elderly_id):
    """Unregister elderly user session"""
    elderly_notification_system.unregister_elderly_session(elderly_id)

def send_elderly_notification(elderly_id, medicine):
    """Send notification to elderly person"""
    return elderly_notification_system.send_elderly_notification(elderly_id, medicine)

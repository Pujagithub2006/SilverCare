"""
Advanced Medicine Reminder System
Voice alerts + interactive notifications with taken/snooze/missed tracking
"""

import json
import os
import time
import threading
from datetime import datetime, timedelta
from medicine_management import load_medicines, load_elderly, save_medicines
import winsound  # For Windows voice alerts
import platform
from elderly_notifications import elderly_notification_system

class MedicineReminderSystem:
    def __init__(self):
        self.active_reminders = {}  # Track active reminders
        self.snoozed_reminders = {}  # Track snoozed reminders
        
    def play_voice_alert(self, message):
        """Play voice alert for medicine reminder"""
        try:
            if platform.system() == "Windows":
                # Use Windows speech API
                import win32com.client
                speaker = win32com.client.Dispatch("SAPI.SpVoice")
                speaker.Speak(message)
            else:
                # Fallback to beep for other systems
                winsound.Beep(1000, 1000)  # 1kHz beep for 1 second
                print(f"🔊 VOICE: {message}")
        except ImportError:
            # Fallback if speech API not available
            print(f"🔊 VOICE: {message}")
            # Simple beep pattern
            for _ in range(3):
                winsound.Beep(800, 200)
                time.sleep(0.1)
    
    def send_notification(self, elderly_id, medicine, message, options):
        """Send notification with interactive options"""
        notification_data = {
            "elderly_id": elderly_id,
            "medicine": medicine,
            "message": message,
            "options": options,
            "timestamp": datetime.now().isoformat(),
            "type": "medicine_reminder"
        }
        
        # Store notification for frontend to pick up
        self.store_notification(notification_data)
        
        print(f"📱 NOTIFICATION: {message}")
        print(f"   Options: {', '.join(options)}")
        
    def store_notification(self, notification_data):
        """Store notification for frontend consumption"""
        notifications_file = 'data/active_notifications.json'
        
        try:
            if os.path.exists(notifications_file):
                with open(notifications_file, 'r') as f:
                    notifications = json.load(f)
            else:
                notifications = {}
        except:
            notifications = {}
        
        # Use elderly_id + medicine_id as key
        key = f"{notification_data['elderly_id']}_{notification_data['medicine']['id']}"
        notifications[key] = notification_data
        
        with open(notifications_file, 'w') as f:
            json.dump(notifications, f, indent=2)
    
    def handle_response(self, elderly_id, medicine_id, response):
        """Handle user response to medicine reminder"""
        key = f"{elderly_id}_{medicine_id}"
        
        if response == "taken":
            self.mark_medicine_taken(elderly_id, medicine_id)
            self.clear_reminder(key)
            print(f"✅ Medicine marked as taken for {elderly_id}")
            
        elif response == "snooze":
            self.snooze_reminder(key, 2)  # Snooze for 2 minutes
            print(f"⏰ Medicine reminder snoozed for 2 minutes")
            # Don't create notification immediately - let snooze system handle it
            
        elif response == "not_taken":
            self.mark_medicine_missed(elderly_id, medicine_id)
            self.clear_reminder(key)
            print(f"❌ Medicine marked as missed for {elderly_id}")
    
    def mark_medicine_taken(self, elderly_id, medicine_id):
        """Mark medicine as taken"""
        try:
            medicines = load_medicines()
            if elderly_id in medicines:
                for medicine in medicines[elderly_id]:
                    if medicine['id'] == medicine_id:
                        if 'confirmation_history' not in medicine:
                            medicine['confirmation_history'] = []
                        
                        confirmation = {
                            "time_taken": datetime.now().strftime("%H:%M"),
                            "taken": True,
                            "timestamp": datetime.now().isoformat(),
                            "type": "automatic_reminder"
                        }
                        
                        medicine['confirmation_history'].append(confirmation)
                        save_medicines(medicines)
                        break
        except Exception as e:
            print(f"❌ Error marking medicine as taken: {e}")
    
    def mark_medicine_missed(self, elderly_id, medicine_id):
        """Mark medicine as missed"""
        try:
            medicines = load_medicines()
            if elderly_id in medicines:
                for medicine in medicines[elderly_id]:
                    if medicine['id'] == medicine_id:
                        if 'confirmation_history' not in medicine:
                            medicine['confirmation_history'] = []
                        
                        confirmation = {
                            "time_taken": datetime.now().strftime("%H:%M"),
                            "taken": False,
                            "timestamp": datetime.now().isoformat(),
                            "type": "missed_dose"
                        }
                        
                        medicine['confirmation_history'].append(confirmation)
                        save_medicines(medicines)
                        break
        except Exception as e:
            print(f"❌ Error marking medicine as missed: {e}")
    
    def snooze_reminder(self, key, minutes):
        """Snooze reminder for specified minutes"""
        snooze_until = datetime.now() + timedelta(minutes=minutes)
        self.snoozed_reminders[key] = snooze_until
    
    def clear_reminder(self, key):
        """Clear active reminder"""
        if key in self.active_reminders:
            del self.active_reminders[key]
        
        # Clear from notifications file
        notifications_file = 'data/active_notifications.json'
        try:
            if os.path.exists(notifications_file):
                with open(notifications_file, 'r') as f:
                    notifications = json.load(f)
                
                if key in notifications:
                    del notifications[key]
                
                with open(notifications_file, 'w') as f:
                    json.dump(notifications, f, indent=2)
        except:
            pass
    
    def check_medicine_times(self):
        """Main loop to check medicine times and send reminders"""
        while True:
            try:
                current_time = datetime.now().strftime("%H:%M")
                current_date = datetime.now().strftime("%Y-%m-%d")
                
                medicines = load_medicines()
                elderly_data = load_elderly()
                
                # Check snoozed reminders
                self.check_snoozed_reminders()
                
                for elderly_id, medicine_list in medicines.items():
                    if elderly_id not in elderly_data:
                        continue
                        
                    elderly_info = elderly_data[elderly_id]
                    
                    for medicine in medicine_list:
                        if not medicine.get('active', True):
                            continue
                            
                        # Check if medicine is within date range
                        start_date = medicine.get('start_date')
                        end_date = medicine.get('end_date')
                        
                        if start_date and end_date:
                            if not (start_date <= current_date <= end_date):
                                continue
                        
                        # Check if current time matches medicine times (within 1 minute window)
                        medicine_times = medicine.get('times', [])
                        for med_time in medicine_times:
                            if med_time == current_time:
                                key = f"{elderly_id}_{medicine['id']}"
                                
                                # Check if already reminded recently (within last 2 minutes)
                                if key not in self.active_reminders:
                                    self.trigger_medicine_reminder(elderly_id, medicine, elderly_info)
                                    self.active_reminders[key] = datetime.now()
                                elif (datetime.now() - self.active_reminders[key]).seconds > 120:
                                    # Allow reminder again after 2 minutes (for snooze)
                                    # But only if notification is still active (user hasn't responded)
                                    from elderly_notifications import elderly_notification_system
                                    if f"{elderly_id}_{medicine['id']}" in elderly_notification_system.active_notifications:
                                        self.trigger_medicine_reminder(elderly_id, medicine, elderly_info, is_snooze=True)
                                        self.active_reminders[key] = datetime.now()
                            
            except Exception as e:
                print(f"❌ [MEDICINE REMINDER] Error: {e}")
            
            # Check every 30 seconds for more precise timing
            time.sleep(30)
    
    def check_snoozed_reminders(self):
        """Check and reactivate snoozed reminders"""
        current_time = datetime.now()
        snoozed_to_remove = []
        
        for key, snooze_until in self.snoozed_reminders.items():
            if current_time >= snooze_until:
                # Reactivate the reminder
                elderly_id, medicine_id = key.split('_')
                medicines = load_medicines()
                elderly_data = load_elderly()
                
                if elderly_id in elderly_data and elderly_id in medicines:
                    for medicine in medicines[elderly_id]:
                        if medicine['id'] == int(medicine_id):
                            elderly_info = elderly_data[elderly_id]
                            self.trigger_medicine_reminder(elderly_id, medicine, elderly_info, is_snooze=True)
                            break
                
                snoozed_to_remove.append(key)
        
        # Remove processed snoozed reminders
        for key in snoozed_to_remove:
            del self.snoozed_reminders[key]
    
    def trigger_medicine_reminder(self, elderly_id, medicine, elderly_info, is_snooze=False):
        """Trigger medicine reminder with voice and device notification only for elderly person"""
        elderly_name = elderly_info.get('name', 'Unknown')
        medicine_name = medicine.get('medicine_name', 'Unknown')
        dosage = medicine.get('dosage', 'Unknown')
        
        # Voice alert (for system where this is running)
        voice_message = f"Time to take medicine. {elderly_name}, please take {dosage} of {medicine_name}"
        self.play_voice_alert(voice_message)
        
        # Send notification to elderly person's device ONLY
        elderly_notification_sent = elderly_notification_system.send_elderly_notification(elderly_id, medicine)
        
        # NO guardian notification - only elderly gets medicine reminders
        print(f"🔔 [MEDICINE REMINDER] Triggered for {elderly_name}")
        print(f"   Medicine: {medicine_name}")
        print(f"   Dosage: {dosage}")
        print(f"   SMS: DISABLED - No SMS for medicine alerts")
        print(f"   Elderly Device Notified: {'Yes' if elderly_notification_sent else 'No - Not logged in'}")
        print(f"   Guardian Notified: NO - Elderly handles own medicine")
        
        return elderly_notification_sent

# Global instance
reminder_system = MedicineReminderSystem()

def start_medicine_reminder_system():
    """Start the advanced medicine reminder system"""
    reminder_thread = threading.Thread(target=reminder_system.check_medicine_times, daemon=True)
    reminder_thread.start()
    print("🔔 [MEDICINE] Advanced medicine reminder system started")

def handle_medicine_response(elderly_id, medicine_id, response):
    """Handle medicine reminder response"""
    reminder_system.handle_response(elderly_id, medicine_id, response)

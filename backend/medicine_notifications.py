"""
Medicine Notification System
Checks medicine times and sends notifications to guardians
"""

import json
import os
import time
import threading
from datetime import datetime
from medicine_management import load_medicines, load_elderly

def check_medicine_times():
    """Background thread to check medicine times and send notifications"""
    while True:
        try:
            current_time = datetime.now().strftime("%H:%M")
            current_date = datetime.now().strftime("%Y-%m-%d")
            
            medicines = load_medicines()
            elderly_data = load_elderly()
            
            for elderly_id, medicine_list in medicines.items():
                if elderly_id not in elderly_data:
                    continue
                    
                elderly_info = elderly_data[elderly_id]
                guardian_phone = elderly_info.get('phone', '')
                
                for medicine in medicine_list:
                    if not medicine.get('active', True):
                        continue
                        
                    # Check if medicine is within date range
                    start_date = medicine.get('start_date')
                    end_date = medicine.get('end_date')
                    
                    if start_date and end_date:
                        if not (start_date <= current_date <= end_date):
                            continue
                    
                    # Check if current time matches medicine times
                    medicine_times = medicine.get('times', [])
                    for med_time in medicine_times:
                        if med_time == current_time:
                            print(f"🔔 [MEDICINE REMINDER] Time to take medicine!")
                            print(f"   Elderly: {elderly_info.get('name', 'Unknown')}")
                            print(f"   Medicine: {medicine.get('medicine_name', 'Unknown')}")
                            print(f"   Dosage: {medicine.get('dosage', 'Unknown')}")
                            print(f"   Guardian: {elderly_info.get('guardian_username', 'Unknown')}")
                            
                            # Here you can integrate with SMS service
                            # if guardian_phone:
                            #     send_medicine_reminder_sms(guardian_phone, medicine, elderly_info)
                            
        except Exception as e:
            print(f"❌ [MEDICINE REMINDER] Error: {e}")
        
        # Check every minute
        time.sleep(60)

def start_medicine_notifications():
    """Start the medicine notification system"""
    notification_thread = threading.Thread(target=check_medicine_times, daemon=True)
    notification_thread.start()
    print("🔔 [MEDICINE] Medicine notification system started")

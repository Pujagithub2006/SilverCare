from twilio.rest import Client

TWILIO_CONFIG = {
    "ACCOUNT_SID": "ACac18cc74bcf1b1190b4f12209bfab258",      
    "AUTH_TOKEN": "e827f5e89c1fe9f2144af636c2da5c35",       
    "TWILIO_PHONE": "+15707084443",              
}

# Verification flag - set to True since valid credentials are provided
TWILIO_ENABLED = True


class TwilioService:
    """Handles all Twilio phone call operations"""
    
    def __init__(self):
        """Initialize Twilio client"""
        if TWILIO_ENABLED:
            self.client = Client(
                TWILIO_CONFIG["ACCOUNT_SID"],
                TWILIO_CONFIG["AUTH_TOKEN"]
            )
        else:
            self.client = None
            print("[TWILIO] ⚠️ WARNING: Twilio not configured!")
    
    def send_fall_alert_sms(self, guardian_phone, elderly_name, location, device_id):
        """
        Send SMS alert to guardian about fall detection
        
        Args:
            guardian_phone: Guardian's phone number
            elderly_name: Name of elderly person
            location: Location of elderly person
            device_id: Device ID that detected the fall
        
        Returns:
            True if SMS sent, False otherwise
        """
        
        if not TWILIO_ENABLED:
            print(f"[SMS] Mock SMS to: {guardian_phone}")
            print(f"[SMS] Message: FALL ALERT - {elderly_name} at {location}")
            return True
        
        try:
            message = self.client.messages.create(
                body=f"🚨 SILVERCARE ALERT: Fall detected for {elderly_name} at {location} (Device: {device_id}). Please check on them immediately.",
                from_=TWILIO_CONFIG["TWILIO_PHONE"],
                to=guardian_phone
            )
            
            print(f"[SMS] ✅ Fall alert SMS sent!")
            print(f"[SMS] Message SID: {message.sid}")
            print(f"[SMS] To: {guardian_phone}")
            
            return True
            
        except Exception as e:
            print(f"[SMS] ❌ Error sending SMS: {e}")
            return False
    
    def send_urgent_alert_sms(self, guardian_phone, elderly_name, location, device_id):
        """
        Send URGENT SMS alert when elderly doesn't respond to fall alert
        
        Args:
            guardian_phone: Guardian's phone number
            elderly_name: Name of elderly person
            location: Location of elderly person
            device_id: Device ID that detected the fall
        
        Returns:
            True if SMS sent, False otherwise
        """
        
        if not TWILIO_ENABLED:
            print(f"[SMS] Mock URGENT SMS to: {guardian_phone}")
            print(f"[SMS] Message: URGENT - {elderly_name} NOT RESPONDING at {location}")
            return True
        
        try:
            message = self.client.messages.create(
                body=f"🚨🚨 URGENT SILVERCARE ALERT: {elderly_name} has NOT RESPONDED to fall alert at {location}! This is an emergency. Please call them immediately or check on them. Device: {device_id}",
                from_=TWILIO_CONFIG["TWILIO_PHONE"],
                to=guardian_phone
            )
            
            print(f"[SMS] ✅ URGENT alert SMS sent!")
            print(f"[SMS] Message SID: {message.sid}")
            print(f"[SMS] To: {guardian_phone}")
            
            return True
            
        except Exception as e:
            print(f"[SMS] ❌ Error sending urgent SMS: {e}")
            return False
    
    def make_emergency_call(self, guardian_phone, elderly_name, location):
        """
        Make an emergency call to guardian
        
        Args:
            guardian_phone: Guardian's phone number (e.g., "+919876543210")
            elderly_name: Name of elderly person
            location: Location of elderly person
        
        Returns:
            True if call initiated, False otherwise
        """
        
        if not TWILIO_ENABLED:
            print(f"[TWILIO] Twilio not enabled. Mock call to: {guardian_phone}")
            print(f"[TWILIO] Message: Emergency call from {elderly_name}")
            return True
        
        try:
            # TwiML is Twilio's XML-based language for controlling calls
            # This creates an automated message that will be read to the guardian
            message = f"""
            <Response>
                <Say voice="alice">
                    Emergency alert! Your ward, {elderly_name}, needs immediate help.
                    They are located at {location}.
                    Please respond immediately.
                </Say>
                <Pause length="2"/>
                <Say voice="alice">
                    Press any key to acknowledge this emergency.
                </Say>
            </Response>
            """
            
            call = self.client.calls.create(
                to=guardian_phone,
                from_=TWILIO_CONFIG["TWILIO_PHONE"],
                twiml=message
            )
            
            print(f"[TWILIO] ✅ Emergency call initiated!")
            print(f"[TWILIO] Call SID: {call.sid}")
            print(f"[TWILIO] To: {guardian_phone}")
            print(f"[TWILIO] Message: Emergency from {elderly_name} at {location}")
            
            return True
            
        except Exception as e:
            print(f"[TWILIO] ❌ Error making call: {e}")
            return False
    
    def make_no_response_alert_call(self, guardian_phone, elderly_name, location):
        """
        Make urgent call when elderly doesn't respond
        INCLUDES SIREN SOUND
        
        Args:
            guardian_phone: Guardian's phone number
            elderly_name: Name of elderly person
            location: Location of elderly person
        
        Returns:
            True if call initiated, False otherwise
        """
        
        if not TWILIO_ENABLED:
            print(f"[TWILIO] Mock URGENT call to: {guardian_phone}")
            print(f"[TWILIO] SIREN: 🚨🚨🚨")
            return True
        
        try:
            # This creates a more urgent call with warnings
            message = f"""
            <Response>
                <Play loop="3">
                    https://raw.githubusercontent.com/twilio-labs/media-repository/master/alarms/siren.mp3
                </Play>
                <Say voice="alice">
                    URGENT! URGENT! URGENT!
                </Say>
                <Say voice="alice">
                    Your ward, {elderly_name}, has NOT responded to the fall alert!
                    This is an emergency.
                    They are at {location}.
                </Say>
                <Say voice="alice">
                    Please respond immediately by pressing any key.
                </Say>
            </Response>
            """
            
            call = self.client.calls.create(
                to=guardian_phone,
                from_=TWILIO_CONFIG["TWILIO_PHONE"],
                twiml=message
            )
            
            print(f"[TWILIO] 🚨 URGENT call initiated!")
            print(f"[TWILIO] Call SID: {call.sid}")
            print(f"[TWILIO] NO RESPONSE ALERT - Elderly did not confirm safety")
            
            return True
            
        except Exception as e:
            print(f"[TWILIO] ❌ Error making urgent call: {e}")
            return False
    
    def make_safe_confirmation_call(self, guardian_phone, elderly_name):
        """
        Make a quick call to confirm elderly is safe (false alarm)
        
        Args:
            guardian_phone: Guardian's phone number
            elderly_name: Name of elderly person
        
        Returns:
            True if call initiated, False otherwise
        """
        
        if not TWILIO_ENABLED:
            print(f"[TWILIO] Mock call to: {guardian_phone}")
            print(f"[TWILIO] Message: All clear - {elderly_name} is safe")
            return True
        
        try:
            message = f"""
            <Response>
                <Say voice="alice">
                    All clear. Your ward, {elderly_name}, confirmed they are safe.
                    This was a false alarm.
                </Say>
            </Response>
            """
            
            call = self.client.calls.create(
                to=guardian_phone,
                from_=TWILIO_CONFIG["TWILIO_PHONE"],
                twiml=message
            )
            
            print(f"[TWILIO] ✅ Safe confirmation call sent")
            print(f"[TWILIO] Call SID: {call.sid}")
            
            return True
            
        except Exception as e:
            print(f"[TWILIO] ❌ Error making call: {e}")
            return False


# Create a global instance
twilio_service = TwilioService()

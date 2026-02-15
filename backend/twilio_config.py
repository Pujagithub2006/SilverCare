"""
Twilio Configuration for SMS Notifications
Replace with your actual Twilio credentials
"""

TWILIO_ACCOUNT_SID = 'ACyour_twilio_account_sid'  # Replace with your Account SID
TWILIO_AUTH_TOKEN = 'your_twilio_auth_token'    # Replace with your Auth Token
TWILIO_PHONE_NUMBER = '+1234567890'           # Replace with your Twilio phone number

def send_medicine_sms(phone_number, elderly_name, medicine_name, dosage):
    """Send SMS notification to elderly person"""
    try:
        from twilio.rest import Client
        from twilio.base.exceptions import TwilioRestException
        
        client = Client(TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN)
        
        message = f"🔔 SilverCare Reminder: Time to take your medicine! {medicine_name} - {dosage}. Please take it now and stay healthy!"
        
        client.messages.create(
            body=message,
            from_=TWILIO_PHONE_NUMBER,
            to=phone_number
        )
        
        print(f"📱 [SMS] Sent to {phone_number}: {message}")
        return True
        
    except TwilioRestException as e:
        print(f"❌ [SMS ERROR] Failed to send SMS: {e}")
        return False
    except Exception as e:
        print(f"❌ [SMS ERROR] Unexpected error: {e}")
        return False

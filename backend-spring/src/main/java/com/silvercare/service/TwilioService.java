package com.silvercare.service;

import com.twilio.Twilio;
import com.twilio.rest.api.v2010.account.Call;
import com.twilio.rest.api.v2010.account.Message;
import com.twilio.type.PhoneNumber;
import com.twilio.type.Twiml;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import jakarta.annotation.PostConstruct;

@Service
public class TwilioService {

    @Value("${twilio.account.sid:YOUR_TWILIO_ACCOUNT_SID}")
    private String accountSid;

    @Value("${twilio.auth.token:YOUR_TWILIO_AUTH_TOKEN}")
    private String authToken;

    @Value("${twilio.phone.number:YOUR_TWILIO_PHONE_NUMBER}")
    private String fromPhone;

    @Value("${twilio.enabled:true}")
    private boolean twilioEnabled;

    @PostConstruct
    public void init() {
        if (twilioEnabled && accountSid != null && !accountSid.isEmpty() && !accountSid.startsWith("YOUR_")) {
            try {
                Twilio.init(accountSid, authToken);
                System.out.println("📞 [TWILIO] Twilio client initialized successfully");
            } catch (Exception e) {
                System.err.println("⚠️ [TWILIO] Error initializing Twilio client: " + e.getMessage());
            }
        } else {
            System.out.println("⚠️ [TWILIO] Twilio not configured or disabled");
        }
    }

    public boolean sendFallAlertSms(String guardianPhone, String elderlyName, String location, String deviceId) {
        if (!twilioEnabled) {
            System.out.println("[SMS] Mock SMS to: " + guardianPhone);
            System.out.println("[SMS] Message: FALL ALERT - " + elderlyName + " at " + location);
            return true;
        }

        try {
            String bodyText = String.format("🚨 SILVERCARE ALERT: Fall detected for %s at %s (Device: %s). Please check on them immediately.",
                    elderlyName, location, deviceId);
            Message message = Message.creator(
                    new PhoneNumber(guardianPhone),
                    new PhoneNumber(fromPhone),
                    bodyText
            ).create();

            System.out.println("[SMS] ✅ Fall alert SMS sent! SID: " + message.getSid());
            return true;
        } catch (Exception e) {
            System.err.println("[SMS] ❌ Error sending SMS: " + e.getMessage());
            return false;
        }
    }

    public boolean sendUrgentAlertSms(String guardianPhone, String elderlyName, String location, String deviceId) {
        if (!twilioEnabled) {
            System.out.println("[SMS] Mock URGENT SMS to: " + guardianPhone);
            return true;
        }

        try {
            String bodyText = String.format("🚨🚨 URGENT SILVERCARE ALERT: %s has NOT RESPONDED to fall alert at %s! This is an emergency. Please call them immediately or check on them. Device: %s",
                    elderlyName, location, deviceId);
            Message message = Message.creator(
                    new PhoneNumber(guardianPhone),
                    new PhoneNumber(fromPhone),
                    bodyText
            ).create();

            System.out.println("[SMS] ✅ URGENT alert SMS sent! SID: " + message.getSid());
            return true;
        } catch (Exception e) {
            System.err.println("[SMS] ❌ Error sending urgent SMS: " + e.getMessage());
            return false;
        }
    }

    public boolean makeEmergencyCall(String guardianPhone, String elderlyName, String location) {
        if (!twilioEnabled) {
            System.out.println("[TWILIO] Mock emergency call to: " + guardianPhone);
            return true;
        }

        try {
            String twimlXml = String.format("""
                    <Response>
                        <Say voice="alice">
                            Emergency alert! Your ward, %s, needs immediate help.
                            They are located at %s.
                            Please respond immediately.
                        </Say>
                        <Pause length="2"/>
                        <Say voice="alice">
                            Press any key to acknowledge this emergency.
                        </Say>
                    </Response>
                    """, elderlyName, location);

            Call call = Call.creator(
                    new PhoneNumber(guardianPhone),
                    new PhoneNumber(fromPhone),
                    new Twiml(twimlXml)
            ).create();

            System.out.println("[TWILIO] ✅ Emergency call initiated! SID: " + call.getSid());
            return true;
        } catch (Exception e) {
            System.err.println("[TWILIO] ❌ Error making call: " + e.getMessage());
            return false;
        }
    }

    public boolean makeNoResponseAlertCall(String guardianPhone, String elderlyName, String location) {
        if (!twilioEnabled) {
            System.out.println("[TWILIO] Mock URGENT call to: " + guardianPhone + " SIREN: 🚨🚨🚨");
            return true;
        }

        try {
            String twimlXml = String.format("""
                    <Response>
                        <Play loop="3">
                            https://raw.githubusercontent.com/twilio-labs/media-repository/master/alarms/siren.mp3
                        </Play>
                        <Say voice="alice">
                            URGENT! URGENT! URGENT!
                        </Say>
                        <Say voice="alice">
                            Your ward, %s, has NOT responded to the fall alert!
                            This is an emergency.
                            They are at %s.
                        </Say>
                        <Say voice="alice">
                            Please respond immediately by pressing any key.
                        </Say>
                    </Response>
                    """, elderlyName, location);

            Call call = Call.creator(
                    new PhoneNumber(guardianPhone),
                    new PhoneNumber(fromPhone),
                    new Twiml(twimlXml)
            ).create();

            System.out.println("[TWILIO] 🚨 URGENT call initiated! SID: " + call.getSid());
            return true;
        } catch (Exception e) {
            System.err.println("[TWILIO] ❌ Error making urgent call: " + e.getMessage());
            return false;
        }
    }
}

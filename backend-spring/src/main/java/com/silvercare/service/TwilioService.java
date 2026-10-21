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

    @Value("${twilio.account.sid:}")
    private String accountSid;

    @Value("${twilio.auth.token:}")
    private String authToken;

    @Value("${twilio.phone.number:}")
    private String fromPhone;

    private volatile boolean initialized = false;
    private volatile String lastErrorMessage = "";

    @PostConstruct
    public void init() {
        if (accountSid != null && !accountSid.trim().isEmpty() && authToken != null && !authToken.trim().isEmpty()) {
            try {
                Twilio.init(accountSid, authToken);
                initialized = true;
                System.out.println("[TWILIO] Live Twilio client initialized successfully with Account SID: " + accountSid);
            } catch (Exception e) {
                lastErrorMessage = "Twilio.init failed: " + e.getMessage();
                System.err.println("[TWILIO ERROR] Failed to initialize live Twilio client: " + e.getMessage());
            }
        } else {
            lastErrorMessage = "Twilio Account SID or Auth Token missing. Set TWILIO_ACCOUNT_SID and TWILIO_AUTH_TOKEN.";
            System.err.println("[TWILIO WARNING] Twilio Account SID or Auth Token missing. Please set TWILIO_ACCOUNT_SID and TWILIO_AUTH_TOKEN in application.properties or environment variables.");
        }
    }

    public boolean isInitialized() {
        return initialized;
    }

    public boolean hasAccountSid() {
        return accountSid != null && !accountSid.trim().isEmpty();
    }

    public boolean hasAuthToken() {
        return authToken != null && !authToken.trim().isEmpty();
    }

    public String getFromPhone() {
        return fromPhone;
    }

    public String getLastErrorMessage() {
        return lastErrorMessage;
    }

    public static String formatPhoneNumber(String phone) {
        if (phone == null) return null;
        String trimmed = phone.trim();
        if (trimmed.isEmpty()) return "";

        String cleaned = trimmed.replaceAll("[^0-9+]", "");
        if (cleaned.isEmpty()) return "";

        if (cleaned.startsWith("+")) {
            return cleaned;
        }

        if (cleaned.length() == 10) {
            return "+91" + cleaned;
        }

        if (cleaned.length() == 11 && cleaned.startsWith("0")) {
            return "+91" + cleaned.substring(1);
        }

        return "+" + cleaned;
    }

    private String getFormattedFromPhone() {
        return formatPhoneNumber(fromPhone);
    }

    public boolean sendFallAlertSms(String guardianPhone, String elderlyName, String location, String deviceId, Double lat, Double lng) {
        String targetPhone = formatPhoneNumber(guardianPhone);
        String senderPhone = getFormattedFromPhone();
        if (targetPhone == null || targetPhone.isEmpty()) {
            lastErrorMessage = "Target guardian phone is empty or null.";
            return false;
        }
        if (senderPhone == null || senderPhone.isEmpty()) {
            lastErrorMessage = "Twilio sender phone number is empty. Please set TWILIO_PHONE_NUMBER.";
            return false;
        }
        try {
            String locationText = location;
            if (lat != null && lng != null) {
                locationText = String.format("GPS: %.6f, %.6f (%s)", lat, lng, location != null ? location : "Home");
            }
            String bodyText = String.format(
                    "SILVERCARE ALERT: Fall detected for %s at %s (Device: %s). Please check on them immediately.",
                    elderlyName != null ? elderlyName : "Senior", locationText, deviceId != null ? deviceId : "Belt");
            Message message = Message.creator(
                    new PhoneNumber(targetPhone),
                    new PhoneNumber(senderPhone),
                    bodyText).create();

            System.out.println("[TWILIO SMS LIVE] Fall alert SMS sent to " + targetPhone + "! SID: " + message.getSid());
            return true;
        } catch (Exception e) {
            lastErrorMessage = e.getClass().getSimpleName() + ": " + e.getMessage();
            System.err.println("[TWILIO SMS ERROR] Error sending live SMS to " + targetPhone + ": " + e.getMessage());
            return false;
        }
    }

    public boolean sendUrgentAlertSms(String guardianPhone, String elderlyName, String location, String deviceId) {
        String targetPhone = formatPhoneNumber(guardianPhone);
        String senderPhone = getFormattedFromPhone();
        if (targetPhone == null || targetPhone.isEmpty()) {
            lastErrorMessage = "Target guardian phone is empty or null.";
            return false;
        }
        if (senderPhone == null || senderPhone.isEmpty()) {
            lastErrorMessage = "Twilio sender phone number is empty. Please set TWILIO_PHONE_NUMBER.";
            return false;
        }
        try {
            String bodyText = String.format(
                    "URGENT SILVERCARE ALERT: %s has NOT RESPONDED to fall alert at %s! Emergency assistance required. Device: %s",
                    elderlyName != null ? elderlyName : "Senior", location != null ? location : "Home", deviceId != null ? deviceId : "Belt");
            Message message = Message.creator(
                    new PhoneNumber(targetPhone),
                    new PhoneNumber(senderPhone),
                    bodyText).create();

            System.out.println("[TWILIO SMS LIVE] URGENT alert SMS sent to " + targetPhone + "! SID: " + message.getSid());
            return true;
        } catch (Exception e) {
            lastErrorMessage = e.getClass().getSimpleName() + ": " + e.getMessage();
            System.err.println("[TWILIO SMS ERROR] Error sending live urgent SMS to " + targetPhone + ": " + e.getMessage());
            return false;
        }
    }

    public boolean makeEmergencyCall(String guardianPhone, String elderlyName, String location) {
        String targetPhone = formatPhoneNumber(guardianPhone);
        String senderPhone = getFormattedFromPhone();
        if (targetPhone == null || targetPhone.isEmpty()) {
            lastErrorMessage = "Target guardian phone is empty or null.";
            return false;
        }
        if (senderPhone == null || senderPhone.isEmpty()) {
            lastErrorMessage = "Twilio sender phone number is empty. Please set TWILIO_PHONE_NUMBER.";
            return false;
        }
        try {
            String twimlXml = String.format("""
                    <Response>
                        <Say voice="alice">
                            Emergency alert! Your ward, %s, needs immediate help at %s.
                            Please respond immediately.
                        </Say>
                        <Pause length="2"/>
                        <Say voice="alice">
                            Press any key to acknowledge this emergency.
                        </Say>
                    </Response>
                    """, elderlyName != null ? elderlyName : "Senior", location != null ? location : "Home");

            Call call = Call.creator(
                    new PhoneNumber(targetPhone),
                    new PhoneNumber(senderPhone),
                    new Twiml(twimlXml)).create();

            System.out.println("[TWILIO CALL LIVE] Emergency call initiated to " + targetPhone + "! SID: " + call.getSid());
            return true;
        } catch (Exception e) {
            lastErrorMessage = e.getClass().getSimpleName() + ": " + e.getMessage();
            System.err.println("[TWILIO CALL ERROR] Error making live call to " + targetPhone + ": " + e.getMessage());
            return false;
        }
    }

    public boolean makeNoResponseAlertCall(String guardianPhone, String elderlyName, String location) {
        String targetPhone = formatPhoneNumber(guardianPhone);
        String senderPhone = getFormattedFromPhone();
        if (targetPhone == null || targetPhone.isEmpty()) return false;
        try {
            String twimlXml = String.format("""
                    <Response>
                        <Play loop="2">
                            https://raw.githubusercontent.com/twilio-labs/media-repository/master/alarms/siren.mp3
                        </Play>
                        <Say voice="alice">
                            URGENT EMERGENCY! Your ward, %s, has NOT responded to fall alert at %s.
                        </Say>
                    </Response>
                    """, elderlyName != null ? elderlyName : "Senior", location != null ? location : "Home");

            Call call = Call.creator(
                    new PhoneNumber(targetPhone),
                    new PhoneNumber(senderPhone),
                    new Twiml(twimlXml)).create();

            System.out.println("[TWILIO CALL LIVE] URGENT siren call initiated to " + targetPhone + "! SID: " + call.getSid());
            return true;
        } catch (Exception e) {
            System.err.println("[TWILIO CALL ERROR] Error making urgent call to " + targetPhone + ": " + e.getMessage());
            return false;
        }
    }

    public boolean sendPrefallSms(String toPhone, String elderlyName, String location, Number hr, Number spo2, Number temp, Double lat, Double lng) {
        String targetPhone = formatPhoneNumber(toPhone);
        if (targetPhone == null || targetPhone.isEmpty()) return false;
        
        String locationText = location;
        if (lat != null && lng != null) {
            locationText = String.format("GPS: %.6f, %.6f (%s)", lat, lng, location != null ? location : "Home");
        }
        
        String msg = String.format(
                "⚠️ PRE-FALL ALERT: Instability detected for %s at %s. Vitals: HR: %s BPM, SpO2: %s%%, Temp: %s°C. Please check immediately.",
                elderlyName != null ? elderlyName : "Senior",
                locationText,
                hr != null ? hr : "--",
                spo2 != null ? spo2 : "--",
                temp != null ? temp : "--"
        );
        return sendSms(targetPhone, msg);
    }

    public boolean sendLocationRequestSms(String elderlyPhone, String elderlyName) {
        String targetPhone = formatPhoneNumber(elderlyPhone);
        if (targetPhone == null || targetPhone.isEmpty()) return false;
        
        String msg = String.format(
                "SilverCare: Your guardian has requested your current location. Please share your GPS coordinates if possible.",
                elderlyName != null ? elderlyName : "Senior"
        );
        return sendSms(targetPhone, msg);
    }

    public boolean makeNeighborCall(String neighborPhone, String elderlyName, String location) {
        String targetPhone = formatPhoneNumber(neighborPhone);
        String senderPhone = getFormattedFromPhone();
        if (targetPhone == null || targetPhone.isEmpty()) return false;
        try {
            String twimlXml = String.format("""
                    <Response>
                        <Say voice="alice">
                            Emergency notification for neighbor! Your neighbor, %s, needs urgent check at %s.
                            Please visit immediately.
                        </Say>
                    </Response>
                    """, elderlyName != null ? elderlyName : "Senior", location != null ? location : "Home");
            Call call = Call.creator(new PhoneNumber(targetPhone), new PhoneNumber(senderPhone), new Twiml(twimlXml)).create();
            System.out.println("[TWILIO CALL LIVE] Neighbor call initiated to " + targetPhone + "! SID: " + call.getSid());
            return true;
        } catch (Exception e) {
            System.err.println("[TWILIO CALL ERROR] Error calling neighbor " + targetPhone + ": " + e.getMessage());
            return false;
        }
    }

    public boolean makeSecondaryGuardianCall(String guardianPhone, String elderlyName, String location) {
        String targetPhone = formatPhoneNumber(guardianPhone);
        String senderPhone = getFormattedFromPhone();
        if (targetPhone == null || targetPhone.isEmpty()) return false;
        try {
            String twimlXml = String.format("""
                    <Response>
                        <Say voice="alice">
                            Secondary Guardian Escalation Alert! Your ward, %s, experienced a fall at %s and primary contact is unacknowledged.
                            Please respond immediately.
                        </Say>
                    </Response>
                    """, elderlyName != null ? elderlyName : "Senior", location != null ? location : "Home");
            Call call = Call.creator(new PhoneNumber(targetPhone), new PhoneNumber(senderPhone), new Twiml(twimlXml)).create();
            System.out.println("[TWILIO CALL LIVE] Secondary Guardian call initiated to " + targetPhone + "! SID: " + call.getSid());
            return true;
        } catch (Exception e) {
            System.err.println("[TWILIO CALL ERROR] Error calling secondary guardian " + targetPhone + ": " + e.getMessage());
            return false;
        }
    }

    public boolean sendSosSmsAndCall(String guardianPhone, String elderlyName, Double lat, Double lng) {
        String targetPhone = formatPhoneNumber(guardianPhone);
        String senderPhone = getFormattedFromPhone();
        if (targetPhone == null || targetPhone.isEmpty()) return false;
        String locStr = (lat != null && lng != null) ? String.format("(Lat: %.4f, Lng: %.4f)", lat, lng) : "GPS Location";
        String msg = String.format("🆘 EMERGENCY SOS BUTTON PRESSED: %s pressed the SOS Button at %s! Go quickly!", elderlyName != null ? elderlyName : "Senior", locStr);
        sendSms(targetPhone, msg);

        try {
            String twimlXml = String.format("""
                    <Response>
                        <Say voice="alice">
                            Emergency SOS alert! %s pressed the emergency SOS button. Immediate help required!
                        </Say>
                    </Response>
                    """, elderlyName != null ? elderlyName : "Senior");
            Call call = Call.creator(new PhoneNumber(targetPhone), new PhoneNumber(senderPhone), new Twiml(twimlXml)).create();
            System.out.println("[TWILIO CALL LIVE] SOS call initiated to " + targetPhone + "! SID: " + call.getSid());
            return true;
        } catch (Exception e) {
            System.err.println("[TWILIO CALL ERROR] Error calling SOS to " + targetPhone + ": " + e.getMessage());
            return false;
        }
    }

    public boolean sendSms(String toPhone, String messageText) {
        String targetPhone = formatPhoneNumber(toPhone);
        String senderPhone = getFormattedFromPhone();
        if (targetPhone == null || targetPhone.isEmpty()) {
            lastErrorMessage = "Target phone number is empty or null.";
            return false;
        }
        if (senderPhone == null || senderPhone.isEmpty()) {
            lastErrorMessage = "Twilio sender phone number is empty. Please set TWILIO_PHONE_NUMBER.";
            return false;
        }
        try {
            Message message = Message.creator(
                    new PhoneNumber(targetPhone),
                    new PhoneNumber(senderPhone),
                    messageText
            ).create();
            System.out.println("[TWILIO SMS LIVE] Emergency SMS sent to " + targetPhone + "! SID: " + message.getSid());
            return true;
        } catch (Exception e) {
            lastErrorMessage = e.getClass().getSimpleName() + ": " + e.getMessage();
            System.err.println("[TWILIO SMS ERROR] Error sending live SMS to " + targetPhone + ": " + e.getMessage());
            return false;
        }
    }
}

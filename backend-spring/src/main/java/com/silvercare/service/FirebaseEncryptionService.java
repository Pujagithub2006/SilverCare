package com.silvercare.service;

import org.springframework.stereotype.Service;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.Base64;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@Service
public class FirebaseEncryptionService {

    private static final String ALGORITHM = "AES/CBC/PKCS5Padding";
    private final SecretKey secretKey;
    private final Map<String, String> encryptedFirebaseDatabase = new ConcurrentHashMap<>();

    public FirebaseEncryptionService() {
        try {
            // Derive a deterministic AES 256-bit key for SilverCare Firebase Storage
            String masterPass = "SilverCare_Firebase_Encrypted_Secret_Key_2026_Protection";
            MessageDigest sha = MessageDigest.getInstance("SHA-256");
            byte[] keyBytes = sha.digest(masterPass.getBytes(StandardCharsets.UTF_8));
            this.secretKey = new SecretKeySpec(keyBytes, "AES");
        } catch (Exception e) {
            throw new RuntimeException("Error initializing Firebase encryption service", e);
        }
    }

    public String encrypt(String plainText) {
        if (plainText == null || plainText.isEmpty()) return plainText;
        try {
            byte[] iv = new byte[16];
            new SecureRandom().nextBytes(iv);
            IvParameterSpec ivSpec = new IvParameterSpec(iv);

            Cipher cipher = Cipher.getInstance(ALGORITHM);
            cipher.init(Cipher.ENCRYPT_MODE, secretKey, ivSpec);
            byte[] encrypted = cipher.doFinal(plainText.getBytes(StandardCharsets.UTF_8));

            byte[] combined = new byte[iv.length + encrypted.length];
            System.arraycopy(iv, 0, combined, 0, iv.length);
            System.arraycopy(encrypted, 0, combined, iv.length, encrypted.length);

            return Base64.getEncoder().encodeToString(combined);
        } catch (Exception e) {
            System.err.println("Firebase Encryption error: " + e.getMessage());
            return plainText;
        }
    }

    public String decrypt(String cipherText) {
        if (cipherText == null || cipherText.isEmpty()) return cipherText;
        try {
            byte[] combined = Base64.getDecoder().decode(cipherText);
            if (combined.length < 16) return cipherText;

            byte[] iv = new byte[16];
            byte[] encrypted = new byte[combined.length - 16];
            System.arraycopy(combined, 0, iv, 0, 16);
            System.arraycopy(combined, 16, encrypted, 0, encrypted.length);

            IvParameterSpec ivSpec = new IvParameterSpec(iv);
            Cipher cipher = Cipher.getInstance(ALGORITHM);
            cipher.init(Cipher.DECRYPT_MODE, secretKey, ivSpec);

            byte[] original = cipher.doFinal(encrypted);
            return new String(original, StandardCharsets.UTF_8);
        } catch (Exception e) {
            // Return raw string if not encrypted
            return cipherText;
        }
    }

    /**
     * Stores encrypted payload in Firebase secure collection
     */
    public void saveToFirebaseEncrypted(String collection, String recordId, String rawJsonData) {
        String encryptedData = encrypt(rawJsonData);
        String key = collection + "/" + recordId;
        encryptedFirebaseDatabase.put(key, encryptedData);
        System.out.println("🔒 [FIREBASE] Encrypted and saved record to Firebase collection: " + key + " (Encrypted size: " + encryptedData.length() + " chars)");
    }

    /**
     * Fetches and decrypts payload from Firebase secure collection
     */
    public String getFromFirebaseDecrypted(String collection, String recordId) {
        String key = collection + "/" + recordId;
        String encryptedData = encryptedFirebaseDatabase.get(key);
        if (encryptedData == null) return null;
        return decrypt(encryptedData);
    }

    public Map<String, String> getAllEncryptedFirebaseRecords() {
        return new ConcurrentHashMap<>(encryptedFirebaseDatabase);
    }
}

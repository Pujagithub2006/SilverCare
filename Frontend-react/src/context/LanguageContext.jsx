import React, { createContext, useContext, useState, useEffect } from 'react';
import axios from 'axios';

const en = {
  welcome: "Welcome",
  belt_connected: "Belt Connected",
  belt_disconnected: "Belt Disconnected",
  sos: "SOS",
  press_for_help: "PRESS FOR HELP",
  press_and_hold: "Press and hold for 3 seconds",
  reminders: "Reminders",
  alerts: "Alerts",
  no_alerts: "No new alerts",
  caregiver_notes: "Caregiver Advice & Notes",
  no_notes: "No advice notes from guardian yet.",
  home: "Home",
  assistant: "Mitra",
  health: "Health",
  logout: "Logout",
  confirm_logout: "Are you sure you want to logout?",
  health_dashboard: "Health Dashboard",
  total_doses: "Total Doses",
  taken: "Taken",
  missed: "Missed",
  todays_medicines: "Today's Medicines",
  no_medicines: "No medicines scheduled today",
  active: "Active",
  offline: "Offline",
  active_reminder: "Active Medicine Reminder!",
  mark_taken: "✓ Taken",
  snooze: "⏰ Snooze",
  mark_missed: "✗ Missed",
  close: "Close",
  medicine_schedule: "Medicine Schedule",
  ai_title: "Mitra 💙",
  ask_ai: "Ask Mitra...",
  ai_init_msg: "Hello! I am Mitra 💙 your health companion. How can I help you today?",
  ai_help_msg: "I am here with you! If you need emergency help, press the big red SOS button on your Home screen.",
  ai_med_msg: "Your scheduled medicines are tracked in your Health & Reminders section. Remember to take them with water!",
  ai_greet_msg: "Hello there! Wishing you a peaceful and healthy day! 😊",
  daily_quote_title: "Daily Inspiration 💖",
  daily_quote: "Every day is a new gift. Stay happy, take your medicines with a smile, and know you are deeply loved!",
  device_status_title: "Wearable Device Status 🛡️",
  smart_belt: "Smart Belt",
  wrist_band: "Wrist Band",
  worn: "Worn",
  not_worn: "Not Worn"
};

const hi = {
  welcome: "स्वागत है",
  belt_connected: "बेल्ट जुड़ा हुआ है",
  belt_disconnected: "बेल्ट डिस्कनेक्ट है",
  sos: "मदद (SOS)",
  press_for_help: "मदद के लिए दबाएं",
  press_and_hold: "3 सेकंड तक दबाए रखें",
  reminders: "दवा की यादें",
  alerts: "सूचनाएं",
  no_alerts: "कोई नया अलर्ट नहीं",
  caregiver_notes: "अभिभावक की सलाह और नोट्स",
  no_notes: "अभिभावक की तरफ से कोई नोट नहीं है।",
  home: "होम",
  assistant: "मित्र",
  health: "सेहत",
  logout: "लॉगआउट",
  confirm_logout: "क्या आप लॉगआउट करना चाहते हैं?",
  health_dashboard: "स्वास्थ्य डैशबोर्ड",
  total_doses: "कुल खुराक",
  taken: "खा लिया",
  missed: "छूट गया",
  todays_medicines: "आज की दवाएं",
  no_medicines: "आज कोई दवा निर्धारित नहीं है",
  active: "चालू",
  offline: "ऑफलाइन",
  active_reminder: "दवा का समय हो गया!",
  mark_taken: "✓ खा लिया",
  snooze: "⏰ याद दिलाएं",
  mark_missed: "✗ छूट गई",
  close: "बंद करें",
  medicine_schedule: "दवा का समय-पत्रक",
  ai_title: "मित्र 💙",
  ask_ai: "मित्र से पूछें...",
  ai_init_msg: "नमस्ते! मैं मित्र 💙 आपका स्वास्थ्य साथी हूँ। मैं आपकी क्या मदद कर सकता हूँ?",
  ai_help_msg: "मैं आपके साथ हूँ! अगर आपको आपातकालीन मदद चाहिए, तो होम स्क्रीन पर बड़ा लाल SOS बटन दबाएं।",
  ai_med_msg: "आपकी तय दवाएं स्वास्थ्य और रिमाइंडर अनुभाग में हैं। उन्हें पानी के साथ लेना न भूलें!",
  ai_greet_msg: "नमस्ते! आपका दिन शुभ और स्वास्थ्यवर्धक रहे! 😊",
  daily_quote_title: "आज की प्रेरणा 💖",
  daily_quote: "हर नया दिन एक सुंदर उपहार है। मुस्कुराते रहें, अपनी दवाएं समय पर लें और याद रखें कि आप सबके लिए अनमोल हैं!",
  device_status_title: "सुरक्षा डिवाइस स्थिति 🛡️",
  smart_belt: "स्मार्ट बेल्ट",
  wrist_band: "रिस्ट बैंड",
  worn: "पहना हुआ",
  not_worn: "नहीं पहना"
};

const mr = {
  welcome: "स्वागत आहे",
  belt_connected: "बेल्ट जोडलेले आहे",
  belt_disconnected: "बेल्ट डिस्कनेक्ट आहे",
  sos: "मदत (SOS)",
  press_for_help: "मदतीसाठी दाबा",
  press_and_hold: "3 सेकंद दाबून ठेवा",
  reminders: "औषधांची आठवण",
  alerts: "सूचना",
  no_alerts: "कोणताही नवीन अलर्ट नाही",
  caregiver_notes: "पालकांचे सल्ले आणि नोट्स",
  no_notes: "पालकांकडून अजून कोणताही सल्ला आलेला नाही.",
  home: "होम",
  assistant: "मित्र",
  health: "आरोग्य",
  logout: "लॉगआउट",
  confirm_logout: "तुम्हाला लॉगआउट करायचे आहे का?",
  health_dashboard: "आरोग्य डॅशबोर्ड",
  total_doses: "एकूण डोस",
  taken: "घेतले",
  missed: "चुकले",
  todays_medicines: "आजची औषधे",
  no_medicines: "आज कोणतीही औषधे नियोजित नाहीत",
  active: "सक्रिय",
  offline: "ऑफलाइन",
  active_reminder: "औषध घेण्याची वेळ झाली!",
  mark_taken: "✓ घेतले",
  snooze: "⏰ नंतर आठवण करा",
  mark_missed: "✗ चुकले",
  close: "बंद करा",
  medicine_schedule: "औषधांचे वेळापत्रक",
  ai_title: "मित्र 💙",
  ask_ai: "मित्रला विचारा...",
  ai_init_msg: "नमस्कार! मी मित्र 💙 तुमचा आरोग्य सोबती आहे. मी तुम्हाला कशी मदत करू शकतो?",
  ai_help_msg: "मी तुमच्यासोबत आहे! तुम्हाला तातडीची मदत हवी असल्यास, होम स्क्रीनवरील मोठा लाल SOS बटण दाबा.",
  ai_med_msg: "तुमची नियोजित औषधे आरोग्य आणि आठवण विभागात आहेत. ती पाण्यासोबत घ्यायला विसरू नका!",
  ai_greet_msg: "नमस्कार! तुमचा आजचा दिवस आनंदी आणि निरोगी जावो! 😊",
  daily_quote_title: "आजची सुंदर प्रेरणा 💖",
  daily_quote: "प्रत्येक नवीन दिवस एक सुंदर भेट आहे. आनंदी राहा, वेळोवेळी औषधे घ्या आणि तुम्ही प्रत्येकासाठी खूप खास आहात!",
  device_status_title: "सुरक्षा डिव्हाइस स्थिती 🛡️",
  smart_belt: "स्मार्ट बेल्ट",
  wrist_band: "रिस्ट बँड",
  worn: "परिधान केले आहे",
  not_worn: "परिधान केलेले नाही"
};

const dictionaries = { en, hi, mr };

// Dynamic Translation API Cache
const apiTranslationCache = {};

const LanguageContext = createContext();

export const LanguageProvider = ({ children }) => {
  const [lang, setLang] = useState(() => {
    const elderlyId = localStorage.getItem('elderly_id');
    return (elderlyId && localStorage.getItem(`elderly_language_${elderlyId}`)) || localStorage.getItem('app_lang') || 'en';
  });

  const syncLanguageForElderly = (id) => {
    const targetId = id || localStorage.getItem('elderly_id');
    if (targetId) {
      const preferred = localStorage.getItem(`elderly_language_${targetId}`);
      if (preferred && preferred !== lang) {
        setLang(preferred);
      }
    }
  };

  useEffect(() => {
    const handleStorageChange = () => {
      const elderlyId = localStorage.getItem('elderly_id');
      const preferred = (elderlyId && localStorage.getItem(`elderly_language_${elderlyId}`)) || localStorage.getItem('app_lang') || 'en';
      setLang(preferred);
    };

    window.addEventListener('storage', handleStorageChange);
    return () => window.removeEventListener('storage', handleStorageChange);
  }, []);

  const changeLanguage = (newLang, targetElderlyId) => {
    setLang(newLang);
    localStorage.setItem('app_lang', newLang);
    const id = targetElderlyId || localStorage.getItem('elderly_id');
    if (id) {
      localStorage.setItem(`elderly_language_${id}`, newLang);
    }
  };

  const t = (key) => {
    return dictionaries[lang]?.[key] || dictionaries.en[key] || key;
  };

  // Dynamic API Translation function for arbitrary dynamic content (API-backed)
  const translateDynamic = async (text, overrideLang) => {
    const target = overrideLang || lang;
    if (!text || target === 'en') return text;

    const cacheKey = `${target}_${text}`;
    if (apiTranslationCache[cacheKey]) {
      return apiTranslationCache[cacheKey];
    }

    try {
      const res = await axios.get(`https://api.mymemory.translated.net/get?q=${encodeURIComponent(text)}&langpair=en|${target}`);
      if (res.data && res.data.responseData && res.data.responseData.translatedText) {
        const translated = res.data.responseData.translatedText;
        apiTranslationCache[cacheKey] = translated;
        return translated;
      }
    } catch (err) {
      console.warn('Dynamic API translation error:', err);
    }

    return t(text) || text;
  };

  return (
    <LanguageContext.Provider value={{ lang, changeLanguage, syncLanguageForElderly, t, translateDynamic }}>
      {children}
    </LanguageContext.Provider>
  );
};

export const useLanguage = () => useContext(LanguageContext);

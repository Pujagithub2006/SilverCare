import React, { useState, useRef, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { useLanguage } from '../context/LanguageContext';
import api, { sendChatMessage } from '../services/api';
import '../styles.css';

const AssistantPage = () => {
  const navigate = useNavigate();
  const { t } = useLanguage();

  const [chatHistory, setChatHistory] = useState([]);
  const [input, setInput] = useState('');
  const [loading, setLoading] = useState(false);
  const [isListening, setIsListening] = useState(false);
  const chatEndRef = useRef(null);

  const scrollToBottom = () => {
    chatEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  useEffect(() => {
    scrollToBottom();
  }, [chatHistory, loading]);

  useEffect(() => {
    if ('speechSynthesis' in window) {
      window.speechSynthesis.getVoices();
    }
  }, []);

  const speakText = (text) => {
    if ('speechSynthesis' in window) {
      window.speechSynthesis.cancel();
      // Clean emojis and symbols so speech flows naturally without reading emoji names
      const cleanText = text.replace(/[\u{1F600}-\u{1F64F}\u{1F300}-\u{1F5FF}\u{1F680}-\u{1F6FF}\u{2600}-\u{26FF}\u{2700}-\u{27BF}]/gu, '').replace(/[🙏🌸🌿☕🌙💧🚨💊🩺😴🥗👋😊]/g, '').trim();
      
      const utterance = new SpeechSynthesisUtterance(cleanText);

      // Select high quality natural human voices (e.g. Samantha, Google US English, Zira)
      const voices = window.speechSynthesis.getVoices();
      const naturalHumanVoice = voices.find(v =>
        (v.name.includes('Samantha') || v.name.includes('Google US English') || v.name.includes('Microsoft Zira') || v.name.includes('Natural') || v.name.includes('Neural'))
      ) || voices.find(v => v.lang === 'en-US' || v.lang === 'en-GB') || voices.find(v => v.lang.startsWith('en')) || voices[0];

      if (naturalHumanVoice) {
        utterance.voice = naturalHumanVoice;
      }

      utterance.rate = 1.0;  // Standard natural human speaking speed
      utterance.pitch = 1.0; // Natural human pitch
      utterance.volume = 1.0; // Clear human volume

      window.speechSynthesis.speak(utterance);
    }
  };

  const fullSeniorName = localStorage.getItem('elderly_name') || localStorage.getItem('user_name') || localStorage.getItem('name') || '';
  const currentSeniorFirstName = fullSeniorName ? fullSeniorName.trim().split(/\s+/)[0] : '';

  const handleSendText = async (textToSend) => {
    const userText = textToSend.trim();
    if (!userText || loading) return;

    setChatHistory((prev) => [...prev, { sender: 'user', text: userText }]);
    setInput('');
    setLoading(true);

    try {
      const elderlyId = localStorage.getItem('elderly_id') || fullSeniorName || 'default_senior';
      const rawData = await sendChatMessage(userText, elderlyId, currentSeniorFirstName, chatHistory);
      const botReply =
        rawData?.reply ||
        rawData?.message ||
        (typeof rawData === 'string' ? rawData : null) ||
        "I'm happy to chat about anything on your mind. How can I help you feel relaxed and healthy right now?";

      setChatHistory((prev) => [...prev, { sender: 'assistant', text: botReply }]);
      speakText(botReply);
    } catch (error) {
      console.error('Chatbot API error:', error);
      const lower = userText.toLowerCase();
      const nameGreet = currentSeniorFirstName ? ` ${currentSeniorFirstName}` : '';
      let fallbackMsg = "I am right here with you! Your health and comfort mean everything to me. What else is on your mind today?";
      
      if (lower.includes('my name is') || lower.includes('call me')) {
        fallbackMsg = `It is so wonderful to meet you${nameGreet}! I'm Mitra, your SilverCare health companion. How has your day been going?`;
      } else if (lower.includes('feeling good') || lower.includes('feel good') || lower.includes('great') || lower.includes('happy')) {
        fallbackMsg = `That is fantastic to hear${nameGreet}! Having a positive mood is great for your heart and health. Have you had a chance to get some fresh air today?`;
      } else if (lower.includes('alone') || lower.includes('lonely') || lower.includes('sad')) {
        fallbackMsg = "I am right here with you! You are never alone—I am always here to listen, keep you company, and talk with you. What would you like to chat about?";
      } else if (lower.includes('not well') || lower.includes('unwell') || lower.includes('sick') || lower.includes('ill') || lower.includes('bad')) {
        fallbackMsg = "I am so sorry to hear that you are not feeling well. Please sit down comfortably, drink a warm glass of water, and rest. Would you like me to notify your guardian or help check your medicines?";
      } else if (lower.includes('joke') || lower.includes('funny')) {
        fallbackMsg = "Why did the medicine go to school? To get a little smarter! A happy smile is the best medicine for the heart.";
      } else if (lower.length <= 15 && (lower.includes('hi') || lower.includes('hello') || lower.includes('hey'))) {
        fallbackMsg = `Hello${nameGreet}! It is wonderful to talk with you. How are you feeling today?`;
      }
      
      setChatHistory((prev) => [...prev, { sender: 'assistant', text: fallbackMsg }]);
      speakText(fallbackMsg);
    } finally {
      setLoading(false);
    }
  };

  const handleFormSubmit = (e) => {
    e.preventDefault();
    handleSendText(input);
  };

  const startVoiceInput = () => {
    const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (!SpeechRecognition) {
      alert('Voice recognition is not supported in this browser. Please type your message.');
      return;
    }
    try {
      const recognition = new SpeechRecognition();
      recognition.lang = 'en-US';
      recognition.continuous = false;
      recognition.interimResults = false;

      recognition.onstart = () => setIsListening(true);
      recognition.onend = () => setIsListening(false);
      recognition.onerror = () => setIsListening(false);

      recognition.onresult = (event) => {
        const transcript = event.results[0][0].transcript;
        if (transcript) {
          setInput(transcript);
          handleSendText(transcript);
        }
      };

      recognition.start();
    } catch (e) {
      console.error('Error starting voice recognition:', e);
      setIsListening(false);
    }
  };

  return (
    <div className="container" style={{ paddingBottom: '90px', display: 'flex', flexDirection: 'column', height: '100vh' }}>
      {/* Header */}
      <header className="header" style={{ borderBottom: '1px solid #e5e7eb', background: '#3b82f6', color: 'white', display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '16px 20px' }}>
        <button
          className="back-btn"
          onClick={() => navigate('/home')}
          title="Go Back"
          style={{ background: 'rgba(255,255,255,0.2)', border: 'none', color: 'white', padding: '8px', borderRadius: '50%', cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center' }}
        >
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>
        <h1 style={{ fontSize: '17px', fontWeight: '800', margin: 0, color: 'white', textAlign: 'center', flex: 1 }}>
          🤖 {t('ai_title')}
        </h1>
        <div style={{ width: '36px' }}></div>
      </header>

      {/* Chat Messages Container */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '20px', display: 'flex', flexDirection: 'column', gap: '12px', background: '#f8fafc' }}>
        
        {/* Initial Greeting Message */}
        <div
          style={{
            alignSelf: 'flex-start',
            backgroundColor: '#ffffff',
            color: '#1f2937',
            padding: '14px 18px',
            borderRadius: '20px 20px 20px 4px',
            maxWidth: '85%',
            fontSize: '15px',
            fontWeight: '500',
            textAlign: 'left',
            boxShadow: '0 2px 8px rgba(0,0,0,0.06)',
            border: '1px solid #e2e8f0'
          }}
        >
          Hello{currentSeniorFirstName ? ` ${currentSeniorFirstName}` : ''}! I am Mitra, your SilverCare companion. How can I help you feel relaxed and healthy today?
        </div>

        {/* Dynamic Chat Messages */}
        {chatHistory.map((msg, idx) => (
          <div
            key={idx}
            style={{
              alignSelf: msg.sender === 'user' ? 'flex-end' : 'flex-start',
              backgroundColor: msg.sender === 'user' ? '#2563eb' : '#ffffff',
              color: msg.sender === 'user' ? '#ffffff' : '#1f2937',
              padding: '14px 18px',
              borderRadius: msg.sender === 'user' ? '20px 20px 4px 20px' : '20px 20px 20px 4px',
              maxWidth: '85%',
              fontSize: '15px',
              fontWeight: '500',
              textAlign: 'left',
              boxShadow: '0 2px 8px rgba(0,0,0,0.06)',
              border: msg.sender === 'user' ? 'none' : '1px solid #e2e8f0',
              lineHeight: '1.5'
            }}
          >
            {msg.sender === 'assistant' && '🤖 '}
            {msg.text}
          </div>
        ))}

        {/* Loading Indicator */}
        {loading && (
          <div
            style={{
              alignSelf: 'flex-start',
              backgroundColor: '#ffffff',
              color: '#64748b',
              padding: '12px 18px',
              borderRadius: '20px 20px 20px 4px',
              fontSize: '14px',
              fontWeight: '600',
              boxShadow: '0 2px 8px rgba(0,0,0,0.06)',
              border: '1px solid #e2e8f0',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}
          >
            <span>🤖 Mitra AI is thinking...</span>
          </div>
        )}

        <div ref={chatEndRef} />
      </div>

      {/* Input Bar */}
      <form onSubmit={handleFormSubmit} style={{ display: 'flex', gap: '8px', padding: '12px 16px', borderTop: '1px solid #e2e8f0', backgroundColor: '#ffffff', alignItems: 'center' }}>
        <button
          type="button"
          onClick={startVoiceInput}
          title="Voice Mic Input"
          style={{
            backgroundColor: isListening ? '#ef4444' : '#eff6ff',
            color: isListening ? '#ffffff' : '#2563eb',
            border: isListening ? 'none' : '1px solid #bfdbfe',
            width: '44px',
            height: '44px',
            borderRadius: '50%',
            fontSize: '18px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            flexShrink: 0,
            boxShadow: '0 2px 6px rgba(0,0,0,0.05)'
          }}
        >
          🎙️
        </button>

        <input
          type="text"
          placeholder={isListening ? "Listening... Speak now..." : t('ask_ai')}
          value={input}
          onChange={(e) => setInput(e.target.value)}
          style={{ flex: 1, padding: '12px 16px', borderRadius: '24px', border: '1px solid #cbd5e1', fontSize: '15px', outline: 'none' }}
        />

        <button
          type="submit"
          disabled={loading || !input.trim()}
          style={{
            backgroundColor: loading || !input.trim() ? '#cbd5e1' : '#2563eb',
            color: 'white',
            border: 'none',
            width: '44px',
            height: '44px',
            borderRadius: '50%',
            fontSize: '18px',
            cursor: loading || !input.trim() ? 'default' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            flexShrink: 0,
            boxShadow: '0 2px 6px rgba(37,99,235,0.2)'
          }}
        >
          ➤
        </button>
      </form>

      {/* Bottom Taskbar */}
      <nav className="bottom-nav">
        <a href="#" className="nav-item" onClick={(e) => { e.preventDefault(); navigate('/home'); }}>
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M3 9l9-7 9 7v11a2 2 0 01-2 2H5a2 2 0 01-2-2V9z"/>
            <polyline points="9 22 9 12 15 12 15 22"/>
          </svg>
          <span>{t('home')}</span>
        </a>

        <a href="#" className="nav-item" onClick={(e) => { e.preventDefault(); navigate('/health'); }}>
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M22 12h-4l-3 9L9 3l-3 9H2"/>
          </svg>
          <span>{t('health')}</span>
        </a>

        <button className="nav-item active" onClick={(e) => e.preventDefault()} style={{ background: 'none', border: 'none' }}>
          <svg width="26" height="26" viewBox="0 0 24 24" fill="none">
            <path d="M12 4.5V2" stroke="#3b82f6" strokeWidth="2" strokeLinecap="round"/>
            <circle cx="12" cy="1.8" r="1.8" fill="#ef4444"/>
            <rect x="3" y="4.5" width="18" height="15" rx="7.5" fill="#eff6ff" stroke="#3b82f6" strokeWidth="2"/>
            <circle cx="6.5" cy="13.5" r="1.2" fill="#f472b6" opacity="0.8"/>
            <circle cx="17.5" cy="13.5" r="1.2" fill="#f472b6" opacity="0.8"/>
            <path d="M7.5 10c.8-.8 2.2-.8 3 0" stroke="#1d4ed8" strokeWidth="2.2" strokeLinecap="round"/>
            <path d="M13.5 10c.8-.8 2.2-.8 3 0" stroke="#1d4ed8" strokeWidth="2.2" strokeLinecap="round"/>
            <path d="M9.5 14c1.2 1.3 3.8 1.3 5 0" stroke="#1d4ed8" strokeWidth="2.2" strokeLinecap="round"/>
          </svg>
          <span>{t('assistant')}</span>
        </button>
      </nav>
    </div>
  );
};

export default AssistantPage;

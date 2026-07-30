import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useLanguage } from '../context/LanguageContext';
import '../styles.css';

const AssistantPage = () => {
  const navigate = useNavigate();
  const { lang, changeLanguage, t } = useLanguage();

  const [chatHistory, setChatHistory] = useState([]);
  const [input, setInput] = useState('');

  const handleSend = (e) => {
    e.preventDefault();
    if (!input.trim()) return;

    const userText = input.trim();
    setChatHistory(prev => [...prev, { sender: 'user', text: userText }]);
    setInput('');

    setTimeout(() => {
      let botReplyKey = 'ai_help_msg';
      const lower = userText.toLowerCase();
      if (lower.includes('medicine') || lower.includes('pill') || lower.includes('दवा') || lower.includes('औषध')) {
        botReplyKey = 'ai_med_msg';
      } else if (lower.includes('hi') || lower.includes('hello') || lower.includes('नमस्ते') || lower.includes('नमस्कार')) {
        botReplyKey = 'ai_greet_msg';
      }
      setChatHistory(prev => [...prev, { sender: 'assistant', textKey: botReplyKey }]);
    }, 600);
  };

  return (
    <div className="container" style={{ paddingBottom: '90px', display: 'flex', flexDirection: 'column', height: '100vh' }}>
      {/* Header matching frontend/test.html with Multilingual selector */}
      <header className="header" style={{ borderBottom: '1px solid #e5e7eb', background: '#3b82f6', color: 'white' }}>
        <button
          className="back-btn"
          onClick={() => navigate('/home')}
          title="Go Back"
          style={{ background: 'rgba(255,255,255,0.2)', border: 'none', color: 'white', padding: '8px', borderRadius: '50%', cursor: 'pointer' }}
        >
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>
        <h1 style={{ fontSize: '17px', fontWeight: '800', margin: 0, color: 'white' }}>
          {t('ai_title')}
        </h1>
        <div style={{ width: '36px' }}></div>
      </header>

      {/* Chat Messages */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
        
        {/* Dynamic Initial Greeting Message */}
        <div
          style={{
            alignSelf: 'flex-start',
            backgroundColor: '#f3f4f6',
            color: '#1f2937',
            padding: '12px 16px',
            borderRadius: '18px 18px 18px 4px',
            maxWidth: '80%',
            fontSize: '15px',
            fontWeight: '500',
            textAlign: 'left',
            boxShadow: '0 2px 6px rgba(0,0,0,0.05)'
          }}
        >
          {t('ai_init_msg')}
        </div>

        {/* User & AI Messages */}
        {chatHistory.map((msg, idx) => (
          <div
            key={idx}
            style={{
              alignSelf: msg.sender === 'user' ? 'flex-end' : 'flex-start',
              backgroundColor: msg.sender === 'user' ? '#3b82f6' : '#f3f4f6',
              color: msg.sender === 'user' ? '#ffffff' : '#1f2937',
              padding: '12px 16px',
              borderRadius: msg.sender === 'user' ? '18px 18px 4px 18px' : '18px 18px 18px 4px',
              maxWidth: '80%',
              fontSize: '15px',
              fontWeight: '500',
              textAlign: 'left',
              boxShadow: '0 2px 6px rgba(0,0,0,0.05)'
            }}
          >
            {msg.sender === 'user' ? msg.text : t(msg.textKey)}
          </div>
        ))}
      </div>

      {/* Input Box */}
      <form onSubmit={handleSend} style={{ display: 'flex', gap: '8px', padding: '12px 20px', borderTop: '1px solid #e5e7eb', backgroundColor: '#ffffff' }}>
        <input
          type="text"
          placeholder={t('ask_ai')}
          value={input}
          onChange={(e) => setInput(e.target.value)}
          style={{ flex: 1, padding: '12px 16px', borderRadius: '24px', border: '1px solid #d1d5db', fontSize: '15px', outline: 'none' }}
        />
        <button
          type="submit"
          style={{ backgroundColor: '#3b82f6', color: 'white', border: 'none', width: '44px', height: '44px', borderRadius: '50%', fontSize: '18px', cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center' }}
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

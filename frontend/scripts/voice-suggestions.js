class VoiceSuggestions {
    constructor() {
        this.recognition = null;
        this.isListening = false;
        this.currentLanguage = 'en';
        this.targetTextarea = null;
        
        this.initializeSpeechRecognition();
    }

    initializeSpeechRecognition() {
        if (!('webkitSpeechRecognition' in window) && !('SpeechRecognition' in window)) {
            console.warn('Speech recognition not supported in this browser');
            return;
        }

        const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
        this.recognition = new SpeechRecognition();
        
        // Configure recognition
        this.recognition.continuous = false;
        this.recognition.interimResults = true;
        this.recognition.maxAlternatives = 1;

        // Language mapping for speech recognition
        this.languageMap = {
            'en': 'en-US',
            'hi': 'hi-IN',
            'mr': 'mr-IN'
        };

        this.setupEventListeners();
    }

    setupEventListeners() {
        this.recognition.onstart = () => {
            this.isListening = true;
            this.updateUI(true);
            console.log('Voice recognition started in:', this.currentLanguage);
        };

        this.recognition.onresult = (event) => {
            let finalTranscript = '';
            let interimTranscript = '';

            for (let i = event.resultIndex; i < event.results.length; i++) {
                const transcript = event.results[i][0].transcript;
                if (event.results[i].isFinal) {
                    finalTranscript += transcript;
                } else {
                    interimTranscript += transcript;
                }
            }

            if (this.targetTextarea) {
                if (finalTranscript) {
                    // Append final transcript to textarea
                    const currentValue = this.targetTextarea.value;
                    this.targetTextarea.value = currentValue + (currentValue ? ' ' : '') + finalTranscript;
                }
                
                // Show interim result in placeholder or feedback
                this.showInterimResult(interimTranscript);
            }
        };

        this.recognition.onerror = (event) => {
            console.error('Speech recognition error:', event.error);
            this.handleError(event.error);
        };

        this.recognition.onend = () => {
            this.isListening = false;
            this.updateUI(false);
            console.log('Voice recognition ended');
        };
    }

    startListening(textareaId, selectedLanguage) {
        if (!this.recognition) {
            alert('Speech recognition is not supported in your browser. Please try Chrome or Edge.');
            return;
        }

        this.targetTextarea = document.getElementById(textareaId);
        this.currentLanguage = selectedLanguage;
        
        // Set recognition language
        this.recognition.lang = this.languageMap[selectedLanguage] || 'en-US';
        
        try {
            this.recognition.start();
        } catch (error) {
            console.error('Error starting recognition:', error);
            // Restart if already running
            this.recognition.stop();
            setTimeout(() => {
                this.recognition.start();
            }, 100);
        }
    }

    stopListening() {
        if (this.recognition && this.isListening) {
            this.recognition.stop();
        }
    }

    updateUI(isListening) {
        // Update all voice buttons
        document.querySelectorAll('.voice-btn').forEach(btn => {
            if (isListening) {
                btn.classList.add('listening');
                btn.innerHTML = '🔴 Stop';
                btn.title = 'Stop recording';
            } else {
                btn.classList.remove('listening');
                btn.innerHTML = '🎤 Voice';
                btn.title = 'Start voice recording';
            }
        });

        // Update status indicators
        document.querySelectorAll('.voice-status').forEach(status => {
            if (isListening) {
                status.textContent = `🎤 Listening in ${this.getLanguageName(this.currentLanguage)}...`;
                status.style.color = '#dc3545';
            } else {
                status.textContent = '';
                status.style.color = '#28a745';
            }
        });
    }

    showInterimResult(interimText) {
        const statusElement = document.querySelector('.voice-status');
        if (statusElement && interimText) {
            statusElement.textContent = `🎤 "${interimText}"`;
        }
    }

    handleError(error) {
        let errorMessage = 'Voice recognition error: ';
        
        switch(error) {
            case 'no-speech':
                errorMessage += 'No speech was detected.';
                break;
            case 'audio-capture':
                errorMessage += 'Microphone not found or not allowed.';
                break;
            case 'not-allowed':
                errorMessage += 'Microphone permission denied.';
                break;
            case 'network':
                errorMessage += 'Network error occurred.';
                break;
            default:
                errorMessage += error;
        }

        console.error(errorMessage);
        
        // Show user-friendly error
        const statusElement = document.querySelector('.voice-status');
        if (statusElement) {
            statusElement.textContent = '❌ ' + errorMessage;
            statusElement.style.color = '#dc3545';
        }

        // Reset UI after error
        setTimeout(() => {
            this.updateUI(false);
        }, 3000);
    }

    getLanguageName(code) {
        const names = {
            'en': 'English',
            'hi': 'हिन्दी',
            'mr': 'मराठी'
        };
        return names[code] || code;
    }

    // Check if browser supports speech recognition
    isSupported() {
        return 'webkitSpeechRecognition' in window || 'SpeechRecognition' in window;
    }

    // Request microphone permission
    async requestPermission() {
        try {
            await navigator.mediaDevices.getUserMedia({ audio: true });
            return true;
        } catch (error) {
            console.error('Microphone permission denied:', error);
            return false;
        }
    }
}

// Create global instance
const voiceSuggestions = new VoiceSuggestions();

// Helper function to create voice suggestion UI
function createVoiceSuggestionUI(textareaId, containerId) {
    const container = document.getElementById(containerId);
    if (!container) return;

    const voiceUI = document.createElement('div');
    voiceUI.className = 'voice-suggestion-ui';
    voiceUI.innerHTML = `
        <div class="voice-controls">
            <div class="voice-language-selector">
                <label for="voice-lang-${textareaId}">Language:</label>
                <select id="voice-lang-${textareaId}" class="voice-language-select">
                    <option value="en">English</option>
                    <option value="hi">हिन्दी</option>
                    <option value="mr">मराठी</option>
                </select>
            </div>
            <button type="button" class="voice-btn" onclick="toggleVoiceListening('${textareaId}')" title="Start voice recording">
                🎤 Voice
            </button>
        </div>
        <div class="voice-status" id="voice-status-${textareaId}"></div>
    `;

    container.appendChild(voiceUI);
}

// Toggle voice listening
function toggleVoiceListening(textareaId) {
    const languageSelect = document.getElementById(`voice-lang-${textareaId}`);
    const selectedLanguage = languageSelect ? languageSelect.value : 'en';
    
    if (voiceSuggestions.isListening) {
        voiceSuggestions.stopListening();
    } else {
        // Request permission before starting
        voiceSuggestions.requestPermission().then(granted => {
            if (granted) {
                voiceSuggestions.startListening(textareaId, selectedLanguage);
            } else {
                alert('Microphone permission is required for voice input. Please allow microphone access and try again.');
            }
        });
    }
}

// Add CSS styles
const voiceStyles = `
<style>
.voice-suggestion-ui {
    margin: 10px 0;
    padding: 15px;
    border: 1px solid #ddd;
    border-radius: 8px;
    background: #f8f9fa;
}

.voice-controls {
    display: flex;
    align-items: center;
    gap: 15px;
    flex-wrap: wrap;
}

.voice-language-selector {
    display: flex;
    align-items: center;
    gap: 8px;
}

.voice-language-selector label {
    font-weight: 500;
    color: #333;
}

.voice-language-select {
    padding: 6px 10px;
    border: 1px solid #ccc;
    border-radius: 4px;
    font-size: 14px;
    min-width: 120px;
}

.voice-btn {
    padding: 8px 16px;
    border: none;
    border-radius: 6px;
    background: #007bff;
    color: white;
    cursor: pointer;
    font-size: 14px;
    display: flex;
    align-items: center;
    gap: 5px;
    transition: all 0.3s ease;
}

.voice-btn:hover {
    background: #0056b3;
    transform: translateY(-1px);
}

.voice-btn.listening {
    background: #dc3545;
    animation: pulse 1.5s infinite;
}

.voice-btn.listening:hover {
    background: #c82333;
}

@keyframes pulse {
    0% { opacity: 1; }
    50% { opacity: 0.7; }
    100% { opacity: 1; }
}

.voice-status {
    margin-top: 10px;
    font-size: 14px;
    color: #28a745;
    min-height: 20px;
    font-style: italic;
}

.voice-status:empty {
    display: none;
}

/* Responsive design */
@media (max-width: 600px) {
    .voice-controls {
        flex-direction: column;
        align-items: stretch;
    }
    
    .voice-language-selector {
        justify-content: space-between;
    }
    
    .voice-btn {
        width: 100%;
        justify-content: center;
    }
}
</style>
`;

// Inject styles into the page
if (!document.querySelector('#voice-styles')) {
    const styleElement = document.createElement('div');
    styleElement.id = 'voice-styles';
    styleElement.innerHTML = voiceStyles;
    document.head.appendChild(styleElement);
}

// Export for use in other scripts
if (typeof module !== 'undefined' && module.exports) {
    module.exports = VoiceSuggestions;
}

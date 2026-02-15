class Translator {
    constructor() {
        this.currentLanguage = 'en';
        this.translations = {};
        // Don't call loadLanguage in constructor, let initialize handle it
    }

    async loadLanguage(lang) {
        try {
            const response = await fetch(`translations/${lang}.json`);
            this.translations = await response.json();
            this.currentLanguage = lang;
            localStorage.setItem('selectedLanguage', lang);
            this.updatePageLanguage();
        } catch (error) {
            console.error('Error loading language:', error);
            // Fallback to English if language file fails to load
            if (lang !== 'en') {
                await this.loadLanguage('en');
            }
        }
    }

    t(key) {
        const keys = key.split('.');
        let value = this.translations;
        
        for (const k of keys) {
            value = value?.[k];
        }
        
        const result = value || key; // Return key if translation not found
        return result;
    }

    updatePageLanguage() {
        // Update all elements with data-translate attribute
        document.querySelectorAll('[data-translate]').forEach(element => {
            const key = element.getAttribute('data-translate');
            const translation = this.t(key);
            
            if (element.tagName === 'INPUT' && element.type === 'placeholder') {
                element.placeholder = translation;
            } else if (element.tagName === 'INPUT' && element.type === 'value') {
                element.value = translation;
            } else {
                element.textContent = translation;
            }
        });

        // Update page title
        const titleKey = document.querySelector('title')?.getAttribute('data-translate');
        if (titleKey) {
            document.title = this.t(titleKey);
        }

        // Update language selector
        const languageSelector = document.getElementById('language');
        if (languageSelector) {
            languageSelector.value = this.currentLanguage;
        }

        // Update HTML lang attribute
        document.documentElement.lang = this.currentLanguage;
    }

    async switchLanguage(lang) {
        if (lang !== this.currentLanguage) {
            await this.loadLanguage(lang);
        }
    }

    async initialize() {
        // Load saved language from localStorage
        const savedLanguage = localStorage.getItem('selectedLanguage') || 'en';
        await this.loadLanguage(savedLanguage);

        // Set up language selector
        const languageSelector = document.getElementById('language');
        if (languageSelector) {
            languageSelector.addEventListener('change', (e) => {
                this.switchLanguage(e.target.value);
            });
        }
    }

    // Helper function to get language display name
    getLanguageName(code) {
        const names = {
            'en': 'English',
            'hi': 'हिन्दी',
            'mr': 'मराठी'
        };
        return names[code] || code;
    }

    // Format numbers according to locale
    formatNumber(number) {
        const locales = {
            'en': 'en-US',
            'hi': 'hi-IN',
            'mr': 'mr-IN'
        };
        return new Intl.NumberFormat(locales[this.currentLanguage] || 'en-US').format(number);
    }

    // Format dates according to locale
    formatDate(date) {
        const locales = {
            'en': 'en-US',
            'hi': 'hi-IN',
            'mr': 'mr-IN'
        };
        return new Intl.DateTimeFormat(locales[this.currentLanguage] || 'en-US').format(date);
    }
}

// Create global translator instance
const translator = new Translator();

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    translator.initialize().then(() => {
        // Ensure translations are applied after initialization
        setTimeout(() => {
            translator.updatePageLanguage();
        }, 100);
    }).catch(error => {
        console.error('Translator initialization failed:', error);
    });
});

// Export for use in other scripts
if (typeof module !== 'undefined' && module.exports) {
    module.exports = Translator;
}

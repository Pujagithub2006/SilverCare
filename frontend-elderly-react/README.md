# SilverCare Elderly Frontend - ReactJS

This is the ReactJS version of the SilverCare elderly frontend, converted from the original HTML/CSS/JavaScript implementation while preserving the exact UI design.

## Project Structure

```
frontend-elderly-react/
├── src/
│   ├── components/
│   │   ├── ElderlyLogin.jsx       # Login page
│   │   ├── ElderlyAuth.jsx        # Registration page
│   │   ├── ElderlyDashboard.jsx   # Dashboard with medicine reminders
│   │   ├── ElderlyHome.jsx        # Main home page with SOS button
│   │   └── FallAlert.jsx          # Fall detection alert page
│   ├── services/
│   │   └── api.js                 # API service layer with Spring Boot support
│   ├── App.jsx                    # Main app with React Router
│   ├── main.jsx                   # Entry point
│   └── styles.css                 # Exact styles from original implementation
├── index.html                     # HTML template
├── package.json                   # Dependencies
└── vite.config.js                 # Vite configuration
```

## Features

- **Exact UI Design**: Preserved the original design from the HTML/CSS implementation
- **React Router**: Client-side routing between pages
- **API Service Layer**: Configured for Spring Boot backend with fallback to Python backend
- **State Management**: React hooks for managing component state
- **LocalStorage**: Preserved local storage functionality for user data persistence

## Installation

1. Navigate to the project directory:
```bash
cd frontend-elderly-react
```

2. Install dependencies:
```bash
npm install
```

## Running the Project

### Development Mode
```bash
npm run dev
```
The application will be available at `http://localhost:3000`

### Production Build
```bash
npm run build
```

### Preview Production Build
```bash
npm run preview
```

## Backend Integration

The API service layer (`src/services/api.js`) is configured to work with both:

1. **Spring Boot Backend** (Primary): `http://localhost:8080/api`
2. **Python Flask Backend** (Fallback): `http://127.0.0.1:5001`

To switch to Spring Boot backend exclusively, update the `API_BASE` constant in `src/services/api.js`:

```javascript
const API_BASE = 'http://localhost:8080/api';
```

## Available Routes

- `/` - Redirects to login
- `/login` - Elderly login page
- `/register` - Elderly registration page
- `/home` - Main home page with SOS button
- `/dashboard` - Dashboard with medicine reminders
- `/fall-alert` - Fall detection alert page

## Components

### ElderlyLogin
- Login form with name and phone number
- Remember me functionality
- Auto-login for remembered users
- Navigation to registration page

### ElderlyAuth
- Registration form with personal information
- Guardian connection section
- Medical history input
- Remember me functionality

### ElderlyDashboard
- Real-time medicine reminder display
- Notification system
- Medicine status tracking (taken, snooze, not taken)
- Session management with backend

### ElderlyHome
- SOS button with press-and-hold functionality
- Medicine reminders modal
- Navigation to health page and assistant
- Real-time medicine count display
- Language selector
- Logout functionality

### FallAlert
- Countdown timer for fall detection
- Emergency contact functionality
- Safe confirmation option
- Guardian notification system

## Styling

All styles are preserved from the original `styles.css` file. The design includes:
- Mobile-first responsive design
- Gradient backgrounds
- Modern card-based layouts
- Touch-friendly buttons
- Status badges and indicators

## Future Enhancements

When the Spring Boot backend is ready:
1. Update API endpoints in `src/services/api.js`
2. Remove fallback to Python backend
3. Add proper error handling for Spring Boot responses
4. Implement JWT authentication if needed

## Notes

- The current implementation maintains compatibility with the existing Python Flask backend
- All localStorage keys and data structures are preserved for seamless migration
- The UI design is identical to the original HTML implementation
- All interactive features (SOS button, medicine reminders, fall alerts) are fully functional

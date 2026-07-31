# Run Instructions for SilverCare Project

This document provides a step‑by‑step guide to set up and run the **SilverCare** platform locally.

## Prerequisites

- **Git** installed on your machine.
- **Java 17** (required for Spring Boot backend).
- **Maven** for building the backend.
- **Node.js** (>=18) and npm for the React frontend.
- **Docker** & **Docker Compose** (optional but recommended for containerised services).
- **Arduino IDE** for flashing the ESP32 firmware.
- **Twilio account** (or GSM module) credentials for SMS/voice alerts.
- **PostgreSQL** (local or Docker) for persistent storage.
- **Firebase** project (real‑time DB & auth) for sensor data synchronization.

## 1️⃣ Clone the repository

```bash
git clone https://github.com/Pujagithub2006/SilverCare.git
cd SilverCare
```

## 2️⃣ Backend (Spring Boot)

### a) Build the backend

```bash
cd backend-spring
mvn clean package -DskipTests
```

### b) Run with Docker Compose (recommended)

Create a `.env` file inside `backend-spring` containing the required secrets:

```dotenv
TWILIO_ACCOUNT_SID=your_sid
TWILIO_AUTH_TOKEN=your_token
TWILIO_FROM_NUMBER=+1234567890
POSTGRES_URL=jdbc:postgresql://postgres:5432/silvercare
POSTGRES_USER=postgres
POSTGRES_PASSWORD=postgres
```

Then start the stack:

```bash
docker compose up -d
```

The API will be reachable at `http://localhost:8080/api`.

### c) Run locally (without Docker)

```bash
export SPRING_PROFILES_ACTIVE=dev   # Windows: set SPRING_PROFILES_ACTIVE=dev
java -jar target/silvercare-backend-1.0.0.jar
```

## 3️⃣ Front‑end (React)

```bash
cd Frontend-react
npm install
npm run dev   # Vite dev server at http://localhost:5173
```

The frontend reads the backend URL from the environment variable `VITE_API_BASE_URL` (defined in a `.env` file in this folder).

## 4️⃣ ESP32 Firmware

1. Open `ESP32_codes/ESP32.ino` in Arduino IDE.
2. Install the libraries listed in the sketch header.
3. Configure your Wi‑Fi SSID and password inside the sketch.
4. Select **ESP32 Dev Module** and the appropriate COM port.
5. Click **Upload**.

The firmware will POST sensor data to `/api/esp32/data` on the backend.

## 5️⃣ Database setup (PostgreSQL)

If you use Docker, the `docker-compose.yml` already starts a PostgreSQL container. Otherwise, run:

```bash
docker run --name pg -e POSTGRES_PASSWORD=postgres -p 5432:5432 -d postgres:15
```

Run any DB migrations located in `backend-spring/src/main/resources/db/migration`.

## 6️⃣ Firebase configuration

Create a `firebase-config.js` in `Frontend-react/src`:

```javascript
export const firebaseConfig = {
  apiKey: "...",
  authDomain: "...",
  projectId: "...",
  storageBucket: "...",
  messagingSenderId: "...",
  appId: "..."
};
```

## 7️⃣ Verify the system

- Open the React app in a browser and log in with the demo credentials (see README).
- The ESP32 should appear as a connected device and stream live sensor data.
- Trigger a test alert via `POST /api/alert/test` and confirm the notification appears in the UI.

## 8️⃣ Optional: Full Docker production deployment

```bash
docker compose -f docker-compose.prod.yml up -d
```

This brings up PostgreSQL, the Spring Boot API, and the React UI behind a reverse‑proxy.

---

*If you encounter any issues, consult the project's issue tracker or the `README.md` for troubleshooting tips.*

# Run Instructions for SilverCare Project

This document provides a step‑by‑step guide to set up and run the **SilverCare** platform locally.

## Prerequisites

- **Git** installed on your machine.
- **Python 3.9+** (ensure `python --version` works).
- **Node.js** (optional, only if you plan to develop the frontend).
- **Arduino IDE** for flashing the ESP32 firmware.
- **Twilio account** (or GSM module) credentials for SMS/voice alerts.
- **Docker** (optional) if you prefer containerised backend.

## 1. Clone the Repository

```bash
git clone https://github.com/Pujagithub2006/SilverCare.git
cd SilverCare
```

## 2. Backend Setup

### a. Virtual Environment (recommended)
```bash
python -m venv venv
source venv/bin/activate   # on Windows: venv\Scripts\activate
```
### b. Install Python dependencies
```bash
pip install -r requirements.txt
```
### c. Configure environment variables
Create a `.env` file in the `backend` directory with the following keys (replace placeholders with real values):
```
TWILIO_ACCOUNT_SID=your_account_sid
TWILIO_AUTH_TOKEN=your_auth_token
TWILIO_FROM_NUMBER=+1234567890
GSM_MODULE_PORT=/dev/ttyUSB0   # adjust for your system
```
### d. Run the backend services
```bash
# Start the main API server
python backend/main.py &
# Start the portal/websocket server
python backend/portal_server.py &
```
You should see logs indicating the servers are listening on their respective ports.

## 3. ESP32 Firmware Upload

1. Open `ESP32_codes/ESP32.ino` in **Arduino IDE**.
2. Select the correct **Board** (e.g., "ESP32 Dev Module") and the appropriate **COM port**.
3. Install any required libraries (listed in the sketch comments).
4. Click **Upload** and wait for the compilation and flashing to finish.

## 4. Front‑end / Web Interface

The simple web UI resides in `frontend/portal.html`.

- Open `frontend/portal.html` directly in a browser, or
- Serve the folder with a lightweight server (e.g., `python -m http.server 8080` in the `frontend` directory) and navigate to `http://localhost:8080/portal.html`.

## 5. Verify the System

- The ESP32 should connect to Wi‑Fi and start sending sensor data.
- The backend will receive data, store it in Firebase (ensure your Firebase config is set), and trigger alerts when a fall or medication reminder occurs.
- Check the browser UI for live sensor readings and alert notifications.

## 6. (Optional) Dockerised Backend

If you prefer Docker, run:
```bash
cd backend-spring
docker build -t silvercare-backend .
 docker run -p 8080:8080 -e TWILIO_ACCOUNT_SID=... -e TWILIO_AUTH_TOKEN=... silvercare-backend
```

---

*For any issues, refer to the `README.md` which links back to this document.*

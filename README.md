# SilverCare - Senior Safety Monitoring System

## 🚀 Complete Hardware-Software Integration

This system integrates your ESP32 hardware with a web interface for real-time monitoring and voice alerts.

### 📁 Files Created:
- `bridge_server.py` - Python server connecting ESP32 to web
- `templates/index.html` - Web interface with voice alerts
- `esp32_enhanced_code.ino` - Enhanced Arduino code with computer integration
- `requirements.txt` - Python dependencies

### 🔧 Setup Instructions:

#### 1. Install Python Dependencies
```bash
pip install -r requirements.txt
```

#### 2. Update ESP32 Code
- Replace your existing Arduino code with `esp32_enhanced_code.ino`
- Upload to your ESP32

#### 3. Find Your ESP32 Port
- **Windows**: Check Device Manager for COM port
- **Linux/Mac**: Use `ls /dev/tty*` or `ls /dev/cu.*`

#### 4. Update Port in Bridge Server
- Open `bridge_server.py`
- Change `ESP_PORT = "COM3"` to your ESP32's port

#### 5. Start the System
```bash
python bridge_server.py
```

#### 6. Open Web Interface
- Go to: `http://localhost:5000`

### 🌟 Features:

#### Real-time Monitoring:
- Heart rate, SpO2, temperature
- Movement detection
- Belt worn status
- System state indicators

#### Voice Alerts:
- **Pre-Fall**: "Are you okay?" (repeats every 3 seconds)
- **Fall Detection**: "Fall detected! Are you okay?" (with 30-second countdown)
- Auto-escalation to emergency if no response

#### Response Options:
- **"I'm OK" button** - Stops alerts, resets system
- **"Emergency" button** - Triggers immediate emergency call
- **Keyboard shortcuts**: Enter/Space for "OK", Escape for "Emergency"

#### Two-Way Communication:
- ESP32 sends sensor data to computer
- Web interface sends responses back to ESP32
- Automatic emergency escalation

### 🔄 How It Works:

1. **ESP32 detects fall/pre-fall** → Sends structured data via Serial
2. **Python bridge server** reads Serial data → Sends to web interface
3. **Web interface** shows real-time data + triggers voice alerts
4. **User responds** → Web sends command back to ESP32
5. **ESP32 receives command** → Resets system or triggers emergency

### 🚨 Emergency Flow:
1. Fall detected → Voice alert starts
2. 30-second countdown begins
3. If no response → Automatic emergency call
4. SMS sent to guardian number
5. System logs emergency event

### 📱 Mobile Access:
- Access web interface from any device on same network
- Use phone/tablet for monitoring
- Voice alerts work on any device with speakers

### 🔍 Troubleshooting:

#### ESP32 Not Connecting:
- Check USB cable
- Verify COM port in Device Manager
- Ensure ESP32 is powered on

#### Web Interface Not Loading:
- Check if Python server is running
- Verify port 5000 is not blocked
- Try `http://127.0.0.1:5000`

#### Voice Alerts Not Working:
- Ensure browser supports Web Speech API
- Check speaker volume
- Try Chrome/Edge browser

### 🎯 Key Benefits:
- **Real-time monitoring** of all vitals
- **Voice interaction** for hands-free operation
- **Automatic escalation** for emergencies
- **Remote monitoring** via web interface
- **Two-way communication** between hardware and software

Your SilverCare system is now fully integrated with computer-based monitoring and voice alerts!

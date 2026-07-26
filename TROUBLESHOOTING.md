# 🛠️ SilverCare Troubleshooting Guide

## 🚨 Common Issues & Solutions

### **❌ Server Won't Start**

#### Issue: "Python not found"
**Solution:**
```bash
# Install Python 3.7+ from https://python.org
# Verify installation:
python --version
```

#### Issue: "Failed to install dependencies"
**Solution:**
```bash
# Try installing manually:
pip install flask flask-socketio pyserial python-socketio eventlet

# Or use virtual environment:
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

### **🔌 ESP32 Connection Issues**

#### Issue: "No ESP32 ports found automatically"
**Solution:**
1. Check USB cable connection
2. Install CH340/CP210 drivers:
   - **Windows**: Download from manufacturer website
   - **Mac**: `brew install ch340`
   - **Linux**: `sudo modprobe ch341`
3. Check Device Manager for COM ports

#### Issue: "Permission denied" (Linux/Mac)
**Solution:**
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER
# Reboot required
sudo reboot
```

#### Issue: "Port already in use"
**Solution:**
```bash
# Find process using port:
netstat -ano | findstr :5000  # Windows
lsof -i :5000                  # Mac/Linux

# Kill process:
taskkill /PID <PID> /F         # Windows
kill -9 <PID>                  # Mac/Linux
```

### **🌐 Web Interface Issues**

#### Issue: "Connection refused" in browser
**Solution:**
1. Check if server is running (look for "Starting Web Server" message)
2. Try `http://127.0.0.1:5000` instead of `localhost`
3. Check firewall settings
4. Disable VPN/Proxy temporarily

#### Issue: "Disconnected" status
**Solution:**
1. Refresh the page
2. Check browser console (F12) for errors
3. Ensure WebSocket is not blocked by network
4. Try different browser (Chrome/Edge recommended)

#### Issue: Voice alerts not working
**Solution:**
1. Use Chrome/Edge browser (best Web Speech API support)
2. Check speaker volume
3. Allow microphone permissions (required for some browsers)
4. Test with: `speechSynthesis.speak(new SpeechSynthesisUtterance('test'))`

### **📡 Data Not Updating**

#### Issue: No real-time data in web interface
**Solution:**
1. Check ESP32 Serial Monitor (Arduino IDE) - should see `DATA:TYPE:VALUE` format
2. Verify ESP32 is powered and sensors connected
3. Check server console for "Error reading from ESP32" messages
4. Restart both ESP32 and server

#### Issue: Garbled data in Serial Monitor
**Solution:**
1. Ensure baud rate is 115200
2. Check USB cable quality
3. Try different USB port
4. Restart Arduino IDE

### **🚨 Emergency System Issues**

#### Issue: SMS not sending
**Solution:**
1. Check GSM module power (LED should be on)
2. Verify SIM card has balance and is not locked
3. Check antenna connection
4. Test with manual SMS:
   ```cpp
   gsmSerial.println("AT+CMGS=\"+919075522754\"");
   // Wait for > prompt, type message, Ctrl+Z to send
   ```

#### Issue: Emergency escalation not working
**Solution:**
1. Check server console for "Emergency escalation" messages
2. Verify countdown timer appears in web interface
3. Test manual emergency: Click "Emergency" button
4. Check GSM module response in Serial Monitor

### **🔄 Performance Issues**

#### Issue: System lag/slow response
**Solution:**
1. Close other applications using COM port
2. Reduce Serial Monitor baud rate to 9600 temporarily
3. Check CPU usage
4. Restart server after long running periods

#### Issue: Memory issues
**Solution:**
1. Clear browser cache
2. Restart server every few hours
3. Check for memory leaks in task manager

### **📱 Mobile Access Issues**

#### Issue: Cannot access from phone
**Solution:**
1. Find computer's IP address:
   - **Windows**: `ipconfig` (look for IPv4 Address)
   - **Mac**: `ifconfig` (look for en0/eth0)
2. Ensure both devices on same WiFi network
3. Disable firewall temporarily for testing
4. Use: `http://COMPUTER_IP:5000`

### **🔍 Debug Mode**

#### Enable detailed logging:
```python
# In bridge_server.py, add:
import logging
logging.basicConfig(level=logging.DEBUG)
```

#### Test individual components:
```bash
# Test ESP32 connection:
python -c "import serial; s=serial.Serial('COM3',115200); print(s.readline())"

# Test web server:
python -c "from flask import Flask; app=Flask(__name__); app.run(port=5000)"
```

### **🆘 Emergency Reset**

If everything fails:
1. **Hardware reset**: Press ESP32 reset button
2. **Software reset**: Delete all `.pyc` files and restart
3. **Clean reinstall**: 
   ```bash
   pip uninstall flask flask-socketio pyserial -y
   pip install -r requirements.txt
   ```

### **📞 Getting Help**

#### Collect this information:
1. Operating system and version
2. Python version (`python --version`)
3. ESP32 model and connection method
4. Exact error messages
5. Browser and version

#### Test commands to run:
```bash
python --version
pip list | grep flask
python -c "import serial.tools.list_ports; print(serial.tools.list_ports.comports())"
```

---

**💡 Pro Tips:**
- Always use the `start.bat` (Windows) or `start.sh` (Mac/Linux) script
- Keep ESP32 connected to USB power during operation
- Test system weekly with simulated falls
- Keep backup of working configuration
- Document your COM port for quick setup

**🎯 Quick Test Sequence:**
1. Run `start.bat`
2. Open `http://localhost:5000`
3. Check "Connected" status
4. Verify data updating in dashboard
5. Test voice alerts with browser console
6. Test emergency button

If all 6 steps work, your system is 100% operational!

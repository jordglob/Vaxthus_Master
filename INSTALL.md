# Installation Guide - Vaxthus Master V3

Complete beginner-friendly guide to install and run Vaxthus Master V3 on your ESP32.

## 📋 What You Need

### Hardware:
- ✅ ESP32-WROOM-32 (DevKit V1) board
- ✅ USB cable (data cable, not charge-only!)
- ✅ Computer (Windows/Mac/Linux)

### Software (we'll install these):
- Python 3.7 or newer
- PlatformIO
- USB drivers for ESP32

---

## 🚀 Step-by-Step Installation

### Step 1: Install Python

**Windows:**
1. Download from https://www.python.org/downloads/
2. Run installer
3. ✅ **IMPORTANT**: Check "Add Python to PATH"
4. Click Install

**Verify installation:**
```bash
python --version
```
Should show: `Python 3.x.x`

---

### Step 2: Install PlatformIO

**Open Command Prompt / Terminal and run:**

```bash
pip install platformio
```

**Verify installation:**
```bash
pio --version
```
Should show: `PlatformIO Core, version x.x.x`

---

### Step 3: Install USB Drivers

**Windows:**
- ESP32 usually works automatically
- If not, install CP210x drivers from: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

**Mac:**
- Usually works automatically
- If not: `brew install --cask cp210x-driver`

**Linux:**
```bash
sudo usermod -a -G dialout $USER
# Then logout and login again
```

---

### Step 4: Clone the Project

**Option A - Using Git (recommended):**
```bash
git clone https://github.com/jordglob/Vaxthus_Master.git
cd Vaxthus_Master
```

**Option B - Download ZIP:**
1. Go to https://github.com/jordglob/Vaxthus_Master
2. Click green "Code" button → Download ZIP
3. Extract to a folder
4. Open terminal in that folder

---

### Step 5: Choose Your Version

**Headless Version (Recommended):**
```bash
git checkout headless
```
✅ Best for most users  
✅ Sun simulation  
✅ MQTT/Home Assistant ready  
✅ Full documentation

**Display Version (with energy metering):**
```bash
git checkout lilygo-t-display
```
⚠️ Requires LilyGo T-Display board

---

### Step 6: Connect ESP32

1. Connect ESP32 to computer via USB cable
2. Check which COM port it's on:

**Windows:**
```bash
pio device list
```
Look for something like `COM6` or `COM7`

**Mac/Linux:**
```bash
ls /dev/tty.*
```
Look for something like `/dev/ttyUSB0`

---

### Step 7: Configure COM Port

**Edit `platformio.ini`:**

```ini
upload_port = COM6      ; Change to your port
monitor_port = COM6     ; Change to your port
```

Or skip this and specify on command line later.

---

### Step 8: First Upload!

**Compile and upload:**

```bash
pio run -t upload
```

This will:
1. ✅ Download all libraries
2. ✅ Compile the code
3. ✅ Upload to ESP32
4. ✅ Take 2-5 minutes first time

**If you see "Connecting..." stuck:**
- Hold BOOT button on ESP32
- Keep holding until upload starts
- Release when you see "Writing at 0x..."

---

### Step 9: Check if it Works

**Start serial monitor:**

```bash
pio device monitor
```

You should see:
```
=================================
  Vaxthus_Master_V3
  Grow Light Controller
=================================

Loading settings from NVM...
Initializing WiFi...
  AP started: Vaxthus_Master
  AP IP: 192.168.4.1
Setup complete!
```

**Press Ctrl+C to exit monitor**

---

### Step 10: Configure WiFi

1. Connect your phone/computer to WiFi network: **Vaxthus_Master**
2. Password: **123456789**
3. Open browser: http://192.168.4.1
4. Click **Settings**
5. Enter your home WiFi name and password
6. Click **Save & Reboot**

**Wait 30 seconds, then check serial monitor for IP address!**

---

## ✅ Success! What Next?

1. Access web interface at the IP shown in serial monitor
2. Set up MQTT if you have Home Assistant (see FAQ.md)
3. Configure sun simulation times (see README.md)
4. Start growing! 🌱

---

## 🔧 Troubleshooting

### "Python not found"
- Reinstall Python
- Make sure "Add to PATH" was checked
- Restart computer

### "pio: command not found"
```bash
# Try this instead:
python -m platformio run -t upload
```

### "Serial port not found"
- Check USB cable (must be data cable, not charge-only)
- Try different USB port
- Check Device Manager (Windows) for COM port number
- Install CP210x drivers

### "Permission denied" (Linux/Mac)
```bash
sudo chmod 666 /dev/ttyUSB0
# Or add user to dialout group (see Step 3)
```

### Upload fails repeatedly
- Hold BOOT button on ESP32 during "Connecting..."
- Try different USB cable
- Check if antivirus is blocking
- Try: `pio run -t upload --upload-port COM6` with your port

### Libraries won't download
```bash
# Clear cache and retry
pio pkg uninstall -g
pio run -t upload
```

---

## 📚 Next Steps

- Read [README.md](README.md) for features overview
- Check [FAQ.md](FAQ.md) for common questions
- See [BUILD.md](BUILD.md) for advanced build options
- Review [AI_PRIMER.md](AI_PRIMER.md) for code details

---

## 🆘 Still Having Issues?

1. Check serial monitor output
2. Search FAQ.md for your error
3. Open issue on GitHub with:
   - Operating system
   - Error message
   - Serial monitor output
   - What you've tried

**Happy Growing! 🌱**

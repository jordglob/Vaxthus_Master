# Vaxthus Master V3 - Grow Light Controller

![Version](https://img.shields.io/badge/version-3.0.0-brightgreen)
![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-ESP32-orange)

**Vaxthus Master V3** is an advanced ESP32-based grow light controller with automatic sun simulation, PWM control, WiFi connectivity, and Home Assistant integration via MQTT.

## ✨ Features

- **🌅 Automatic Sun Simulation**: Mimics natural daylight cycles with sunrise (06:00-10:00) and sunset (18:00-22:00) transitions
- **🎛️ 3-Channel PWM Control**: Independent control of White, Red, and UV LED channels (0-255 brightness levels)
- **📱 Web Interface**: Responsive web dashboard accessible from any device
- **🏠 Home Assistant Integration**: MQTT auto-discovery for seamless smart home integration
- **⚙️ Manual Override**: 40-minute manual control before returning to automatic mode
- **🔧 Easy Configuration**: Web-based settings for WiFi and MQTT
- **🕐 NTP Time Sync**: Automatic time synchronization for accurate sun simulation
- **💾 Persistent Storage**: Settings and light states saved to non-volatile memory
- **📊 Status Monitoring**: Real-time WiFi signal strength and connection status

## 🛠️ Hardware Requirements

### Required Components
- **ESP32-WROOM-32** (DevKit V1 or compatible)
- **Power Supply**: 5V USB or appropriate voltage for your LED setup
- **LED Drivers**: MOSFET or LED driver circuits for each channel
- **LEDs**: White, Red, and UV LED strips or modules

### Pin Configuration
| Channel | GPIO Pin | Function |
|---------|----------|----------|
| White   | GPIO 16  | PWM Output (RX2) |
| Red     | GPIO 17  | PWM Output (TX2) |
| UV      | GPIO 18  | PWM Output |

### PWM Specifications
- **Frequency**: 5 kHz
- **Resolution**: 8-bit (0-255)
- **Voltage**: 3.3V logic level

## 📡 Network Requirements

> **⚠️ IMPORTANT**: The ESP32 **only supports 2.4 GHz WiFi networks**. It will NOT connect to 5 GHz networks.

For reliable operation:
- Use a **2.4 GHz WiFi network** (802.11 b/g/n)
- Keep the ESP32 **within 3 meters** (10 feet) of your WiFi access point for initial setup
- Ensure good signal strength (RSSI > -70 dBm recommended)
- Avoid WiFi networks with captive portals

## 🚀 Quick Start

### 1. Flash the Firmware

```bash
# Clone the repository
git clone https://github.com/yourusername/Vaxthus_Master_V3.git
cd Vaxthus_Master_V3

# Install PlatformIO (if not already installed)
pip install platformio

# Build and upload
pio run -t upload

# Monitor serial output
pio device monitor
```

### 2. Initial Configuration

On first boot, the device creates a WiFi access point:

**Default AP Credentials:**
- **SSID**: `Vaxthus_Master`
- **Password**: `123456789`
- **IP Address**: `192.168.4.1`

1. Connect to the `Vaxthus_Master` network
2. Open browser and navigate to `http://192.168.4.1`
3. Click **Settings**
4. Configure your home WiFi credentials
5. (Optional) Configure MQTT settings for Home Assistant
6. Click **Save & Reboot**

### 3. Access the Dashboard

After connecting to your home network:
- The device will display its IP address in the serial monitor
- Access the web interface at `http://[device-ip]`
- Control lights manually or let the automatic sun simulation run

## 🌞 Sun Simulation Schedule

The controller mimics natural sunlight cycles:

| Time Period | Light Level | Description |
|-------------|-------------|-------------|
| 00:00 - 06:00 | 0% | Night (all lights off) |
| 06:00 - 10:00 | 0% → 100% | Sunrise (gradual ramp up) |
| 10:00 - 18:00 | 100% | Full daylight |
| 18:00 - 22:00 | 100% → 0% | Sunset (gradual ramp down) |
| 22:00 - 00:00 | 0% | Night (all lights off) |

### Manual Override
- Adjusting any light channel activates **manual mode** for 40 minutes
- System automatically returns to sun simulation after timeout
- Current mode is indicated in the serial monitor output

## 🏠 Home Assistant Integration

### MQTT Configuration

1. Configure MQTT settings in the web interface
2. Default server: `mqtt.revolt-energy.org:1883`
3. Enable MQTT and reboot
4. Devices auto-discover in Home Assistant

### MQTT Topics

**Command Topics** (set brightness 0-255):
```
bastun/vaxtljus/white/set
bastun/vaxtljus/red/set
bastun/vaxtljus/uv/set
```

**State Topics** (receive current brightness):
```
bastun/vaxtljus/white/state
bastun/vaxtljus/red/state
bastun/vaxtljus/uv/state
```

### Home Assistant Entities

Three light entities will appear:
- `light.grow_light_white`
- `light.grow_light_red`
- `light.grow_light_uv`

## 📋 Web Interface Features

### Main Dashboard
- Real-time light control sliders for all three channels
- WiFi connection status and signal strength
- MQTT connection indicator
- Responsive design for mobile and desktop

### Settings Page
- WiFi SSID and password configuration
- MQTT server, port, and credentials
- Enable/disable MQTT integration
- Save & reboot functionality

## 🔧 Advanced Configuration

### Modify Sun Simulation Schedule

Edit `src/main.cpp` and adjust these constants:

```cpp
#define SUNRISE_START_HOUR   6   // Sunrise begins
#define SUNRISE_END_HOUR     10  // Full brightness reached
#define SUNSET_START_HOUR    18  // Sunset begins
#define SUNSET_END_HOUR      22  // Lights off
```

### Change Manual Override Duration

```cpp
#define MANUAL_OVERRIDE_DURATION 2400000  // 40 minutes (in milliseconds)
```

### Adjust PWM Frequency

```cpp
#define PWM_FREQ        5000  // 5 kHz
#define PWM_RESOLUTION  8     // 8-bit (0-255)
```

## 🔍 Troubleshooting

### WiFi Won't Connect

**Problem**: Device can't connect to home WiFi  
**Solutions**:
- ✅ Verify you're using a **2.4 GHz network** (not 5 GHz)
- ✅ Move ESP32 within **3 meters** of your router
- ✅ Check SSID and password are correct (case-sensitive)
- ✅ Ensure network doesn't use WPA3-only security
- ✅ Disable MAC address filtering temporarily
- ✅ Check serial monitor for specific error codes

### Time Not Syncing

**Problem**: "Failed to sync time" error  
**Solutions**:
- ✅ Ensure WiFi is connected to internet-enabled network
- ✅ Check firewall isn't blocking NTP (port 123 UDP)
- ✅ Wait 5 minutes for automatic retry
- ✅ Verify router DNS settings are correct

### MQTT Not Connecting

**Problem**: MQTT shows "Disconnected"  
**Solutions**:
- ✅ Verify MQTT broker address is correct
- ✅ Check MQTT credentials (if required)
- ✅ Ensure MQTT broker is running and accessible
- ✅ Check firewall rules on MQTT broker
- ✅ Verify port 1883 is open

### Lights Not Responding

**Problem**: PWM outputs show no voltage  
**Solutions**:
- ✅ Verify correct GPIO pins (16, 17, 18)
- ✅ Check power supply to LED drivers
- ✅ Measure voltage with multimeter (should see 0-3.3V PWM)
- ✅ Ensure LED drivers are properly connected
- ✅ Test with oscilloscope (5 kHz signal expected)

## 📊 Serial Monitor Output

Normal operation shows:

```
=================================
  Vaxthus_Master_V3
  Grow Light Controller
=================================

Loading settings from NVM...
  WiFi SSID: your_network
  MQTT Server: mqtt.revolt-energy.org:1883
  MQTT Enabled: Yes
Initializing PWM channels...
  White: 0, Red: 0, UV: 0
Initializing WiFi...
  [WiFi] Got IP: 192.168.1.100
  Connected! IP: 192.168.1.100
Initializing web server...
  Web server started on port 80
Connecting to MQTT...
MQTT connected!
Initializing NTP time sync...
  Time synced: 2026-01-25 05:30:15
Setup complete!
[Sun Sim] 05:31 → Light: 0% (Auto mode)
```

## 🏗️ Architecture

Based on the excellent **Battery-Emulator** architecture by [dalathegreat](https://github.com/dalathegreat), this project follows similar patterns:

- **Preferences-based settings storage**
- **Dual-mode WiFi (AP + STA)**
- **Modular code structure**
- **Event-driven WiFi handling**
- **Non-blocking operations**

See [AI_PRIMER.md](AI_PRIMER.md) for detailed architecture documentation.

## 📚 Documentation

- [CHANGELOG.md](CHANGELOG.md) - Version history and release notes
- [AI_PRIMER.md](AI_PRIMER.md) - Architecture and AI assistant guide
- [FAQ.md](FAQ.md) - Frequently asked questions

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

### Development Setup

```bash
# Clone repository
git clone https://github.com/yourusername/Vaxthus_Master_V3.git

# Open in VS Code with PlatformIO extension
code Vaxthus_Master_V3

# Build
pio run

# Upload and monitor
pio run -t upload && pio device monitor
```

## 📄 License

This project is open source and available under the MIT License.

## 🙏 Acknowledgments

- **dalathegreat** - For the Battery-Emulator architecture that inspired this project
- **ArduinoJson** - JSON library by Benoit Blanchon
- **PubSubClient** - MQTT library by Nick O'Leary

## 📞 Support

If you encounter issues:
1. Check the [FAQ.md](FAQ.md)
2. Review the [Troubleshooting](#-troubleshooting) section
3. Check serial monitor output for error messages
4. Open an issue on GitHub with:
   - ESP32 board model
   - PlatformIO version
   - Serial monitor output
   - Steps to reproduce the issue

---

**Made with ❤️ for growing plants**

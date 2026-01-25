# Build & Deployment Guide

Advanced guide for building and deploying Vaxthus Master V3 firmware.

## 📦 Quick Commands

```bash
# Build only (no upload)
pio run

# Build and upload
pio run -t upload

# Build, upload, and monitor
pio run -t upload && pio device monitor

# Clean build
pio run -t clean

# Upload to specific port
pio run -t upload --upload-port COM7
```

---

## 🏗️ Build Process Explained

### What Happens When You Run `pio run`:

1. **Dependency Resolution**
   - Downloads ArduinoJson 7.0.0
   - Downloads PubSubClient 2.8
   - Downloads ESP32 Arduino framework

2. **Compilation**
   - Compiles `src/main.cpp`
   - Links libraries
   - Creates firmware binary

3. **Output Location**
   - Binary: `.pio/build/esp32/firmware.bin`
   - ELF: `.pio/build/esp32/firmware.elf`
   - Map: `.pio/build/esp32/firmware.map`

---

## 🔧 Build Configurations

### Standard Build (Release)

```bash
pio run
```

This is the default configuration optimized for production.

### Debug Build

Edit `platformio.ini` and add:

```ini
build_flags =
    -D CORE_DEBUG_LEVEL=5  # Maximum debug output
    -D CONFIG_ASYNC_TCP_RUNNING_CORE=1
```

Then:
```bash
pio run -t upload
pio device monitor
```

---

## 📡 Upload Methods

### Method 1: USB Serial (Default)

```bash
pio run -t upload
```

Uses UART over USB. Works with most ESP32 boards.

### Method 2: Manual Port Selection

```bash
# Windows
pio run -t upload --upload-port COM7

# Mac/Linux  
pio run -t upload --upload-port /dev/ttyUSB0
```

### Method 3: OTA (Not Yet Supported)

OTA updates are planned for v3.1.0. Currently requires USB cable.

---

## 🌿 Building Different Versions

### Headless Version (Production)

```bash
git checkout headless
pio run -t upload
```

**Features:**
- Sun simulation
- MQTT integration
- Web interface
- No display required

### Display Version (LilyGo)

```bash
git checkout lilygo-t-display
pio run -t upload
```

**Features:**
- Energy metering
- Screen display
- Advanced features
- Requires LilyGo T-Display board

### v3.2 (Future/Development)

```bash
git checkout v3.2
pio run -t upload
```

**Status:** Documentation only, not fully implemented yet

---

## ⚙️ Advanced Build Options

### Custom Defines

Edit `platformio.ini`:

```ini
build_flags =
    -D SUNRISE_START_HOUR=7     # Custom sunrise time
    -D SUNSET_START_HOUR=19     # Custom sunset time
    -D CORE_DEBUG_LEVEL=3       # Debug level
    -D CONFIG_ASYNC_TCP_RUNNING_CORE=1
```

### Memory Optimization

```ini
board_build.partitions = huge_app.csv  # Larger app partition
```

### Build Speed Optimization

```ini
build_flags =
    -j 4  # Use 4 CPU cores for parallel compilation
```

---

## 📊 Build Artifacts

After successful build, you'll find:

```
.pio/build/esp32/
├── firmware.bin      # Main firmware (flashable)
├── firmware.elf      # Debug symbols
├── firmware.map      # Memory map
├── bootloader.bin    # ESP32 bootloader
├── partitions.bin    # Partition table
└── spiffs.bin        # File system (if used)
```

---

## 🔍 Verifying Build

### Check Binary Size

```bash
pio run --target size
```

Output shows:
- RAM usage
- Flash usage
- Free space

**Typical sizes:**
- Firmware: ~1.2 MB
- Free Flash: ~2.8 MB
- RAM usage: ~60 KB

### Check for Warnings

```bash
pio run 2>&1 | grep warning
```

Fix any warnings before deploying.

---

## 🚀 Deployment Workflows

### Development Workflow

```bash
# Make code changes
# Then:
pio run -t upload && pio device monitor

# Watch for errors in serial output
# Press Ctrl+C to stop monitor
# Repeat
```

### Production Deployment

```bash
# 1. Clean build
pio run -t clean

# 2. Build fresh
pio run

# 3. Run tests (manual for now)
pio device monitor
# Verify functionality

# 4. Tag release
git tag -a v3.0.1 -m "Production release"
git push --tags

# 5. Upload to devices
pio run -t upload --upload-port COM6
```

### Multi-Device Deployment

```bash
# Device 1
pio run -t upload --upload-port COM6

# Device 2  
pio run -t upload --upload-port COM7

# Device 3
pio run -t upload --upload-port COM8
```

---

## 🐛 Debugging

### Enable Verbose Output

```bash
pio run -t upload --verbose
```

### ESP32 Exception Decoder

When ESP32 crashes, serial monitor shows stack trace:

```
Backtrace: 0x40081234:0x3ffb1234 0x40082345:0x3ffb2345
```

**Decode it:**
```bash
pio device monitor --filter esp32_exception_decoder
```

### Monitor with Timestamp

```bash
pio device monitor --filter time
```

### Monitor Multiple Filters

```bash
pio device monitor --filter time --filter esp32_exception_decoder
```

---

## 📝 Build Scripts

### Automated Build Script (Windows)

Create `build.bat`:
```batch
@echo off
echo Cleaning...
pio run -t clean

echo Building...
pio run

echo Upload? (Y/N)
set /p answer=
if /i "%answer%"=="Y" (
    pio run -t upload
    pio device monitor
)
```

### Automated Build Script (Linux/Mac)

Create `build.sh`:
```bash
#!/bin/bash
echo "Cleaning..."
pio run -t clean

echo "Building..."
pio run

read -p "Upload? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    pio run -t upload
    pio device monitor
fi
```

Make executable:
```bash
chmod +x build.sh
./build.sh
```

---

## 🔐 Security Considerations

### Before Deploying:

1. **Change default AP password** in src/main.cpp:
```cpp
String ap_password = "YourSecurePassword123";  // Change this!
```

2. **Remove debug flags** from platformio.ini:
```ini
# Remove or comment out in production:
# -D CORE_DEBUG_LEVEL=5
```

3. **Enable MQTT TLS** (requires code modification)

4. **Disable serial output** for production (optional):
   - Comment out Serial.println() in critical sections

---

## 📦 Creating Distributable Firmware

### Generate Flashable Binary

```bash
pio run
```

Binary is at: `.pio/build/esp32/firmware.bin`

### Flash Using esptool (without PlatformIO)

```bash
# Install esptool
pip install esptool

# Flash
esptool.py --chip esp32 --port COM6 write_flash 0x10000 .pio/build/esp32/firmware.bin
```

### Package for Distribution

```bash
# Create release package
mkdir release
cp .pio/build/esp32/firmware.bin release/
cp README.md release/
cp INSTALL.md release/
zip -r vaxthus-v3.0.0.zip release/
```

---

## 🧪 Testing Checklist

Before deploying:

- [ ] Compiles without errors
- [ ] No critical warnings
- [ ] WiFi connects successfully
- [ ] Web interface loads
- [ ] MQTT connects (if enabled)
- [ ] PWM outputs work (measure with multimeter)
- [ ] Sun simulation runs correctly
- [ ] Settings persist after reboot
- [ ] Manual override works
- [ ] Serial output is clean

---

## 🆘 Build Troubleshooting

### "Library not found"

```bash
pio pkg install
```

### "Espressif 32 platform not installed"

```bash
pio platform install espressif32@6.10.0
```

### "Out of memory" during compilation

Add to `platformio.ini`:
```ini
board_build.partitions = huge_app.csv
```

### Build is very slow

```bash
# Clear cache
pio run -t clean

# Update platforms
pio platform update

# Check disk space
```

### Upload fails

```bash
# Try holding BOOT button on ESP32
# Or lower upload speed:
```

Add to `platformio.ini`:
```ini
upload_speed = 115200  # Down from 921600
```

---

## 📚 Further Reading

- [PlatformIO CLI Documentation](https://docs.platformio.org/en/latest/core/quickstart.html)
- [ESP32 Arduino Documentation](https://docs.espressif.com/projects/arduino-esp32/)
- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)

---

## 💡 Pro Tips

1. **Use VS Code** with PlatformIO IDE extension for easier development
2. **Keep backups** of working firmware binaries
3. **Test on one device** before deploying to many
4. **Use git tags** for version tracking
5. **Document custom modifications** in code comments

---

**Happy Building! 🛠️**

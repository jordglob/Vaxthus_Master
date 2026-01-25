# Frequently Asked Questions (FAQ)

## 📡 WiFi & Connectivity

### Q: Why won't my ESP32 connect to WiFi?

**A:** The most common reason is trying to connect to a **5 GHz WiFi network**. The ESP32 **ONLY supports 2.4 GHz (802.11 b/g/n)** networks.

**Solutions:**
1. ✅ **Check your router settings** - Ensure you're connecting to the 2.4 GHz band, not 5 GHz
2. ✅ **Keep ESP32 within 3 meters (10 feet)** of your WiFi access point during initial setup
3. ✅ Ensure your WiFi password is correct (case-sensitive)
4. ✅ Check that your router isn't using WPA3-only security (use WPA2 or WPA2/WPA3 mixed mode)
5. ✅ Disable MAC address filtering temporarily
6. ✅ Try a different WiFi channel (1, 6, or 11 are usually best for 2.4 GHz)

### Q: How do I know if my WiFi is 2.4 GHz or 5 GHz?

**A:** Most routers show this in the network name (SSID):
- **2.4 GHz networks** often have names like `MyNetwork` or `MyNetwork-2.4G`
- **5 GHz networks** often have names like `MyNetwork-5G` or `MyNetwork-5GHz`

If unsure, check your router's admin panel or contact your ISP. Many modern routers broadcast both bands with the same name (band steering), which can cause confusion. In this case, temporarily disable the 5 GHz band during ESP32 setup.

### Q: Why does the WiFi requirement mention "within 3 meters"?

**A:** During initial configuration:
- The ESP32 needs a **strong, stable signal** to download settings and sync time
- 2.4 GHz WiFi signal strength **degrades quickly** through walls and obstacles
- Being close (< 3 meters / 10 feet) during setup ensures reliable connection
- Once configured and running, the ESP32 can operate further away (typically 10-15 meters indoors)

**Recommendation:** Place your ESP32 **permanently within good WiFi range** for best reliability.

### Q: What WiFi signal strength do I need?

**A:** Check the serial monitor for RSSI (Received Signal Strength Indicator):
- **-30 to -50 dBm**: Excellent (100% strength)
- **-50 to -60 dBm**: Very Good (75-90%)
- **-60 to -70 dBm**: Good (50-75%) ✅ **Recommended minimum**
- **-70 to -80 dBm**: Fair (25-50%) - May have occasional dropouts
- **-80 to -90 dBm**: Weak (0-25%) - Connection problems likely
- **Below -90 dBm**: Very Weak - Won't connect reliably

The web interface shows signal strength as a percentage for easy monitoring.

### Q: Can I use a WiFi extender or mesh network?

**A:** Yes! WiFi extenders and mesh networks work well, **as long as they broadcast on 2.4 GHz**. 

**Tips:**
- Ensure the extender uses the **same SSID and password** as your main router (seamless roaming)
- Or set up the extender with a unique SSID and configure the ESP32 to connect to it
- Place the extender so the ESP32 gets good signal strength (-60 dBm or better)

### Q: Does the ESP32 need internet access?

**A:** **Not required for basic operation**, but recommended:
- ✅ **Without internet**: Lights work, web interface accessible on local network, manual control works
- ✅ **With internet**: NTP time sync works (required for accurate sun simulation), MQTT may work (if broker is local)
  
**Sun simulation requires time sync**, which needs internet access at least once during setup.

## ⚙️ Configuration & Setup

### Q: What are the default Access Point credentials?

**A:** On first boot (or if WiFi isn't configured):
- **SSID**: `Vaxthus_Master`
- **Password**: `123456789`
- **IP Address**: `192.168.4.1`

Connect to this network with your phone/computer and navigate to `http://192.168.4.1`

### Q: How do I reset to factory defaults?

**A:** There are two methods:

**Method 1 - Via Serial Monitor:**
```cpp
// Open serial monitor and upload modified code with this in setup():
settings.clear();  // Add this line temporarily
ESP.restart();
```

**Method 2 - Flash Blank:**
```bash
pio run -t erase
pio run -t upload
```

### Q: Can I change the default AP password?

**A:** Yes, edit `src/main.cpp`:

```cpp
String ap_password = "YourNewPassword123";  // Change from "123456789"
```

Then re-upload the firmware.

### Q: Will my settings survive a power cycle?

**A:** Yes! All settings are stored in **non-volatile memory (NVM)**:
- WiFi credentials
- MQTT configuration
- Last light states

Settings persist through power cycles and reboots.

## 🌞 Sun Simulation

### Q: How do I change the sunrise/sunset times?

**A:** Edit the constants in `src/main.cpp`:

```cpp
#define SUNRISE_START_HOUR   6   // Default: 06:00
#define SUNRISE_END_HOUR     10  // Default: 10:00
#define SUNSET_START_HOUR    18  // Default: 18:00
#define SUNSET_END_HOUR      22  // Default: 22:00
```

Then re-upload the firmware.

### Q: Why isn't sun simulation working?

**A:** Check these requirements:
1. ✅ **Time must be synced** - Check serial monitor for "Time synced:" message
2. ✅ **WiFi must have internet access** - NTP requires internet
3. ✅ **Not in manual override mode** - Check serial monitor for "[Sun Sim]" messages
4. ✅ **Within simulation hours** - Outside 06:00-22:00 (default), lights are off

If time sync fails, wait 5 minutes for automatic retry.

### Q: How does manual override work?

**A:** When you adjust any light channel (via web or MQTT):
- System enters **manual mode** for **40 minutes**
- Sun simulation is paused
- Lights stay at your set values
- After 40 minutes, automatically returns to sun simulation
- Serial monitor shows `[Manual] Override activated for 40 minutes`

### Q: Can I have different schedules for each color?

**A:** Not in version 3.0.0, but it's on the roadmap. Currently, all channels follow the same schedule.

See [AI_PRIMER.md](AI_PRIMER.md#4-add-per-channel-sun-simulation) for modification instructions if you want to implement it.

## ⚡ Electricity Price Dimming (Tibber Integration)

### Q: Can the system automatically dim lights when electricity is expensive?

**A:** **YES!** The headless version already supports this via MQTT and Home Assistant, **no code changes needed**.

**How it works:**
```
Tibber → Home Assistant → MQTT → Vaxthus Lights
```

The ESP32 receives MQTT commands from Home Assistant, which monitors electricity prices from Tibber.

### Q: How do I set up Tibber electricity price dimming?

**A:** You need:
1. **Tibber account** with API access
2. **Home Assistant** with Tibber integration
3. **MQTT** configured on Vaxthus (see MQTT setup below)

**Step-by-step:**

**1. Install Tibber in Home Assistant:**
- Go to Settings → Integrations → Add Integration
- Search for "Tibber"
- Enter your Tibber API token
- You'll get entities like `sensor.tibber_current_price`

**2. Create Home Assistant Automation:**

```yaml
automation:
  - alias: "Dim Grow Lights During High Electricity Prices"
    description: "Reduce light intensity when electricity > 1.5 SEK/kWh"
    
    trigger:
      - platform: numeric_state
        entity_id: sensor.tibber_current_price
        above: 1.5  # Adjust threshold to your preference
        
    action:
      - service: light.turn_on
        target:
          entity_id:
            - light.grow_light_white
            - light.grow_light_red
            - light.grow_light_uv
        data:
          brightness: 128  # 50% brightness to save electricity
          
  - alias: "Restore Grow Lights When Electricity Price Normal"
    description: "Return to full brightness when price drops"
    
    trigger:
      - platform: numeric_state
        entity_id: sensor.tibber_current_price
        below: 1.0  # Normal price
        
    action:
      - service: light.turn_on
        target:
          entity_id:
            - light.grow_light_white
            - light.grow_light_red
            - light.grow_light_uv
        data:
          brightness: 255  # 100% brightness
```

**3. Advanced: Multi-Level Dimming Based on Price Tiers:**

```yaml
automation:
  - alias: "Dynamic Grow Light Dimming Based on Electricity Price"
    trigger:
      - platform: state
        entity_id: sensor.tibber_current_price
        
    action:
      - choose:
          # Very High Price (>2.0 SEK/kWh) - 30% brightness
          - conditions:
              - condition: numeric_state
                entity_id: sensor.tibber_current_price
                above: 2.0
            sequence:
              - service: light.turn_on
                target:
                  entity_id:
                    - light.grow_light_white
                    - light.grow_light_red
                    - light.grow_light_uv
                data:
                  brightness: 77  # 30%
                  
          # High Price (1.5-2.0 SEK/kWh) - 50% brightness
          - conditions:
              - condition: numeric_state
                entity_id: sensor.tibber_current_price
                above: 1.5
                below: 2.0
            sequence:
              - service: light.turn_on
                target:
                  entity_id:
                    - light.grow_light_white
                    - light.grow_light_red
                    - light.grow_light_uv
                data:
                  brightness: 128  # 50%
                  
          # Medium Price (1.0-1.5 SEK/kWh) - 75% brightness
          - conditions:
              - condition: numeric_state
                entity_id: sensor.tibber_current_price
                above: 1.0
                below: 1.5
            sequence:
              - service: light.turn_on
                target:
                  entity_id:
                    - light.grow_light_white
                    - light.grow_light_red
                    - light.grow_light_uv
                data:
                  brightness: 191  # 75%
                  
        # Default: Low Price (<1.0 SEK/kWh) - 100% brightness
        default:
          - service: light.turn_on
            target:
              entity_id:
                - light.grow_light_white
                - light.grow_light_red
                - light.grow_light_uv
            data:
              brightness: 255  # 100%
```

### Q: Will price-based dimming interfere with sun simulation?

**A:** Yes, it overrides sun simulation for 40 minutes (manual mode). **For v3.2**, this will be improved:
- Option to disable manual override when price changes
- Ability to combine sun simulation + price multiplier
- Example: 50% price dim × 75% sun simulation = 37.5% brightness

### Q: Can I use other electricity price sources instead of Tibber?

**A:** Yes! Any price sensor in Home Assistant works:
- **Nordpool** - Common in Scandinavia
- **Entsoe** - European market prices
- **Octopus Energy** - UK
- **Custom sensors** from your electricity provider

Just replace `sensor.tibber_current_price` with your price sensor in the automation.

## 🏠 Home Assistant & MQTT

### Q: How do I set up Home Assistant integration?

**A:** 
1. Configure MQTT broker in Home Assistant (Mosquitto add-on)
2. In Vaxthus web interface, go to **Settings**
3. Check **Enable MQTT**
4. Enter your MQTT broker address (e.g., `192.168.1.100`)
5. Enter username/password if required
6. Click **Save & Reboot**
7. Entities auto-discover as `light.grow_light_white`, `light.grow_light_red`, `light.grow_light_uv`

### Q: What if I don't have MQTT/Home Assistant?

**A:** **MQTT is completely optional!** The system works perfectly without it:
- Use the web interface for control
- Sun simulation still works
- All features available except Home Assistant integration

### Q: Can I use a public MQTT broker?

**A:** **Not recommended** for security reasons. The default `mqtt.revolt-energy.org` is a placeholder - replace with your own broker.

For local operation, install Mosquitto on a Raspberry Pi or use Home Assistant's built-in MQTT broker.

### Q: My MQTT broker requires authentication but I don't want to store credentials

**A:** Consider these options:
1. Set up a **dedicated MQTT user** with limited permissions (read/write only to `bastun/vaxtljus/*`)
2. Use **TLS/SSL encryption** (requires code modification)
3. Keep MQTT broker on **isolated VLAN** for IoT devices
4. Use **certificate-based authentication** instead of passwords (requires code modification)

## 💡 Hardware & Wiring

### Q: Which GPIO pins are used for PWM?

**A:** 
- **GPIO 16** - White LED channel (RX2)
- **GPIO 17** - Red LED channel (TX2)
- **GPIO 18** - UV LED channel

These output **0-3.3V PWM** at **5 kHz** frequency.

### Q: Do I need a MOSFET driver?

**A:** **Yes!** The ESP32 GPIO pins can only supply ~40mA max. You need:
- **MOSFETs** (e.g., IRLZ44N) for high-current LED strips
- **LED driver modules** (e.g., constant current drivers)
- Or **PWM-compatible LED power supplies**

**Never connect high-power LEDs directly to GPIO pins!**

### Q: Can I control more than 3 channels?

**A:** Yes! The ESP32 has 16 PWM channels. See [AI_PRIMER.md](AI_PRIMER.md#2-add-a-4th-pwm-channel) for instructions on adding more channels.

### Q: What PWM frequency should I use?

**A:** The default **5 kHz** works well for most LED applications:
- **Above human perception** (no visible flicker)
- **Compatible with most LED drivers**
- **Low electrical noise**

You can change it by modifying `#define PWM_FREQ 5000` in `src/main.cpp`.

### Q: My LEDs flicker at low brightness

**A:** Try these solutions:
1. **Increase PWM frequency** to 10 kHz or higher
2. **Check your LED driver** - Some don't work well with PWM dimming
3. **Use a constant current driver** instead of voltage-based dimming
4. **Add capacitors** across LED power supply (100µF-1000µF)

## 🔧 Troubleshooting

### Q: Web interface won't load

**A:** Check these items:
1. ✅ Connected to correct WiFi network
2. ✅ Using correct IP address (check serial monitor)
3. ✅ Try `http://` explicitly (not `https://`)
4. ✅ Disable VPN if active
5. ✅ Try different browser
6. ✅ Clear browser cache
7. ✅ Firewall not blocking port 80

### Q: ESP32 keeps rebooting

**A:** Common causes:
- **Insufficient power supply** - Use 5V 2A+ power supply
- **Bad USB cable** - Try different cable
- **Watchdog timeout** - Check for blocking code in loop()
- **Memory overflow** - Check serial monitor for crash dumps
- **Hardware issue** - Try different ESP32 board

### Q: Serial monitor shows gibberish

**A:** Set baud rate to **115200** in your serial monitor tool. This is defined in `platformio.ini`:
```ini
monitor_speed = 115200
```

### Q: How do I update the firmware?

**A:** Currently (v3.0.0), you need to use a USB cable:

```bash
cd Vaxthus_Master_V3
git pull  # Get latest code
pio run -t upload  # Upload to ESP32 via USB
```

**Note:** OTA (Over-The-Air) updates are **NOT supported** in v3.0.0, but are **planned for v3.1.0**.

### Q: Does it support OTA (Over-The-Air) updates?

**A:** **Not yet.** OTA updates are planned for version 3.1.0, which will allow you to update firmware via the web interface without needing a USB cable.

For now, firmware updates require:
- USB cable connection
- PlatformIO installation
- Running `pio run -t upload`

### Q: Where can I get help?

**A:**
1. Check this FAQ
2. Read [README.md](README.md) troubleshooting section
3. Review [AI_PRIMER.md](AI_PRIMER.md) for technical details
4. Check serial monitor output for error messages
5. Open an issue on GitHub with:
   - ESP32 board model
   - PlatformIO version
   - Complete serial monitor output
   - Steps to reproduce

## 🔒 Security

### Q: Is the web interface secure?

**A:** **No authentication** is implemented in version 3.0.0. This is intended for **local network use only**.

**Recommendations:**
- ⚠️ **Do NOT expose** to the internet
- ⚠️ **Change default AP password**
- ⚠️ Use on **trusted networks only**
- ⚠️ Consider **network segmentation** (IoT VLAN)

Web authentication is planned for version 3.1.0.

### Q: Are MQTT credentials encrypted?

**A:** Credentials are stored in **plain text in NVM**. This is standard for ESP32 applications.

**Security measures:**
- Use **dedicated MQTT user** with minimal permissions
- Keep device on **isolated network segment**
- Use **TLS/SSL** for MQTT (requires code modification)
- Physical security - keep device in secure location

### Q: Can someone access my WiFi password?

**A:** WiFi credentials are stored in NVM. Someone with **physical access** to your ESP32 could potentially extract them using:
- Serial monitor connection
- Flash memory dump tools

**Mitigation:**
- Keep device in **secure location**
- Use **guest network** for IoT devices
- Enable **flash encryption** (advanced, requires ESP-IDF)

## 📈 Performance

### Q: How much power does it use?

**A:** ESP32 power consumption:
- **Active WiFi**: ~160-260mA @ 3.3V (~0.5-0.9W)
- **Light sleep**: ~15-20mA (not implemented)
- **Deep sleep**: ~10µA (not implemented)

Add your LED power consumption separately (typically 10-100W depending on setup).

### Q: Can it run on battery?

**A:** **Not recommended** for this application:
- High power draw when WiFi active (~200mA)
- Continuous operation required for sun simulation
- LEDs require significant power

For battery operation, you'd need:
- Large battery pack (10,000+ mAh)
- Solar charging
- Power management code modifications

### Q: How reliable is it for long-term use?

**A:** Very reliable when properly set up:
- ✅ Runs continuously for weeks/months
- ✅ Automatic WiFi reconnection
- ✅ MQTT reconnection handling
- ✅ Watchdog timer prevents hangs
- ✅ Persistent state survives power cycles

**Recommendations for 24/7 operation:**
- Use quality power supply
- Ensure good WiFi signal
- Monitor serial output periodically
- Keep firmware updated

## 🌱 Growing Tips

### Q: What light schedule is best for plants?

**A:** Depends on plant type and growth stage:

**Vegetative Growth:**
- 18-24 hours light per day
- Modify sunrise to 00:00 or 06:00, sunset to 18:00-24:00

**Flowering:**
- 12 hours light, 12 hours dark
- Example: sunrise 06:00→10:00, sunset 18:00→22:00

**Seedlings:**
- 16-18 hours light per day
- Lower intensity (50-75% brightness)

### Q: What color spectrum should I use?

**A:** General guidelines:
- **Blue/White (400-500nm)**: Vegetative growth, compact plants
- **Red (600-700nm)**: Flowering, fruiting
- **UV (315-400nm)**: Stress response, terpene production (use sparingly!)

The sun simulation applies equally to all channels. For advanced use, modify code for per-channel schedules.

### Q: How close should lights be to plants?

**A:** Depends on LED power:
- **Low power (10-20W)**: 15-30 cm (6-12 inches)
- **Medium power (50-100W)**: 30-60 cm (12-24 inches)
- **High power (200W+)**: 60-100 cm (24-40 inches)

Watch for signs of light stress (bleaching, curling leaves) and adjust accordingly.

---

## 💬 Still Have Questions?

If your question isn't answered here:
1. Check the [README.md](README.md) for general information
2. Review [AI_PRIMER.md](AI_PRIMER.md) for technical details
3. Open an issue on GitHub
4. Join the discussion forum (if available)

**Happy Growing! 🌱**

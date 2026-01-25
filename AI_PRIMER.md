# AI Primer - Vaxthus Master V3

This document provides a comprehensive overview of the Vaxthus Master V3 architecture, designed to help AI assistants (and developers) quickly understand the codebase structure, design patterns, and best practices.

## 📋 Table of Contents

- [Project Overview](#project-overview)
- [Architecture Philosophy](#architecture-philosophy)
- [Code Structure](#code-structure)
- [Key Design Patterns](#key-design-patterns)
- [Data Flow](#data-flow)
- [State Management](#state-management)
- [Hardware Abstraction](#hardware-abstraction)
- [Common Modification Patterns](#common-modification-patterns)
- [Testing Guidelines](#testing-guidelines)
- [Troubleshooting Guide](#troubleshooting-guide)

## Project Overview

**Vaxthus Master V3** is an ESP32-based grow light controller with:
- 3-channel PWM control (White, Red, UV)
- Automatic sun simulation based on real-time clock
- Web interface for configuration and control
- MQTT integration for Home Assistant
- Dual-mode WiFi (AP + STA)
- Non-volatile settings storage

### Inspired By
This project is based on the **Battery-Emulator** architecture by [dalathegreat](https://github.com/dalathegreat), which provides excellent patterns for:
- Preferences-based configuration
- Dual-mode WiFi handling
- Modular code organization
- Non-blocking operations

## Architecture Philosophy

### Core Principles

1. **Non-Blocking Operations**: The main loop must never block. All time-consuming operations use timeouts and state machines.

2. **Fail-Safe Design**: If WiFi disconnects or MQTT fails, the lights continue operating based on last known state or sun simulation.

3. **User-Friendly Configuration**: No code editing required for basic configuration. Everything is accessible via web interface.

4. **Persistent State**: All user settings and states survive power cycles and reboots.

5. **Observable Behavior**: Serial monitor provides detailed status information for debugging and monitoring.

## Code Structure

### File Organization

```
Vaxthus_Master_V3/
├── src/
│   └── main.cpp              # Single-file application (intentionally monolithic for simplicity)
├── include/                  # Empty (all headers in main.cpp)
├── platformio.ini           # PlatformIO configuration
├── README.md                # User documentation
├── CHANGELOG.md             # Version history
├── AI_PRIMER.md             # This file
├── FAQ.md                   # Common questions
└── driver/                  # PlatformIO platform files (ESP32 boards)
```

### Main.cpp Structure

The file is organized into clear sections:

```cpp
// 1. Pin Definitions & Constants
#define PWM_WHITE_PIN 16
#define PWM_RED_PIN 17
// ...

// 2. Global Variables
Preferences settings;
WebServer server(80);
// ...

// 3. Forward Declarations
void init_wifi();
void init_mqtt();
// ...

// 4. Setup Function
void setup() { /* initialization */ }

// 5. Main Loop
void loop() { /* non-blocking operations */ }

// 6. Implementation Functions (grouped by feature)
// - Settings Management
// - PWM Control
// - WiFi Management
// - Time & Sun Simulation
// - MQTT
// - Web Server
```

### Why Single File?

For embedded projects of this size (< 1000 lines):
- **Easier to navigate**: Everything in one place
- **No header/implementation synchronization issues**
- **Simpler build process**
- **Better for AI assistants**: Full context in single file

For larger projects, consider splitting into modules.

## Key Design Patterns

### 1. State Machine Pattern (WiFi & MQTT)

```cpp
void wifi_monitor() {
    if (millis() - lastWifiCheck < 10000) return;  // Check every 10s
    lastWifiCheck = millis();
    
    if (wifi_ssid.length() > 0 && WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();  // Trigger reconnection
    }
}
```

**Pattern**: Check condition periodically without blocking, take action if needed.

### 2. Preferences Pattern (Settings Storage)

```cpp
void load_settings() {
    settings.begin("vaxthus", false);  // namespace, read-only=false
    wifi_ssid = settings.getString("SSID", "");  // key, default
    mqtt_enabled = settings.getBool("MQTTENABLED", false);
    // ...
}

void save_settings() {
    settings.putString("SSID", wifi_ssid);
    settings.putBool("MQTTENABLED", mqtt_enabled);
}
```

**Pattern**: Load on startup, save on change. No manual EEPROM management.

### 3. Manual Override Pattern

```cpp
void set_light(uint8_t channel, uint8_t value) {
    autoMode = false;  // Disable auto mode
    manualOverrideStart = millis();  // Record when override started
    
    // Set the light...
    
    save_light_state();  // Persist for power cycle
}

void update_sun_simulation() {
    // Check if 40 minutes have passed
    if (!autoMode && (millis() - manualOverrideStart > MANUAL_OVERRIDE_DURATION)) {
        autoMode = true;  // Return to auto mode
    }
    
    if (!autoMode) return;  // Skip if in manual mode
    
    // Calculate and apply sun simulation...
}
```

**Pattern**: User action triggers manual mode with timeout. Automatically returns to auto mode.

### 4. Event-Driven WiFi Pattern

```cpp
WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("Connected to AP");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf("Disconnected, reason: %d\n", info.wifi_sta_disconnected.reason);
            break;
        // ...
    }
});
```

**Pattern**: Register callbacks for network events instead of polling.

### 5. Sun Simulation Algorithm

```cpp
uint8_t calculate_light_level(int hour, int minute) {
    int totalMinutes = hour * 60 + minute;
    
    if (totalMinutes < SUNRISE_START) return 0;  // Night
    if (totalMinutes >= SUNSET_END) return 0;     // Night
    if (totalMinutes >= SUNRISE_END && totalMinutes < SUNSET_START) return 255;  // Day
    
    // During sunrise or sunset, calculate linear ramp
    if (totalMinutes >= SUNRISE_START && totalMinutes < SUNRISE_END) {
        int elapsed = totalMinutes - SUNRISE_START;
        int duration = SUNRISE_END - SUNRISE_START;
        return (elapsed * 255) / duration;  // Linear interpolation
    }
    
    // Sunset (ramp down)
    int elapsed = totalMinutes - SUNSET_START;
    int duration = SUNSET_END - SUNSET_START;
    return 255 - ((elapsed * 255) / duration);
}
```

**Pattern**: Convert time to minutes since midnight, use linear interpolation for smooth transitions.

## Data Flow

### Startup Sequence

```
1. Serial.begin(115200)
2. load_settings() → Read from NVM
3. init_pwm() → Configure GPIO, restore last state
4. init_wifi() → Start AP, connect to STA
5. init_webserver() → Register HTTP handlers
6. init_mqtt() → Connect if enabled
7. init_time() → Sync with NTP
8. Enter main loop
```

### Main Loop Flow

```
loop() {
    server.handleClient()         // Handle web requests (non-blocking)
    wifi_monitor()                // Check WiFi every 10s
    mqtt_loop()                   // Handle MQTT messages
    update_sun_simulation()       // Update lights every 60s
    delay(10)                     // Small delay to prevent watchdog timeout
}
```

### User Light Control Flow

```
1. User adjusts slider on web interface
2. JavaScript calls /setLight?white=128
3. server.on("/setLight") handler receives request
4. set_light(PWM_CHANNEL_WHITE, 128) is called
5. autoMode = false (manual override activated)
6. ledcWrite() outputs PWM signal
7. publish_state() sends to MQTT (if connected)
8. save_light_state() persists to NVM
9. After 40 minutes, autoMode = true automatically
```

### MQTT Command Flow

```
1. Home Assistant sends MQTT message to bastun/vaxtljus/white/set = "200"
2. mqtt_callback() receives message
3. set_light(PWM_CHANNEL_WHITE, 200) is called
4. Manual override activated (same as web control)
5. State published back to bastun/vaxtljus/white/state = "200"
```

## State Management

### Global State Variables

```cpp
// Light state (0-255) - Current PWM values
uint8_t light_white = 0;
uint8_t light_red = 0;
uint8_t light_uv = 0;

// Mode tracking
bool autoMode = true;                    // Auto sun simulation vs manual control
unsigned long manualOverrideStart = 0;   // When manual mode was activated

// Timing
unsigned long lastMqttReconnect = 0;     // Last MQTT connection attempt
unsigned long lastWifiCheck = 0;         // Last WiFi status check
unsigned long lastSunUpdate = 0;         // Last sun simulation update

// Connection tracking
bool ha_discovery_sent = false;          // Whether HA discovery was sent
```

### Persistent State (NVM)

Stored in ESP32 Preferences (key-value store):

```cpp
// WiFi settings
settings.putString("SSID", wifi_ssid);
settings.putString("PASSWORD", wifi_password);

// MQTT settings
settings.putString("MQTTSERVER", mqtt_server);
settings.putUInt("MQTTPORT", mqtt_port);
settings.putString("MQTTUSER", mqtt_user);
settings.putString("MQTTPASS", mqtt_password);
settings.putBool("MQTTENABLED", mqtt_enabled);

// Light states (restored on boot)
settings.putUChar("LIGHTWHITE", light_white);
settings.putUChar("LIGHTRED", light_red);
settings.putUChar("LIGHTUV", light_uv);
```

## Hardware Abstraction

### PWM Channels

ESP32 has 16 independent PWM channels. We use 3:

```cpp
#define PWM_CHANNEL_WHITE 0  // Channel 0 → GPIO 16
#define PWM_CHANNEL_RED   1  // Channel 1 → GPIO 17
#define PWM_CHANNEL_UV    2  // Channel 2 → GPIO 18

ledcSetup(PWM_CHANNEL_WHITE, PWM_FREQ, PWM_RESOLUTION);
ledcAttachPin(PWM_WHITE_PIN, PWM_CHANNEL_WHITE);
ledcWrite(PWM_CHANNEL_WHITE, brightness);  // 0-255
```

### GPIO Pin Selection

**Why GPIO 16, 17, 18?**
- Not used by flash/PSRAM
- Support all PWM functions
- Available on standard DevKit pinout
- Grouped together on board (easier wiring)
- GPIO 16/17 are RX2/TX2 (UART2) but we don't use serial2

**Avoid These Pins:**
- GPIO 0: Boot mode (pulled up, needs to be low for programming)
- GPIO 2: Boot mode (on-board LED on some boards)
- GPIO 12: Boot voltage (MTDI)
- GPIO 6-11: Connected to flash (don't use!)
- GPIO 34-39: Input only (no PWM output)

## Common Modification Patterns

### 1. Change Sun Simulation Schedule

```cpp
// Modify these constants at the top of main.cpp:
#define SUNRISE_START_HOUR   7   // Changed from 6
#define SUNRISE_END_HOUR     9   // Changed from 10
#define SUNSET_START_HOUR    20  // Changed from 18
#define SUNSET_END_HOUR      23  // Changed from 22
```

### 2. Add a 4th PWM Channel

```cpp
// 1. Add pin definition
#define PWM_BLUE_PIN 19
#define PWM_CHANNEL_BLUE 3

// 2. Add global variable
uint8_t light_blue = 0;

// 3. Initialize in init_pwm()
ledcSetup(PWM_CHANNEL_BLUE, PWM_FREQ, PWM_RESOLUTION);
ledcAttachPin(PWM_BLUE_PIN, PWM_CHANNEL_BLUE);
ledcWrite(PWM_CHANNEL_BLUE, light_blue);

// 4. Add to set_light() switch statement
case PWM_CHANNEL_BLUE:
    light_blue = value;
    ledcWrite(PWM_CHANNEL_BLUE, value);
    publish_state("blue", value);
    break;

// 5. Add to MQTT callback
else if (topicStr.endsWith("/blue/set")) {
    set_light(PWM_CHANNEL_BLUE, value);
}

// 6. Add to web interface HTML (slider + endpoint)
// 7. Add to publish_ha_discovery() array
// 8. Add to NVM save/load
```

### 3. Change Manual Override Duration

```cpp
// 40 minutes = 2400000 ms
#define MANUAL_OVERRIDE_DURATION 3600000  // Change to 60 minutes
```

### 4. Add Per-Channel Sun Simulation

Currently all channels use the same schedule. To make them independent:

```cpp
// 1. Create separate schedules
struct ChannelSchedule {
    uint8_t sunrise_start;
    uint8_t sunrise_end;
    uint8_t sunset_start;
    uint8_t sunset_end;
};

ChannelSchedule white_schedule = {6, 10, 18, 22};
ChannelSchedule red_schedule = {7, 11, 17, 21};
ChannelSchedule uv_schedule = {10, 12, 16, 18};  // Shorter UV exposure

// 2. Modify calculate_light_level() to accept schedule parameter
uint8_t calculate_light_level(int hour, int minute, ChannelSchedule sched)

// 3. Update update_sun_simulation() to call for each channel
uint8_t white_level = calculate_light_level(hour, minute, white_schedule);
uint8_t red_level = calculate_light_level(hour, minute, red_schedule);
uint8_t uv_level = calculate_light_level(hour, minute, uv_schedule);
```

### 5. Add Temperature Sensor

```cpp
// 1. Add library (e.g., DHT22)
#include <DHT.h>
#define DHT_PIN 4
DHT dht(DHT_PIN, DHT22);

// 2. Initialize in setup()
dht.begin();

// 3. Read periodically in loop()
void read_temperature() {
    static unsigned long lastRead = 0;
    if (millis() - lastRead < 60000) return;  // Every 60s
    lastRead = millis();
    
    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();
    
    // Publish to MQTT
    mqtt.publish("bastun/vaxtljus/temperature", String(temp).c_str());
    mqtt.publish("bastun/vaxtljus/humidity", String(humidity).c_str());
}

// 4. Call in loop()
loop() {
    server.handleClient();
    wifi_monitor();
    mqtt_loop();
    update_sun_simulation();
    read_temperature();  // Add this
    delay(10);
}
```

## Testing Guidelines

### Unit Testing Strategy

For ESP32 embedded projects, traditional unit tests are challenging. Instead:

1. **Serial Monitor Testing**: Watch output during operation
2. **Web Interface Testing**: Verify all controls work
3. **MQTT Testing**: Use MQTT Explorer to monitor messages
4. **Oscilloscope Testing**: Verify PWM signals (5 kHz, 0-3.3V)
5. **Long-Run Testing**: Leave running for 24+ hours

### Test Scenarios

```cpp
// Test 1: WiFi Reconnection
// - Disconnect WiFi AP
// - Wait 10 seconds
// - Reconnect AP
// - Verify device reconnects automatically

// Test 2: Manual Override Timeout
// - Set light manually via web interface
// - Wait 40 minutes
// - Verify system returns to auto mode

// Test 3: MQTT Resilience
// - Stop MQTT broker
// - Verify lights continue working
// - Restart broker
// - Verify reconnection within 5 seconds

// Test 4: Power Cycle Persistence
// - Configure WiFi and MQTT
// - Set lights to specific values
// - Power cycle ESP32
// - Verify settings and states restored

// Test 5: Sun Simulation Accuracy
// - Set system time to 06:00
// - Verify lights ramp from 0% to 100% over 4 hours
// - Check intermediate values at 07:00, 08:00, 09:00
```

### Debugging Techniques

```cpp
// 1. Increase debug level in platformio.ini
build_flags =
    -D CORE_DEBUG_LEVEL=5  // 0=None, 5=Verbose

// 2. Add debug prints
Serial.printf("[DEBUG] Variable x = %d\n", x);

// 3. Monitor WiFi events
WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("[WiFi Event] %d\n", event);
});

// 4. Track memory usage
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

// 5. Use watchdog timer to detect hangs
// (already built into ESP32 Arduino framework)
```

## Troubleshooting Guide

### Common Issues for AI Assistants

#### Issue: WiFi Won't Connect

**Symptoms**: Status 6 (WL_DISCONNECTED) repeatedly  
**Likely Causes**:
- 5 GHz network (ESP32 only supports 2.4 GHz)
- Weak signal (RSSI < -80 dBm)
- Wrong password
- WPA3-only network

**Solution**:
```cpp
// Add more detailed error reporting
Serial.printf("WiFi status: %d (0=IDLE, 3=CONNECTED, 6=DISCONNECTED)\n", WiFi.status());
Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
```

#### Issue: NTP Time Won't Sync

**Symptoms**: "Failed to sync time"  
**Likely Causes**:
- No internet connection
- Firewall blocking NTP (port 123 UDP)
- Router DNS issues

**Solution**:
```cpp
// Add fallback NTP servers
configTime(3600, 3600, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
```

#### Issue: PWM Not Working

**Symptoms**: No voltage on GPIO pins  
**Likely Causes**:
- Wrong pin numbers
- Channel not initialized
- Pin conflict

**Solution**:
```cpp
// Verify pin configuration
Serial.printf("PWM Frequency: %d Hz\n", ledcReadFreq(PWM_CHANNEL_WHITE));
Serial.printf("PWM Duty: %d\n", ledcRead(PWM_CHANNEL_WHITE));
```

#### Issue: Web Server Not Responding

**Symptoms**: Can't access web interface  
**Likely Causes**:
- Wrong IP address
- Port 80 blocked
- Server not initialized

**Solution**:
```cpp
// Add request logging
server.onNotFound([]() {
    Serial.printf("404: %s\n", server.uri().c_str());
    server.send(404, "text/plain", "Not Found");
});
```

## Best Practices for AI Assistants

### When Modifying Code

1. **Always preserve the section structure** (Pin Definitions, Global Variables, etc.)
2. **Use forward declarations** for all functions before setup()
3. **Add Serial.println() for debugging** new features
4. **Test WiFi functionality** after any WiFi-related changes
5. **Verify MQTT topics** match Home Assistant expectations
6. **Check NVM keys** don't exceed 15 characters
7. **Ensure loop() remains non-blocking** - no long delays!

### When Adding Features

1. **Start with minimal working implementation**
2. **Add serial debug output**
3. **Test in isolation** before integrating
4. **Update web interface** if user-facing
5. **Add MQTT support** for Home Assistant integration
6. **Persist to NVM** if it's user configuration
7. **Document in CHANGELOG.md**

### Code Style

```cpp
// Good: Clear, documented, with error handling
void init_wifi() {
    Serial.println("Initializing WiFi...");
    WiFi.mode(WIFI_AP_STA);
    
    if (!WiFi.softAP(ap_ssid.c_str(), ap_password.c_str())) {
        Serial.println("  [ERROR] Failed to start AP");
        return;
    }
    
    Serial.printf("  AP started: %s\n", ap_ssid.c_str());
}

// Bad: No output, no error handling
void init_wifi() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
}
```

## Conclusion

This architecture provides a solid foundation for embedded IoT devices with:
- Robust WiFi and MQTT handling
- User-friendly configuration
- Reliable operation
- Easy extensibility

The single-file approach works well for projects of this size. For larger systems, consider splitting into modules while maintaining these core patterns.

## Further Reading

- [ESP32 Arduino Core Documentation](https://docs.espressif.com/projects/arduino-esp32/)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [Home Assistant MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)
- [Battery-Emulator Project](https://github.com/dalathegreat/Battery-Emulator)

---

**Version**: 3.0.0  
**Last Updated**: 2026-01-25  
**Maintainer**: Vaxthus Master Project

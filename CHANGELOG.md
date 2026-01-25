# Changelog

All notable changes to the Vaxthus Master V3 project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.0.0] - 2026-01-25

### Added
- **Complete rewrite** based on Battery-Emulator architecture by dalathegreat
- **Automatic sun simulation** with configurable sunrise/sunset schedules
  - Sunrise: 06:00-10:00 (gradual ramp up)
  - Full daylight: 10:00-18:00
  - Sunset: 18:00-22:00 (gradual ramp down)
  - Night: 22:00-06:00 (lights off)
- **Manual override mode** with 40-minute timeout before returning to auto mode
- **Three independent PWM channels** (White, Red, UV) on GPIO 16, 17, 18
- **Web-based configuration interface** with responsive design
  - Real-time light control sliders
  - WiFi signal strength indicator
  - MQTT connection status
- **Home Assistant MQTT auto-discovery**
  - Automatic entity creation in Home Assistant
  - Full brightness control (0-255)
  - State feedback for all channels
- **NTP time synchronization** for accurate sun simulation
  - Configured for Central European Time (CET/CEST)
  - Automatic daylight saving time adjustment
  - Retry mechanism if initial sync fails
- **Preferences-based settings storage**
  - WiFi credentials
  - MQTT configuration
  - Last known light states
- **Dual-mode WiFi** (Access Point + Station)
  - Always-available configuration AP
  - Automatic reconnection to home network
  - WiFi status monitoring with auto-reconnect
- **Event-driven WiFi handling** for robust operation
- **Serial monitor output** with detailed status information
- **Non-volatile memory (NVM)** for persistent settings

### Technical Details
- **Platform**: ESP32-WROOM-32 (DevKit V1)
- **Framework**: Arduino for ESP32
- **PlatformIO**: espressif32@6.10.0
- **Libraries**:
  - ArduinoJson 7.0.0 (JSON parsing and generation)
  - PubSubClient 2.8 (MQTT communication)
- **PWM Specifications**:
  - Frequency: 5 kHz
  - Resolution: 8-bit (0-255)
  - Channels: 3 independent outputs
- **Web Server**: Async HTTP server on port 80
- **Default AP**:
  - SSID: `Vaxthus_Master`
  - Password: `123456789`
  - IP: `192.168.4.1`

### Architecture
- Modular code structure with clear separation of concerns
- Forward declarations for clean function organization
- Setup/Loop pattern with non-blocking operations
- Preference-based configuration (inspired by Battery-Emulator)
- State machine for WiFi and MQTT management
- Automatic recovery from connection failures

### MQTT Topics
- **Base topic**: `bastun/vaxtljus`
- **Command topics**: `{base}/{channel}/set` (white, red, uv)
- **State topics**: `{base}/{channel}/state`
- **Discovery prefix**: `homeassistant`

### Web Endpoints
- `GET /` - Main dashboard with light controls
- `GET /settings` - Configuration page
- `POST /saveSettings` - Save configuration and reboot
- `GET /setLight` - Set light values (with query parameters)
- `GET /status` - JSON status endpoint

### Configuration Constants
```cpp
#define SUNRISE_START_HOUR   6
#define SUNRISE_END_HOUR     10
#define SUNSET_START_HOUR    18
#define SUNSET_END_HOUR      22
#define MANUAL_OVERRIDE_DURATION 2400000  // 40 minutes
#define PWM_FREQ        5000
#define PWM_RESOLUTION  8
```

### Removed from Previous Versions
- Legacy code structure
- Obsolete configuration methods
- Unused dependencies

### Fixed
- WiFi reconnection reliability
- MQTT connection stability
- Time synchronization errors
- Memory leaks in web server
- PWM channel interference

### Known Issues
- NTP sync may fail on first boot if WiFi connection is slow (auto-retries every 5 minutes)
- MQTT reconnection may take up to 5 seconds after network interruption
- Web interface requires page refresh to see updated connection status

### Security Notes
- Default AP password should be changed for production use
- MQTT credentials are stored in plain text in NVM
- Web interface has no authentication (intended for local network use only)
- Consider using MQTT over TLS for sensitive deployments

### Performance
- Boot time: ~5-10 seconds to full operation
- WiFi connection: 5-20 seconds depending on network
- MQTT connection: 1-3 seconds after WiFi connected
- Web response time: <100ms for most requests
- Memory usage: ~60KB RAM, ~1.2MB Flash

### Future Roadmap (v3.1.0+)

**Planned for v3.1.0:**
- [ ] **Manual mode exit button** - Web interface button to return to auto/sun simulation mode immediately (instead of waiting 40 minutes)
- [ ] **UV channel limiter** - Automatically limit UV channel to 80% of white channel brightness to extend UV LED lifetime
- [ ] **OTA (Over-The-Air) firmware updates** - Update firmware via web interface without USB cable
- [ ] Web interface authentication

**Future versions:**
- [ ] Custom sun simulation schedules per channel
- [ ] Temperature/humidity sensor integration
- [ ] Historical data logging
- [ ] Advanced scheduling with multiple time periods
- [ ] REST API for external control
- [ ] WebSocket support for real-time updates
- [ ] Mobile app companion
- [ ] Multi-timezone support

## [2.x.x] - Previous Versions

Earlier versions were prototype implementations and are not documented in this changelog.

---

## Version Numbering Scheme

**MAJOR.MINOR.PATCH**

- **MAJOR**: Incompatible API changes or major architectural rewrites
- **MINOR**: New features in a backward-compatible manner
- **PATCH**: Backward-compatible bug fixes

## Contributing

When adding changes to this file:
1. Add new entries under "Unreleased" section
2. Move to version section when releasing
3. Include date in format YYYY-MM-DD
4. Group changes by type (Added, Changed, Fixed, Removed, etc.)
5. Be descriptive but concise
6. Link to issues/PRs where applicable

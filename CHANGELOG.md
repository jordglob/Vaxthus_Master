# Changelog

All notable changes to the Vaxthus Master V3 project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.4.0] - 2026-01-25

### Added
- **Real-time clock display** on main web interface
  - Shows current time with format "Time: HH:MM:SS"
  - Updates every 2 seconds via status API
  - Displays "Time: Not synced" if NTP hasn't synchronized
- **Improved mode indicators** with visual badges
  - **AUTO MODE**: Green badge with sun icon when in automatic sun simulation
  - **MANUAL MODE**: Yellow badge with hand icon when in manual override
  - Clear visual distinction between operating modes
- **Manual mode countdown timer**
  - Shows minutes remaining in manual override mode
  - Format: "(X min remaining)" next to MANUAL MODE badge
  - Helps users understand when system will return to auto mode
- **Enhanced "Return to Auto Mode" button**
  - More prominent button styling with larger padding
  - Only visible when in manual mode (auto-hides in auto mode)
  - Immediate mode switch without page reload
- **UV LED lifespan protection in auto mode**
  - UV channel automatically limited to 80% of calculated light level
  - Only active in automatic sun simulation mode
  - Manual mode still allows full 0-255 control
  - Extends UV LED lifetime significantly
- **Clean English user interface**
  - All user-facing text converted to English
  - Proper UTF-8 charset declaration for symbol support
  - Clear informational text about UV limiter behavior
- **Improved web interface styling**
  - Better CSS with enhanced spacing and shadows
  - Smooth hover effects on buttons
  - Professional badge design for mode indicators
  - Improved readability and visual hierarchy

### Changed
- **Status API enhanced** (`/status` endpoint)
  - Added `time` field with full timestamp
  - Added `time_synced` boolean field
  - Added `manual_minutes_left` for countdown display
  - Maintains backward compatibility with v3.3.0
- **Web interface update frequency** increased to 2 seconds (from 5 seconds)
  - Provides more responsive user feedback
  - Better real-time experience for mode changes
- **Sun simulation logging** now includes UV percentage
  - Serial output shows: "Light: X% | UV: Y% (Auto mode)"
  - Helps verify UV limiter is working correctly

### Fixed
- Character encoding issues with emoji symbols in web interface
- Mode indicator not updating immediately after manual override timeout
- Status updates not reflecting real-time changes in some browsers

### Technical Details
- **RAM**: 15.3% (49,972 bytes) - No change from v3.3.0
- **Flash**: 67.1% (879,381 bytes) - +176 bytes from v3.3.0
- **Compilation**: SUCCESS ✅
- **OTA Upload**: Tested and verified on 192.168.38.112
- **Browser Testing**: Chrome, Edge - all features working

### Development Notes
```cpp
// New constants used:
// (No new constants - used existing framework)

// Modified functions:
- update_sun_simulation() // Added UV limiter logic
- get_index_html() // Complete UI redesign
- /status endpoint // Added time and countdown fields
```

### Upgrade Notes
- **Fully backward compatible** with v3.3.0
- **OTA upgrade** from v3.3.0 works flawlessly
- **No configuration changes** required
- **No breaking changes** to MQTT topics or API
- Settings from v3.3.0 are preserved during upgrade

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

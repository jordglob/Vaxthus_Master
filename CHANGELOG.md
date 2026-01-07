# Changelog

All notable changes to the **Vaxthus Master** project will be documented in this file.

## [v1.3] - 2026-01-07
### Added
- **Settings Menu:** New menu page accessible after "STÄLL TID".
- **Eco Toggle:** Ability to toggle Eco Mode directly from the device menu.
- **Reboot:** Option to reboot the device from the menu.
- **OTA:** Added support for Over-The-Air updates (`ArduinoOTA`).
- **Flicker Free:** Implemented double buffering (Sprites) for the display header to remove flickering.

## [v1.2] - 2026-01-06
### Added
- **Intro Demo:** "Amiga-style" boot sequence with 3D starfield, warp effect, and glitch text.
- **Eco Mode:** New MQTT switch (`.../powersave/set`) to enable power saving mode (reduces brightness by 50% during high electricity prices).
- **Display:** Added "ECO" indicator in the top header.
- **Discovery:** Auto-discovery for the Eco Mode switch in Home Assistant (`switch.vaxtljus_powersave`).

### Changed
- **Clock Menu:** Now displays seconds and hundredths (`HH:MM:SS.ms`) when manually setting time for higher precision.
- **Performance:** Dynamic display refresh rate (increased to 50ms in clock menu vs 200ms normal operation).

## [v1.1] - 2026-01-06
### Changed
- **Core:** Complete rewrite of boot logic. `setup()` is now non-blocking.
- **Core:** WiFi connection handling moved to background operations in `loop()`. System boots immediately to menu/schedule regardless of network status.
- **Schedule:** Increased ramp-up (sunrise) and ramp-down (sunset) duration from 1h to **2h**.

### Added
- **Manual Clock:** Dedicated menu "STÄLL TID" allows setting time manually via buttons (useful if running offline without NTP).
- **UX:** Smart menu visibility - Clock setting only appears if time is invalid or manually requested.
- **Docs:** Added `protokoll.md` describing MQTT API and internal logic.

## [v1.0] - Initial Release
### Added
- Basic MQTT Control for White, Red, and UV channels.
- Automatic Day/Night Cycle based on NTP time.
- PWM Control for LED Driver.
- Basic Display Interface (Time, Status, WiFi).
- Home Assistant MQTT Discovery for Lights.

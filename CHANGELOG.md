# Changelog

All notable changes to the **Vaxthus Master** project will be documented in this file.

## [v3.0.0] - 2026-01-08 -- "The Professional Update"
### Changed
- **Architecture (Major):** Complete refactor to Object-Oriented Programming (OOP). Replaced loose global variables and helper functions with a robust `LightChannel` class.
- **Code hygiene:** `main.cpp` is significantly cleaner and easier to read.
- **Display:** Footer updated to show Total Energy Usage in real-time.

### Added
- **Energy Metering:** The system now calculates energy consumption (Watt-hours) for each channel based on usage and intensity.
- **MQTT Energy Reporting:** Publishes total energy stats to `<topic_prefix>/energy/total` every 60 seconds.
- **LightChannel Class:** Encapsulates PWM control, fading logic, and power calculations into a single reusable component.

## [v2.1.0] - 2026-01-08 -- "The Isolation Update"
### Changed
- **Architecture (Critical):** Implemented a "Trap Loop" architecture. When in AP Mode, the device now enters a completely isolated `loopAP()` cycle. This bypasses all heavy logic (MQTT, NTP sync attempts) that was causing >1500ms latency, making the device lightning fast even without an internet connection.
- **Refactoring:** 
  - Moved all AP-specific logic out of `main.cpp` into new files `src/APMode.cpp` and `include/APMode.h`.
  - Moved HTML generation and Web API handling to `APMode.cpp` to separate UI logic from hardware logic.
  - Introduced `extern` usage in `Globals.h` to properly share state between modules without circular dependencies.

### Added
- **AP Dashboard:** The AP Mode landing page is no longer just a "Hello" message. It is now a fully functional dashboard that allows:
  - Controlling White/Red/Blue light channels individually.
  - Setting the device time manually (essential since NTP fails offline).
- **UX:** The QR Code screen on the display now explicitly prints the IP address (e.g., `192.168.4.1`) to help users who cannot scan the code.

### Fixed
- **Documentation:** Major overhaul of `README.md` including a "State Comparison Table" and professional OTA instructions.

## [v2.0.3] - 2026-01-08 -- "The Nerd Update"
### Added
- **Captive Portal:** Implemented a DNS Server in AP mode. Connecting to the `Vaxthus-Master` WiFi network will now automatically trigger the "Sign In" prompt on mobile devices, directing users immediately to the web interface.
  - Redirects all DNS queries to the device IP.
  - Added "Catch-All" web handler to serve the app regardless of the requested URL (e.g., handling Apple/Android connectivity checks).
- **Setup Flow:** When starting in AP mode (offline), the QR code is now displayed indefinitely until a button is pressed, ensuring users have time to scan and connect.

## [v2.0.2] - 2026-01-08
### Fixed
- **Stability:** Fixed critical bug where device would "hang" or stutter when running in AP Mode (Offline). This was caused by the radio trying to scan for WiFi stations on the main thread.
- **Display:** Fixed artifacts/ghosting when switching between different menu views (Lists vs Fullscreen).
- **Status Icons:** Wifi/MQTT icons now correctly turn RED (or "NO-W") immediately upon disconnection.

### Added
- **Smart QR:** The QR Code menu now displays a `WIFI:T:WPA;...` config code when offline, allowing one-tap connection to the device's Access Point. When online, it shows the Web Interface URL.
- **Offline Logic:**
  - Boot sequence shortened.
  - If WiFi fails: Show connection QR for 10s.
  - Enter "AP Mode" (Access Point: `Vaxthus-Master`).
  - Retry connecting to Home WiFi every 30 seconds in background.

## [v2.0.1] - 2026-01-08
### Added
- **Soft Fading:** Implemented smooth transitions (fading) when changing brightness values, both via MQTT and manual control.
- **Presets:** New menu item "PRESETS" containing pre-defined light recipes:
  - **Seed:** (Low White)
  - **Veg:** (High White/Red)
  - **Bloom:** (Red Dominant)
  - **Full:** (Max Power)

## [v2.0] - 2026-01-07
> **Major Release:** "Refactoring & Modularization"
> This release marks the transition from a single-file prototype (v1.x) to a professional, modular C++ architecture.

### Changed
- **Architecture:** 
  - Split the 1200+ line `main.cpp` monolith into distinct classes:
    - `DisplayManager`: Handles all TFT rendering, Sprites, and UI logic.
    - `ButtonHandler`: Encapsulates debouncing, double-clicks, and long-press detection.
    - `Globals.h`: Centralized verification of pin mappings and shared variables.
- **Security:** Introduced `secrets.h` to protect WiFi credentials (removed from main codebase).
- **OTA:** Enabled `ArduinoOTA` for wireless firmware updates, eliminating the need for USB cables after installation.

### Removed
- **Spaghetti Code:** Removed direct display drawing calls from the main loop to fix "flicker" and maintainability issues.

## [v1.4] - 2026-01-07
### Added
- **Web Interface:** Built-in mobile-friendly web dashboard accessible via device IP.
- **REST API:** JSON endpoints for status (`/api/status`) and control (`/api/set`).
- **Web Controls:** Real-time sliders for all light channels, Eco mode toggle, and Reboot button via web browser.
- **QR Code:** New menu item to display a QR code for quick access to the Web Interface.

### Fixed
- **UI:** Resolved flickering issues in the Settings menu by implementing Sprite-based rendering.
- **Layout:** Fixed overlapping text between footer and menu items.
- **Navigation:** Improved button sensitivity (increased double-click timeout) and added "Panic Button" shortcut (Long Press Bottom) to jump to Settings.
- **Rendering:** Fixed clipping issues with the QR code display.

## [v1.3] - 2026-01-07
### Added
- **Language Support:** Complete Swedish and English translation. Toggle in Settings menu.
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

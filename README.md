# Växtljus Master (Greenhouse Light Controller)

Detta projekt är en ESP32-baserad styrenhet för växtbelysning, skriven i C++ med PlatformIO och Arduino framework. Den är designad för **LilyGo T-Display S3** och styr tre PWM-kanaler (Vit, Röd, UV) via LED-drivare.

## Funktioner

*   **WiFi & MQTT:** Ansluter till nätverk och kommunicerar med MQTT-broker.
*   **Home Assistant Discovery:** Dyker automatiskt upp som tre dimbara lampor i Home Assistant.
*   **TFT Display:** Visar klocka (NTP), anslutningsstatus, och aktuell ljusstyrka för alla kanaler.
*   **PWM Styrning:** 8-bitars upplösning (0-255) för mjuk ljuskontroll.
*   **Knappstyrning:** Avancerad logik för att styra enheten manuellt vid skärmen.

## Hårdvara

*   **MCU:** ESP32-S3 (LilyGo T-Display S3)
*   **Skärm:** Inbyggd ST7789 TFT
*   **Knappar:** Inbyggda (GPIO 14 och GPIO 0)
*   **PWM Utgångar:**
    *   Vit: GPIO 1
    *   Röd: GPIO 2
    *   UV: GPIO 3

## Installation

1.  Klona detta repository.
2.  Öppna projektet i VS Code med PlatformIO.
3.  Redigera `src/main.cpp` och fyll i dina uppgifter:
    *   `ssid` & `password` (WiFi)
    *   `mqtt_server`, `mqtt_user`, `mqtt_pass`
4.  Ladda upp till enheten.

## Användning

Enheten har ett menysystem där du kan välja att styra **ALLA** lampor samtidigt, eller en specifik kanal (**VIT**, **RÖD**, **UV**).

### Navigering & Styrning

| Knapp | Handling | Funktion |
| :--- | :--- | :--- |
| **Övre (GPIO 14)** | **Enkelklick** | **Öka** ljusstyrka (+10%) på vald kanal. |
| **Övre (GPIO 14)** | **Dubbelklick** | **Byt kanal**. Stegar igenom: `ALLA` -> `VIT` -> `RÖD` -> `UV` -> `ALLA`... |
| **Nedre (GPIO 0)** | **Enkelklick** | **Minska** ljusstyrka (-10%) på vald kanal. |
| **Nedre (GPIO 0)** | **Dubbelklick** | **Stäng av** vald kanal helt (0%). |

### Displayikoner
*   **W:** WiFi Status (Grön = Ansluten, Röd = Ej ansluten).
*   **M:** MQTT Status (Grön = Ansluten, Röd = Ej ansluten).
*   **Klocka:** Hämtas automatiskt via NTP (Europe/Stockholm).

## Home Assistant Integration

Enheten använder MQTT Discovery. Se till att du har MQTT-integrationen aktiverad i Home Assistant. Tre "Lights" kommer automatiskt att skapas:
*   `light.vaxtljus_white`
*   `light.vaxtljus_red`
*   `light.vaxtljus_uv`

Ingen YAML-konfiguration krävs i Home Assistant.

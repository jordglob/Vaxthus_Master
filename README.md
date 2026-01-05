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
3.  **Viktigt - Konfiguration:**
    
    För att hantera lösenord säkert ingår inte filen `secrets.h` i repot. Du måste skapa den manuellt.
    
    1.  Skapa en ny fil i mappen `include/` och döp den till `secrets.h`.
    2.  Klistra in följande kod i filen och byt ut uppgifterna mot dina egna:

    ```cpp
    #ifndef SECRETS_H
    #define SECRETS_H

    // WiFi
    const char* ssid = "Ditt_WiFi_Namn";
    const char* password = "Ditt_WiFi_Lösenord";

    // MQTT
    const char* mqtt_server = "din.mqtt.server.org";
    const int mqtt_port = 1883;
    const char* mqtt_user = "Ditt_MQTT_Användarnamn";
    const char* mqtt_pass = "Ditt_MQTT_Lösenord";

    #endif
    ```

4.  Ladda upp koden till enheten via PlatformIO.

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

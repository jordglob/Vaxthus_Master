# Systemprotokoll: Växthus Master (ESP32-S3)

Detta dokument beskriver det tekniska protokollet, MQTT-strukturen och den interna logiken för Växthus-styrningen.

## 1. Systemöversikt
Systemet är byggt på en ESP32-S3 (LilyGo T-Display S3) som styr tre PWM-kanaler (Vit, Röd, UV) via MOSFETs/Drivere. Enheten agerar som en MQTT-klient och integreras automatiskt i Home Assistant.

## 2. MQTT API

**Bas-topic:** `bastun/vaxtljus`

### 2.1 Styrning (Inkommande)
Enheten prenumererar på kommandon via följande topics. Payload förväntas vara JSON.

| Topic | Beskrivning | JSON Payload Exempel |
|-------|-------------|----------------------|
| `.../white/set` | Styr Vit kanal | `{"state": "ON", "brightness": 255}` |
| `.../red/set` | Styr Röd kanal | `{"state": "OFF"}` |
| `.../uv/set` | Styr UV kanal | `{"state": "ON", "brightness": 128}` |

**Obs:** Varje inkommande kommando triggar automatiskt **MANUELLT LÄGE**.

### 2.2 Status (Utgående)
Vid ändring (via MQTT, Knappar eller Schema) publicerar enheten sin status.

| Topic | Beskrivning |
|-------|-------------|
| `.../white/state` | Bekräftelse för Vit kanal |
| `.../red/state` | Bekräftelse för Röd kanal |
| `.../uv/state` | Bekräftelse för UV kanal |

**Payload Format:**
```json
{
  "state": "ON",      // eller "OFF"
  "brightness": 200   // 0-255
}
```

### 2.3 Home Assistant Discovery
Vid uppstart (reconnect) skickar enheten konfiguration till Home Assistant enligt [MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#discovery-messages).

*   **Discovery Prefix:** `homeassistant/light/bastun_vaxtljus_[typ]/config`
*   **Unika IDn:** `vaxtljus_white`, `vaxtljus_red`, `vaxtljus_uv`

## 3. Intern Logik & Driftlägen

### 3.1 AUTO (Standard)
Systemet följer ett hårdkodat tidsschema baserat på NTP-tid (Svensk tid).

*   **NATT (22:00 - 05:30):** Allt släckt (0%).
*   **SOLUPPGÅNG (05:30 - 07:30):** Linjär ökning (rampning) från 0% till 100%.
*   **DAG (07:30 - 20:00):** Full styrka (100% Vit/Röd, UV begränsad till 80%).
*   **SOLNEDGÅNG (20:00 - 22:00):** Linjär minskning (rampning) från 100% till 0%.

**Säkerhetsbegränsning:**
I Auto-läge skalas UV-kanalen alltid 0-80% (maxvärde 204 av 255) för att skydda växterna.

### 3.2 MANUAL (Override)
Aktiveras vid:
1.  Fysisk knapptryckning (Justering eller byte av kanal).
2.  Inkommande MQTT-kommando.

**Beteende:**
*   Visas som orange text "MAN" på displayen.
*   Schemaläggaren pausas temporärt.
*   UV-kanalen kan tvingas till 100% manuellt (ingen spärr).
*   **Timeout:** Efter 45 minuter utan aktivitet återgår systemet automatiskt till AUTO.
*   **Manuell återgång:** Håll in TOP-knappen (1s) för att direkt gå till AUTO.

## 4. Användargränssnitt (Hårdvara)

### 4.1 Display
*   **Header:** Visar Tid, Driftläge (AUTO/MAN), WiFi-status, MQTT-status.
*   **Lista:** Markör `>` visar vald kanal. Visar %-värde för varje kanal.
*   **Footer:** Rullande text (Ticker) med instruktioner.

### 4.2 Knappar

| Knapp | Handling | Funktion |
|-------|----------|----------|
| **TOP (Pin 14)** | Enkelklick | Öka ljusstyrka (+10%) |
| | Dubbelklick | Byt vald kanal (ALL -> VIT -> RÖD -> UV) |
| | Långtryck (1s) | Återställ till AUTO-läge |
| **BTM (Pin 0)** | Enkelklick | Minska ljusstyrka (-10%) |
| | Dubbelklick | Stäng av vald kanal helt (0%) |

## 5. Uppstart & Handskakning
Beskriver sekvensen av händelser när enheten startar upp (Boot Process).

### 5.1 Boot Sekvens (Initiering)
1.  **Hårdvara:** Initierar Display, Knappar och PWM-kanaler (LEDs startar alltid på 0 / svart).
2.  **WiFi-anslutning:**
    *   Ansluter till SSID angivet i `secrets.h`.
    *   Displayen visar "WiFi: ..." och blockerar vidare exekvering tills anslutning är etablerad ("OK!").
3.  **Tidssynk (NTP):**
    *   Hämtar aktuell tid från `pool.ntp.org` för tidszon `CET-1CEST`.
    *   Systemet kräver giltig tid för att kunna köra Auto-schemat.

### 5.2 MQTT Handskakning
När WiFi är etablerat påbörjas MQTT-uppkopplingen. Detta sker antingen direkt vid start eller via `reconnect()` om anslutningen bryts.

1.  **Connect:** Enheten ansluter till brokern med ett slumpmässigt ClientID (`ESP32Client-xxxx`).
2.  **Discovery (Home Assistant):**
    *   Enheten skickar omedelbart 3 st "Configuration"-meddelanden (ett för varje kanal).
    *   Detta gör att Home Assistant automatiskt hittar ("upptäcker") enheterna utan manuell konfiguration i YAML-filer.
3.  **Prenumeration (Subscribe):**
    *   Enheten lyssnar på `bastun/vaxtljus/+/set` för att fånga upp kommandon till alla tre kanaler.

### 5.3 Initialt Tillstånd efter Boot
*   Systemet startar alltid i **AUTO**-läge.
*   Direkt efter uppstart körs funktionen `handleSchedule()`.
*   Om klockan är mitt på dagen, rampar ljuset upp till korrekt nivå direkt. Om det är natt förblir det släckt.


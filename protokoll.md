# Systemprotokoll: Växthus Master (ESP32-S3) v2.0

Detta dokument beskriver det tekniska protokollet, MQTT-strukturen och den interna logiken för Växthus-styrningen.

## 1. Systemöversikt
Systemet är byggt på en ESP32-S3 (LilyGo T-Display S3) som styr tre PWM-kanaler (Vit, Röd, UV) via MOSFETs/Drivere. Enheten agerar som en MQTT-klient och integreras automatiskt i Home Assistant. 

**Nytt i v2.0:** Systemet använder nu "Soft Fading" för att mjukt övergå mellan ljusstyrkor, samt stödjer förinställda lägen ("Presets") via menyn.

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

### 2.2 Eco Mode / Power Save
Ett speciellt läge för att spara energi (t.ex. vid högt elpris). När aktivt skalas ljusstyrkan ner med 50% i AUTO-läge.

| Topic | Typ | Payload |
|-------|-----|---------|
| `.../powersave/set` | Switch | `ON` / `OFF` |
| `.../powersave/state`| Status | `ON` / `OFF` |

### 2.3 Status (Utgående)
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
*   **Unika IDn:** `vaxtljus_white`, `vaxtljus_red`, `vaxtljus_uv`, `vaxtljus_powersave`

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
1.  Fysisk knapptryckning (Justering, byte av kanal eller aktivering av Preset).
2.  Inkommande MQTT-kommando.
3.  Ändring i Web Interface.

**Beteende:**
*   Visas som orange text "MAN" på displayen.
*   Schemaläggaren pausas temporärt.
*   UV-kanalen kan tvingas till 100% manuellt (ingen spärr).
*   **Timeout:** Efter 45 minuter utan aktivitet återgår systemet automatiskt till AUTO.
*   **Manuell återgång:** Håll in TOP-knappen (1s) för att direkt gå till AUTO.

### 3.3 Presets (Nytt i v2.0)
Systemet har inbyggda ljusprofiler för olika växtstadier. Dessa nås via menyn på enheten.

*   **Seed/Clone:** Lågt ljus, mest vitt (White: 100, Red: 40, UV: 0).
*   **Veg (Tillväxt):** Kraftigt vegetativt ljus (White: 220, Red: 80, UV: 10).
*   **Bloom (Blomning):** Maximerat rött ljus (White: 100, Red: 255, UV: 60).
*   **Full Blast:** Allt på max (White: 255, Red: 255, UV: 204/255).

## 4. Användargränssnitt (Hårdvara)

### 4.1 Display
*   **Header:** Visar Tid, Driftläge (AUTO/MAN), WiFi-status, MQTT-status, ECO-indikator.
*   **Meny:** Markör `>` visar vald kanal eller menyval (Clock, Presets, Settings).
*   **Footer:** Visar WiFi-signal (dBm) samt Mjukvaruversion (v2.0).

### 4.2 Knappar

| Knapp | Handling | Funktion |
|-------|----------|----------|
| **TOP (Pin 14)** | Enkelklick | Öka ljusstyrka (+10%) / Nästa värde i meny |
| | Dubbelklick | Byt menyval (ALL -> VIT -> RÖD -> UV -> **PRESET** -> CLOCK -> SETTINGS) |
| | Långtryck (1s) | Återställ till AUTO-läge |
| **BTM (Pin 0)** | Enkelklick | Minska ljusstyrka (-10%) / Välj/Ändra i meny |
| | Dubbelklick | Stäng av vald kanal helt (0%) |
| | Långtryck | Gå direkt till Inställningar (Settings) |

## 5. Web Interface
Systemet hostar en webbserver på port 80.

*   **/**: Dashboard med status och styrning.
*   **/api/status**: JSON-status för integrationer.
*   **/update**: OTA-uppladdning för firmware.
2.  **WiFi-anslutning:**
    *   Ansluter till SSID angivet i `secrets.h`.
    *   Displayen visar "WiFi: ..." och blockerar vidare exekvering tills anslutning är etablerad ("OK!").
    *   **KRITISK PUNKT:** Om WiFi ligger nere vid strömavbrott/omstart fastnar enheten här. Ingen styrning (varken automatisk eller manuell) är möjlig förrän nätverket är tillbaka.
3.  **Tidssynk (NTP):**
    *   Hämtar aktuell tid från `pool.ntp.org` för tidszon `CET-1CEST`.
    *   Systemet kräver giltig tid för att kunna köra Auto-schemat.
    *   **Felhantering:** Om Internet saknas (men WiFi finns) startar enheten, men AUTO-läget förblir inaktivt (lamporna tänds ej). Manuell styrning via knappar fungerar dock.

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


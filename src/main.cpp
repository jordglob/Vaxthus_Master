#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "time.h"
#include "secrets.h"

// --------------------------------------------------------------------------
// KONFIGURATION
// --------------------------------------------------------------------------

// WiFi & MQTT konfiguration hämtas nu från include/secrets.h

// MQTT Topics
const char* topic_prefix = "bastun/vaxtljus";

// NTP Server
const char* ntpServer = "pool.ntp.org";
const char* timezone_info = "CET-1CEST,M3.5.0,M10.5.0/3"; // Stockholm/Sverige regeln

// Hårdvara - PWM Pins
const int PIN_WHITE = 1;
const int PIN_RED = 2;
const int PIN_UV = 3;

// Hårdvara - Knappar (LilyGo T-Display S3)
const int PIN_BTN_TOP = 14;
const int PIN_BTN_BTM = 0;

// Knapp Konfig
const int DEBOUNCE_MS = 50;
const int DOUBLE_CLICK_MS = 300; // Tid för att registrera dubbelklick
const int LONG_PRESS_MS = 1000; // 1 sekund för långt tryck

// Schema & Begränsningar
const int UV_MAX_LIMIT = 204; // 80% av 255
const unsigned long MANUAL_TIMEOUT_MS = 45 * 60 * 1000; // 45 minuter

// Start/Stopp tider (Minuter sedan midnatt)
// 05:30 = 5*60 + 30 = 330
// 22:00 = 22*60 = 1320
const int TIME_SUNRISE_START = 330; 
const int TIME_DAY_START = 390;     // 06:30 (1h uppgång)
const int TIME_SUNSET_START = 1260; // 21:00
const int TIME_NIGHT_START = 1320;  // 22:00 (1h nedgång)

// PWM Inställningar
const int PWM_FREQ = 5000;
const int PWM_RES = 8; // 8-bitar (0-255)

// Kanaler för LEDC (ESP32 PWM)
const int CH_WHITE = 0;
const int CH_RED = 1;
const int CH_UV = 2;

// Menylägen
enum MenuSelection {
  SEL_ALL = 0,
  SEL_WHITE = 1,
  SEL_RED = 2,
  SEL_UV = 3
};

// --------------------------------------------------------------------------
// OBJEKT
// --------------------------------------------------------------------------
WiFiClient espClient;
PubSubClient client(espClient);
TFT_eSPI tft = TFT_eSPI();

// --------------------------------------------------------------------------
// STATE VARIABLER
// --------------------------------------------------------------------------
unsigned long lastReconnectAttempt = 0;
unsigned long lastDisplayUpdate = 0;

// Auto/Manual Logik
bool manual_mode = false;
unsigned long manual_timer_start = 0;

// Ljusvärden (0-255). Om > 0 räknas det som ON.
int val_white = 0;
int val_red = 0;
int val_uv = 0;

// Logik
MenuSelection current_selection = SEL_ALL;

// --------------------------------------------------------------------------
// KNAPP HANTERING CLASS
// --------------------------------------------------------------------------
class ButtonHandler {
  private:
    int pin;
    int state;
    unsigned long lastChange;
    bool waitingForDoubleClick;
    unsigned long clickTimestamp;
    bool longPressTriggered;
    
  public:
    ButtonHandler(int p) {
      pin = p;
      state = HIGH; // Input pullup defaults high
      lastChange = 0;
      waitingForDoubleClick = false;
      clickTimestamp = 0;
      longPressTriggered = false;
    }

    void init() {
      pinMode(pin, INPUT_PULLUP);
    }

    // Returnerar: 0=Inget, 1=Enkel, 2=Dubbel, 3=Långt tryck
    int update() {
      int reading = digitalRead(pin);
      int result = 0;
      unsigned long now = millis();

      if (reading != state) {
        if ((now - lastChange) > DEBOUNCE_MS) {
          state = reading;
          if (state == LOW) {
            // Knapp trycktes ner
            longPressTriggered = false;
          } else {
            // Knapp släpptes upp
            if (!longPressTriggered) {
              if (waitingForDoubleClick) {
                result = 2; // Dubbel
                waitingForDoubleClick = false;
              } else {
                // Potential enkel
                waitingForDoubleClick = true;
                clickTimestamp = now;
              }
            }
          }
        }
        lastChange = now;
      }

      // Detektera långt tryck (medan knappen hålls nere)
      if (state == LOW && !longPressTriggered) {
        if (now - lastChange > LONG_PRESS_MS) {
            longPressTriggered = true;
            result = 3; // Lång
            waitingForDoubleClick = false;
        }
      }

      // Check timeout for single click
      if (waitingForDoubleClick && result == 0) {
        if ((now - clickTimestamp) > DOUBLE_CLICK_MS) {
          result = 1; // Enkel
          waitingForDoubleClick = false;
        }
      }

      return result;
    }
};

ButtonHandler btnTop(PIN_BTN_TOP);
ButtonHandler btnBtm(PIN_BTN_BTM);

// --------------------------------------------------------------------------
// FUNKTIONER FRAMÅTDEKLARATION
// --------------------------------------------------------------------------
void publishOne(const char* type, int val);
void updatePWM();
void updateDisplay();
void activateManualMode();

// --------------------------------------------------------------------------
// HJÄLPFUNKTIONER
// --------------------------------------------------------------------------

void activateManualMode() {
  manual_mode = true;
  manual_timer_start = millis();
}

// Hanterar schemaläggning
void handleSchedule() {
  // Om vi är i manuellt läge, kolla timeout
  if (manual_mode) {
    if (millis() - manual_timer_start > MANUAL_TIMEOUT_MS) {
      manual_mode = false;
      Serial.println("Manual timeout. Back to INFO/AUTO.");
    } else {
      return; // Gör inget, låt användaren bestämma
    }
  }

  // Hämta tid
  struct tm ti;
  if (!getLocalTime(&ti)) return; // Ingen tid än

  int current_minutes = ti.tm_hour * 60 + ti.tm_min;
  int target_bri = 0;

  // Räkna ut ljusstyrka baserat på tid
  if (current_minutes < TIME_SUNRISE_START || current_minutes >= TIME_NIGHT_START) {
    // NATT (22:00 - 05:30)
    target_bri = 0;
  } 
  else if (current_minutes >= TIME_SUNRISE_START && current_minutes < TIME_DAY_START) {
    // SOLUPPGÅNG (05:30 - 06:30)
    // Mappa tiden (330 till 390) till styrkan (0 till 255)
    target_bri = map(current_minutes, TIME_SUNRISE_START, TIME_DAY_START, 0, 255);
  }
  else if (current_minutes >= TIME_DAY_START && current_minutes < TIME_SUNSET_START) {
    // DAG (06:30 - 21:00)
    target_bri = 255;
  }
  else if (current_minutes >= TIME_SUNSET_START && current_minutes < TIME_NIGHT_START) {
    // SKYMNING (21:00 - 22:00)
    // Mappa tiden (1260 till 1320) till styrkan (255 till 0)
    target_bri = map(current_minutes, TIME_SUNSET_START, TIME_NIGHT_START, 255, 0);
  }

  // Uppdatera variablers om de ändrats
  int target_uv = map(target_bri, 0, 255, 0, UV_MAX_LIMIT); // UV skalas alltid 0-80% i auto-läge

  if (val_white != target_bri || val_red != target_bri || val_uv != target_uv) {
     val_white = target_bri;
     val_red = target_bri;
     val_uv = target_uv;
     updatePWM();
  }
}

// Ändra ljusstyrka på vald kanal
// amount kan vara positivt (öka) eller negativt (minska)
void adjustBrightness(int amount) {
  activateManualMode(); // Trigga manuellt läge
  bool changed = false;
  
  // VIT
  if (current_selection == SEL_ALL || current_selection == SEL_WHITE) {
    int old = val_white;
    val_white = constrain(val_white + amount, 0, 255);
    if (old != val_white) {
      publishOne("white", val_white);
      changed = true;
    }
  }

  // RÖD
  if (current_selection == SEL_ALL || current_selection == SEL_RED) {
    int old = val_red;
    val_red = constrain(val_red + amount, 0, 255);
    if (old != val_red) {
      publishOne("red", val_red);
      changed = true;
    }
  }

  // UV
  if (current_selection == SEL_ALL || current_selection == SEL_UV) {
    int old = val_uv;
    val_uv = constrain(val_uv + amount, 0, 255);
    if (old != val_uv) {
      publishOne("uv", val_uv);
      changed = true;
    }
  }

  if (changed) {
    updatePWM();
    updateDisplay();
  }
}

void setChannelOff() {
  activateManualMode(); // Trigga manuellt läge
  bool changed = false;
  if (current_selection == SEL_ALL || current_selection == SEL_WHITE) {
    val_white = 0; publishOne("white", val_white); changed = true;
  }
  if (current_selection == SEL_ALL || current_selection == SEL_RED) {
    val_red = 0; publishOne("red", val_red); changed = true;
  }
  if (current_selection == SEL_ALL || current_selection == SEL_UV) {
    val_uv = 0; publishOne("uv", val_uv); changed = true;
  }
  if (changed) {
    updatePWM();
    updateDisplay();
  }
}

void updatePWM() {
  ledcWrite(CH_WHITE, val_white);
  ledcWrite(CH_RED, val_red);
  ledcWrite(CH_UV, val_uv);
}

// --------------------------------------------------------------------------
// DISPLAY
// --------------------------------------------------------------------------

void updateDisplay() {
  // Undvik flimmer, rita bara om vid behov eller var 500ms för klockan
  static unsigned long lastDraw = 0;
  // Om mindre än 200ms har gått, vänta (förutom om MQTT tvingar uppdatering)
  if (millis() - lastDraw < 200) return; 
  lastDraw = millis();

  // Hämta tid
  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo);

  tft.fillRect(0, 0, 320, 30, TFT_DARKGREY); 
  
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(2); // Medelstorlek
  tft.setCursor(5, 7);

  // Klocka
  if (timeValid) {
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    tft.print(timeStr);
  } else {
    tft.print("--:--");
  }

  // Visa MODE (A=Auto, M=Manuell)
  tft.setCursor(80, 7);
  if (manual_mode) {
    tft.setTextColor(TFT_ORANGE, TFT_DARKGREY);
    tft.print("M"); // Manuell
  } else {
    tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
    tft.print("A"); // Auto
  } 

  // Status Ikoner (enkla textmarkörer)
  // WiFi
  int xPos = 120;
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.drawString("W", xPos, 7);
  } else {
    tft.setTextColor(TFT_RED, TFT_DARKGREY);
    tft.drawString("W", xPos, 7);
  }

  // MQTT
  if (client.connected()) {
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.drawString("M", xPos + 25, 7);
  } else {
    tft.setTextColor(TFT_RED, TFT_DARKGREY);
    tft.drawString("M", xPos + 25, 7);
  }

  // === KANAL LISTA ===
  tft.setTextSize(2);
  int yStart = 35; // Flyttat upp lite
  int lineH = 28;  // Lite tajtare rader för att få plats med footer

  // Helper för att rita rader
  auto drawLine = [&](int idx, String label, int value, int color) {
    int y = yStart + (idx * lineH);
    bool isSelected = false;

    // Logic för markering
    if (current_selection == SEL_ALL && idx == 0) isSelected = true;
    if (current_selection == SEL_WHITE && idx == 1) isSelected = true;
    if (current_selection == SEL_RED && idx == 2) isSelected = true;
    if (current_selection == SEL_UV && idx == 3) isSelected = true;

    // Rita markör
    if (isSelected) {
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString(">", 5, y);
    } else {
      // Sudda markören om den inte är vald
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString(">", 5, y);
    }

    // Sätt färg
    tft.setTextColor(color, TFT_BLACK);
    
    // Label
    tft.drawString(label, 30, y);

    // Värde % 
    if (idx == 0) {
      // ALLA raden - Rensa eventuellt gammalt skräp
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString("100%  ", 220, y); 
    } else {
      int pct = map(value, 0, 255, 0, 100);
      // Lägg till mellanslag på slutet för att sudda tidigare bredare text (t.ex. 100% vs 99%)
      String pctStr = String(pct) + "%  "; 
      tft.setTextColor(color, TFT_BLACK); // Sätt färgen igen för säkerhets skull
      tft.drawString(pctStr, 220, y);
    }
  };

  drawLine(0, "ALLA", 0, TFT_WHITE);
  drawLine(1, "VIT", val_white, TFT_WHITE);
  drawLine(2, "ROD", val_red, TFT_RED);
  drawLine(3, "UV", val_uv, TFT_MAGENTA);

  // Information footer
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextDatum(BR_DATUM); // Bottom Right alignment
  tft.drawString("BTN TOP: + / Byt / Auto(1s)", 315, 158);
  tft.drawString("BTN BTM: - / Stang av", 315, 168);
  tft.setTextDatum(TL_DATUM); // Reset to Top Left default
}

// --------------------------------------------------------------------------
// MQTT
// --------------------------------------------------------------------------

void publishOne(const char* type, int val) {
  char topic[100];
  snprintf(topic, sizeof(topic), "%s/%s/state", topic_prefix, type);

  JsonDocument doc;
  doc["state"] = (val > 0) ? "ON" : "OFF";
  doc["brightness"] = val;
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  client.publish(topic, buffer, true);
}


void callback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) return;

  String topicStr = String(topic);
  String light_type = "";

  if (topicStr.indexOf("/white/set") > 0) light_type = "white";
  else if (topicStr.indexOf("/red/set") > 0) light_type = "red";
  else if (topicStr.indexOf("/uv/set") > 0) light_type = "uv";

  if (light_type != "") {
    activateManualMode(); // Om HA ändrar något räknas det som manuellt
    int* targetVal = nullptr;
    if (light_type == "white") targetVal = &val_white;
    if (light_type == "red") targetVal = &val_red;
    if (light_type == "uv") targetVal = &val_uv;

    if (targetVal) {
      if (doc.containsKey("state")) {
        const char* s = doc["state"];
        if (strcmp(s, "OFF") == 0) *targetVal = 0;
        else if (strcmp(s, "ON") == 0 && *targetVal == 0) *targetVal = 255; // Default max if ON command without brightness
      }
      if (doc.containsKey("brightness")) {
        *targetVal = doc["brightness"];
      }
      
      // Publicera tillbaka nytt läge
      publishOne(light_type.c_str(), *targetVal);
    }
    
    updatePWM();
    updateDisplay(); // Force update
  }
}

void sendDiscovery(const char* name, const char* light_type) {
  char topic[100];
  snprintf(topic, sizeof(topic), "homeassistant/light/%s_%s/config", "bastun_vaxtljus", light_type);

  JsonDocument doc;
  doc["name"] = name;
  char unique_id[64];
  snprintf(unique_id, sizeof(unique_id), "vaxtljus_%s", light_type);
  doc["unique_id"] = unique_id;
  doc["schema"] = "json";
  
  char cmd_topic[100];
  snprintf(cmd_topic, sizeof(cmd_topic), "%s/%s/set", topic_prefix, light_type);
  doc["command_topic"] = cmd_topic;

  char state_topic[100];
  snprintf(state_topic, sizeof(state_topic), "%s/%s/state", topic_prefix, light_type);
  doc["state_topic"] = state_topic;
  
  doc["brightness"] = true;
  doc["optimistic"] = false;
  doc["retain"] = true;

  char buffer[512];
  serializeJson(doc, buffer);
  client.publish(topic, buffer, true);
}

void reconnect() {
  if (!client.connected()) {
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      sendDiscovery("Växtljus Vit", "white");
      sendDiscovery("Växtljus Röd", "red");
      sendDiscovery("Växtljus UV", "uv");

      char sub_topic[100];
      snprintf(sub_topic, sizeof(sub_topic), "%s/+/set", topic_prefix);
      client.subscribe(sub_topic);
    }
  }
}

// --------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  // 1. Skärm
  tft.init();
  tft.setRotation(1); // 3 = Landscape/Flipped om usb är höger. 1 = Landscape.
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.println("Booting...");

  // 2. Knappar
  btnTop.init();
  btnBtm.init();

  // 3. PWM
  ledcSetup(CH_WHITE, PWM_FREQ, PWM_RES);
  ledcSetup(CH_RED, PWM_FREQ, PWM_RES);
  ledcSetup(CH_UV, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_WHITE, CH_WHITE);
  ledcAttachPin(PIN_RED, CH_RED);
  ledcAttachPin(PIN_UV, CH_UV);
  updatePWM();

  // 4. WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  tft.print("WiFi: ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }
  tft.println(" OK!");

  // 5. Tid / NTP
  tft.println("Syncing Time...");
  configTzTime(timezone_info, ntpServer);

  // 6. MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(1024);
  
  delay(500);
  tft.fillScreen(TFT_BLACK);
}

void loop() {
  // MQTT & WiFi Underhåll
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (WiFi.status() == WL_CONNECTED) reconnect();
      else WiFi.reconnect();
    }
  } else {
    client.loop();
  }

  // Knapp Logik
  int clickTop = btnTop.update();
  int clickBtm = btnBtm.update();

  // Kör schema-logik
  handleSchedule();

  // --- KNAPP TOP (Upp / Byt) ---
  if (clickTop == 1) { 
    // ENKEL: Öka ljusstyrka (+25 av 255 ≈ 10%)
    Serial.println("TOP: Single Click (Brightness +)");
    adjustBrightness(25);
  } 
  else if (clickTop == 2) {
    // DUBBEL: Byt Kanal (Loopa: All -> White -> Red -> UV -> All)
    Serial.println("TOP: Double Click (Next Channel)");
    if (current_selection == SEL_ALL) current_selection = SEL_WHITE;
    else if (current_selection == SEL_WHITE) current_selection = SEL_RED;
    else if (current_selection == SEL_RED) current_selection = SEL_UV;
    else current_selection = SEL_ALL;
    updateDisplay();
  }
  else if (clickTop == 3) {
    // LÅNG: Återgå till AUTO
    Serial.println("TOP: Long Press (Restore Auto)");
    manual_mode = false;
    handleSchedule(); // Applicera schema direkt
    updateDisplay();
  }

  // --- KNAPP BTM (Ner / Stäng av) ---
  if (clickBtm == 1) {
    // ENKEL: Minska ljusstyrka
    Serial.println("BTM: Single Click (Brightness -)");
    adjustBrightness(-25);
  }
  else if (clickBtm == 2) {
    // DUBBEL: Stäng av vald kanal helt (0%)
    Serial.println("BTM: Double Click (Turn Off Selected)");
    setChannelOff();
  }

  updateDisplay();
}

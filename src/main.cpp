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
    
  public:
    ButtonHandler(int p) {
      pin = p;
      state = HIGH; // Input pullup defaults high
      lastChange = 0;
      waitingForDoubleClick = false;
      clickTimestamp = 0;
    }

    void init() {
      pinMode(pin, INPUT_PULLUP);
    }

    // Returnerar: 0=Inget, 1=Enkel, 2=Dubbel
    int update() {
      int reading = digitalRead(pin);
      int result = 0;
      unsigned long now = millis();

      if (reading != state) {
        if ((now - lastChange) > DEBOUNCE_MS) {
          state = reading;
          if (state == LOW) {
            // Knapp trycktes ner
          } else {
            // Knapp släpptes upp
            if (waitingForDoubleClick) {
              // Vi fick ett andra klick inom tiden!
              result = 2;
              waitingForDoubleClick = false;
            } else {
              // Första klicket, starta timer
              waitingForDoubleClick = true;
              clickTimestamp = now;
            }
          }
        }
        lastChange = now;
      }

      // Check timeout for single click
      if (waitingForDoubleClick && result == 0) {
        if ((now - clickTimestamp) > DOUBLE_CLICK_MS) {
          result = 1;
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

// --------------------------------------------------------------------------
// HJÄLPFUNKTIONER
// --------------------------------------------------------------------------

// Ändra ljusstyrka på vald kanal
// amount kan vara positivt (öka) eller negativt (minska)
void adjustBrightness(int amount) {
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

  // Rensa skärm (eller delar av den, men fillScreen är enklast för ren kod)
  tft.fillScreen(TFT_BLACK);

  // === HEADER (TID & STATUS) ===
  tft.fillRect(0, 0, 320, 30, TFT_DARKGREY); // Header bakgrund
  
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
  int yStart = 45;
  int lineH = 35;

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
    }

    // Sätt färg
    tft.setTextColor(color, TFT_BLACK);
    
    // Label
    tft.drawString(label, 30, y);

    // Värde % (Visa inte procent på "ALLA" om de är olika, men vi visar --- isf)
    if (idx == 0) {
      // ALLA raden
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("", 200, y); 
    } else {
      int pct = map(value, 0, 255, 0, 100);
      String pctStr = String(pct) + "%";
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
  tft.setCursor(5, 150);
  tft.println("BTN TOP: + / Byt Kanal");
  tft.setCursor(5, 160);
  tft.println("BTN BTM: - / Stang av");
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

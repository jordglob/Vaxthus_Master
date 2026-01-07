#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <ArduinoOTA.h>
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
const int TIME_DAY_START = 450;     // 07:30 (2h uppgång)
const int TIME_SUNSET_START = 1200; // 20:00
const int TIME_NIGHT_START = 1320;  // 22:00 (2h nedgång)

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
  SEL_UV = 3,
  SEL_CLOCK = 4,
  SEL_SETTINGS = 5
};

// Inställningsmenyns val
enum SettingOption {
  SET_ECO = 0,
  SET_REBOOT = 1
};
int current_setting_option = 0; // Vilket val i settings-menyn vi står på

// --------------------------------------------------------------------------
// OBJEKT
// --------------------------------------------------------------------------
WiFiClient espClient;
PubSubClient client(espClient);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite footerSpr = TFT_eSprite(&tft); // Sprite för rullande text
TFT_eSprite headerSpr = TFT_eSprite(&tft); // Sprite för header (klocka/status) för att undvika flimmer

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
bool powerSaveMode = false; // Sparläge (t.ex. vid högt elpris)

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
void drawFooterTicker();
void activateManualMode();
void adjustTime(long deltaSec);

// --------------------------------------------------------------------------
// HJÄLPFUNKTIONER
// --------------------------------------------------------------------------

bool isTimeValid() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return false;
  if (timeinfo.tm_year < (2022 - 1900)) return false; // Säkerställ att vi inte är på 1970
  return true;
}

bool shouldShowClockMenu() {
  // Visa bara menyvalet om WiFi saknas ELLER tiden är ogiltig
  return (WiFi.status() != WL_CONNECTED || !isTimeValid());
}

void adjustTime(long deltaSec) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  tv.tv_sec += deltaSec;
  settimeofday(&tv, NULL);
  updateDisplay();
}

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

  // Applicera Power Save (50%) om aktivt
  if (powerSaveMode) {
      target_bri = target_bri / 2;
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
  // Undvik flimmer, rita bara om vid behov eller var 500ms(normalt) / 50ms(inställning)
  static unsigned long lastDraw = 0;
  
  int refreshRate = 200;
  if (current_selection == SEL_CLOCK) refreshRate = 50; // Snabbare uppdatering när vi ställer klockan

  // Om mindre än refreshRate har gått, vänta (förutom om MQTT tvingar uppdatering)
  if (millis() - lastDraw < refreshRate) return; 
  lastDraw = millis();

  // Hämta tid
  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo);

  // Rita på Header Sprite istället för direkt på skärmen
  headerSpr.fillSprite(TFT_DARKGREY); 
  
  headerSpr.setTextColor(TFT_WHITE, TFT_DARKGREY);
  headerSpr.setTextSize(2); 
  headerSpr.setTextDatum(TL_DATUM); // Top Left
  headerSpr.setCursor(5, 7);

  // Klocka
  char timeStr[16]; 
  if (timeValid) {
    if (current_selection == SEL_CLOCK) {
       struct timeval tv;
       gettimeofday(&tv, NULL);
       int hundredths = tv.tv_usec / 10000;
       sprintf(timeStr, "%02d:%02d:%02d.%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, hundredths);
    } else {
       sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    }
    headerSpr.print(timeStr);
  } else {
    headerSpr.print("--:--");
  }

  // Visa MODE (A=Auto, M=Manuell)
  headerSpr.setCursor(100, 7);
  if (manual_mode) {
    headerSpr.setTextColor(TFT_ORANGE, TFT_DARKGREY);
    headerSpr.print("MAN"); 
  } else {
    headerSpr.setTextColor(TFT_CYAN, TFT_DARKGREY);
    headerSpr.print("AUTO"); 
  } 

  // Visa ECO Mode
  if (powerSaveMode) {
      headerSpr.setCursor(160, 7);
      headerSpr.setTextColor(TFT_GREEN, TFT_DARKGREY);
      headerSpr.print("ECO");
  }

  // Status (Rensa gamla röran, kör rent högerjusterat här nu)
  headerSpr.setTextDatum(TR_DATUM); // Top Right
  int xRight = 315;
  
  if (WiFi.status() == WL_CONNECTED) {
    headerSpr.setTextColor(TFT_GREEN, TFT_DARKGREY);
    headerSpr.drawString("WIFI", xRight, 7);
    xRight -= 50;
  } else {
    headerSpr.setTextColor(TFT_RED, TFT_DARKGREY);
    headerSpr.drawString("NO-W", xRight, 7);
    xRight -= 50;
  }
  
  if (client.connected()) {
     headerSpr.setTextColor(TFT_GREEN, TFT_DARKGREY);
     headerSpr.drawString("MQTT", xRight, 7);
  }

  // Pusha sprite till skärmen
  headerSpr.pushSprite(0, 0);

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

  // MANUELL KLOCKA (Endast om relevant)
  if (shouldShowClockMenu()) {
    int y = yStart + (4 * lineH);
    if (current_selection == SEL_CLOCK) {
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString(">", 5, y);
      
      tft.setTextColor(TFT_YELLOW, TFT_BLACK); 
      tft.drawString("TID: +1h / +1m", 150, y);
    } else {
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString(">", 5, y);
      // Rensa eventuell instruktionstext
      tft.drawString("              ", 150, y);
    }
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("STALL TID", 30, y);
  } else {
     // Om menyn försvinner, se till att radera den gamla texten så den inte "spökar"
     // MEN: Om vi är i Settings-läget måste vi låta bli att sudda, för då ritar settings där.
     if (current_selection != SEL_SETTINGS) {
         int y = yStart + (4 * lineH);
         tft.fillRect(0, y, 320, 60, TFT_BLACK); // Sudda lite mer för säkerhets skull
     }
  }

  // SETTINGS MENY
  if (current_selection == SEL_SETTINGS) {
     int y = yStart + (4 * lineH); 
     
     // SUDDA RADEN (för att ta bort "STÄLL TID" om den råkade vara där)
     tft.fillRect(0, y, 320, 30, TFT_BLACK);
     
     tft.setTextColor(TFT_YELLOW, TFT_BLACK);
     tft.drawString(">", 5, y);
     tft.setTextColor(TFT_CYAN, TFT_BLACK);
     tft.drawString("SETTINGS", 30, y);
     
     // Visa aktivt val under
     int ySub = y + 25;
     tft.fillRect(0, ySub, 320, 30, TFT_BLACK); // Rensa sub-raden
     
     // Option 1: ECO
     if (current_setting_option == SET_ECO) {
         tft.setTextColor(TFT_WHITE, TFT_BLACK);
         tft.drawString("< ECO MODE >", 100, ySub);
         if (powerSaveMode) {
             tft.setTextColor(TFT_GREEN, TFT_BLACK);
             tft.drawString("ON", 240, ySub);
         } else {
             tft.setTextColor(TFT_RED, TFT_BLACK);
             tft.drawString("OFF", 240, ySub);
         }
     }
     // Option 2: REBOOT
     else if (current_setting_option == SET_REBOOT) {
         tft.setTextColor(TFT_RED, TFT_BLACK);
         tft.drawString("< REBOOT >", 100, ySub);
         tft.setTextColor(TFT_WHITE, TFT_BLACK);
         tft.drawString("PRESS BTM", 240, ySub);
     }
  }

  // Footer ritas nu av drawFooterTicker();
}

void drawFooterTicker() {
  static int32_t xPos = 320;
  static unsigned long lastScroll = 0;
  const int SCROLL_DELAY = 40; // Hastighet på scroll

  if (millis() - lastScroll < SCROLL_DELAY) return;
  lastScroll = millis();

  const char* msg = "INSTRUKTIONER: [TOPP] Enkelklick: Oka ljus | Dubbelklick: Byt menyval | Langtryck (1s): Aktivera AUTO-lage.   ***   [BOTTEN] Enkelklick: Minska ljus | Håll: Stang av (Manuell Noll)   ***   Vaxthus Master v1.1   ***   ";
  
  footerSpr.fillSprite(TFT_BLACK);
  footerSpr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  footerSpr.setTextSize(1);
  footerSpr.drawString(msg, xPos, 8); // Y=8 för att centrera lite i spriten (höjd 25)

  // Grön linje som avgränsare
  footerSpr.drawFastHLine(0, 0, 320, TFT_DARKGREY);

  footerSpr.pushSprite(0, 145); // Placera längst ner (170 - 25 = 145)

  xPos--;
  if (xPos < -1 * (int)tft.textWidth(msg)) {
    xPos = 320;
  }
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
  else if (topicStr.indexOf("/powersave/set") > 0) {
      // Hantera Power Save Switch
      const char* s = doc["state"];
      // Acceptera ON/OFF eller true/false
      if (s) {
          if (strcasecmp(s, "ON") == 0 || strcasecmp(s, "true") == 0) powerSaveMode = true;
          else if (strcasecmp(s, "OFF") == 0 || strcasecmp(s, "false") == 0) powerSaveMode = false;
          
          // Bekräfta state
          char stateTopic[64];
          snprintf(stateTopic, sizeof(stateTopic), "%s/powersave/state", topic_prefix);
          client.publish(stateTopic, powerSaveMode ? "ON" : "OFF");
          
          handleSchedule(); // Uppdatera direkt
          updateDisplay(); 
      }
      return;
  }

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

      // Discovery för Power Save Switch
      {
         JsonDocument docSwitch;
         docSwitch["name"] = "Växtljus Eco Mode";
         docSwitch["unique_id"] = "vaxtljus_powersave";
         docSwitch["command_topic"] = String(topic_prefix) + "/powersave/set";
         docSwitch["state_topic"] = String(topic_prefix) + "/powersave/state";
         docSwitch["payload_on"] = "ON";
         docSwitch["payload_off"] = "OFF";
         docSwitch["retain"] = true;
         // Ikon: Leaf
         docSwitch["icon"] = "mdi:leaf";

         char buffer[512];
         serializeJson(docSwitch, buffer);
         char distopic[128];
         snprintf(distopic, sizeof(distopic), "homeassistant/switch/bastun_vaxtljus_powersave/config");
         client.publish(distopic, buffer, true);
      }

      char sub_topic[100];
      snprintf(sub_topic, sizeof(sub_topic), "%s/+/set", topic_prefix);
      client.subscribe(sub_topic);
    }
  }
}

// --------------------------------------------------------------------------
// INTRO DEMO (Starfield + Glitch Text)
// --------------------------------------------------------------------------
void runIntroSequence() {
  const int NUM_STARS = 120;
  float stars[NUM_STARS][3]; 
  uint16_t starColors[NUM_STARS];
  
  // Init stjärnor
  for(int i=0; i<NUM_STARS; i++) {
    stars[i][0] = random(-160, 160);
    stars[i][1] = random(-85, 85);
    stars[i][2] = random(10, 400); 
    starColors[i] = tft.color565(random(180,255), random(180,255), 255);
  }

  unsigned long startTime = millis();
  float speed = 2.0;

  TFT_eSprite frame = TFT_eSprite(&tft);
  frame.setColorDepth(16);
  // Fullskärms-sprite (använder PSRAM på S3)
  if (!frame.createSprite(320, 170)) return; 

  while(millis() - startTime < 6000) { 
    // Avbryt på knapptryck
    if(digitalRead(PIN_BTN_TOP) == LOW || digitalRead(PIN_BTN_BTM) == LOW) break;

    frame.fillSprite(TFT_BLACK); 

    unsigned long elapsed = millis() - startTime;
    
    // Warp speed effekt
    if (elapsed < 2000) speed = 3.0 + (elapsed / 80.0); 
    else if (elapsed > 4500) speed *= 0.88; 
    
    if(speed > 45) speed = 45;
    if(speed < 1) speed = 1;

    // Rita stjärnor
    for(int i=0; i<NUM_STARS; i++) {
        stars[i][2] -= speed;
        
        if(stars[i][2] <= 1) {
            stars[i][0] = random(-400, 400); 
            stars[i][1] = random(-400, 400);
            stars[i][2] = 400; 
            starColors[i] = tft.color565(random(200,255), random(200,255), 255);
        }

        float fov = 140.0;
        float factor = fov / stars[i][2];
        int x2d = 160 + (int)(stars[i][0] * factor);
        int y2d = 85 + (int)(stars[i][1] * factor);

        if(x2d >= 0 && x2d < 320 && y2d >= 0 && y2d < 170) {
            if(speed > 10) {
               // Warp lines
               float prevFactor = fov / (stars[i][2] + speed + 5); 
               int xPrev = 160 + (int)(stars[i][0] * prevFactor);
               int yPrev = 85 + (int)(stars[i][1] * prevFactor);
               frame.drawLine(xPrev, yPrev, x2d, y2d, starColors[i]);
            } else {
               frame.drawPixel(x2d, y2d, starColors[i]);
            }
        }
    }

    // Text & Glitch
    frame.setTextDatum(MC_DATUM);
    if(elapsed > 800 && elapsed < 3200) {
        if(random(0,10) > 8) {
            frame.setTextColor(TFT_GREEN, TFT_BLACK); 
            frame.drawString("VAXTHUS", 160+random(-3,3), 85+random(-3,3), 4);
        } else {
            frame.setTextColor(TFT_WHITE, TFT_BLACK);
            frame.drawString("VAXTHUS", 160, 85, 4);
        }
    } 
    else if(elapsed > 3200 && elapsed < 5200) {
        if(random(0,10) > 8) {
            frame.setTextColor(TFT_MAGENTA, TFT_BLACK); 
            frame.drawString("MASTER", 160+random(-3,3), 85+random(-3,3), 4);
        } else {
            frame.setTextColor(TFT_WHITE, TFT_BLACK);
            frame.drawString("MASTER", 160, 85, 4);
        }
    }
    else if(elapsed > 5200) {
        if ((elapsed/150)%2) {
             frame.setTextColor(TFT_CYAN, TFT_BLACK);
             frame.drawString("SYSTEM READY", 160, 85, 2);
        }
    }
    
    frame.pushSprite(0, 0);
  }
  frame.deleteSprite();
}

// --------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  // 1. Skärm
  tft.init();
  tft.setRotation(1); 
  tft.setSwapBytes(true); // Bra för färger i Sprite

  runIntroSequence();
  
  // Init Sprite
  footerSpr.createSprite(320, 25);
  footerSpr.fillSprite(TFT_BLACK);
  
  headerSpr.createSprite(320, 30);
  headerSpr.fillSprite(TFT_DARKGREY);

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

  // 4. WiFi (Icke-blockerande)
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  tft.println("Connect WiFi...");

  // 5. Tid / NTP
  configTzTime(timezone_info, ntpServer);

  // 6. MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(1024);

  // 7. OTA
  ArduinoOTA.setHostname("vaxtlus-master-s3");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch";
    else type = "filesystem";
    // NOTE: if updating FS this would be the place to unmount FS using SPIFFS.end()
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 100);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("OTA UPDATE...");
  });
  ArduinoOTA.begin();
  
  delay(1000);
  tft.fillScreen(TFT_BLACK);
}

void loop() {
  ArduinoOTA.handle(); 
  unsigned long now = millis();

  // WiFi Återanslutning (om det tappats eller aldrig lyckats)
  static unsigned long lastWiFiCheck = 0;
  if ((WiFi.status() != WL_CONNECTED) && (now - lastWiFiCheck > 10000)) {
    lastWiFiCheck = now;
    WiFi.reconnect();
  }

  // MQTT Underhåll
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        reconnect();
      }
    } else {
      client.loop();
    }
  }

  // Knapp Logik
  int clickTop = btnTop.update();
  int clickBtm = btnBtm.update();

  // Animera footer
  drawFooterTicker();

  // Kör schema-logik
  handleSchedule();

  // --- KNAPP TOP (Upp / Byt) ---
  if (clickTop == 1) { 
    // ENKEL: Öka ljusstyrka (+25 av 255 ≈ 10%)
    Serial.println("TOP: Single Click (Brightness +)");
    if (current_selection == SEL_CLOCK) {
      adjustTime(3600); // +1 Timme
    } 
    else if (current_selection == SEL_SETTINGS) {
       // Byt inställningsval
       current_setting_option++;
       if (current_setting_option > 1) current_setting_option = 0;
       updateDisplay();
    }
    else {
      adjustBrightness(25);
    }
  } 
  else if (clickTop == 2) {
    // DUBBEL: Byt Kanal (Loopa: All -> White -> Red -> UV -> Clock -> All)
    Serial.println("TOP: Double Click (Next Channel)");
    if (current_selection == SEL_ALL) current_selection = SEL_WHITE;
    else if (current_selection == SEL_WHITE) current_selection = SEL_RED;
    else if (current_selection == SEL_RED) current_selection = SEL_UV;
    else if (current_selection == SEL_UV) {
       current_selection = SEL_CLOCK;
    }
    else if (current_selection == SEL_CLOCK) {
       current_selection = SEL_SETTINGS;
       current_setting_option = 0; // Reset till första val
    }
    else if (current_selection == SEL_SETTINGS) {
       current_selection = SEL_ALL;
    }
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
    if (current_selection == SEL_CLOCK) {
      adjustTime(60); // +1 Minut
    } 
    else if (current_selection == SEL_SETTINGS) {
       // Utför vald inställning
       if (current_setting_option == SET_ECO) {
          powerSaveMode = !powerSaveMode; // Toggle ECO
          // Rapportera state om MQTT är uppe
          if(client.connected()) {
             char stateTopic[64];
             snprintf(stateTopic, sizeof(stateTopic), "%s/powersave/state", topic_prefix);
             client.publish(stateTopic, powerSaveMode ? "ON" : "OFF");
          }
          handleSchedule(); // Applicera direkt
       }
       else if (current_setting_option == SET_REBOOT) {
          ESP.restart();
       }
       updateDisplay();
    }
    else {
      adjustBrightness(-25);
    }
  }
  else if (clickBtm == 2) {
    // DUBBEL: Stäng av vald kanal helt (0%)
    Serial.println("BTM: Double Click (Turn Off Selected)");
    if (current_selection != SEL_CLOCK && current_selection != SEL_SETTINGS) {
       setChannelOff();
    }
  }

  updateDisplay();
}

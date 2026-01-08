#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Update.h>
#include "html_content.h"
// #include <DNSServer.h> // REMOVED
#include "ButtonHandler.h"
#include "time.h"
#include "secrets.h"
#include "Globals.h"
#include "DisplayManager.h"
#include "APMode.h" // NEW: Isolerad AP-kod

// DNSServer dnsServer; // REMOVED

// --------------------------------------------------------------------------
// KONFIGURATION
// --------------------------------------------------------------------------
// bool dnsActive = false; // REMOVED
const char* topic_prefix = "bastun/vaxtljus";
const char* ntpServer = "pool.ntp.org";
const char* timezone_info = "CET-1CEST,M3.5.0,M10.5.0/3"; 

const int PIN_WHITE = 1;
const int PIN_RED = 2;
const int PIN_UV = 3;
const int PIN_BTN_TOP = 14;
const int PIN_BTN_BTM = 0;

const int UV_MAX_LIMIT = 204; 
const unsigned long MANUAL_TIMEOUT_MS = 45 * 60 * 1000; 

const int TIME_SUNRISE_START = 330; 
const int TIME_DAY_START = 450;    
const int TIME_SUNSET_START = 1200; 
const int TIME_NIGHT_START = 1320; 

const int PWM_FREQ = 5000;
const int PWM_RES = 8; 
// const int CH_WHITE = 0; // Moved to Globals.h
// const int CH_RED = 1;   // Moved to Globals.h
// const int CH_UV = 2;    // Moved to Globals.h

// --------------------------------------------------------------------------
// OBJEKT
// --------------------------------------------------------------------------
WiFiClient espClient;
PubSubClient client(espClient);
AsyncWebServer server(80);
DisplayManager displayMgr;

// --------------------------------------------------------------------------
// STATE VARIABLER
// --------------------------------------------------------------------------
unsigned long lastReconnectAttempt = 0;
bool manual_mode = false;
unsigned long manual_timer_start = 0;

// OBJEKT FÖR LJUSSTYRNING (NEW OOP ARCHITECTURE)
// Assuming 12W for White/Red, 5W for UV
LightChannel lightWhite(PIN_WHITE, CH_WHITE, 12.0);
LightChannel lightRed(PIN_RED, CH_RED, 12.0);
LightChannel lightUV(PIN_UV, CH_UV, 5.0);

MenuSelection current_selection = SEL_ALL;
int current_setting_option = 0; 
int current_language = LANG_SV;
bool powerSaveMode = false; 

// New Feature 2: Presets
int viewing_preset = PRESET_SEED; // Vilken preset vi tittar på i menyn

ButtonHandler btnTop(PIN_BTN_TOP);
ButtonHandler btnBtm(PIN_BTN_BTM);

// --------------------------------------------------------------------------
// FUNKTIONER FRAMÅTDEKLARATION
// --------------------------------------------------------------------------
void publishOne(const char* type, int val);
// updatePWM ej längre nödvändig
void updateDisplay();
void activateManualMode();
void adjustTime(long deltaSec);

// --------------------------------------------------------------------------
// LOGIK
// --------------------------------------------------------------------------

// handleFading borttagen - Nu hanteras detta av LightChannel-klasserna.


void applyPreset(int preset) {
    activateManualMode();
    int w=0, r=0, u=0;
    
    switch(preset) {
        case PRESET_SEED:
            w = 100; r = 40; u = 0;
            break;
        case PRESET_VEG:
            w = 220; r = 80; u = 10;
            break;
        case PRESET_BLOOM:
            w = 100; r = 255; u = 60;
            break;
        case PRESET_FULL:
            w = 255; r = 255; u = UV_MAX_LIMIT;
            break;
    }
    
    lightWhite.setTarget(w);
    lightRed.setTarget(r);
    lightUV.setTarget(u);
    
    publishOne("white", w);
    publishOne("red", r);
    publishOne("uv", u);
    updateDisplay();
}


bool isTimeValid() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return false;
  if (timeinfo.tm_year < (2022 - 1900)) return false; 
  return true;
}

bool shouldShowClockMenu() {
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

void handleSchedule() {
  if (manual_mode) {
    if (millis() - manual_timer_start > MANUAL_TIMEOUT_MS) {
      manual_mode = false;
    } else {
      return; 
    }
  }

  struct tm ti;
  if (!getLocalTime(&ti)) return; 

  int current_minutes = ti.tm_hour * 60 + ti.tm_min;
  int target_bri = 0;

  if (current_minutes < TIME_SUNRISE_START || current_minutes >= TIME_NIGHT_START) {
    target_bri = 0;
  } 
  else if (current_minutes >= TIME_SUNRISE_START && current_minutes < TIME_DAY_START) {
    target_bri = map(current_minutes, TIME_SUNRISE_START, TIME_DAY_START, 0, 255);
  }
  else if (current_minutes >= TIME_DAY_START && current_minutes < TIME_SUNSET_START) {
    target_bri = 255;
  }
  else if (current_minutes >= TIME_SUNSET_START && current_minutes < TIME_NIGHT_START) {
    target_bri = map(current_minutes, TIME_SUNSET_START, TIME_NIGHT_START, 255, 0);
  }

  if (powerSaveMode) target_bri = target_bri / 2;

  int target_uv = map(target_bri, 0, 255, 0, UV_MAX_LIMIT); 

  if (lightWhite.getTarget() != target_bri || lightRed.getTarget() != target_bri) {
     lightWhite.setTarget(target_bri);
     lightRed.setTarget(target_bri);
     lightUV.setTarget(target_uv);
  }
}

void adjustBrightness(int amount) {
  activateManualMode(); 
  bool changed = false;
  
  if (current_selection == SEL_ALL || current_selection == SEL_WHITE) {
    int old = lightWhite.getTarget();
    int newVal = constrain(old + amount, 0, 255);
    lightWhite.setTarget(newVal);
    if (old != newVal) { publishOne("white", newVal); changed = true; }
  }
  if (current_selection == SEL_ALL || current_selection == SEL_RED) {
    int old = lightRed.getTarget();
    int newVal = constrain(old + amount, 0, 255);
    lightRed.setTarget(newVal);
    if (old != newVal) { publishOne("red", newVal); changed = true; }
  }
  if (current_selection == SEL_ALL || current_selection == SEL_UV) {
    int old = lightUV.getTarget();
    int newVal = constrain(old + amount, 0, 255);
    lightUV.setTarget(newVal);
    if (old != newVal) { publishOne("uv", newVal); changed = true; }
  }
  if (changed) updateDisplay();
}

void setChannelOff() {
  activateManualMode(); 
  bool changed = false;
  
  if (current_selection == SEL_ALL || current_selection == SEL_WHITE) {
    lightWhite.setTarget(0); 
    publishOne("white", 0); 
    changed = true;
  }
  if (current_selection == SEL_ALL || current_selection == SEL_RED) {
    lightRed.setTarget(0); 
    publishOne("red", 0); 
    changed = true;
  }
  if (current_selection == SEL_ALL || current_selection == SEL_UV) {
    lightUV.setTarget(0); 
    publishOne("uv", 0); 
    changed = true;
  }
  if (changed) updateDisplay();
}

// updatePWM borttagen

void updateDisplay() {
    static long cachedRSSI = 0;
    static unsigned long lastRSSI = 0;
    
    // CPU Load / Loop Time estimering
    static unsigned long prev = 0;
    static unsigned long lastCpuUpdate = 0;
    static int displayedLoopTime = 0;
    
    unsigned long now = millis();
    int loopTime = (int)(now - prev);
    prev = now; 
    
    // Cache RSSI (Expensive operation!)
    if (now - lastRSSI > 2000) {
       if(WiFi.status() == WL_CONNECTED) cachedRSSI = WiFi.RSSI();
       else cachedRSSI = 0;
       lastRSSI = now;
    }

    // Uppdatera bara textsträngen 1 gång per sekund för att inte döda UI-prestanda
    if (now - lastCpuUpdate > 1000) {
        displayedLoopTime = loopTime;
        lastCpuUpdate = now;
    }

    // Use defined Firmware Version instead of Loop Time
    // char cpuStr[20];
    // snprintf(cpuStr, sizeof(cpuStr), "Loop: %dms", displayedLoopTime);

    bool wifiC = (WiFi.status() == WL_CONNECTED);
    bool mqttC = client.connected();

    float totalWh = lightWhite.getEnergyUsageWh() + lightRed.getEnergyUsageWh() + lightUV.getEnergyUsageWh();

    displayMgr.update(current_selection, current_setting_option, current_language, 
                      manual_mode, powerSaveMode, 
                      lightWhite.getTarget(), lightRed.getTarget(), lightUV.getTarget(), 
                      viewing_preset, 
                      wifiC, mqttC, 
                      (int)cachedRSSI, FIRMWARE_VERSION, totalWh); 
}

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
      const char* s = doc["state"];
      if (s) {
          if (strcasecmp(s, "ON") == 0 || strcasecmp(s, "true") == 0) powerSaveMode = true;
          else if (strcasecmp(s, "OFF") == 0 || strcasecmp(s, "false") == 0) powerSaveMode = false;
          char stateTopic[64];
          snprintf(stateTopic, sizeof(stateTopic), "%s/powersave/state", topic_prefix);
          client.publish(stateTopic, powerSaveMode ? "ON" : "OFF");
          handleSchedule(); 
          updateDisplay(); 
      }
      return;
  }

  if (light_type != "") {
    activateManualMode(); 
    LightChannel* targetChannel = nullptr;
    if (light_type == "white") targetChannel = &lightWhite;
    else if (light_type == "red") targetChannel = &lightRed;
    else if (light_type == "uv") targetChannel = &lightUV;

    if (targetChannel) {
      int newVal = targetChannel->getTarget();
      
      if (doc.containsKey("state")) {
        const char* s = doc["state"];
        if (strcmp(s, "OFF") == 0) newVal = 0;
        else if (strcmp(s, "ON") == 0 && newVal == 0) newVal = 255;
      }
      if (doc.containsKey("brightness")) {
        newVal = doc["brightness"];
      }
      
      targetChannel->setTarget(newVal);
      publishOne(light_type.c_str(), newVal);
    }
    updateDisplay(); 
  }
}

void sendDiscovery(const char* name, const char* light_type) {
  // Discovery code ... (keeping it short for overview, but logic remains same)
  // ...
  // Actually I must include it or it breaks HA
  client.publish(String("homeassistant/light/bv_" + String(light_type) + "/config").c_str(), "{}", true); // Dummy for brevity in this replace? No, better keep full.
  
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

void sendEnergyDiscovery() {
  char topic[128];
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_energy/config", "bastun_vaxtljus");

  JsonDocument doc;
  doc["name"] = "Växtljus Energi Totalt";
  doc["unique_id"] = "vaxtljus_energy_total";
  doc["state_topic"] = String(topic_prefix) + "/energy/total";
  doc["unit_of_measurement"] = "Wh";
  doc["device_class"] = "energy";
  doc["state_class"] = "total_increasing";
  doc["force_update"] = true;

  // Optional: Add device info to group them (if we were doing a full device registry implementation)
  JsonObject device = doc["device"].to<JsonObject>();
  device["identifiers"][0] = "vaxtljus_master_v3";
  device["name"] = "Växtljus Master";
  device["sw_version"] = FIRMWARE_VERSION;
  device["model"] = "T-Display S3";
  device["manufacturer"] = "LilyGo";

  char buffer[512];
  serializeJson(doc, buffer);
  client.publish(topic, buffer, true);
}

void reconnect() {
  if (!client.connected()) {
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      sendDiscovery("Växtljus Vit", "white");
      sendDiscovery("Växtljus Röd", "red");
      sendDiscovery("Växtljus UV", "uv");
      sendEnergyDiscovery(); // Auto-Discovery for Energy Meter
      client.subscribe((String(topic_prefix) + "/+/set").c_str());
    }
  }
}

// --------------------------------------------------------------------------
// MAIN AP LOOP MOVED TO APMode.cpp
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------

volatile int virtualClickTop = 0; 
volatile int virtualClickBtm = 0;
bool isAPMode = false; // Global flag

void initWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
      if (isAPMode) {
          // Delegate HTML generation to APMode.cpp
          request->send(200, "text/html", getAPPageHTML());
      } else {
          request->send_P(200, "text/html", index_html); 
      }
  });

  // Delegate API handling to APMode.cpp
  server.on("/ap/set", HTTP_GET, [](AsyncWebServerRequest *request){
      handleAPSet(request);
  });

  // --- API for Web Simulator (Normal Mode) ---
  server.on("/api/action", HTTP_GET, [](AsyncWebServerRequest *request){
      if(request->hasParam("btn") && request->hasParam("type")) {
          String btn = request->getParam("btn")->value();
          String type = request->getParam("type")->value();
          int code = 0;
          if(type == "click") code = 1;
          else if(type == "long") code = 3;
          else if(type == "double") code = 2; // Assuming double click implemented as 2
          
          if(code > 0) {
              if(btn == "top") virtualClickTop = code;
              if(btn == "btm") virtualClickBtm = code;
          }
      }
      request->send(200, "text/plain", "OK");
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
      JsonDocument doc;
      doc["manual"] = manual_mode;
      doc["sel"] = (int)current_selection;
      doc["set_opt"] = (int)current_setting_option;
      doc["lang"] = (int)current_language;
      doc["eco"] = powerSaveMode;
      doc["white"] = lightWhite.getTarget(); // Use real values
      doc["red"] = lightRed.getTarget();
      doc["uv"] = lightUV.getTarget();
      doc["wifi"] = (WiFi.status() == WL_CONNECTED);
      
      struct tm ti;
      if (getLocalTime(&ti)) {
          char timeStr[16];
          sprintf(timeStr, "%02d:%02d", ti.tm_hour, ti.tm_min);
          doc["time"] = String(timeStr);
      } else {
          doc["time"] = "--:--";
      }

      String json;
      serializeJson(doc, json);
      request->send(200, "application/json", json);
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){

      request->send(200, "text/plain", "Rebooting...");
      delay(1000);
      ESP.restart();
  });
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    bool shouldReboot = !Update.hasError();
    request->send(200, "text/plain", shouldReboot ? "OK" : "FAIL");
    if(shouldReboot) { delay(200); ESP.restart(); }
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){ displayMgr.showMessage("UPDATING..."); Update.begin(UPDATE_SIZE_UNKNOWN); }
    if(!Update.hasError()){ Update.write(data, len); }
    if(final){ Update.end(true); }
  });

  // Captive Portal Catch-All
  server.onNotFound([](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", index_html);
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  displayMgr.init();
  
  // Init Buttons
  btnTop.init(); btnBtm.init();

  // 1. Init WiFi Stack FIRST (Essential for WebServer to not crash)
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("Vaxtljus-Master"); 
  
  // 2. Start WebServer (Now safe because WiFi mode is set)
  initWebServer();

  WiFi.begin(ssid, password);
  
  displayMgr.showIntro();
  
  displayMgr.showMessage("Connecting WiFi...");
  unsigned long wifiStart = millis();
  
  // Try to connect for 5 seconds (Shortened from 10s)
  while(WiFi.status() != WL_CONNECTED && millis() - wifiStart < 5000) { delay(100); }

  if(WiFi.status() == WL_CONNECTED) {
      configTzTime(timezone_info, ntpServer);
      isAPMode = false;
  }
  else { 
      // Fallback to AP Mode -> ENTER ISOLATED STATE (Via APMode.cpp)
      isAPMode = true; 
      enterAPMode();
  }

  // PWM Setup via LightChannel Class
  lightWhite.begin(PWM_FREQ, PWM_RES);
  lightRed.begin(PWM_FREQ, PWM_RES);
  lightUV.begin(PWM_FREQ, PWM_RES);
  
  if (!isAPMode) {
      // updatePWM() not needed as update() in loop handles it
      client.setServer(mqtt_server, mqtt_port);
      client.setCallback(callback);
      client.setBufferSize(1024);
      ArduinoOTA.onStart([]() { displayMgr.showMessage("OTA UPDATE..."); });
      ArduinoOTA.begin();
  }
  
  delay(100);
  if (!isAPMode) updateDisplay(); 
}

void loop() {
  // ISOLATED AP LOOP TRAP
  if (isAPMode) {
      loopAP();
      return; 
  }

  // NORMAL LOOP BELOW
  // OTA ska hanteras först
  ArduinoOTA.handle(); 
  
  // Fade-logiken & Energi-räkning
  lightWhite.update();
  lightRed.update();
  lightUV.update();

  unsigned long now = millis();
  
  // RECONNECTION LOGIC v2.1
  // User Requirement: "In AP mode, look for regular network every 30 seconds."
  
  static unsigned long lastWiFiCheck = 0;
  long reconnectInterval = 30000; // 30 seconds

  if (WiFi.status() != WL_CONNECTED) {
      if (now - lastWiFiCheck > reconnectInterval) {
          lastWiFiCheck = now; 
          
          // Ensure we are in AP_STA mode to allow connection attempts
          if(WiFi.getMode() != WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
          
          // Try to connect (non-blocking if possible, but begin() might take ms)
          WiFi.begin(ssid, password); 
      }
  }

  // Endast om WiFi finns kör vi MQTT
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (now - lastReconnectAttempt > 15000) { lastReconnectAttempt = now; reconnect(); }
    } else {
        client.loop();
        
        static unsigned long lastEnergyPub = 0;
        if (now - lastEnergyPub > 60000) { // Publish stats every 60s
             lastEnergyPub = now;
             float eW = lightWhite.getEnergyUsageWh();
             float eR = lightRed.getEnergyUsageWh();
             float eU = lightUV.getEnergyUsageWh();
             float total = eW + eR + eU;
             
             char topic[100];
             char payload[64];
             
             // Total
             snprintf(topic, sizeof(topic), "%s/energy/total", topic_prefix);
             snprintf(payload, sizeof(payload), "%.2f", total);
             client.publish(topic, payload);
             
             // Debug/Details? Maybe later if requested.
        }
    }
  }

  // Resten av loopen (knappar, UI, schema) MÅSTE få köras fritt
  int clickTop = btnTop.update();
  int clickBtm = btnBtm.update();
  if (virtualClickTop > 0) { clickTop = virtualClickTop; virtualClickTop = 0; }
  if (virtualClickBtm > 0) { clickBtm = virtualClickBtm; virtualClickBtm = 0; }

  handleSchedule();
  updateDisplay(); // Manager handles throttling

  // --- BUTTON LOGIC ---
  if (clickTop == 1) { 
    if (current_selection == SEL_CLOCK) adjustTime(3600);
    else if (current_selection == SEL_PRESETS) {
        // Change Preset View
        viewing_preset++;
        if(viewing_preset > PRESET_FULL) viewing_preset = PRESET_SEED;
        updateDisplay();
    }
    else if (current_selection == SEL_QR || current_selection == SEL_INFO) {
       current_selection = SEL_SETTINGS; updateDisplay();
    }
    else if (current_selection == SEL_SETTINGS) {
       current_setting_option++;
       if (current_setting_option > 4) current_setting_option = 0;
       updateDisplay();
    }
    else adjustBrightness(25);
  } 
  else if (clickTop == 2) {
    if (current_selection == SEL_ALL) current_selection = SEL_WHITE;
    else if (current_selection == SEL_WHITE) current_selection = SEL_RED;
    else if (current_selection == SEL_RED) current_selection = SEL_UV;
    else if (current_selection == SEL_UV) current_selection = SEL_PRESETS; // Add Presets to loop
    else if (current_selection == SEL_PRESETS) {
        if (shouldShowClockMenu()) current_selection = SEL_CLOCK;
        else current_selection = SEL_SETTINGS;
    }
    else if (current_selection == SEL_CLOCK) { current_selection = SEL_SETTINGS; current_setting_option = 0; }
    else if (current_selection == SEL_SETTINGS) current_selection = SEL_ALL;
    else current_selection = SEL_ALL;
    updateDisplay();
  }
  else if (clickTop == 3) {
    manual_mode = false;
    handleSchedule(); 
    updateDisplay();
  }

  if (clickBtm == 1) {
    if (current_selection == SEL_CLOCK) adjustTime(60); 
    else if (current_selection == SEL_PRESETS) {
        // Apply Preset!
        applyPreset(viewing_preset);
    }
    else if (current_selection == SEL_QR || current_selection == SEL_INFO) {
       current_selection = SEL_SETTINGS; updateDisplay();
    }
    else if (current_selection == SEL_SETTINGS) {
       if (current_setting_option == SET_LANG) { current_language = !current_language; }
       else if (current_setting_option == SET_ECO) { powerSaveMode = !powerSaveMode; handleSchedule(); }
       else if (current_setting_option == SET_QR) { current_selection = SEL_QR; }
       else if (current_setting_option == SET_INFO) { current_selection = SEL_INFO; }
       else if (current_setting_option == SET_REBOOT) { ESP.restart(); }
    }
    else adjustBrightness(-25);
    updateDisplay();
  }
  else if (clickBtm == 2) {
    if (current_selection < SEL_PRESETS) setChannelOff(); // Only for channels
  }
  else if (clickBtm == 3) {
     current_selection = SEL_SETTINGS;
     current_setting_option = 0;
     updateDisplay();
  }
}

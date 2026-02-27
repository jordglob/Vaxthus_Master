/**
 * Vaxthus_Master_V3 - Grow Light Controller
 *
 * Architecture based on Battery-Emulator by dalathegreat
 * Repurposed for PWM grow light control with MQTT/Home Assistant integration
 *
 * Hardware: ESP32-WROOM-32 (DevKit V1)
 * PWM Channels:
 *   - White: GPIO 16
 *   - Red:   GPIO 17
 *   - UV:    GPIO 18
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <esp_arduino_version.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define PWM_WHITE_PIN   16
#define PWM_RED_PIN     17
#define PWM_UV_PIN      18

#define PWM_FREQ        5000
#define PWM_RESOLUTION  8  // 0-255

#define PWM_CHANNEL_WHITE 0
#define PWM_CHANNEL_RED   1
#define PWM_CHANNEL_UV    2

// Ramping Settings
#define RAMP_DURATION_MIN 15
// Calculate step for 100ms update loop
// 255 steps / (15 * 60 * 10 updates) = 0.02833
const float RAMP_STEP = 255.0f / (RAMP_DURATION_MIN * 60.0f * 10.0f);

#define MANUAL_OVERRIDE_TIMEOUT 2700000  // 45 minutes in milliseconds
#define UV_LIMITER_PERCENTAGE 80  // UV max 80% (extends UV LED lifespan)

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
Preferences settings;
WebServer server(80);
WiFiClient espClient;
PubSubClient mqtt(espClient);

// WiFi settings
String wifi_ssid = "";
String wifi_password = "";
String ap_ssid = "Vaxthus_Master";
String ap_password = "123456789";

// MQTT settings
String mqtt_server = "mqtt.revolt-energy.org";
uint16_t mqtt_port = 1883;
String mqtt_user = "";
String mqtt_password = "";
bool mqtt_enabled = false;

// Current PWM State (Float for smooth ramping)
float current_white = 0.0f;
float current_red = 0.0f;
float current_uv = 0.0f;

// Target PWM State
uint8_t target_white = 0;
uint8_t target_red = 0;
uint8_t target_uv = 0;

// MQTT topics
const char* TOPIC_BASE = "bastun/vaxtljus";
const char* HA_DISCOVERY_PREFIX = "homeassistant";

// Timing
unsigned long lastMqttReconnect = 0;
unsigned long lastWifiCheck = 0;
unsigned long lastRampUpdate = 0;
unsigned long lastScheduleCheck = 0;
bool ha_discovery_sent = false;

// Mode
bool manual_override = false;
unsigned long last_manual_action = 0;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void init_wifi();
void init_ota();
void init_mqtt();
void init_webserver();
void init_pwm();
void load_settings();
void save_settings();
void wifi_monitor();
void mqtt_loop();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void publish_state(const char* channel, uint8_t value);
void publish_ha_discovery();
void set_light_manual(uint8_t channel, uint8_t value);
void checkSchedule();
void update_pwm_ramp();
void init_time();
int get_wifi_signal_strength();
String get_index_html();
String get_settings_html();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=================================");
    Serial.println("  Vaxthus_Master_V3");
    Serial.println("  Grow Light Controller");
    Serial.println("=================================\n");

    load_settings();
    init_pwm();
    init_wifi();
    init_ota();
    init_webserver();
    init_mqtt();
    init_time();

    Serial.println("Setup complete!");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    ArduinoOTA.handle();
    server.handleClient();
    wifi_monitor();
    mqtt_loop();
    
    checkSchedule();
    update_pwm_ramp();
    
    delay(5);
}

// ============================================================================
// SETTINGS
// ============================================================================
void load_settings() {
    Serial.println("Loading settings from NVM...");
    settings.begin("vaxthus", false);

    wifi_ssid = settings.getString("SSID", "");
    wifi_password = settings.getString("PASSWORD", "");
    mqtt_server = settings.getString("MQTTSERVER", "mqtt.revolt-energy.org");
    mqtt_port = settings.getUInt("MQTTPORT", 1883);
    mqtt_user = settings.getString("MQTTUSER", "");
    mqtt_password = settings.getString("MQTTPASS", "");
    mqtt_enabled = settings.getBool("MQTTENABLED", false);

    // Initial state is off or whatever ramping starts at
}

void save_settings() {
    Serial.println("Saving settings to NVM...");
    settings.putString("SSID", wifi_ssid);
    settings.putString("PASSWORD", wifi_password);
    settings.putString("MQTTSERVER", mqtt_server);
    settings.putUInt("MQTTPORT", mqtt_port);
    settings.putString("MQTTUSER", mqtt_user);
    settings.putString("MQTTPASS", mqtt_password);
    settings.putBool("MQTTENABLED", mqtt_enabled);
    Serial.println("Settings saved!");
}

// ============================================================================
// PWM CONTROL
// ============================================================================
void write_pwm(uint8_t channel, uint8_t pin, uint8_t value) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(pin, value);
#else
    ledcWrite(channel, value);
#endif
}

void init_pwm() {
    Serial.println("Initializing PWM channels...");

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PWM_WHITE_PIN, PWM_FREQ, PWM_RESOLUTION);
    ledcAttach(PWM_RED_PIN, PWM_FREQ, PWM_RESOLUTION);
    ledcAttach(PWM_UV_PIN, PWM_FREQ, PWM_RESOLUTION);
#else
    ledcSetup(PWM_CHANNEL_WHITE, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_RED, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_UV, PWM_FREQ, PWM_RESOLUTION);

    ledcAttachPin(PWM_WHITE_PIN, PWM_CHANNEL_WHITE);
    ledcAttachPin(PWM_RED_PIN, PWM_CHANNEL_RED);
    ledcAttachPin(PWM_UV_PIN, PWM_CHANNEL_UV);
#endif

    write_pwm(PWM_CHANNEL_WHITE, PWM_WHITE_PIN, 0);
    write_pwm(PWM_CHANNEL_RED, PWM_RED_PIN, 0);
    write_pwm(PWM_CHANNEL_UV, PWM_UV_PIN, 0);
}

void update_pwm_ramp() {
    if (millis() - lastRampUpdate < 100) return; // 10Hz update
    lastRampUpdate = millis();

    bool changed = false;

    // Helper lambda for ramping
    auto ramp = [&](float &current, uint8_t target) {
        if (abs(current - target) < RAMP_STEP) {
            current = target;
        } else if (current < target) {
            current += RAMP_STEP;
        } else {
            current -= RAMP_STEP;
        }
    };

    if (manual_override) {
        // Instant change in manual mode
        current_white = target_white;
        current_red = target_red;
        current_uv = target_uv;
    } else {
        // Slow ramp in auto mode
        ramp(current_white, target_white);
        ramp(current_red, target_red);
        ramp(current_uv, target_uv);
    }

    // Apply to hardware
    write_pwm(PWM_CHANNEL_WHITE, PWM_WHITE_PIN, (uint8_t)current_white);
    write_pwm(PWM_CHANNEL_RED, PWM_RED_PIN, (uint8_t)current_red);
    write_pwm(PWM_CHANNEL_UV, PWM_UV_PIN, (uint8_t)current_uv);

    // Publish state only if significantly changed (to avoid flooding MQTT)
    static uint8_t last_pub_w = 0, last_pub_r = 0, last_pub_uv = 0;
    if (abs((int)current_white - last_pub_w) > 1 || (current_white == target_white && last_pub_w != target_white)) {
        publish_state("white", (uint8_t)current_white);
        last_pub_w = (uint8_t)current_white;
    }
    if (abs((int)current_red - last_pub_r) > 1 || (current_red == target_red && last_pub_r != target_red)) {
        publish_state("red", (uint8_t)current_red);
        last_pub_r = (uint8_t)current_red;
    }
    if (abs((int)current_uv - last_pub_uv) > 1 || (current_uv == target_uv && last_pub_uv != target_uv)) {
        publish_state("uv", (uint8_t)current_uv);
        last_pub_uv = (uint8_t)current_uv;
    }
}

void set_light_manual(uint8_t channel, uint8_t value) {
    // Activate manual override
    if (!manual_override) {
        manual_override = true;
        String topic = String(TOPIC_BASE) + "/mode";
        mqtt.publish(topic.c_str(), "MANUAL", true);
        Serial.println("[Manual] Override activated");
    }
    last_manual_action = millis();
    
    switch(channel) {
        case PWM_CHANNEL_WHITE:
            target_white = value;
            break;
        case PWM_CHANNEL_RED:
            target_red = value;
            break;
        case PWM_CHANNEL_UV:
            // UV Safety Limiter logic is now in Schedule, but user can override up to 100% manually?
            // "UV max 80% (extends UV LED lifespan)" in define.
            // Let's apply limiter here too if desired, or let user override.
            // Prompt says "UV 100%" in schedule 12:00-14:00, so 100% is allowed.
            // The define UV_LIMITER_PERCENTAGE 80 was used for "Auto mode" logic in old code.
            // I'll allow 100% manual.
            target_uv = value;
            break;
    }
}

// ============================================================================
// WIFI
// ============================================================================
void init_wifi() {
    Serial.println("Initializing WiFi...");

    WiFi.mode(WIFI_AP_STA);
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case ARDUINO_EVENT_WIFI_STA_START:
                Serial.println("  [WiFi] STA started");
                break;
            case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                Serial.println("  [WiFi] STA connected to AP");
                break;
            case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                Serial.printf("  [WiFi] Got IP: %s\n", WiFi.localIP().toString().c_str());
                break;
            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                Serial.printf("  [WiFi] Disconnected, reason: %d\n", info.wifi_sta_disconnected.reason);
                break;
            default:
                break;
        }
    });

    WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
    Serial.printf("  AP started: %s (pw: %s)\n", ap_ssid.c_str(), ap_password.c_str());

    if (wifi_ssid.length() > 0) {
        Serial.printf("  Connecting to: %s\n", wifi_ssid.c_str());
        WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
        WiFi.setAutoReconnect(true);
        WiFi.setSleep(false);
    }
}

void wifi_monitor() {
    if (millis() - lastWifiCheck < 10000) return;
    lastWifiCheck = millis();

    if (wifi_ssid.length() > 0 && WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
    }
}

int get_wifi_signal_strength() {
    if (WiFi.status() != WL_CONNECTED) return 0;
    int rssi = WiFi.RSSI();
    int quality = 2 * (rssi + 100);
    if (quality > 100) quality = 100;
    if (quality < 0) quality = 0;
    return quality;
}

// ============================================================================
// OTA
// ============================================================================
void init_ota() {
    if (WiFi.status() != WL_CONNECTED) return;
    ArduinoOTA.setHostname("vaxthus-master");
    ArduinoOTA.setPassword(ap_password.c_str());
    ArduinoOTA.begin();
}

// ============================================================================
// TIME & SCHEDULE
// ============================================================================
void init_time() {
    Serial.println("Initializing Time...");
    // Set Timezone for Sweden (CET-1CEST,M3.5.0,M10.5.0/3)
    configTime(0, 0, "pool.ntp.org");
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
}

void checkSchedule() {
    // Check manual override timeout
    if (manual_override) {
        if (millis() - last_manual_action > MANUAL_OVERRIDE_TIMEOUT) {
            manual_override = false;
            String topic = String(TOPIC_BASE) + "/mode";
            mqtt.publish(topic.c_str(), "AUTO", true);
            Serial.println("[Schedule] Manual override expired. Resuming Auto Mode.");
            // Next pass will update targets
        } else {
            return; // Stay in manual mode
        }
    }

    // Run every 10 seconds to update target based on time
    if (millis() - lastScheduleCheck < 10000) return;
    lastScheduleCheck = millis();

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        static bool time_warned = false;
        if (!time_warned) {
            Serial.println("[Schedule] Time not synced yet.");
            time_warned = true;
        }
        return;
    }

    int current_mins = timeinfo.tm_hour * 60 + timeinfo.tm_min;

    // Schedule Logic
    // 06:00 (360) - 12:00 (720): White 100%, Red 80%, UV 0%
    // 12:00 (720) - 14:00 (840): White 100%, Red 100%, UV 100%
    // 14:00 (840) - 20:00 (1200): White 80%, Red 100%, UV 0%
    // 20:00 (1200) - 20:30 (1230): White 0%, Red 50%, UV 0%
    // 20:30 (1230) - 06:00 (360): All OFF
    
    uint8_t new_w = 0;
    uint8_t new_r = 0;
    uint8_t new_uv = 0;

    if (current_mins >= 360 && current_mins < 720) {
        new_w = 255; new_r = (int)(255 * 0.8); new_uv = 0;
    } else if (current_mins >= 720 && current_mins < 840) {
        new_w = 255; new_r = 255; new_uv = 255;
    } else if (current_mins >= 840 && current_mins < 1200) {
        new_w = (int)(255 * 0.8); new_r = 255; new_uv = 0;
    } else if (current_mins >= 1200 && current_mins < 1230) {
        new_w = 0; new_r = 128; new_uv = 0;
    } else {
        new_w = 0; new_r = 0; new_uv = 0;
    }

    if (new_w != target_white || new_r != target_red || new_uv != target_uv) {
        Serial.printf("[Schedule] Update Target: %02d:%02d -> W:%d R:%d UV:%d\n", 
            timeinfo.tm_hour, timeinfo.tm_min, new_w, new_r, new_uv);
        target_white = new_w;
        target_red = new_r;
        target_uv = new_uv;
    }
}

// ============================================================================
// MQTT
// ============================================================================
void init_mqtt() {
    if (!mqtt_enabled || mqtt_server.length() == 0) return;
    mqtt.setServer(mqtt_server.c_str(), mqtt_port);
    mqtt.setCallback(mqtt_callback);
    mqtt.setBufferSize(1024);
}

void mqtt_loop() {
    if (!mqtt_enabled || mqtt_server.length() == 0) return;
    if (WiFi.status() != WL_CONNECTED) return;

    if (!mqtt.connected()) {
        if (millis() - lastMqttReconnect > 5000) {
            lastMqttReconnect = millis();
            String clientId = "vaxthus_" + String((uint32_t)ESP.getEfuseMac(), HEX);

            bool connected = (mqtt_user.length() > 0) ? 
                mqtt.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_password.c_str()) :
                mqtt.connect(clientId.c_str());

            if (connected) {
                Serial.println("MQTT connected!");
                mqtt.subscribe((String(TOPIC_BASE) + "/white/set").c_str());
                mqtt.subscribe((String(TOPIC_BASE) + "/red/set").c_str());
                mqtt.subscribe((String(TOPIC_BASE) + "/uv/set").c_str());
                mqtt.subscribe((String(TOPIC_BASE) + "/mode/set").c_str());

                if (!ha_discovery_sent) {
                    publish_ha_discovery();
                    ha_discovery_sent = true;
                }
                
                String topic = String(TOPIC_BASE) + "/mode";
                mqtt.publish(topic.c_str(), manual_override ? "MANUAL" : "AUTO", true);
            }
        }
    }
    mqtt.loop();
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String payloadStr = "";
    for (unsigned int i = 0; i < length; i++) payloadStr += (char)payload[i];

    Serial.printf("MQTT received: %s = %s\n", topic, payloadStr.c_str());

    if (topicStr.endsWith("/mode/set")) {
        if (payloadStr.equalsIgnoreCase("AUTO")) {
            manual_override = false;
            mqtt.publish((String(TOPIC_BASE) + "/mode").c_str(), "AUTO", true);
            Serial.println("[MQTT] Switched to AUTO");
        } else if (payloadStr.equalsIgnoreCase("MANUAL")) {
            manual_override = true;
            last_manual_action = millis();
            mqtt.publish((String(TOPIC_BASE) + "/mode").c_str(), "MANUAL", true);
            Serial.println("[MQTT] Switched to MANUAL");
        }
        return;
    }

    int value = payloadStr.toInt();
    if (value < 0) value = 0;
    if (value > 255) value = 255;

    if (topicStr.endsWith("/white/set")) {
        set_light_manual(PWM_CHANNEL_WHITE, value);
    } else if (topicStr.endsWith("/red/set")) {
        set_light_manual(PWM_CHANNEL_RED, value);
    } else if (topicStr.endsWith("/uv/set")) {
        set_light_manual(PWM_CHANNEL_UV, value);
    }
}

void publish_state(const char* channel, uint8_t value) {
    if (!mqtt.connected()) return;
    String topic = String(TOPIC_BASE) + "/" + channel + "/state";
    mqtt.publish(topic.c_str(), String(value).c_str(), true);
}

void publish_ha_discovery() {
    // Keep existing discovery logic
    const char* channels[] = {"white", "red", "uv"};
    const char* names[] = {"Grow Light White", "Grow Light Red", "Grow Light UV"};

    for (int i = 0; i < 3; i++) {
        JsonDocument doc;
        doc["name"] = names[i];
        doc["unique_id"] = String("vaxthus_") + channels[i];
        doc["command_topic"] = String(TOPIC_BASE) + "/" + channels[i] + "/set";
        doc["state_topic"] = String(TOPIC_BASE) + "/" + channels[i] + "/state";
        doc["brightness_scale"] = 255;
        doc["schema"] = "template";
        doc["command_on_template"] = "{{ brightness }}";
        doc["command_off_template"] = "0";
        doc["state_template"] = "{% if value | int > 0 %}on{% else %}off{% endif %}";
        doc["brightness_template"] = "{{ value }}";
        
        JsonObject device = doc["device"].to<JsonObject>();
        device["identifiers"][0] = "vaxthus_master_v3";
        device["name"] = "Vaxthus Master V3";
        device["sw_version"] = "3.1.0"; // Version bump

        String topic = String(HA_DISCOVERY_PREFIX) + "/light/vaxthus_" + channels[i] + "/config";
        String payload;
        serializeJson(doc, payload);
        mqtt.publish(topic.c_str(), payload.c_str(), true);
    }
}

// ============================================================================
// WEB SERVER
// ============================================================================
void init_webserver() {
    server.on("/", HTTP_GET, []() { server.send(200, "text/html", get_index_html()); });
    server.on("/settings", HTTP_GET, []() { server.send(200, "text/html", get_settings_html()); });
    
    server.on("/saveSettings", HTTP_POST, []() {
        wifi_ssid = server.arg("ssid");
        wifi_password = server.arg("password");
        mqtt_server = server.arg("mqtt_server");
        mqtt_port = server.arg("mqtt_port").toInt();
        mqtt_user = server.arg("mqtt_user");
        mqtt_password = server.arg("mqtt_pass");
        mqtt_enabled = server.hasArg("mqtt_enabled");
        save_settings();
        server.send(200, "text/html", "Saved. Rebooting...");
        delay(1000);
        ESP.restart();
    });

    server.on("/setLight", HTTP_GET, []() {
        if (server.hasArg("white")) set_light_manual(PWM_CHANNEL_WHITE, server.arg("white").toInt());
        if (server.hasArg("red")) set_light_manual(PWM_CHANNEL_RED, server.arg("red").toInt());
        if (server.hasArg("uv")) set_light_manual(PWM_CHANNEL_UV, server.arg("uv").toInt());
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.on("/exitManual", HTTP_GET, []() {
        manual_override = false;
        String topic = String(TOPIC_BASE) + "/mode";
        mqtt.publish(topic.c_str(), "AUTO", true);
        server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"auto\"}");
    });

    server.on("/status", HTTP_GET, []() {
        JsonDocument doc;
        doc["white"] = (int)current_white;
        doc["red"] = (int)current_red;
        doc["uv"] = (int)current_uv;
        doc["target_white"] = target_white;
        doc["target_red"] = target_red;
        doc["target_uv"] = target_uv;
        doc["auto_mode"] = !manual_override;
        doc["wifi_signal"] = get_wifi_signal_strength();
        doc["mqtt_connected"] = mqtt.connected();

        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[20];
            snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            doc["time"] = String(timeStr);
            doc["time_synced"] = true;
        } else {
            doc["time"] = "Not synced";
            doc["time_synced"] = false;
        }

        if (manual_override) {
            unsigned long remaining = (MANUAL_OVERRIDE_TIMEOUT - (millis() - last_manual_action)) / 60000;
            doc["manual_minutes_left"] = remaining;
        }

        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    server.begin();
}

String get_index_html() {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Vaxthus Master V3</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #1a1a2e; color: #eee; }
        h1 { color: #4ecca3; margin-bottom: 5px; }
        .card { background: #16213e; padding: 20px; border-radius: 10px; margin: 10px 0; box-shadow: 0 2px 5px rgba(0,0,0,0.3); }
        .slider-container { margin: 15px 0; }
        .slider-label { display: flex; justify-content: space-between; margin-bottom: 5px; font-weight: 500; }
        input[type=range] { width: 100%; height: 25px; cursor: pointer; }
        .white { accent-color: #fff; }
        .red { accent-color: #ff4444; }
        .uv { accent-color: #9944ff; }
        .status { font-size: 0.9em; color: #888; margin: 5px 0; }
        .time-display { font-size: 1.3em; color: #4ecca3; margin: 10px 0; font-weight: bold; }
        .mode-indicator { 
            display: inline-block; padding: 8px 16px; border-radius: 20px; 
            font-size: 0.95em; margin: 10px 5px 10px 0; font-weight: bold;
        }
        .mode-auto { background: #28a745; color: #fff; }
        .mode-manual { background: #ffc107; color: #000; }
        a { color: #4ecca3; text-decoration: none; font-weight: 500; }
        .btn { 
            background: #ff6b35; color: #fff; border: none;
            padding: 14px 28px; border-radius: 8px; cursor: pointer;
            font-size: 16px; margin: 15px 0 5px 0; display: inline-block; font-weight: bold;
        }
    </style>
</head>
<body>
    <h1>Vaxthus Master V3</h1>
    <div class='time-display' id='currentTime'>--:--:--</div>
    <div>
        <span class='mode-indicator' id='modeIndicator'>Loading...</span>
        <span id='manualCountdown' style='color:#aaa'></span>
    </div>
    <p class='status'>WiFi: <span id='wifi_signal'>--</span>% | MQTT: <span id='mqtt'>--</span></p>

    <div class='card'>
        <h2>Light Control</h2>
        <div class='slider-container'>
            <div class='slider-label'><span>White</span><span id='white_val'>0%</span></div>
            <input type='range' min='0' max='255' value='0' class='white' id='white' onchange='setLight("white", this.value)' oninput='updateLabel("white", this.value)'>
        </div>
        <div class='slider-container'>
            <div class='slider-label'><span>Red</span><span id='red_val'>0%</span></div>
            <input type='range' min='0' max='255' value='0' class='red' id='red' onchange='setLight("red", this.value)' oninput='updateLabel("red", this.value)'>
        </div>
        <div class='slider-container'>
            <div class='slider-label'><span>UV</span><span id='uv_val'>0%</span></div>
            <input type='range' min='0' max='255' value='0' class='uv' id='uv' onchange='setLight("uv", this.value)' oninput='updateLabel("uv", this.value)'>
        </div>
        <button id='autoBtn' class='btn' onclick='exitManual()' style='display:none'>&#127749; Return to Auto Mode</button>
    </div>
    <p><a href='/settings'>Settings</a></p>

    <script>
        function toPercent(val) { return Math.round((val / 255) * 100) + '%'; }
        function updateLabel(c, v) { document.getElementById(c + '_val').innerText = toPercent(v); }
        function setLight(c, v) { updateLabel(c, v); fetch('/setLight?' + c + '=' + v); }
        function exitManual() { fetch('/exitManual').then(() => updateStatus()); }

        function updateStatus() {
            fetch('/status').then(r => r.json()).then(d => {
                if(d.time_synced) document.getElementById('currentTime').innerText = d.time.split(' ')[1];
                document.getElementById('wifi_signal').innerText = d.wifi_signal;
                document.getElementById('mqtt').innerText = d.mqtt_connected ? 'Connected' : 'Disconnected';
                
                let modeEl = document.getElementById('modeIndicator');
                if (d.auto_mode) {
                    modeEl.className = 'mode-indicator mode-auto';
                    modeEl.innerText = 'AUTO MODE';
                    document.getElementById('autoBtn').style.display = 'none';
                    document.getElementById('manualCountdown').innerText = '';
                } else {
                    modeEl.className = 'mode-indicator mode-manual';
                    modeEl.innerText = 'MANUAL MODE';
                    document.getElementById('autoBtn').style.display = 'inline-block';
                    if(d.manual_minutes_left) document.getElementById('manualCountdown').innerText = '(' + d.manual_minutes_left + ' min left)';
                }

                ['white', 'red', 'uv'].forEach(c => {
                    if (document.activeElement.id !== c) {
                        document.getElementById(c).value = d[c]; // Show actual current value (ramping)
                        document.getElementById(c + '_val').innerText = toPercent(d[c]);
                    }
                });
            });
        }
        setInterval(updateStatus, 2000);
        updateStatus();
    </script>
</body>
</html>
)rawliteral";
}

String get_settings_html() {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Settings</title>
    <style>
        body { font-family: Arial; margin: 20px; background: #1a1a2e; color: #eee; }
        .card { background: #16213e; padding: 20px; border-radius: 10px; }
        input { width: 100%; padding: 10px; margin: 5px 0; background: #0f3460; color: #fff; border: none; }
        button { background: #4ecca3; padding: 15px; width: 100%; border: none; font-weight: bold; cursor: pointer; }
    </style>
</head>
<body>
    <h1>Settings</h1>
    <form action='/saveSettings' method='POST'>
        <div class='card'>
            <h3>WiFi</h3>
            <input type='text' name='ssid' placeholder='SSID' value=')rawliteral" + wifi_ssid + R"rawliteral('>
            <input type='password' name='password' placeholder='Password' value=')rawliteral" + wifi_password + R"rawliteral('>
            <h3>MQTT</h3>
            <label><input type='checkbox' name='mqtt_enabled' style='width:auto' )rawliteral" + String(mqtt_enabled ? "checked" : "") + R"rawliteral(> Enable MQTT</label>
            <input type='text' name='mqtt_server' placeholder='Server' value=')rawliteral" + mqtt_server + R"rawliteral('>
            <input type='number' name='mqtt_port' placeholder='Port' value=')rawliteral" + String(mqtt_port) + R"rawliteral('>
            <input type='text' name='mqtt_user' placeholder='User' value=')rawliteral" + mqtt_user + R"rawliteral('>
            <input type='password' name='mqtt_pass' placeholder='Password' value=')rawliteral" + mqtt_password + R"rawliteral('>
        </div>
        <br>
        <button type='submit'>Save & Reboot</button>
    </form>
    <p><a href='/' style='color:#4ecca3'>Back</a></p>
</body>
</html>
)rawliteral";
}

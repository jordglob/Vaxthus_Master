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
String mqtt_server = "";
uint16_t mqtt_port = 1883;
String mqtt_user = "";
String mqtt_password = "";
bool mqtt_enabled = false;

// Light state (0-255)
uint8_t light_white = 0;
uint8_t light_red = 0;
uint8_t light_uv = 0;

// MQTT topics
const char* TOPIC_BASE = "bastun/vaxtljus";
const char* HA_DISCOVERY_PREFIX = "homeassistant";

// Timing
unsigned long lastMqttReconnect = 0;
unsigned long lastWifiCheck = 0;
bool ha_discovery_sent = false;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void init_wifi();
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
void set_light(uint8_t channel, uint8_t value);
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
    init_webserver();
    init_mqtt();

    Serial.println("Setup complete!");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    server.handleClient();
    wifi_monitor();
    mqtt_loop();
    delay(10);
}

// ============================================================================
// SETTINGS (Preferences-based, like Battery-Emulator)
// ============================================================================
void load_settings() {
    Serial.println("Loading settings from NVM...");
    settings.begin("vaxthus", false);

    wifi_ssid = settings.getString("SSID", "");
    wifi_password = settings.getString("PASSWORD", "");
    mqtt_server = settings.getString("MQTTSERVER", "");
    mqtt_port = settings.getUInt("MQTTPORT", 1883);
    mqtt_user = settings.getString("MQTTUSER", "");
    mqtt_password = settings.getString("MQTTPASS", "");
    mqtt_enabled = settings.getBool("MQTTENABLED", false);

    // Load last light states
    light_white = settings.getUChar("LIGHTWHITE", 0);
    light_red = settings.getUChar("LIGHTRED", 0);
    light_uv = settings.getUChar("LIGHTUV", 0);

    Serial.printf("  WiFi SSID: %s\n", wifi_ssid.c_str());
    Serial.printf("  MQTT Server: %s:%d\n", mqtt_server.c_str(), mqtt_port);
    Serial.printf("  MQTT Enabled: %s\n", mqtt_enabled ? "Yes" : "No");
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

void save_light_state() {
    settings.putUChar("LIGHTWHITE", light_white);
    settings.putUChar("LIGHTRED", light_red);
    settings.putUChar("LIGHTUV", light_uv);
}

// ============================================================================
// PWM CONTROL
// ============================================================================
void init_pwm() {
    Serial.println("Initializing PWM channels...");

    ledcSetup(PWM_CHANNEL_WHITE, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_RED, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_UV, PWM_FREQ, PWM_RESOLUTION);

    ledcAttachPin(PWM_WHITE_PIN, PWM_CHANNEL_WHITE);
    ledcAttachPin(PWM_RED_PIN, PWM_CHANNEL_RED);
    ledcAttachPin(PWM_UV_PIN, PWM_CHANNEL_UV);

    // Restore last state
    ledcWrite(PWM_CHANNEL_WHITE, light_white);
    ledcWrite(PWM_CHANNEL_RED, light_red);
    ledcWrite(PWM_CHANNEL_UV, light_uv);

    Serial.printf("  White: %d, Red: %d, UV: %d\n", light_white, light_red, light_uv);
}

void set_light(uint8_t channel, uint8_t value) {
    switch(channel) {
        case PWM_CHANNEL_WHITE:
            light_white = value;
            ledcWrite(PWM_CHANNEL_WHITE, value);
            publish_state("white", value);
            break;
        case PWM_CHANNEL_RED:
            light_red = value;
            ledcWrite(PWM_CHANNEL_RED, value);
            publish_state("red", value);
            break;
        case PWM_CHANNEL_UV:
            light_uv = value;
            ledcWrite(PWM_CHANNEL_UV, value);
            publish_state("uv", value);
            break;
    }
    save_light_state();
}

// ============================================================================
// WIFI (AP + STA mode, like Battery-Emulator)
// ============================================================================
void init_wifi() {
    Serial.println("Initializing WiFi...");

    WiFi.mode(WIFI_AP_STA);

    // Start Access Point
    WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
    Serial.printf("  AP started: %s (pw: %s)\n", ap_ssid.c_str(), ap_password.c_str());
    Serial.printf("  AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // Connect to home WiFi if configured
    if (wifi_ssid.length() > 0) {
        Serial.printf("  Connecting to: %s\n", wifi_ssid.c_str());
        WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
        WiFi.setAutoReconnect(true);

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n  Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("\n  Failed to connect to home WiFi");
        }
    }
}

void wifi_monitor() {
    if (millis() - lastWifiCheck < 10000) return;
    lastWifiCheck = millis();

    if (wifi_ssid.length() > 0 && WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        WiFi.reconnect();
    }
}

// ============================================================================
// MQTT
// ============================================================================
void init_mqtt() {
    if (!mqtt_enabled || mqtt_server.length() == 0) {
        Serial.println("MQTT disabled or not configured");
        return;
    }

    Serial.printf("Initializing MQTT to %s:%d\n", mqtt_server.c_str(), mqtt_port);
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
            Serial.println("Connecting to MQTT...");

            String clientId = "vaxthus_" + String((uint32_t)ESP.getEfuseMac(), HEX);

            bool connected = false;
            if (mqtt_user.length() > 0) {
                connected = mqtt.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_password.c_str());
            } else {
                connected = mqtt.connect(clientId.c_str());
            }

            if (connected) {
                Serial.println("MQTT connected!");

                // Subscribe to command topics
                String topic_white = String(TOPIC_BASE) + "/white/set";
                String topic_red = String(TOPIC_BASE) + "/red/set";
                String topic_uv = String(TOPIC_BASE) + "/uv/set";

                mqtt.subscribe(topic_white.c_str());
                mqtt.subscribe(topic_red.c_str());
                mqtt.subscribe(topic_uv.c_str());

                Serial.printf("Subscribed to: %s, %s, %s\n",
                    topic_white.c_str(), topic_red.c_str(), topic_uv.c_str());

                // Send HA Discovery
                if (!ha_discovery_sent) {
                    publish_ha_discovery();
                    ha_discovery_sent = true;
                }

                // Publish current states
                publish_state("white", light_white);
                publish_state("red", light_red);
                publish_state("uv", light_uv);
            } else {
                Serial.printf("MQTT connection failed, rc=%d\n", mqtt.state());
            }
        }
    }

    mqtt.loop();
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String payloadStr = "";
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }

    Serial.printf("MQTT received: %s = %s\n", topic, payloadStr.c_str());

    int value = payloadStr.toInt();
    if (value < 0) value = 0;
    if (value > 255) value = 255;

    if (topicStr.endsWith("/white/set")) {
        set_light(PWM_CHANNEL_WHITE, value);
    } else if (topicStr.endsWith("/red/set")) {
        set_light(PWM_CHANNEL_RED, value);
    } else if (topicStr.endsWith("/uv/set")) {
        set_light(PWM_CHANNEL_UV, value);
    }
}

void publish_state(const char* channel, uint8_t value) {
    if (!mqtt.connected()) return;

    String topic = String(TOPIC_BASE) + "/" + channel + "/state";
    mqtt.publish(topic.c_str(), String(value).c_str(), true);
}

void publish_ha_discovery() {
    Serial.println("Publishing HA Discovery...");

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
        device["model"] = "Grow Light Controller";
        device["manufacturer"] = "DIY";
        device["sw_version"] = "3.0.0";

        String topic = String(HA_DISCOVERY_PREFIX) + "/light/vaxthus_" + channels[i] + "/config";
        String payload;
        serializeJson(doc, payload);

        mqtt.publish(topic.c_str(), payload.c_str(), true);
        Serial.printf("  Published: %s\n", topic.c_str());
    }
}

// ============================================================================
// WEB SERVER
// ============================================================================
void init_webserver() {
    Serial.println("Initializing web server...");

    // Main page
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", get_index_html());
    });

    // Settings page
    server.on("/settings", HTTP_GET, []() {
        server.send(200, "text/html", get_settings_html());
    });

    // Save settings
    server.on("/saveSettings", HTTP_POST, []() {
        wifi_ssid = server.arg("ssid");
        wifi_password = server.arg("password");
        mqtt_server = server.arg("mqtt_server");
        mqtt_port = server.arg("mqtt_port").toInt();
        mqtt_user = server.arg("mqtt_user");
        mqtt_password = server.arg("mqtt_pass");
        mqtt_enabled = server.hasArg("mqtt_enabled");

        save_settings();

        server.send(200, "text/html", "<html><body><h1>Settings Saved!</h1><p>Rebooting...</p></body></html>");
        delay(1000);
        ESP.restart();
    });

    // Set light values
    server.on("/setLight", HTTP_GET, []() {
        if (server.hasArg("white")) {
            set_light(PWM_CHANNEL_WHITE, server.arg("white").toInt());
        }
        if (server.hasArg("red")) {
            set_light(PWM_CHANNEL_RED, server.arg("red").toInt());
        }
        if (server.hasArg("uv")) {
            set_light(PWM_CHANNEL_UV, server.arg("uv").toInt());
        }
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // Get status
    server.on("/status", HTTP_GET, []() {
        JsonDocument doc;
        doc["white"] = light_white;
        doc["red"] = light_red;
        doc["uv"] = light_uv;
        doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
        doc["wifi_ip"] = WiFi.localIP().toString();
        doc["mqtt_connected"] = mqtt.connected();

        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    server.begin();
    Serial.println("  Web server started on port 80");
}

String get_index_html() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Vaxthus Master V3</title>
    <style>
        body { font-family: Arial; margin: 20px; background: #1a1a2e; color: #eee; }
        h1 { color: #4ecca3; }
        .card { background: #16213e; padding: 20px; border-radius: 10px; margin: 10px 0; }
        .slider-container { margin: 15px 0; }
        .slider-label { display: flex; justify-content: space-between; margin-bottom: 5px; }
        input[type=range] { width: 100%; height: 25px; }
        .white { accent-color: #fff; }
        .red { accent-color: #ff4444; }
        .uv { accent-color: #9944ff; }
        .status { font-size: 0.9em; color: #888; }
        a { color: #4ecca3; }
    </style>
</head>
<body>
    <h1>Vaxthus Master V3</h1>
    <p class='status'>WiFi: <span id='wifi'>--</span> | MQTT: <span id='mqtt'>--</span></p>

    <div class='card'>
        <h2>Light Control</h2>

        <div class='slider-container'>
            <div class='slider-label'><span>White</span><span id='white_val'>)rawliteral" + String(light_white) + R"rawliteral(</span></div>
            <input type='range' min='0' max='255' value=')rawliteral" + String(light_white) + R"rawliteral(' class='white' id='white' onchange='setLight("white", this.value)'>
        </div>

        <div class='slider-container'>
            <div class='slider-label'><span>Red</span><span id='red_val'>)rawliteral" + String(light_red) + R"rawliteral(</span></div>
            <input type='range' min='0' max='255' value=')rawliteral" + String(light_red) + R"rawliteral(' class='red' id='red' onchange='setLight("red", this.value)'>
        </div>

        <div class='slider-container'>
            <div class='slider-label'><span>UV</span><span id='uv_val'>)rawliteral" + String(light_uv) + R"rawliteral(</span></div>
            <input type='range' min='0' max='255' value=')rawliteral" + String(light_uv) + R"rawliteral(' class='uv' id='uv' onchange='setLight("uv", this.value)'>
        </div>
    </div>

    <p><a href='/settings'>Settings</a></p>

    <script>
        function setLight(channel, value) {
            document.getElementById(channel + '_val').innerText = value;
            fetch('/setLight?' + channel + '=' + value);
        }

        function updateStatus() {
            fetch('/status')
                .then(r => r.json())
                .then(d => {
                    document.getElementById('wifi').innerText = d.wifi_connected ? d.wifi_ip : 'Disconnected';
                    document.getElementById('mqtt').innerText = d.mqtt_connected ? 'Connected' : 'Disconnected';
                });
        }

        setInterval(updateStatus, 5000);
        updateStatus();
    </script>
</body>
</html>
)rawliteral";
    return html;
}

String get_settings_html() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Settings - Vaxthus Master</title>
    <style>
        body { font-family: Arial; margin: 20px; background: #1a1a2e; color: #eee; }
        h1 { color: #4ecca3; }
        .card { background: #16213e; padding: 20px; border-radius: 10px; margin: 10px 0; }
        label { display: block; margin: 10px 0 5px; }
        input[type=text], input[type=password], input[type=number] {
            width: 100%; padding: 10px; border: none; border-radius: 5px;
            background: #0f3460; color: #eee; box-sizing: border-box;
        }
        input[type=checkbox] { width: 20px; height: 20px; }
        button {
            background: #4ecca3; color: #1a1a2e; border: none;
            padding: 15px 30px; border-radius: 5px; cursor: pointer;
            font-size: 16px; margin-top: 20px; width: 100%;
        }
        a { color: #4ecca3; }
    </style>
</head>
<body>
    <h1>Settings</h1>
    <form action='/saveSettings' method='POST'>
        <div class='card'>
            <h2>WiFi</h2>
            <label>SSID:</label>
            <input type='text' name='ssid' value=')rawliteral" + wifi_ssid + R"rawliteral('>
            <label>Password:</label>
            <input type='password' name='password' value=')rawliteral" + wifi_password + R"rawliteral('>
        </div>

        <div class='card'>
            <h2>MQTT</h2>
            <label><input type='checkbox' name='mqtt_enabled' )rawliteral" + String(mqtt_enabled ? "checked" : "") + R"rawliteral(> Enable MQTT</label>
            <label>Server:</label>
            <input type='text' name='mqtt_server' value=')rawliteral" + mqtt_server + R"rawliteral('>
            <label>Port:</label>
            <input type='number' name='mqtt_port' value=')rawliteral" + String(mqtt_port) + R"rawliteral('>
            <label>Username:</label>
            <input type='text' name='mqtt_user' value=')rawliteral" + mqtt_user + R"rawliteral('>
            <label>Password:</label>
            <input type='password' name='mqtt_pass' value=')rawliteral" + mqtt_password + R"rawliteral('>
        </div>

        <button type='submit'>Save & Reboot</button>
    </form>
    <p><a href='/'>Back to Dashboard</a></p>
</body>
</html>
)rawliteral";
    return html;
}

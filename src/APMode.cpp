#include "APMode.h"

// --------------------------------------------------------------------------
// AP MAIN LOGIC
// --------------------------------------------------------------------------

void enterAPMode() {
      // Fallback to AP Mode -> ENTER ISOLATED STATE
      WiFi.disconnect(); 
      delay(100);
      WiFi.mode(WIFI_AP); // Ren AP mode, ingen STA scanning som stör
      WiFi.softAP(AP_SSID, AP_PASS); // Use Globals defines
      
      displayMgr.showAPScreen(); // Rita skärmen EN GÅNG
}

void loopAP() {
    // Endast WebServer och Display uppdateringar - INGET ANNAT
    
    // Hantera knappar för reboot/exit
    int clickBtm = btnBtm.update();
    if (clickBtm > 0) {
        displayMgr.showMessage("Rebooting...");
        delay(500);
        ESP.restart();
    }
    
    // UPDATE LIGHTS (So fading works in AP mode too)
    lightWhite.update();
    lightRed.update();
    lightUV.update();

    // Ingen Loop-tids beräkning, ingen MQTT, ingen sensor-logik.
    delay(10); 
}

// --------------------------------------------------------------------------
// AP WEB HELPERS - Moved from main.cpp to keep codebases clean
// --------------------------------------------------------------------------

String getAPPageHTML() {
      String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
      html += "<style>body{font-family:sans-serif;background:#222;color:#fff;text-align:center;padding:10px;}";
      html += "input[type=range]{width:90%;height:40px;margin:10px 0;}"; 
      html += "input[type=time]{padding:10px;font-size:20px;width:90%;text-align:center;}";
      html += "button{padding:15px;width:100%;margin-top:20px;font-size:18px;background:#c00;color:fff;border:none;border-radius:5px;}";
      html += ".card{background:#333;padding:15px;border-radius:10px;margin-bottom:15px;}";
      html += "</style></head><body>";
      
      html += "<h2>Vaxthus AP (Emergency)</h2>";
      html += "<p>WiFi Down. Manual Control Active.</p>";
      html += "<p>FIRMWARE: " + String(FIRMWARE_VERSION) + "</p>";
      
      // Lights
      html += "<div class='card'><h3>Lights</h3>";
      html += "White: <span id='vW'>" + String(lightWhite.getTarget()) + "</span><br>";
      html += "<input type='range' min='0' max='255' value='" + String(lightWhite.getTarget()) + "' oninput='u(this,\"vW\")' onchange='s(\"white\",this.value)'><br>";
      
      html += "Red: <span id='vR'>" + String(lightRed.getTarget()) + "</span><br>";
      html += "<input type='range' min='0' max='255' value='" + String(lightRed.getTarget()) + "' oninput='u(this,\"vR\")' onchange='s(\"red\",this.value)'><br>";
      
      html += "UV: <span id='vU'>" + String(lightUV.getTarget()) + "</span><br>";
      html += "<input type='range' min='0' max='255' value='" + String(lightUV.getTarget()) + "' oninput='u(this,\"vU\")' onchange='s(\"uv\",this.value)'></div>";
      
      // Time
      html += "<div class='card'><h3>Set Time</h3>";
      html += "<input type='time' id='ti' onchange='st(this.value)'></div>";

      html += "<button onclick=\"location.href='/reboot'\">Reboot / Try WiFi</button>";
      
      html += "<script>";
      html += "function u(e,id){document.getElementById(id).innerText=e.value;}";
      html += "function s(c,v){fetch('/ap/set?ch='+c+'&val='+v);}"; 
      html += "function st(v){fetch('/ap/set?time='+v);}";
      html += "</script>";
      html += "</body></html>";
      return html;
}

void handleAPSet(AsyncWebServerRequest *request) {
      if(!isAPMode) { request->send(403, "text/plain", "Not in AP Mode"); return; }
      
      // Handle Lights
      if(request->hasParam("ch") && request->hasParam("val")) {
          String ch = request->getParam("ch")->value();
          int val = request->getParam("val")->value().toInt();
          
          if(ch == "white") { lightWhite.setTarget(val); }
          else if(ch == "red") { lightRed.setTarget(val); }
          else if(ch == "uv") { lightUV.setTarget(val); }
      }
      
      // Handle Time (Format "14:30")
      if(request->hasParam("time")) {
          String tStr = request->getParam("time")->value(); 
          if(tStr.length() >= 5) {
            int h = tStr.substring(0,2).toInt();
            int m = tStr.substring(3,5).toInt();
            
            struct tm tm;
            tm.tm_year = 2026 - 1900; // Use current year (approx)
            tm.tm_mon = 0;
            tm.tm_mday = 8;
            tm.tm_hour = h;
            tm.tm_min = m;
            tm.tm_sec = 0;
            time_t t = mktime(&tm);
            struct timeval now = { .tv_sec = t, .tv_usec = 0 };
            settimeofday(&now, NULL);
          }
      }
      request->send(200, "text/plain", "OK");
}


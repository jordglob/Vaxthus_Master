#include "DisplayManager.h"
#include <SPI.h>

// Texter [SV, EN]
const char* TXT_ALL[] =       {"ALLA", "ALL"};
const char* TXT_WHITE[] =     {"VIT", "WHITE"};
const char* TXT_RED[] =       {"ROD", "RED"};
const char* TXT_UV[] =        {"UV", "UV"};
const char* TXT_PRESETS[] =   {"LÄGEN", "PRESET"}; // New menu item
const char* TXT_SET_TIME[] =  {"STALL TID", "SET TIME"};
const char* TXT_TIME_HINT[] = {"TID: +1h / +1m", "TIME: +1h / +1m"};
const char* TXT_SETTINGS[] =  {"INSTALLN.", "SETTINGS"};
const char* TXT_ECO[] =       {"ECO LAGE", "ECO MODE"};
const char* TXT_QR[] =        {"VISA QR", "SHOW QR"};
const char* TXT_INFO[] =      {"INFO / HJALP", "INFO / HELP"};
const char* TXT_REBOOT[] =    {"STARTA OM", "REBOOT"};
const char* TXT_LANG[] =      {"SPRAK", "LANGUAGE"};
const char* TXT_PRESS[] =     {"TRYCK BTM", "PRESS BTM"};
const char* TXT_SCAN_ME[] =   {"SCANNA MIG", "SCAN ME"};

// Preset Names
const char* TXT_P_SEED[] =    {"FRO/STICK", "SEED/CLONE"};
const char* TXT_P_VEG[] =     {"TILLVAXT", "VEGETATIVE"};
const char* TXT_P_BLOOM[] =   {"BLOMNING", "BLOOM"};
const char* TXT_P_FULL[] =    {"FULLT OS", "FULL BLAST"};

DisplayManager::DisplayManager() {
}

void DisplayManager::init() {
    tft.init();
    tft.setRotation(1); 
    tft.setSwapBytes(true);
    
    footerSpr.createSprite(320, 25);
    footerSpr.fillSprite(TFT_BLACK);
    
    headerSpr.createSprite(320, 30);
    headerSpr.fillSprite(TFT_DARKGREY);

    settingsSpr.createSprite(320, 60);
    settingsSpr.fillSprite(TFT_BLACK);
    
    tft.fillScreen(TFT_BLACK);
}

void DisplayManager::showMessage(const char* msg) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString(msg, 160, 85);
}

void DisplayManager::showIntro() {
    // Starfield animation implementation
    const int NUM_STARS = 120;
    float stars[NUM_STARS][3]; 
    uint16_t starColors[NUM_STARS];
    
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
    if (!frame.createSprite(320, 170)) return; 

    // Assume setup calls this, so we can use pins if needed, but lets skip check for simplicity
    
    while(millis() - startTime < 1000) { 
        frame.fillSprite(TFT_BLACK); 
        unsigned long elapsed = millis() - startTime;
        speed = 10.0 + (elapsed / 40.0);
        
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
                   float prevFactor = fov / (stars[i][2] + speed + 5); 
                   int xPrev = 160 + (int)(stars[i][0] * prevFactor);
                   int yPrev = 85 + (int)(stars[i][1] * prevFactor);
                   frame.drawLine(xPrev, yPrev, x2d, y2d, starColors[i]);
                } else {
                   frame.drawPixel(x2d, y2d, starColors[i]);
                }
            }
        }
        
        frame.setTextDatum(MC_DATUM);
        if(elapsed > 100 && elapsed < 400) {
            if(random(0,10) > 8) {
                frame.setTextColor(TFT_GREEN, TFT_BLACK); 
                frame.drawString("VAXTHUS", 160+random(-3,3), 85+random(-3,3), 4);
            } else {
                frame.setTextColor(TFT_WHITE, TFT_BLACK);
                frame.drawString("VAXTHUS", 160, 85, 4);
            }
        } 
        else if(elapsed > 400 && elapsed < 700) {
            if(random(0,10) > 8) {
                frame.setTextColor(TFT_MAGENTA, TFT_BLACK); 
                frame.drawString("MASTER", 160+random(-3,3), 85+random(-3,3), 4);
            } else {
                frame.setTextColor(TFT_WHITE, TFT_BLACK);
                frame.drawString("MASTER", 160, 85, 4);
            }
        }
        else if(elapsed > 700) {
             frame.setTextColor(TFT_CYAN, TFT_BLACK);
             frame.drawString("SYSTEM READY", 160, 85, 2);
        }
        
        frame.pushSprite(0, 0);
    }
    frame.deleteSprite();
}

void DisplayManager::showBootQR() {
    tft.fillScreen(TFT_BLACK);
    for(int j=0; j<3; j++) {
        tft.fillScreen(TFT_WHITE);
        String ip = "http://" + WiFi.localIP().toString();
        QRCode qrcode;
        uint8_t qrcodeData[qrcode_getBufferSize(3)];
        qrcode_initText(&qrcode, qrcodeData, 3, 0, ip.c_str());
        
        int scale = 4; 
        int qrSizePx = qrcode.size * scale;
        int xQR = (320 - qrSizePx) / 2;
        int yQR = (170 - qrSizePx) / 2;

        for (uint8_t y = 0; y < qrcode.size; y++) {
            for (uint8_t x = 0; x < qrcode.size; x++) {
                if (qrcode_getModule(&qrcode, x, y)) {
                    tft.fillRect(xQR + (x*scale), yQR + (y*scale), scale, scale, TFT_BLACK);
                }
            }
        }
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_BLACK, TFT_WHITE);
        tft.setTextSize(2);
        tft.drawString(WiFi.localIP().toString(), 160, yQR + qrSizePx + 5);

        delay(800);
        tft.fillScreen(TFT_BLACK);
        delay(200);
    }
}

void DisplayManager::update(MenuSelection sel, int setOpt, int lang, bool manual, bool eco, 
                            int vWhite, int vRed, int vUv, int activePreset, 
                            bool wifiConnected, bool mqttConnected, int rssi, const char* version) {
    
    // Hämta tid för att se om klockan ändrats
    struct tm timeinfo;
    int currentMinute = -1;
    if (getLocalTime(&timeinfo)) {
        currentMinute = timeinfo.tm_min;
    }

    // Dirty Check - Rita bara om något ändrats!
    bool changed = false;
    if (state.sel != sel) changed = true;
    if (state.setOpt != setOpt) changed = true;
    if (state.lang != lang) changed = true;
    if (state.manual != manual) changed = true;
    if (state.eco != eco) changed = true;
    if (state.vWhite != vWhite) changed = true;
    if (state.vRed != vRed) changed = true;
    if (state.vUv != vUv) changed = true;
    if (state.activePreset != activePreset) changed = true; 
    if (state.lastMinute != currentMinute) changed = true; 
    if (state.wifi != wifiConnected) changed = true; 
    if (state.mqtt != mqttConnected) changed = true;
    if (abs(state.rssi - rssi) > 2) changed = true; 
    
    if (this->firstRun) { 
        changed = true; 
        tft.fillScreen(TFT_BLACK); // Clear "Boot Message" or artifacts!
        this->firstRun = false; 
    }

    if (!changed) return; 

    // Spara state
    state.sel = sel;
    state.setOpt = setOpt;
    state.lang = lang;
    state.manual = manual;
    state.eco = eco;
    state.vWhite = vWhite;
    state.vRed = vRed;
    state.vUv = vUv;
    state.activePreset = activePreset;
    state.wifi = wifiConnected;
    state.mqtt = mqttConnected;
    state.rssi = rssi;
    state.lastMinute = currentMinute;

    static MenuSelection lastSel = SEL_ALL;
    bool menuChanged = (sel != lastSel);

    // FIX: Clear screen aggressively if changing "Views" to avoid artifacts
    // List View Group: ALL, WHITE, RED, UV
    bool isList = (sel == SEL_ALL || sel == SEL_WHITE || sel == SEL_RED || sel == SEL_UV);
    bool wasList = (lastSel == SEL_ALL || lastSel == SEL_WHITE || lastSel == SEL_RED || lastSel == SEL_UV);

    if (menuChanged) {
        // If we stay within the List View, do NOT clear (reduces flicker)
        // If we move to/from any other view (Presets, Settings, Clock, etc), CLEAR everything first.
        if (!(isList && wasList)) {
             tft.fillScreen(TFT_BLACK);
        }
    }
    lastSel = sel;

    if (sel == SEL_SETTINGS) {
        drawSettingsMenu(setOpt, lang, eco);
        return; 
    }
    if (sel == SEL_QR) {
        drawQRInfo(lang);
        return;
    }
    if (sel == SEL_INFO) {
        drawInfoPage(lang);
        return;
    }

    // Standard Views (Header/Footer + Content)
    drawHeader(manual, eco, wifiConnected, mqttConnected);
    
    if (sel == SEL_CLOCK) {
        drawClockMenu(lang);
    } 
    else if (sel == SEL_PRESETS) {
        drawPresetsMenu(activePreset, lang); 
    }
    else {
        drawChannelList(sel, lang, vWhite, vRed, vUv);
    }

    drawFooter(rssi, version);
}

void DisplayManager::drawHeader(bool manual, bool eco, bool wifi, bool mqtt) {
    headerSpr.fillSprite(TFT_DARKGREY); 
    headerSpr.setTextColor(TFT_WHITE, TFT_DARKGREY);
    headerSpr.setTextSize(2); 
    headerSpr.setTextDatum(TL_DATUM);
    headerSpr.setCursor(5, 7);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
       char timeStr[16]; 
       sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
       headerSpr.print(timeStr);
    } else {
       headerSpr.print("--:--");
    }

    headerSpr.setCursor(100, 7);
    if (manual) {
      headerSpr.setTextColor(TFT_ORANGE, TFT_DARKGREY);
      headerSpr.print("MAN"); 
    } else {
      headerSpr.setTextColor(TFT_CYAN, TFT_DARKGREY);
      headerSpr.print("AUTO"); 
    } 

    if (eco) {
        headerSpr.setCursor(160, 7);
        headerSpr.setTextColor(TFT_GREEN, TFT_DARKGREY);
        headerSpr.print("ECO");
    }

    headerSpr.setTextDatum(TR_DATUM); 
    int xRight = 315;
    
    // Status Indicators
    if (wifi) { 
        headerSpr.setTextColor(TFT_GREEN, TFT_DARKGREY);
        headerSpr.drawString("WIFI", xRight, 7);
        xRight -= 50;
        
        if (mqtt) {
            headerSpr.setTextColor(TFT_GREEN, TFT_DARKGREY);
            headerSpr.drawString("MQTT", xRight, 7); 
        } else {
            headerSpr.setTextColor(TFT_RED, TFT_DARKGREY);
            headerSpr.drawString("MQTT", xRight, 7); 
        }
    } else {
        headerSpr.setTextColor(TFT_RED, TFT_DARKGREY);
        headerSpr.drawString("NO-W", xRight, 7);
    }

    headerSpr.pushSprite(0, 0);
}

void DisplayManager::drawFooter(int rssi, const char* version) {
    footerSpr.fillSprite(TFT_BLACK);
    footerSpr.drawFastHLine(0, 0, 320, TFT_DARKGREY);

    footerSpr.setTextDatum(ML_DATUM); 
    footerSpr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    footerSpr.drawString("WiFi: ", 5, 12);
    
    if(rssi > -100) {
        if(rssi > -60) footerSpr.setTextColor(TFT_GREEN, TFT_BLACK);
        else if(rssi > -70) footerSpr.setTextColor(TFT_YELLOW, TFT_BLACK);
        else footerSpr.setTextColor(TFT_RED, TFT_BLACK);
        footerSpr.drawString(String(rssi) + " dBm", 40, 12);
    } else {
        footerSpr.setTextColor(TFT_RED, TFT_BLACK);
        footerSpr.drawString("DISCONNECTED", 40, 12);
    }

    footerSpr.setTextDatum(MR_DATUM); 
    footerSpr.setTextColor(TFT_MAGENTA, TFT_BLACK);
    footerSpr.drawString(version, 315, 12);

    footerSpr.pushSprite(0, 145);
}

void DisplayManager::drawChannelList(MenuSelection sel, int lang, int vWhite, int vRed, int vUv) {
    tft.setTextSize(2);
    int yStart = 40; // Moved UP a bit to fit presets line
    int lineH = 22; 

    auto drawLine = [&](int idx, String label, int value, int color) {
        int y = yStart + (idx * lineH);
        bool isSelected = false;
        if (sel == SEL_ALL && idx == 0) isSelected = true;
        if (sel == SEL_WHITE && idx == 1) isSelected = true;
        if (sel == SEL_RED && idx == 2) isSelected = true;
        if (sel == SEL_UV && idx == 3) isSelected = true;
        if (sel == SEL_PRESETS && idx == 4) isSelected = true;

        if (isSelected) tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        else tft.setTextColor(TFT_BLACK, TFT_BLACK); 
        tft.drawString(">", 5, y);

        tft.setTextColor(color, TFT_BLACK);
        tft.drawString(label, 30, y);

        if (idx < 4) { // PWM channels
            if (idx == 0) {
                tft.setTextColor(TFT_BLACK, TFT_BLACK);
                tft.drawString("100%  ", 220, y); 
            } else {
                int pct = map(value, 0, 255, 0, 100);
                String pctStr = String(pct) + "%  "; 
                tft.setTextColor(color, TFT_BLACK); 
                tft.drawString(pctStr, 220, y);
            }
        }
        else { 
            // PRESETS Line
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            tft.drawString(">>>", 220, y);
        }
    };

    drawLine(0, TXT_ALL[lang], 0, TFT_WHITE);
    drawLine(1, TXT_WHITE[lang], vWhite, TFT_WHITE);
    drawLine(2, TXT_RED[lang], vRed, TFT_RED);
    drawLine(3, TXT_UV[lang], vUv, TFT_MAGENTA);
    
    // Add Preset Line at bottom of list
    // Only text
    int y = yStart + (4 * lineH);
    if(sel == SEL_PRESETS) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString(">", 5, y);
    } else {
        tft.setTextColor(TFT_BLACK, TFT_BLACK);
        tft.drawString(">", 5, y);
    }
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(TXT_PRESETS[lang], 30, y);
    // Hint
    if(sel == SEL_PRESETS) {
         tft.setTextSize(1);
         tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
         tft.drawString(lang == LANG_SV ? "TOP: Valj" : "TOP: Select", 180, y+5);
         tft.setTextSize(2);
    } else {
         tft.fillRect(180, y+5, 100, 10, TFT_BLACK); // clear hint
    }
}

void DisplayManager::drawPresetsMenu(int activePreset, int lang) {
    // Show a big list of presets
    // Overlay on bottom half or similar?
    // Let's use the list area
    
    int yCenter = 90;
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(TXT_PRESETS[lang], 160, 40);

    const char* pName = "";
    const char* pDesc = "";
    
    if(activePreset == PRESET_SEED) { pName = TXT_P_SEED[lang]; pDesc = "Start / Clone"; }
    else if(activePreset == PRESET_VEG) { pName = TXT_P_VEG[lang]; pDesc = "Grow / Green"; }
    else if(activePreset == PRESET_BLOOM) { pName = TXT_P_BLOOM[lang]; pDesc = "Flowering"; }
    else if(activePreset == PRESET_FULL) { pName = TXT_P_FULL[lang]; pDesc = "Max Power"; }
    else { pName = "MANUAL"; pDesc = "Custom"; }

    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String("< ") + pName + " >", 160, yCenter);

    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(pDesc, 160, yCenter + 30);
    
    tft.setTextDatum(BC_DATUM);
    tft.drawString(lang == LANG_SV ? "TOP: Byt  |  BTM: Aktivera/Stäng" : "TOP: Next  |  BTM: Apply/Back", 160, 165);
}

void DisplayManager::drawSettingsMenu(int setOpt, int lang, bool eco) {
     tft.setTextDatum(TC_DATUM);
     tft.setTextColor(TFT_CYAN, TFT_BLACK);
     tft.setTextSize(3);
     tft.drawString(TXT_SETTINGS[lang], 160, 15);
     
     tft.fillRect(0, 60, 320, 60, TFT_BLACK);

     tft.setTextDatum(MC_DATUM); 
     int yCenter = 90;
     
     const char* label = "";
     const char* val = "";
     uint16_t valCol = TFT_WHITE;

     if (setOpt == SET_LANG) {
         label = TXT_LANG[lang];
         val = (lang == LANG_SV) ? "SVENSKA" : "ENGLISH";
         valCol = TFT_MAGENTA;
     } else if (setOpt == SET_ECO) {
         label = TXT_ECO[lang];
         val = eco ? "ON" : "OFF";
         valCol = eco ? TFT_GREEN : TFT_RED;
     } else if (setOpt == SET_QR) {
         label = TXT_QR[lang];
         val = TXT_SCAN_ME[lang];
         valCol = TFT_CYAN;
     } else if (setOpt == SET_INFO) {
         label = TXT_INFO[lang];
         val = "READ";
         valCol = TFT_YELLOW;
     } else if (setOpt == SET_REBOOT) {
         label = TXT_REBOOT[lang];
         val = TXT_PRESS[lang];
         valCol = TFT_WHITE;
     }

     tft.setTextColor(TFT_WHITE, TFT_BLACK);
     tft.setTextSize(2);
     tft.drawString(String("< ") + label + " >", 160, yCenter - 25);
     
     tft.setTextColor(valCol, TFT_BLACK);
     tft.setTextSize(3);
     tft.drawString(val, 160, yCenter + 15);
     
     tft.setTextDatum(BC_DATUM);
     tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
     tft.setTextSize(1);
     if(lang == LANG_SV) tft.drawString("TOP: Nästa  |  BTM: Välj/Ändra", 160, 165);
     else tft.drawString("TOP: Next   |  BTM: Select/Toggle", 160, 165);
}

void DisplayManager::drawQRInfo(int lang) {
      tft.fillScreen(TFT_WHITE);
      
      String qrContent;
      String infoText;
      
      // Smart QR Logic (Dual)
      tft.fillScreen(TFT_WHITE);
    
    // ---------------------------------------------------------
    // QR 1: WiFi Connect (Top Half)
    // ---------------------------------------------------------
    String qrWifi = "WIFI:T:WPA;S:Vaxthus-Master;P:vaxthus123;;";
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(3)];
    qrcode_initText(&qrcode, qrcodeData, 3, 0, qrWifi.c_str());
    
    int scale = 3; 
    int x1 = 10; 
    int y1 = 10;
    
    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                tft.fillRect(x1 + (x*scale), y1 + (y*scale), scale, scale, TFT_BLACK);
            }
        }
    }
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(2);
    tft.drawString("1. JOIN WIFI", x1 + (qrcode.size*scale) + 10, y1 + 10);
    tft.setTextSize(1);
    tft.drawString("Vaxthus-Master", x1 + (qrcode.size*scale) + 10, y1 + 35);


    // ---------------------------------------------------------
    // QR 2: Web Server (Bottom Half)
    // ---------------------------------------------------------
    // Use the AP IP: 192.168.4.1
    String qrWeb = "http://192.168.4.1";
    QRCode qrcode2;
    uint8_t qrcodeData2[qrcode_getBufferSize(3)];
    qrcode_initText(&qrcode2, qrcodeData2, 3, 0, qrWeb.c_str());
    
    int y2 = 120; // Lower half
    int x2 = 10;

    for (uint8_t y = 0; y < qrcode2.size; y++) {
        for (uint8_t x = 0; x < qrcode2.size; x++) {
            if (qrcode_getModule(&qrcode2, x, y)) {
                tft.fillRect(x2 + (x*scale), y2 + (y*scale), scale, scale, TFT_BLACK);
            }
        }
    }

    tft.setTextSize(2);
    tft.drawString("2. OPEN APP", x2 + (qrcode2.size*scale) + 10, y2 + 10);
    tft.setTextSize(1);
    tft.drawString("192.168.4.1", x2 + (qrcode2.size*scale) + 10, y2 + 35);

      if(!state.wifi) {
          tft.setTextColor(TFT_RED, TFT_WHITE);
          tft.drawString("(AP MODE)", xText, yText + 75);
      }

      tft.setTextColor(TFT_RED, TFT_WHITE);
      tft.setTextSize(2); 
      tft.drawString("EXIT >", xText, 120); 
}

void DisplayManager::drawInfoPage(int lang) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("CONTROLS", 160, 20, 4);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1); 
    
    int yStart = 50;
    int lineH = 20;

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("TOP BUTTON", 10, yStart);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    if(lang == LANG_SV) {
       tft.drawString("1: Oka/Nav  2: Byt Kanal  3: AUTO", 100, yStart);
    } else {
       tft.drawString("1: Up/Nav   2: Next Ch    3: AUTO", 100, yStart);
    }

    yStart += 50;
    
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.drawString("BTM BUTTON", 10, yStart);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    if(lang == LANG_SV) {
       tft.drawString("1: Minska/Valj  2: OFF  3: Inst.", 100, yStart);
    } else {
       tft.drawString("1: Down/Sel     2: OFF  3: Settings", 100, yStart);
    }

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextDatum(BC_DATUM); 
    tft.drawString("Build: " __DATE__ " " __TIME__, 160, 165);
}

void DisplayManager::drawClockMenu(int lang) {
    int y = 125; 
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(">", 5, y);
    tft.drawString(TXT_TIME_HINT[lang], 100, y);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(TXT_SET_TIME[lang], 30, y);
}

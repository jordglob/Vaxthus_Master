#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <qrcode.h>
#include "Globals.h"

class DisplayManager {
public:
    DisplayManager();
    void init();
    void showIntro();
    void showBootQR();
    void showMessage(const char* msg);
    
    // Updated signature: explicit connection flags
    void update(MenuSelection sel, int setOpt, int lang, bool manual, bool eco, 
                int vWhite, int vRed, int vUv, int activePreset, 
                bool wifiConnected, bool mqttConnected, int rssi, const char* version);

private:
    struct DisplayState {
        MenuSelection sel;
        int setOpt;
        int lang;
        bool manual;
        bool eco;
        int vWhite;
        int vRed;
        int vUv;
        int activePreset;
        bool wifi;  // New
        bool mqtt;  // New
        int rssi;
        int lastMinute; // For header clock
    } state;

    bool firstRun = true; // To clear boot msg

    TFT_eSPI tft = TFT_eSPI();
    TFT_eSprite footerSpr = TFT_eSprite(&tft);
    TFT_eSprite headerSpr = TFT_eSprite(&tft);
    TFT_eSprite settingsSpr = TFT_eSprite(&tft); // Used for generic content lists now

    void drawHeader(bool manual, bool eco, bool wifi, bool mqtt);
    void drawFooter(int rssi, const char* version);
    void drawChannelList(MenuSelection sel, int lang, int vWhite, int vRed, int vUv);
    void drawSettingsMenu(int setOpt, int lang, bool eco);
    void drawQRInfo(int lang);
    void drawInfoPage(int lang);
    void drawClockMenu(int lang);
    void drawPresetsMenu(int activePreset, int lang); // NEW
};

#endif

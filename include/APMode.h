#ifndef AP_MODE_H
#define AP_MODE_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "DisplayManager.h"
#include "ButtonHandler.h"
#include "Globals.h"

// Vi behöver access till globala objekt
extern DisplayManager displayMgr;
extern ButtonHandler btnBtm;

// Huvudfunktioner för AP-läget
void enterAPMode();
void loopAP();

// Web Helper Functions (Implementation in APMode.cpp)
String getAPPageHTML();
void handleAPSet(AsyncWebServerRequest *request);

#endif

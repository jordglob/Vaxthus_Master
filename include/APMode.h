#ifndef AP_MODE_H
#define AP_MODE_H

#include <Arduino.h>
#include <WiFi.h>
#include "DisplayManager.h"
#include "ButtonHandler.h"

// Vi behöver access till globala objekt
extern DisplayManager displayMgr;
extern ButtonHandler btnBtm;

// Huvudfunktioner för AP-läget
void enterAPMode();
void loopAP();

#endif

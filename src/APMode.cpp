#include "APMode.h"

// --------------------------------------------------------------------------
// AP MAIN LOGIC
// --------------------------------------------------------------------------
// Detta är den ISOLERADE koden som körs när inget WiFi finns.
// Den hanterar ENBART:
// 1. Visar QR-koder (via displayMgr)
// 2. Startar om enheten vid knapptryck
// 3. (Implicit) Webservern svarar i bakgrunden via async-libbet
// --------------------------------------------------------------------------

void enterAPMode() {
      // Fallback to AP Mode -> ENTER ISOLATED STATE
      WiFi.disconnect(); 
      delay(100);
      WiFi.mode(WIFI_AP); // Ren AP mode, ingen STA scanning som stör
      WiFi.softAP("Vaxthus-Master", "vaxthus123");
      
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
    
    // Ingen Loop-tids beräkning, ingen MQTT, ingen sensor-logik.
    // Bara ren överlevnad.
    
    delay(10); // Ge tid till WiFi stacken att svara på HTTP requests
}

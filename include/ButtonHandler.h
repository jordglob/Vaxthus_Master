#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>

// Knapp Konfig
const int DEBOUNCE_MS = 50;
const int DOUBLE_CLICK_MS = 500; // Tid för att registrera dubbelklick
const int LONG_PRESS_MS = 1000; // 1 sekund för långt tryck

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

#endif

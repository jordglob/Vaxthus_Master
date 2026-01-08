#ifndef LIGHTCHANNEL_H
#define LIGHTCHANNEL_H

#include <Arduino.h>

class LightChannel {
private:
    uint8_t _pin;
    uint8_t _channel;
    float _maxWattage; // Max consumption at 100% duty (in Watts)
    
    // Values 0-255
    int _targetValue;
    float _currentValue;
    
    // Fading parameters
    float _fadeSpeed; // Step size per update
    
    // Energy Metering
    double _energyTotalWh; // Accumulated Watt-hours
    unsigned long _lastUpdateTime;
    
    // Eco Mode
    bool _ecoMode = false;

public:
    // Constructor
    LightChannel(uint8_t pin, uint8_t channel, float maxWattage = 10.0);

    // Initialization (call in setup)
    void begin(int frequency = 5000, int resolution = 8);

    // Main logic (call in loop)
    bool update();

    // Control
    void setTarget(int target);
    void setDirect(int value); // Skip fading
    void setEco(bool active) { _ecoMode = active; }

    // Getters
    int getTarget() const;
    int getCurrent() const;
    
    // Energy Monitor
    double getEnergyUsageWh() const;
    double getCurrentPowerW() const;
    void resetEnergy();
};

#endif

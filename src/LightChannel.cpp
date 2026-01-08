#include "LightChannel.h"

LightChannel::LightChannel(uint8_t pin, uint8_t channel, float maxWattage) 
    : _pin(pin), _channel(channel), _maxWattage(maxWattage) {
    _targetValue = 0;
    _currentValue = 0.0f;
    _fadeSpeed = 1.6f; // Standard fading speed from previous implementation
    _energyTotalWh = 0.0;
    _lastUpdateTime = 0;
}

void LightChannel::begin(int frequency, int resolution) {
    ledcSetup(_channel, frequency, resolution);
    ledcAttachPin(_pin, _channel);
    ledcWrite(_channel, 0);
    _lastUpdateTime = millis();
}

bool LightChannel::update() {
    unsigned long now = millis();
    unsigned long dt = now - _lastUpdateTime;
    _lastUpdateTime = now;

    // 1. Calculate Energy Usage
    // Power (W) * Time (h)
    // Time in hours = dt / 3600000.0
    float dutyPercent = _currentValue / 255.0f;
    float instantPower = dutyPercent * _maxWattage;
    
    // Protect against weird overflow on first run
    if (dt > 0 && dt < 600000) { // Sanity check (max 10 min lag)
        _energyTotalWh += instantPower * (dt / 3600000.0);
    }

    // 2. Handle Fading
    bool changed = false;
    
    if (abs(_currentValue - _targetValue) > 0.5f) {
        if (_currentValue < _targetValue) {
            _currentValue += _fadeSpeed;
            if (_currentValue > _targetValue) _currentValue = _targetValue;
        } else {
            _currentValue -= _fadeSpeed;
            if (_currentValue < _targetValue) _currentValue = _targetValue;
        }
        
        // Write to Hardware
        ledcWrite(_channel, (int)_currentValue);
        changed = true;
    }

    return changed;
}

void LightChannel::setTarget(int target) {
    if (target < 0) target = 0;
    if (target > 255) target = 255;
    _targetValue = target;
}

void LightChannel::setDirect(int value) {
    setTarget(value);
    _currentValue = _targetValue;
    ledcWrite(_channel, _targetValue);
}

int LightChannel::getTarget() const {
    return _targetValue;
}

int LightChannel::getCurrent() const {
    return (int)_currentValue;
}

double LightChannel::getEnergyUsageWh() const {
    return _energyTotalWh;
}

double LightChannel::getCurrentPowerW() const {
    return (_currentValue / 255.0f) * _maxWattage;
}

void LightChannel::resetEnergy() {
    _energyTotalWh = 0;
}
